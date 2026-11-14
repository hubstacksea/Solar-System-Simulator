// Solar System Voyager — Copyright (c) 2026 hubstacksea
// MIT License (LICENSE を参照) — https://github.com/hubstacksea/Solar-System-Simulator
// 探査機の旅: 地球を出発してから目標に着くまでを探査機のそばから追う。何もない間は時間を速く進め、
// 出発・地球の重力圏の脱出・スイングバイ・目的外の天体への接近・到着では等倍まで落として知らせを出す。
// 行程は軌跡のデータ（assets/probes.bin）から作る。URL の ?voyage=名前&scene=場面 でその場面から始まる。
// main.cpp の一部で、main.cpp が決まった順に読み込む（単体ではコンパイルしない）

// 時間の進み方: できごとの前後 VOY_HOLD_SEC 秒は等倍、そこから離れるほど指数的に速くする（時定数 VOY_RAMP_SEC 秒）
static const double VOY_HOLD_SEC = 8.0, VOY_RAMP_SEC = 1.5;
static const double VOY_CARD_LEAD_SEC = 3000.0;      // できごとの知らせを出す範囲（前後のシミュレータの秒。実時間で 10 秒ほど）
// 巡航の速さの目安: 全行程をこの実時間 [秒] で進む速さ（ゆっくり・標準・速い。[ ] キーで切り替え）
static const double VOY_CRUISE_WALL_SEC[3] = {600.0, 240.0, 60.0};
static const double VOY_SCENE_LEAD_DAYS = 1.0;       // 場面を指定した時は、これだけ前から始める

// 終点を軌跡から決められない探査機（目標の天体がシミュレータにないか、到着の前にデータが終わる）
struct VoyageGoal {
    const char* probe;
    const char* date;       // "YYYYMMDDhhmm"（nullptr: データの終わり、"": 目標の天体への最接近）
    const char* body;       // 目標の天体の英語名（nullptr: なし）
    bool escape;            // 地球の重力圏の脱出を行程に入れるか
    const char* targetJp;
    const char* targetEn;
    const char* titleJp;
    const char* titleEn;
    const char* textJp;
    const char* textEn;
};
static const VoyageGoal VOYAGE_GOALS[] = {
    {"Parker Solar Probe", "202412241153", "Sun", true, "太陽", "the Sun", "太陽に最も近づいた", "Closest approach to the Sun",
     "7 回の金星スイングバイで軌道を縮め、太陽の表面から 約 610 万 km まで近づいた。\n人工物で最も太陽に近く、最も速い", 
     "Seven Venus flybys shrank its orbit until it came within about 6.1 million km of the Sun's surface,\nthe closest and fastest any human-made object has been"},
    {"James Webb Space Telescope", "202201241900", nullptr, false, "L2 点", "L2", "L2 点の周回軌道に到着",
     "Arrived in orbit around L2", "地球から 約 150 万 km 先、太陽と反対側の L2 点を回る軌道に入った。\nここで主鏡と観測装置を極低温に保って観測する",
     "It entered orbit around L2, about 1.5 million km from Earth on the side away from the Sun,\nwhere its mirror and instruments stay cryogenically cold"},
    {"Lucy", "", "Patroclus", true, "パトロクロス", "Patroclus", "トロヤ群の連星パトロクロスに到達",
     "Reached the Trojan binary Patroclus", "木星と同じ軌道を回るトロヤ群小惑星を巡る旅の最後の目標。パトロクロスはほぼ同じ大きさの\nメノイティオスと互いを回る連星（このシミュレータでは主星だけ。形は球で代えている）",
     "The last stop of its tour of Jupiter's Trojan asteroids: Patroclus, which orbits its near-twin Menoetius\n(only the primary is shown here, as a sphere)"},
    {"Psyche", nullptr, nullptr, true, "小惑星プシケ", "asteroid Psyche", "小惑星プシケへ向かう途中",
     "On the way to asteroid Psyche", "金属に富む小惑星プシケには 2029 年に着く予定。軌跡のデータはその手前までなので、ここで旅を終える",
     "It is due to reach the metal-rich asteroid Psyche in 2029. The trajectory data ends before that, so the voyage ends here"},
    {"JUICE", nullptr, "Jupiter", true, "木星", "Jupiter", "木星に到着する直前", "Just before reaching Jupiter",
     "2031 年 7 月に木星を回る軌道に入る予定。軌跡のデータはその手前までなので、ここで旅を終える",
     "It is due to enter orbit around Jupiter in July 2031. The trajectory data ends just before that, so the voyage ends here"},
};
static const VoyageGoal* FindVoyageGoal(const BodyDef& d) {
    for (const VoyageGoal& g : VOYAGE_GOALS)
        if (std::strcmp(g.probe, d.en) == 0) return &g;
    return nullptr;
}

// 延長ミッションの旅（2 番目の区間）: 地球スイングバイ（試料のカプセルの分離）から次の目標まで
struct VoyageLeg {
    const char* probe;
    const char* start;      // 出発にする地球への最接近の日付 "YYYYMMDD"（±3 日で探す）
    const char* body;       // 目標の天体の英語名
    const char* nameJp;
    const char* nameEn;
    const char* startTitleJp;
    const char* startTitleEn;
    const char* startTextJp;
    const char* startTextEn;
    const char* endTitleJp;
    const char* endTitleEn;
    const char* endTextJp;
    const char* endTextEn;
};
static const VoyageLeg VOYAGE_LEGS[] = {
    {"Hayabusa2", "20201205", "Torifune", "はやぶさ2（延長ミッション）", "Hayabusa2 (extended mission)",
     "カプセルを届けて延長ミッションへ", "Capsule delivered; on to the extended mission",
     "リュウグウの試料の入ったカプセルを分離してオーストラリアに届け、本体は地球をかすめて次の旅へ",
     "It released the capsule with the Ryugu samples, which landed in Australia, and swung past Earth on to its next voyage",
     "小惑星トリフネの近くを通過", "Flyby of asteroid Torifune",
     "延長ミッションで最初に近づく小惑星。このあと 2031 年に小惑星 1998 KY26 に着く予定",
     "The first asteroid of the extended mission; it is due to reach asteroid 1998 KY26 in 2031"},
    {"OSIRIS-REx", "20230924", "Apophis", "OSIRIS-APEX（延長ミッション）", "OSIRIS-APEX (extended mission)",
     "カプセルを届けて延長ミッションへ", "Capsule delivered; on to the extended mission",
     "ベンヌの試料の入ったカプセルを分離してユタ州に届け、本体は OSIRIS-APEX と名前を変えてアポフィスへ",
     "It released the capsule with the Bennu samples, which landed in Utah, and set off for Apophis as OSIRIS-APEX",
     "アポフィスに到着", "Arrived at Apophis",
     "2029 年 4 月 13 日に地球の中心から 3.8 万 km を通ったばかりのアポフィスに着き、\n地球の重力で表面や自転がどう変わったかを調べる",
     "It reaches Apophis just after its 13 April 2029 pass 38,000 km from Earth's center,\nto study how Earth's gravity changed its surface and spin"},
};
// 到着の場面の説明（イベント一覧の名場面に説明の無い探査機）
static const struct { const char* probe; const char* jp; const char* en; } VOYAGE_ARRIVAL_TEXT[] = {
    {"Hera", "DART が 2022 年に衝突したディモルフォスを調べに来た ESA の探査機。衝突でできたクレーターの大きさと深さ、\n衝突で形が変わったかを、2 機の小型衛星（ミラニとユベンタス）とともに調べる",
     "ESA's probe to study Dimorphos after DART's 2022 impact: the size and depth of the crater and whether the impact\nreshaped it, together with two CubeSats (Milani and Juventas)"},
};

// 巡航中に順に出すひとこと（探査機そのもの・目的・エピソード）。leg: 区間（1 が最初の旅、2 が延長ミッション）
static const struct VoyageTip { const char* probe; int leg; const char* jp; const char* en; } VOYAGE_TIPS[] = {
    {"Pioneer 10", 1, "重さ 258 kg。太陽から遠く太陽電池では電気が足りないので、プルトニウムの崩壊熱で発電する\n原子力電池（RTG）を 4 基積んでいる",
     "It weighs 258 kg. Sunlight is too weak out there for solar panels, so four RTGs make electricity\nfrom the heat of decaying plutonium"},
    {"Pioneer 10", 1, "初めて小惑星帯を通り抜けた探査機。通る前は、ちりにぶつかって壊れるのではと心配されていた",
     "The first spacecraft to cross the asteroid belt. Before it did, many feared dust impacts would destroy it"},
    {"Pioneer 10", 1, "機体には、人の姿と太陽系の中の地球の場所を刻んだ金属の板（パイオニア銘板）が付いている",
     "It carries a metal plaque engraved with human figures and the location of Earth in the solar system"},
    {"Pioneer 10", 1, "最後に信号を受けたのは 2003 年 1 月。今はおうし座のアルデバランの方向へ進み続けている",
     "Its last signal was received in January 2003. It is still coasting toward Aldebaran in Taurus"},
    {"Pioneer 10", 1, "TRW 社が作り、1972 年 3 月 3 日にアトラス・セントール ロケットでケープカナベラルから打ち上げた",
     "Built by TRW, it was launched on an Atlas-Centaur from Cape Canaveral on 3 March 1972"},
    {"Pioneer 10", 1, "1973 年 12 月、初めて木星のそばを通った探査機になった。地球とは直径 2.74 m のパラボラアンテナで通信した",
     "In December 1973 it became the first spacecraft to fly past Jupiter. It talked to Earth through a 2.74 m dish"},

    {"Pioneer 11", 1, "パイオニア10号の双子の探査機。同じ形で、パイオニア銘板も同じものを積んでいる",
     "The twin of Pioneer 10, with the same design and the same plaque"},
    {"Pioneer 11", 1, "木星の重力で向きを変えて土星へ向かった、初めての探査機。土星では細い F 環を見つけた",
     "The first spacecraft to use Jupiter's gravity to head for Saturn, where it discovered the narrow F ring"},
    {"Pioneer 11", 1, "1995 年に電力が尽きて運用を終えた。今はわし座の方向へ進んでいる",
     "Its power ran out in 1995. It is now heading toward the constellation Aquila"},
    {"Pioneer 11", 1, "重さ 259 kg。1973 年 4 月 6 日、アトラス・セントール ロケットでケープカナベラルから打ち上げた",
     "It weighs 259 kg and was launched on an Atlas-Centaur from Cape Canaveral on 6 April 1973"},
    {"Pioneer 11", 1, "1979 年 9 月、初めて土星のそばを通った探査機になった。雲の上 約 2 万 km まで近づいた",
     "In September 1979 it became the first spacecraft to fly past Saturn, passing about 20,000 km above the clouds"},

    {"Voyager 2", 1, "木星・土星・天王星・海王星が同じ側に並ぶ、約 175 年に一度の配置を使った「グランドツアー」。\n天王星と海王星を訪れた探査機は今もボイジャー2号だけ",
     "Its \"Grand Tour\" used an alignment of Jupiter, Saturn, Uranus and Neptune that comes about once in 175 years.\nIt is still the only spacecraft to have visited Uranus and Neptune"},
    {"Voyager 2", 1, "打ち上げはボイジャー1号より 16 日早い。ボイジャー1号の方が速い軌道で、先に木星に着いた",
     "It launched 16 days before Voyager 1, which took a faster path and reached Jupiter first"},
    {"Voyager 2", 1, "地球の音や画像、55 の言語のあいさつ、世界の音楽を収めた金色のレコード（ゴールデンレコード）を積んでいる",
     "It carries the Golden Record: sounds and images of Earth, greetings in 55 languages and music from around the world"},
    {"Voyager 2", 1, "2018 年 11 月、太陽風の届く範囲（太陽圏）を出て恒星間空間に入った",
     "In November 2018 it left the heliosphere, the bubble of the solar wind, and entered interstellar space"},
    {"Voyager 2", 1, "NASA のジェット推進研究所（JPL）が作った。1977 年 8 月 20 日、タイタン IIIE・セントール ロケットで\nケープカナベラルから打ち上げた",
     "Built by NASA's Jet Propulsion Laboratory (JPL), it was launched on a Titan IIIE-Centaur from Cape Canaveral\non 20 August 1977"},
    {"Voyager 2", 1, "重さ約 0.8 トン。電気は 3 基の原子力電池（RTG）でつくり、地球とは直径 3.7 m のパラボラアンテナで通信する",
     "It weighs about 0.8 tonnes. Three RTGs make its electricity, and a 3.7 m dish talks to Earth"},
    {"Voyager 2", 1, "1986 年 1 月に天王星、1989 年 8 月に海王星のそばを通った。海王星では北極の上 約 4,950 km まで近づいた",
     "It flew past Uranus in January 1986 and Neptune in August 1989, passing about 4,950 km above Neptune's north pole"},
    {"Voyager 2", 1, "海王星の衛星トリトンで、地面から黒い噴煙が高く噴き上がっているのを見つけた",
     "At Neptune's moon Triton it found dark plumes shooting high above the surface"},

    {"Voyager 1", 1, "土星では衛星タイタンのそばを通ることを選び、その重力で惑星の並ぶ面から北へはじき出された。\nそのため土星が最後の惑星になった",
     "At Saturn it was sent close to Titan, whose gravity flung it north out of the plane of the planets,\nso Saturn was its last planet"},
    {"Voyager 1", 1, "1990 年 2 月、約 60 億 km 先から振り返って地球を撮った。光の筋の中の小さな点の写真は\n「ペイル・ブルー・ドット」と呼ばれる",
     "In February 1990 it looked back from about 6 billion km and photographed Earth as a tiny dot in a sunbeam,\nthe \"Pale Blue Dot\""},
    {"Voyager 1", 1, "2012 年 8 月、人工物で初めて恒星間空間に入った。地球から最も遠くにある人工物で、今も信号を送っている",
     "In August 2012 it became the first human-made object in interstellar space. It is the most distant one\nand still sends signals home"},
    {"Voyager 1", 1, "NASA のジェット推進研究所（JPL）が作った。1977 年 9 月 5 日、タイタン IIIE・セントール ロケットで\nケープカナベラルから打ち上げた",
     "Built by NASA's Jet Propulsion Laboratory (JPL), it was launched on a Titan IIIE-Centaur from Cape Canaveral\non 5 September 1977"},
    {"Voyager 1", 1, "重さ約 0.8 トン。ボイジャー2号と同じ形で、同じゴールデンレコードを積んでいる",
     "It weighs about 0.8 tonnes and is identical to Voyager 2, with the same Golden Record on board"},
    {"Voyager 1", 1, "1979 年 3 月の木星の接近で撮った写真から、衛星イオで噴火している火山が見つかった。\n地球の外で初めて見つかった活火山",
     "Its images from the March 1979 Jupiter flyby revealed erupting volcanoes on Io, the first active volcanoes\nfound beyond Earth"},

    {"Galileo", 1, "スペースシャトル・アトランティスから打ち上げた。上段のロケットの力が足りなかったので、金星と地球で 3 回の\nスイングバイを重ねて木星へ向かった",
     "It was launched from the Space Shuttle Atlantis. With a weak upper stage, it needed three gravity assists\nat Venus and Earth to reach Jupiter"},
    {"Galileo", 1, "途中で傘のような主アンテナが開かなくなり、小さなアンテナとデータの圧縮で観測を続けた",
     "Its umbrella-like main antenna failed to open, so it carried on with a small antenna and data compression"},
    {"Galileo", 1, "1993 年、小惑星イダのまわりを回る小さな衛星ダクティルを見つけた。小惑星の衛星の初めての発見",
     "In 1993 it found Dactyl circling the asteroid Ida, the first known moon of an asteroid"},
    {"Galileo", 1, "1994 年、シューメーカー・レビー第 9 彗星のかけらが木星にぶつかるのを、地球からは見えない側を直接見た",
     "In 1994 it had a direct view of fragments of Comet Shoemaker-Levy 9 striking the side of Jupiter hidden from Earth"},
    {"Galileo", 1, "木星に着く 5 か月前に切り離した突入機が、木星の大気に入って直接測った。\n2003 年には、生命がいるかもしれないエウロパを汚さないよう、本体も木星に突入させて運用を終えた",
     "A probe released five months before arrival plunged into Jupiter's atmosphere and measured it directly.\nIn 2003 the orbiter itself was sent into Jupiter so it could never contaminate Europa"},
    {"Galileo", 1, "JPL が作り、1989 年 10 月 18 日にスペースシャトルで打ち上げた。木星の大気に入る突入機を含めて重さ約 2.6 トン",
     "Built by JPL, it was launched by the Space Shuttle on 18 October 1989. With its atmospheric probe it weighed\nabout 2.6 tonnes"},
    {"Galileo", 1, "1991 年 10 月、小惑星ガスプラのそばを通った。小惑星に近づいて撮影した初めての探査機",
     "In October 1991 it flew past the asteroid Gaspra, the first spacecraft to see an asteroid up close"},
    {"Galileo", 1, "1995 年 12 月、初めて木星を回る軌道に入った探査機になった。磁場の観測から、\nエウロパの氷の下には塩水の海があると考えられるようになった",
     "In December 1995 it became the first spacecraft to orbit Jupiter. Its magnetic field measurements suggested\na salty ocean under Europa's ice"},

    {"Cassini", 1, "NASA・ESA・イタリア宇宙機関の共同の探査機。燃料を入れると約 5.7 トンで、惑星探査機でも最大級",
     "A joint mission of NASA, ESA and the Italian Space Agency. Fully fuelled it weighed about 5.7 tonnes,\none of the largest planetary probes"},
    {"Cassini", 1, "ESA の着陸機ホイヘンスを積んでいて、2005 年 1 月にタイタンに降ろした。地球の月以外の衛星への初めての着陸",
     "It carried ESA's Huygens probe, which landed on Titan in January 2005, the first landing on a moon other than ours"},
    {"Cassini", 1, "木星のそばを通った時は、木星を回っていたガリレオと同時に木星を観測した",
     "Passing Jupiter, it observed the planet together with Galileo, which was orbiting there"},
    {"Cassini", 1, "燃料が尽きる前の 2017 年 9 月、衛星を汚さないよう土星の大気に飛び込んで運用を終えた（グランドフィナーレ）",
     "In September 2017, before its fuel ran out, it dived into Saturn's atmosphere so it could never contaminate\nthe moons (the Grand Finale)"},
    {"Cassini", 1, "JPL が作り、1997 年 10 月 15 日にタイタン IVB・セントール ロケットでケープカナベラルから打ち上げた。\n高さ約 6.8 m",
     "Built by JPL, it was launched on a Titan IVB-Centaur from Cape Canaveral on 15 October 1997.\nIt stands about 6.8 m tall"},
    {"Cassini", 1, "2004 年 7 月 1 日に土星を回る軌道に入り、それから 13 年あまり土星を回った",
     "It entered orbit around Saturn on 1 July 2004 and circled it for more than 13 years"},
    {"Cassini", 1, "2005 年、衛星エンケラドスの南極から水蒸気や氷の粒が噴き出しているのを見つけた。氷の下に海がある証拠になった",
     "In 2005 it found water vapour and ice grains jetting from Enceladus's south pole, evidence of an ocean under the ice"},
    {"Cassini", 1, "最後の 5 か月で、土星と環の間のすき間を 22 回くぐった",
     "In its last five months it dived 22 times through the gap between Saturn and its rings"},

    {"Rosetta", 1, "名前は、古代エジプトの文字を解読する手がかりになったロゼッタ・ストーンから。\n着陸機フィラエの名前は、解読を助けたオベリスクが見つかったナイル川のフィラエ島から",
     "It is named after the Rosetta Stone, the key to Egyptian hieroglyphs. Its lander Philae is named after\nthe Nile island whose obelisk also helped the decipherment"},
    {"Rosetta", 1, "電気は太陽電池でつくるので、太陽から遠く離れて電気が足りなくなる間は、ほとんどの機器を止めて眠った。\n眠っていたのは 2011 年 6 月から 2014 年 1 月までの 31 か月。目覚めの合図が届いた時、管制室は歓声に包まれた",
     "It ran on solar power, so while it was too far from the Sun it shut nearly everything down and slept for\n31 months, June 2011 to January 2014. The control room cheered when its wake-up signal arrived"},
    {"Rosetta", 1, "途中で小惑星シュテインス（2008 年）とルテティア（2010 年）のそばを通って撮影した",
     "On the way it flew past and imaged the asteroids Steins (2008) and Lutetia (2010)"},
    {"Rosetta", 1, "2016 年 9 月、2 年あまり回った彗星の核の表面にゆっくり降りて運用を終えた",
     "In September 2016, after more than two years in orbit, it was set down gently on the comet's nucleus"},
    {"Rosetta", 1, "ドイツのアストリウム社を中心に作り、2004 年 3 月 2 日にアリアン 5 ロケットで\n南米のフランス領ギアナにあるクールーから打ち上げた",
     "Built with Astrium Germany as prime contractor, it was launched on an Ariane 5 from Kourou, French Guiana,\non 2 March 2004"},
    {"Rosetta", 1, "重さ約 3 トンで、その半分あまりは燃料。2 枚の太陽電池パネルは端から端まで約 32 m",
     "It weighed about 3 tonnes, more than half of it propellant. Its two solar wings span about 32 m tip to tip"},
    {"Rosetta", 1, "2014 年 8 月、初めて彗星を回る探査機になった。11 月 12 日に降ろしたフィラエは、体を固定する銛が働かず\n2 回跳ね、日の当たりにくい崖の陰に止まった",
     "In August 2014 it became the first spacecraft to orbit a comet. Philae, released on 12 November, failed to fire\nits anchoring harpoons, bounced twice and came to rest in the shadow of a cliff"},

    {"New Horizons", 1, "地球を出る時の速さは、打ち上げた探査機で最も速い。月の軌道を 9 時間ほどで横切った",
     "It left Earth faster than any other spacecraft launched, crossing the Moon's orbit in about nine hours"},
    {"New Horizons", 1, "冥王星を見つけたクライド・トンボーの遺灰の一部を積んでいる",
     "It carries some of the ashes of Clyde Tombaugh, who discovered Pluto"},
    {"New Horizons", 1, "打ち上げから 7 か月後の 2006 年 8 月、冥王星は惑星から準惑星に分け直された",
     "Seven months after launch, in August 2006, Pluto was reclassified from planet to dwarf planet"},
    {"New Horizons", 1, "木星のスイングバイで冥王星までの旅が約 3 年短くなった。冥王星のあと、2019 年 1 月 1 日に\nカイパーベルト天体アロコスのそばも通った",
     "The Jupiter flyby cut about three years off the trip. After Pluto it flew past the Kuiper belt object Arrokoth\non 1 January 2019"},
    {"New Horizons", 1, "ジョンズ・ホプキンズ大学応用物理研究所（APL）が作った。重さ 478 kg で、グランドピアノほどの大きさ",
     "Built by the Johns Hopkins Applied Physics Laboratory (APL), it weighs 478 kg and is about the size\nof a grand piano"},
    {"New Horizons", 1, "2006 年 1 月 19 日、アトラス V ロケットでケープカナベラルから打ち上げた。電気は 1 基の原子力電池（RTG）でつくる",
     "It was launched on an Atlas V from Cape Canaveral on 19 January 2006. A single RTG makes its electricity"},
    {"New Horizons", 1, "2015 年 7 月 14 日、冥王星の表面から約 1 万 2,500 km を通った。集めたデータをすべて地球に送るのに\n1 年以上かかった",
     "On 14 July 2015 it passed about 12,500 km from Pluto's surface. Sending all the data home took more\nthan a year"},

    {"Juno", 1, "木星まで行った探査機で初めて、太陽電池だけで動く。長さ約 9 m の太陽電池パネルが 3 枚",
     "The first spacecraft to work at Jupiter on solar power alone, with three solar arrays about 9 m long"},
    {"Juno", 1, "木星の強い放射線から電子機器を守るため、壁の厚さ約 1 cm のチタンの箱に入れている",
     "Its electronics sit in a titanium vault with walls about 1 cm thick to survive Jupiter's radiation"},
    {"Juno", 1, "木星の極の上を通る細長い軌道を回り、雲の下の構造や重力、磁場を調べる。\n機体にはガリレオ・ガリレイ、ユーピテル、ユーノーの 3 体のレゴ人形が乗っている",
     "From a long orbit over Jupiter's poles it studies the planet's interior, gravity and magnetic field.\nIt also carries three LEGO figures: Galileo Galilei, Jupiter and Juno"},
    {"Juno", 1, "ロッキード・マーチン社が作り、2011 年 8 月 5 日にアトラス V ロケットでケープカナベラルから打ち上げた。\n燃料を入れた重さは約 3.6 トン",
     "Built by Lockheed Martin, it was launched on an Atlas V from Cape Canaveral on 5 August 2011.\nFully fuelled it weighed about 3.6 tonnes"},
    {"Juno", 1, "2016 年 7 月 4 日（アメリカ時間）、木星を回る軌道に入った",
     "It entered orbit around Jupiter on 4 July 2016 (US time)"},
    {"Juno", 1, "カメラ JunoCam の画像は誰でも使えるように公開され、世界中の人が処理して木星の雲の絵を作っている",
     "Raw images from its JunoCam camera are published for anyone to use, and people around the world process them\ninto pictures of Jupiter's clouds"},

    {"Hayabusa2", 1, "イオンエンジンで小惑星リュウグウへ向かった。初代はやぶさのトラブルの経験を生かして作られた",
     "Ion engines carried it to the asteroid Ryugu. It was built on the lessons of the troubled first Hayabusa"},
    {"Hayabusa2", 1, "2018 年、小型ローバー MINERVA-II1 の 2 台が、小惑星の表面を初めて跳ねて移動した",
     "In 2018 its two MINERVA-II1 rovers became the first to hop across the surface of an asteroid"},
    {"Hayabusa2", 1, "2019 年、2 kg の銅の塊を秒速 2 km で撃ち込んで人工のクレーターを作り、その近くに降りて\n地下から飛び出した物質も集めた",
     "In 2019 it fired a 2 kg copper projectile at 2 km/s to make an artificial crater, then touched down nearby\nto collect material thrown up from below the surface"},
    {"Hayabusa2", 1, "地球に届けた試料は約 5.4 g。アミノ酸などの有機物が見つかった",
     "It delivered about 5.4 g of samples, in which amino acids and other organic molecules were found"},
    {"Hayabusa2", 1, "NEC が作り、2014 年 12 月 3 日に H-IIA ロケットで種子島宇宙センターから打ち上げた。重さ約 600 kg",
     "Built by NEC, it was launched on an H-IIA from the Tanegashima Space Center on 3 December 2014.\nIt weighed about 600 kg"},
    {"Hayabusa2", 1, "ドイツとフランスが作った小型の着陸機 MASCOT も積んでいて、2018 年 10 月にリュウグウの表面に降ろした",
     "It also carried MASCOT, a small German-French lander, which it set down on Ryugu in October 2018"},
    {"Hayabusa2", 1, "試料を入れたカプセルは、2020 年 12 月 6 日（日本時間）にオーストラリアのウーメラ砂漠に着地した",
     "The sample capsule landed in the Woomera desert in Australia on 6 December 2020 (Japan time)"},
    {"Hayabusa2", 2, "延長ミッションの最後の目標、小惑星 1998 KY26 は 10 分ほどで 1 回転する、とても速く自転する小さな小惑星",
     "The final target of the extended mission, 1998 KY26, is a small asteroid that spins once about every 10 minutes"},
    {"Hayabusa2", 2, "1998 KY26 に着くまでに、地球のスイングバイを 2 回重ねる",
     "It will swing past Earth twice more before reaching 1998 KY26"},
    {"Hayabusa2", 2, "1998 KY26 に着くのは 2031 年の予定。打ち上げから 16 年あまりの旅になる",
     "It is due to reach 1998 KY26 in 2031, more than 16 years after launch"},

    {"OSIRIS-REx", 1, "目標のベンヌは、22 世紀後半に地球にぶつかる可能性がわずかにある小惑星",
     "Its target Bennu is an asteroid with a small chance of hitting Earth in the late 22nd century"},
    {"OSIRIS-REx", 1, "2020 年 10 月の採取で腕の先を表面に押し当てると、表面は思ったよりずっと柔らかく、\nそのまま沈み込みかけた。ベンヌの表面は固まっていない石の山だった",
     "When it pressed its sampling head to the surface in October 2020, the ground was far softer than expected\nand it began to sink in. Bennu's surface is loosely packed rubble"},
    {"OSIRIS-REx", 1, "試料が多く取れすぎてふたがきちんと閉まらず、少しこぼれていた。地球に届いたのは約 122 g",
     "It collected so much that the lid could not close properly and some escaped. About 122 g reached Earth"},
    {"OSIRIS-REx", 1, "ロッキード・マーチン社が作り、2016 年 9 月 8 日にアトラス V ロケットでケープカナベラルから打ち上げた。\n燃料を入れた重さは約 2.1 トン",
     "Built by Lockheed Martin, it was launched on an Atlas V from Cape Canaveral on 8 September 2016.\nFully fuelled it weighed about 2.1 tonnes"},
    {"OSIRIS-REx", 1, "ベンヌは直径約 500 m の、そろばん玉のような形の小惑星。表面から小石が宇宙へ飛び出しているのも見つかった",
     "Bennu is a spinning-top-shaped asteroid about 500 m across. The spacecraft even saw pebbles being flung\nfrom its surface into space"},
    {"OSIRIS-REx", 1, "試料のカプセルは 2023 年 9 月 24 日、アメリカのユタ州の砂漠に着地した。NASA が小惑星の試料を持ち帰ったのは初めて",
     "The sample capsule landed in the Utah desert on 24 September 2023, NASA's first asteroid sample return"},
    {"OSIRIS-REx", 2, "アポフィスは 2004 年の発見の直後、2029 年に地球にぶつかる確率が 2.7% と計算されて注目された\n（今は衝突しないことがわかっている）",
     "Soon after its discovery in 2004, Apophis was briefly given a 2.7% chance of hitting Earth in 2029\n(it is now known it will not)"},
    {"OSIRIS-REx", 2, "2029 年 4 月 13 日の接近では、アポフィスはヨーロッパやアフリカから肉眼で見えるほど明るくなる",
     "During the 13 April 2029 approach, Apophis will be visible to the naked eye from Europe and Africa"},
    {"OSIRIS-REx", 2, "試料を届けた後、延長ミッションでは名前を OSIRIS-APEX（オシリス・エイペックス）に変えた",
     "After delivering its sample, the spacecraft was renamed OSIRIS-APEX for its extended mission"},

    {"Parker Solar Probe", 1, "太陽側に厚さ約 11 cm の炭素の熱シールドがあり、表面が 1,000℃ を超えても、陰の機器は室温ほどに保たれる",
     "A carbon heat shield about 11 cm thick faces the Sun. Its front exceeds 1,000 °C while the instruments\nbehind it stay near room temperature"},
    {"Parker Solar Probe", 1, "名前は太陽風を予言した物理学者ユージン・パーカーから。存命中の人の名前が付いた初めての NASA の探査機",
     "It is named after Eugene Parker, who predicted the solar wind, the first NASA spacecraft named after a living person"},
    {"Parker Solar Probe", 1, "2021 年 4 月、初めて太陽のコロナの中に入った。最も近い所では秒速 190 km を超える",
     "In April 2021 it became the first spacecraft to fly through the Sun's corona. At its closest it moves\nat more than 190 km/s"},
    {"Parker Solar Probe", 1, "APL が作り、2018 年 8 月 12 日にデルタ IV ヘビー ロケットでケープカナベラルから打ち上げた。重さ約 700 kg",
     "Built by APL, it was launched on a Delta IV Heavy from Cape Canaveral on 12 August 2018. It weighs about 700 kg"},
    {"Parker Solar Probe", 1, "金星のスイングバイを 7 回重ねて、太陽にだんだん近づいた。2024 年 12 月 24 日には\n太陽の表面から約 610 万 km まで近づいた",
     "Seven Venus flybys brought it ever closer to the Sun. On 24 December 2024 it passed about 6.1 million km\nfrom the Sun's surface"},
    {"Parker Solar Probe", 1, "パーカー博士への献辞の板と、一般から寄せられた 113 万人あまりの名前を収めたメモリーカードを積んでいる",
     "It carries a plaque dedicated to Dr Parker and a memory card with more than 1.13 million names sent in\nby the public"},

    {"BepiColombo", 1, "ESA の MPO と JAXA の「みお」の 2 機の水星探査機を重ねて運び、水星に着いてから分かれる",
     "It carries two Mercury orbiters stacked together, ESA's MPO and JAXA's Mio, which separate on arrival"},
    {"BepiColombo", 1, "太陽に向かって落ちると速くなりすぎるので、地球 1 回・金星 2 回・水星 6 回のスイングバイで\n少しずつブレーキをかける",
     "Falling toward the Sun makes it too fast, so it brakes with gravity assists: one at Earth, two at Venus\nand six at Mercury"},
    {"BepiColombo", 1, "名前は、水星の自転と公転の関係を解き明かし、マリナー10号が水星に何度も近づける軌道を考えたジュゼッペ・コロンボから",
     "It is named after Giuseppe Colombo, who explained Mercury's spin-orbit resonance and devised\nthe orbit that let Mariner 10 return to Mercury again and again"},
    {"BepiColombo", 1, "電気推進のエンジンの不調で、水星を回る軌道に入るのは 2025 年 12 月から 2026 年 11 月に延びた",
     "A problem with its electric propulsion delayed its arrival in Mercury orbit from December 2025 to November 2026"},
    {"BepiColombo", 1, "エアバス社を中心に 16 か国の 80 あまりの会社が作り、2018 年 10 月 20 日にアリアン 5 ロケットで\nフランス領ギアナのクールーから打ち上げた",
     "Built by more than 80 companies in 16 countries led by Airbus, it was launched on an Ariane 5 from Kourou,\nFrench Guiana, on 20 October 2018"},
    {"BepiColombo", 1, "打ち上げ時の重さは約 4.1 トン。水星に着くまでは、電気推進のエンジンを持つ運搬用のモジュール（MTM）が\n2 機を運ぶ",
     "It weighed about 4.1 tonnes at launch. Until arrival a transfer module (MTM) with electric engines carries\nthe two orbiters"},

    {"Lucy", 1, "名前は約 320 万年前の人類の祖先の化石「ルーシー」から。トロヤ群小惑星は惑星ができた頃の\n「化石」と考えられている",
     "It is named after \"Lucy\", the 3.2-million-year-old fossil of a human ancestor; the Trojan asteroids\nare thought to be fossils of planet formation"},
    {"Lucy", 1, "直径 7 m あまりの円い太陽電池パネルを 2 枚、扇のように広げている",
     "It unfurled two round solar arrays, each more than 7 m across, like folding fans"},
    {"Lucy", 1, "2023 年 11 月、小惑星ディンキネシュに小さな衛星セラムを見つけた。セラムは 2 つの塊がくっついた形だった",
     "In November 2023 it found that the asteroid Dinkinesh has a small moon, Selam, which turned out\nto be two lobes stuck together"},
    {"Lucy", 1, "1 機でこれまでで最も多くの小惑星を訪れる。木星の前と後ろの 2 つのトロヤ群を両方巡る",
     "It will visit more asteroids than any other mission, touring both Trojan swarms ahead of and behind Jupiter"},
    {"Lucy", 1, "ロッキード・マーチン社が作り、2021 年 10 月 16 日にアトラス V ロケットでケープカナベラルから打ち上げた。\n燃料を入れた重さは約 1.5 トン",
     "Built by Lockheed Martin, it was launched on an Atlas V from Cape Canaveral on 16 October 2021.\nFully fuelled it weighed about 1.5 tonnes"},
    {"Lucy", 1, "化石のルーシーの名前は、ビートルズの歌「ルーシー・イン・ザ・スカイ・ウィズ・ダイアモンズ」から",
     "The fossil Lucy was itself named after the Beatles song \"Lucy in the Sky with Diamonds\""},
    {"Lucy", 1, "未来の人へのメッセージの板を積んでいる。ビートルズのメンバーなどの言葉と、打ち上げの日の惑星の位置の図",
     "It carries a plaque for future generations, with words from the Beatles and others and a diagram\nof the planets' positions on launch day"},

    {"DART", 1, "小惑星の軌道を体当たりで変えられるかを確かめる、初めての惑星防衛の実験",
     "The first planetary defence test: can an impact change an asteroid's orbit?"},
    {"DART", 1, "衝突の様子は、15 日前に切り離したイタリアの小型衛星 LICIACube が撮影した",
     "The Italian CubeSat LICIACube, released 15 days earlier, photographed the impact"},
    {"DART", 1, "2022 年 9 月 26 日の衝突で、ディモルフォスがディディモスを回る周期は約 33 分短くなった",
     "The impact on 26 September 2022 shortened Dimorphos's orbit around Didymos by about 33 minutes"},
    {"DART", 1, "APL が作り、2021 年 11 月 24 日にファルコン 9 ロケットでカリフォルニア州のバンデンバーグから打ち上げた。\n重さ約 600 kg",
     "Built by APL, it was launched on a Falcon 9 from Vandenberg, California, on 24 November 2021.\nIt weighed about 600 kg"},
    {"DART", 1, "最後はカメラ DRACO の画像を見ながら自分で狙いを定め、秒速約 6.1 km でぶつかった",
     "In its final hours it steered itself using images from its DRACO camera and struck at about 6.1 km/s"},

    {"James Webb Space Telescope", 1, "主鏡は金でおおった 18 枚の六角形の鏡を合わせた直径 6.5 m。ロケットに入るよう折りたたんで打ち上げた",
     "Its 6.5 m mirror is made of 18 gold-coated hexagonal segments, folded to fit inside the rocket"},
    {"James Webb Space Telescope", 1, "テニスコートほどの 5 層の日よけで太陽・地球・月の光と熱をさえぎり、赤外線で遠い宇宙の\n初期の銀河を見る",
     "A five-layer sunshield the size of a tennis court blocks the light and heat of the Sun, Earth and Moon,\nso it can see the earliest galaxies in infrared"},
    {"James Webb Space Telescope", 1, "2021 年 12 月 25 日にアリアン 5 で打ち上げた。軌道がとても正確で燃料が節約でき、\n20 年以上観測できる見込み",
     "It launched on an Ariane 5 on 25 December 2021. The launch was so accurate that the saved fuel\nshould let it work for more than 20 years"},
    {"James Webb Space Telescope", 1, "NASA・ESA・カナダ宇宙庁の共同の宇宙望遠鏡。ノースロップ・グラマン社が中心になって作った。重さ約 6.2 トン",
     "A joint telescope of NASA, ESA and the Canadian Space Agency, built with Northrop Grumman as prime contractor.\nIt weighs about 6.2 tonnes"},
    {"James Webb Space Telescope", 1, "太陽と反対側、地球から約 150 万 km 先のラグランジュ点 L2 のまわりを回る。打ち上げから約 1 か月で着いた",
     "It orbits the L2 Lagrange point, about 1.5 million km from Earth on the side away from the Sun,\nwhich it reached about a month after launch"},
    {"James Webb Space Telescope", 1, "日よけは長さ約 21 m・幅約 14 m。打ち上げ後、2 週間ほどかけて少しずつ広げた",
     "Its sunshield is about 21 m by 14 m and was unfolded step by step over about two weeks after launch"},

    {"JUICE", 1, "ESA の木星氷衛星探査機。最後はガニメデを回る、地球の月以外の衛星を回る初めての探査機になる",
     "ESA's Jupiter Icy Moons Explorer. It will end up orbiting Ganymede, the first spacecraft to orbit a moon\nother than ours"},
    {"JUICE", 1, "2024 年 8 月、月と地球のそばを 1 日おいて続けて通る、初めての「月・地球スイングバイ」をした",
     "In August 2024 it made the first lunar-Earth flyby, passing the Moon and then Earth a day later"},
    {"JUICE", 1, "打ち上げ後、長さ 16 m のレーダーのアンテナが引っかかって開かず、機体を揺らすなどして 1 か月後に開いた",
     "After launch its 16 m radar antenna was stuck; it was freed a month later, in part by jolting the spacecraft"},
    {"JUICE", 1, "エアバス社が作り、2023 年 4 月 14 日にアリアン 5 ロケットでクールーから打ち上げた。\n燃料を入れた重さは約 6 トン",
     "Built by Airbus, it was launched on an Ariane 5 from Kourou on 14 April 2023. Fully fuelled it weighed\nabout 6 tonnes"},
    {"JUICE", 1, "太陽電池パネルは合わせて約 85 平方メートル。木星のあたりでは太陽の光の強さが地球の約 27 分の 1 しかない",
     "Its solar arrays total about 85 square metres: at Jupiter sunlight is only about 1/27 as strong as at Earth"},
    {"JUICE", 1, "木星には 2031 年 7 月に着き、2034 年 12 月にガニメデを回る軌道に入る予定",
     "It is due to reach Jupiter in July 2031 and to enter orbit around Ganymede in December 2034"},

    {"Psyche", 1, "目標の小惑星プシケは、惑星になりかけた天体の金属の核がむき出しになったものかもしれない",
     "Its target, asteroid Psyche, may be the exposed metal core of a planet-building body"},
    {"Psyche", 1, "ホール効果スラスターという電気推進で進む。キセノンを使い、青い光を出す",
     "It is driven by Hall-effect thrusters, electric engines that use xenon and glow blue"},
    {"Psyche", 1, "レーザー光で地球と通信する実験（深宇宙光通信）も載せていて、2023 年 12 月には約 3,100 万 km 先から猫の動画を送った",
     "It also carries a laser communication experiment (Deep Space Optical Communications) that in December 2023\nsent a cat video from about 31 million km away"},
    {"Psyche", 1, "JPL が中心になり、機体の本体はマクサー社が作った。2023 年 10 月 13 日にファルコン・ヘビー ロケットで\nケネディ宇宙センターから打ち上げた",
     "Led by JPL, with the main body built by Maxar, it was launched on a Falcon Heavy from Kennedy Space Center\non 13 October 2023"},
    {"Psyche", 1, "燃料を入れた重さは約 2.7 トン。火星のスイングバイ（2026 年 5 月）で向きを変え、2029 年に小惑星プシケに着く予定",
     "Fully fuelled it weighed about 2.7 tonnes. A Mars flyby in May 2026 bends its path toward Psyche,\nwhich it is due to reach in 2029"},

    {"Hera", 1, "ESA の初めての惑星防衛のミッション。名前はギリシア神話の女神ヘラから",
     "ESA's first planetary defence mission, named after the Greek goddess Hera"},
    {"Hera", 1, "2025 年 3 月の火星スイングバイで、火星の小さな衛星ダイモスを撮影した",
     "During its March 2025 Mars flyby it imaged the small Martian moon Deimos"},
    {"Hera", 1, "DART の衝突で、ディモルフォスがどれほど重く、どれほど動いたかを測り、体当たりの効果を確かめる",
     "It will measure Dimorphos's mass and how much it moved, to work out exactly how effective DART's impact was"},
    {"Hera", 1, "ドイツのブレーメンにある OHB 社を中心に作り、2024 年 10 月 7 日にファルコン 9 ロケットで\nケープカナベラルから打ち上げた。燃料を入れた重さは約 1.1 トン",
     "Built by a consortium led by OHB in Bremen, Germany, it was launched on a Falcon 9 from Cape Canaveral\non 7 October 2024. Fully fuelled it weighed about 1.1 tonnes"},

    {"Europa Clipper", 1, "NASA が作った惑星探査機で最大。太陽電池パネルを広げると端から端まで約 30 m",
     "The largest planetary spacecraft NASA has built: about 30 m across with its solar arrays deployed"},
    {"Europa Clipper", 1, "エウロパの氷の下の海が生命のすめる環境かを調べる。木星を回りながらエウロパのそばを約 50 回通る",
     "It will investigate whether the ocean under Europa's ice could support life, flying past Europa about 50 times\nwhile orbiting Jupiter"},
    {"Europa Clipper", 1, "機体には詩人エイダ・リモンが書いた詩と、世界中から寄せられた 260 万人あまりの名前が載っている",
     "It carries a poem by Ada Limón and the names of more than 2.6 million people from around the world"},
    {"Europa Clipper", 1, "JPL と APL が作り、2024 年 10 月 14 日にファルコン・ヘビー ロケットでケネディ宇宙センターから打ち上げた。\n燃料を入れた重さは約 6 トンで、その半分近くが燃料",
     "Built by JPL and APL, it was launched on a Falcon Heavy from Kennedy Space Center on 14 October 2024.\nFully fuelled it weighed about 6 tonnes, nearly half of it propellant"},
    {"Europa Clipper", 1, "木星には 2030 年 4 月に着く予定。途中で火星（2025 年 3 月）と地球（2026 年 12 月）でスイングバイする",
     "It is due to reach Jupiter in April 2030, after gravity assists at Mars (March 2025) and Earth (December 2026)"},
};

// 探査機 d の区間 leg（1 が最初の旅、2 以降が延長ミッション）
static const VoyageLeg* FindVoyageLeg(const BodyDef& d, int leg) {
    int n = 1;
    for (const VoyageLeg& l : VOYAGE_LEGS)
        if (std::strcmp(l.probe, d.en) == 0 && ++n == leg) return &l;
    return nullptr;
}

// 地球から出発する軌跡を持つ探査機か（地球を回る人工衛星は除く）
static bool VoyageCapable(const App& app, int i) {
    const BodyDef& d = app.B[i].d;
    if (d.kind != KIND_PROBE || d.tleN > 0 || d.probe < 0 || d.probe >= (int)g_probes.size()) return false;
    const std::vector<ProbeSeg>& S = g_probes[d.probe].segs;
    return !S.empty() && S.front().body == B_EARTH;
}

// 区間の出発の時刻（最初の旅は軌跡の始まり、延長ミッションは地球への最接近の 6 時間前）。見つからなければ NaN
static double VoyageStart(const App& app, int i, int leg) {
    const std::vector<ProbeSeg>& S = g_probes[app.B[i].d.probe].segs;
    if (leg <= 1) return S.front().t.front();
    const VoyageLeg* l = FindVoyageLeg(app.B[i].d, leg);
    if (!l) return NAN;
    double day = ParseDateDigits(l->start);
    for (const AstroEvent& ev : app.probeEvents)
        if (ev.probe == i && ev.kind == EV_FLYBY && ev.body == B_EARTH && std::fabs(ev.t - day) < 3) return ev.t - 0.25;
    return NAN;
}

// 旅の終点: 時刻・目標の天体・できごとの種類（EV_ARRIVE / EV_FLYBY / EV_IMPACT。到着しないものは EV_END）
static bool VoyageEnd(const App& app, int i, int leg, double* t, int* body, int* kind) {
    const std::vector<ProbeSeg>& S = g_probes[app.B[i].d.probe].segs;
    double dataEnd = S.back().t.back();
    *body = -1;
    *kind = EV_END;
    if (leg > 1) {   // 延長ミッション: 出発より後の、目標の天体への到着か最接近
        const VoyageLeg* l = FindVoyageLeg(app.B[i].d, leg);
        double t0 = VoyageStart(app, i, leg);
        if (!l || !std::isfinite(t0)) return false;
        for (const AstroEvent& ev : app.probeEvents)
            if (ev.probe == i && ev.t > t0 && (ev.kind == EV_ARRIVE || ev.kind == EV_FLYBY) &&
                std::strcmp(app.B[ev.body].d.en, l->body) == 0) {
                *t = ev.t;
                *body = ev.body;
                *kind = ev.kind;
                return true;
            }
        return false;
    }
    if (const VoyageGoal* g = FindVoyageGoal(app.B[i].d)) {
        *t = g->date && g->date[0] ? ParseDateDigits(g->date) : dataEnd - 1.0 / 86400;
        if (g->body)
            for (size_t k = 0; k < app.B.size(); k++)
                if (std::strcmp(app.B[k].d.en, g->body) == 0) *body = (int)k;
        if (g->date && !g->date[0])
            for (const AstroEvent& ev : app.probeEvents)
                if (ev.probe == i && ev.body == *body && ev.kind == EV_FLYBY) {
                    *t = ev.t;
                    *kind = EV_FLYBY;
                }
        return true;
    }
    const AstroEvent* last = nullptr;
    for (const AstroEvent& ev : app.probeEvents) {
        if (ev.probe != i) continue;
        if (ev.kind == EV_ARRIVE || ev.kind == EV_IMPACT) { last = &ev; break; }
        if (ev.kind == EV_FLYBY) last = &ev;
    }
    if (!last) return false;
    *t = last->t;
    *body = last->body;
    *kind = last->kind;
    if (last->kind == EV_IMPACT) *t = dataEnd - 0.5 / 86400;   // 衝突の直前（データの最後の点が衝突の瞬間）
    return true;
}

// 行程を作る。巡航中は 1 日ごと、惑星の近くでは 30 分ごとに調べる
static void BuildVoyage(const App& app, int i, int leg, std::vector<VoyageStep>* steps, double* length) {
    steps->clear();
    *length = 0;
    const BodyDef& d = app.B[i].d;
    const std::vector<ProbeSeg>& S = g_probes[d.probe].segs;
    double t0 = VoyageStart(app, i, leg), tEnd;
    int endBody, endKind;
    if (!std::isfinite(t0) || !VoyageEnd(app, i, leg, &tEnd, &endBody, &endKind)) return;
    const VoyageGoal* goal = leg <= 1 ? FindVoyageGoal(d) : nullptr;
    std::vector<Body> B = app.B;
    for (Body& b : B) b.orbit.clear();   // 軌道の線は引かないので計算を省く
    auto distTo = [&](int k, double t) {
        UpdateBodies(B, t);
        return Len(B[i].pos - B[k].pos);
    };
    auto helioSpeed = [&](double t) {   // km/s
        const double h = 0.02;
        UpdateBodies(B, t - h);
        V3 a = B[i].pos - B[B_SUN].pos;
        UpdateBodies(B, t + h);
        return Len(B[i].pos - B[B_SUN].pos - a) / (2 * h * 86400);
    };

    VoyageStep launch;
    launch.kind = VS_LAUNCH;
    launch.t = t0;
    launch.body = B_EARTH;
    steps->push_back(launch);

    // スイングバイ・到着（探査機のできごとの一覧から）。速さの変化は惑星の影響圏の出入りで比べる
    int swingbys = 0;
    for (const AstroEvent& ev : app.probeEvents) {
        if (ev.probe != i || ev.kind != EV_FLYBY || ev.t >= tEnd - 1e-6 || ev.t <= t0 + 0.5) continue;
        VoyageStep st;
        if (ev.body > B_PLUTO) {   // 小惑星: 実際の軌跡での距離（スイングバイではない）
            st.kind = VS_FLYBY;
            st.t = ev.t;
            st.body = ev.body;
            st.dist = ev.dist;
            steps->push_back(st);
            continue;
        }
        st.kind = VS_SWINGBY;
        st.t = ev.t;
        st.body = ev.body;
        st.dist = ev.dist;
        for (const ProbeSeg& sg : S)
            if (sg.body == ev.body && sg.t.front() <= ev.t && ev.t <= sg.t.back()) {
                st.vIn = helioSpeed(sg.t.front() + 0.05);
                st.vOut = helioSpeed(sg.t.back() - 0.05);
            }
        steps->push_back(st);
        swingbys++;
    }

    // 目標の小惑星そのものが途中で地球の静止衛星の軌道（中心から 42,164 km）より内側を通る（アポフィス）
    for (const AstroEvent& ev : app.cometEvents)
        if (ev.kind == EV_COMET_EARTH && ev.moon != 1 && ev.body == endBody && ev.t > t0 && ev.t < tEnd && ev.dist < 42164) {
            VoyageStep st;
            st.kind = VS_TARGET_EARTH;
            st.t = ev.t;
            st.body = ev.body;
            st.dist = ev.dist;
            st.vIn = distTo(ev.body, ev.t);   // 探査機からの距離
            steps->push_back(st);
        }

    // 地球の重力圏の脱出・目的外の天体への接近・飛行距離: 軌跡をたどって調べる
    bool wantEscape = !goal || goal->escape, escaped = false;
    struct Near { int body; double r, d0, d1, t0, t1; };
    std::vector<Near> cand;
    for (size_t k = 1; k < B.size(); k++) {
        const BodyDef& o = B[k].d;
        if ((int)k <= B_PLUTO && o.parent == 0) continue;   // 惑星は上のスイングバイで
        if ((int)k == endBody || o.kind == KIND_PROBE || o.kind == KIND_STAR) continue;
        bool listed = false;   // 軌跡で最接近を持つ天体は上で入れた
        for (const AstroEvent& ev : app.probeEvents)
            if (ev.probe == i && ev.body == (int)k) listed = true;
        if (listed) continue;
        if (HostStar(B, (int)k) != B_SUN) continue;
        cand.push_back({(int)k, std::max(o.radius * 100.0, o.parent > 0 ? 0.0 : 200000.0), 1e300, 1e300, t0, t0});
    }
    V3 prevPos;
    bool havePrev = false;
    double t = t0;
    while (t < tEnd) {
        UpdateBodies(B, t);
        int segBody = 0;
        for (const ProbeSeg& sg : S)
            if (sg.t.front() <= t && t <= sg.t.back()) segBody = sg.body;
        V3 pos = B[i].pos;
        if (havePrev) *length += Len(pos - B[B_SUN].pos - prevPos);
        prevPos = pos - B[B_SUN].pos;
        havePrev = true;
        double dEarth = Len(pos - B[B_EARTH].pos);
        if (wantEscape && !escaped && dEarth > B[B_EARTH].soi) {
            escaped = true;
            double a = std::max(t0, t - 1.0), b = t;
            for (int it = 0; it < 30; it++) {
                double m = (a + b) / 2;
                (distTo(B_EARTH, m) > B[B_EARTH].soi ? b : a) = m;
            }
            VoyageStep st;
            st.kind = VS_ESCAPE;
            st.t = b;
            st.body = B_EARTH;
            st.dist = B[B_EARTH].soi;
            st.vOut = helioSpeed(b);
            steps->push_back(st);
            UpdateBodies(B, t);
        }
        for (Near& n : cand) {
            int par = B[n.body].d.parent;
            if (par > 0 && par != segBody) {   // 母天体の近くにいない間の衛星は調べない
                n.d0 = n.d1 = 1e300;
                continue;
            }
            double dk = Len(pos - B[n.body].pos);
            if (n.d1 < n.d0 && n.d1 <= dk && n.d1 < std::max(n.r * 5, 5e6)) {   // 極小を挟んだので細かく探す
                double a = n.t0, b = t;
                double tm = MinimizeT([&](double x) { return distTo(n.body, x); }, a, b), dm = distTo(n.body, tm);
                if (dm < n.r && tm < tEnd - 0.1) {
                    VoyageStep st;
                    st.kind = VS_NEAR;
                    st.t = tm;
                    st.body = n.body;
                    st.dist = dm;
                    steps->push_back(st);
                }
                UpdateBodies(B, t);
            }
            n.t0 = n.t1;
            n.t1 = t;
            n.d0 = n.d1;
            n.d1 = dk;
        }
        t += segBody > 0 ? 1.0 / 48 : 1.0;
    }

    VoyageStep end;
    end.kind = VS_ARRIVE;
    end.t = tEnd;
    end.body = endBody;
    end.count = swingbys;
    end.evKind = endKind;
    if (endBody >= 0) end.dist = distTo(endBody, tEnd);
    steps->push_back(end);
    std::stable_sort(steps->begin(), steps->end(), [](const VoyageStep& a, const VoyageStep& b) { return a.t < b.t; });
}

// 時刻 t に追う機体: 探査機の旅はその探査機、アポロ計画はその時刻より前の最後のできごとの機体
static int VoyageFocusAt(const App& app, double t) {
    if (app.apolloMission < 0) return app.voyProbe;
    const ApolloMission& m = APOLLO_MISSIONS[app.apolloMission];
    int v = 0;
    for (int k = 0; k < m.eventCount; k++)
        if (ApolloT(m, m.events[k].get) <= t + 1e-9) v = m.events[k].vehicle;
    int b = ApolloBody(app, app.apolloMission, v);
    // 記録が終わった機体（月面に落とした上昇段、太陽を回る軌道へ送った上昇段など）は追わず、もう一方の機体を追う
    // （記録の外の機体は遠くに置かれていて、追うとカメラが何百光年も飛ぶ）
    auto inData = [&](int i) {
        int tr = i >= 0 ? app.B[i].d.apollo : -1;
        if (tr < 0 || tr >= (int)g_apollo.size() || g_apollo[tr].segs.empty()) return false;
        return t >= g_apollo[tr].segs.front().t.front() - 120.0 / 86400 && t <= g_apollo[tr].segs.back().t.back();
    };
    if (b >= 0 && !inData(b)) {
        int o = ApolloBody(app, app.apolloMission, 1 - v);
        if (o >= 0 && inData(o)) b = o;
    }
    return b >= 0 ? b : app.voyProbe;
}

// 右上の図の原点: 探査機の旅は太陽、アポロ計画は地球
static int VoyageOrigin(const App& app) { return app.apolloMission >= 0 ? B_EARTH : B_SUN; }

// 右上の小さな全体図のために、全行程の位置（原点から）を等間隔の時刻で求めておく。アポロ計画では月の位置も
static void SampleVoyageTrack(App& app) {
    const int N = 1500;
    std::vector<Body> B = app.B;
    for (Body& b : B) b.orbit.clear();
    double t0 = app.voySteps.front().t, t1 = app.voySteps.back().t;
    app.voyTrack.clear();
    app.voyTrackT.clear();
    app.voyStepPos.clear();
    app.voyMoonTrack.clear();
    int o = VoyageOrigin(app);
    for (int k = 0; k <= N; k++) {
        double t = t0 + (t1 - t0) * k / N;
        UpdateBodies(B, t);
        app.voyTrack.push_back(B[VoyageFocusAt(app, t)].pos - B[o].pos);
        app.voyTrackT.push_back(t);
        if (app.apolloMission >= 0) app.voyMoonTrack.push_back(B[B_MOON].pos - B[o].pos);
    }
    for (const VoyageStep& st : app.voySteps) {
        UpdateBodies(B, st.t);
        app.voyStepPos.push_back(B[VoyageFocusAt(app, st.t)].pos - B[o].pos);
    }
}

// イベント一覧の「探査機の旅」のページ。ev.t は出発、ev.body は目標、ev.dist は到着の時刻
// ev.moon は区間（1 が最初の旅、2 以降が延長ミッション）
static std::vector<AstroEvent> FindVoyages(const App& app) {
    std::vector<AstroEvent> out;
    for (size_t i = 0; i < app.B.size(); i++) {
        if (!VoyageCapable(app, (int)i)) continue;
        for (int leg = 1; leg == 1 || FindVoyageLeg(app.B[i].d, leg); leg++) {
            AstroEvent ev;
            int kind;
            ev.t = VoyageStart(app, (int)i, leg);
            if (!std::isfinite(ev.t) || !VoyageEnd(app, (int)i, leg, &ev.dist, &ev.body, &kind)) continue;
            ev.kind = EV_VOYAGE;
            ev.probe = (int)i;
            ev.moon = leg;
            out.push_back(ev);
        }
    }
    std::sort(out.begin(), out.end(), [](const AstroEvent& a, const AstroEvent& b) { return a.t < b.t; });
    return out;
}

static std::string VoyageTargetName(const App& app, int i, int leg, int body) {
    if (const VoyageGoal* g = leg <= 1 ? FindVoyageGoal(app.B[i].d) : nullptr) return TR(g->targetJp, g->targetEn);
    return body >= 0 ? TR(app.B[body].d.jp, app.B[body].d.en) : "";
}

// アポロ: 機体の形に合わせたカメラの離れ方の倍率（サターン V は 110 m、月着陸船は 7 m）と、見る点の機体の底からの高さ km
// （原点が機体の底の形では中ほど）
static void ApolloCamFit(const App& app, double* k, double* mid) {
    int mission, vehicle;
    int model = ApolloVehicleOf(app, app.voyProbe, &mission, &vehicle) ? ApolloModel(APOLLO_MISSIONS[mission], vehicle, app.tDays) : PM_APOLLO_CSM;
    double outer = model >= 0 ? app.probeOuter[model] : 0.006;
    *k = std::max(1.0, outer / 0.008);
    *mid = model == PM_SATURN_V || model == PM_APOLLO_LM || model == PM_LM_STOWED || model == PM_LM_ASCENT ? outer * 0.45 : 0.0;
}

// アポロ: 地面の近くの見方へ移す割合。天体固定の区間（打ち上げ・月面への降下と上昇・着水）で、高度 600 m から 100 m の間に
// 上空での見方から移る（UpdateVoyage で、画面の 1 秒ほどかけて追う。時間を速めて降りる時に一気に振り向かないように）
static double ApolloGroundWant(const App& app) {
    const Body& b = app.B[app.voyProbe];
    int gb = ApolloFixedBody(b.d, app.tDays);
    return gb < 0 ? 0.0 : ApolloSmooth((0.6 - SurfaceDist(app.B[gb], b.pos)) / 0.5);
}

// 探査機のそばのカメラの向きと位置: 探査機を画面の右上に入れて（左上と下は表示で埋まる）、見たい方向 dir を正面にする
static void PlaceVoyageCamera(App& app, V3 dir) {
    Ship& s = app.ship;
    s.ref = app.voyProbe;
    s.speed = 0;
    s.thrusting = false;
    if (app.voyLookYaw != 0 || app.voyLookPitch != 0) {   // 見回している分だけ向きを回す（探査機は画面の同じ所に残る）
        V3 right = Cross(dir, Y_UP);
        right = Len(right) > 1e-6 ? Norm(right) : V3(1, 0, 0);
        V3 up = Cross(right, dir);
        double y = app.voyLookYaw, p = app.voyLookPitch;
        dir = Norm((dir * std::cos(y) + right * std::sin(y)) * std::cos(p) + up * std::sin(p));
    }
    V3 upHint = app.apolloMission >= 0 ? app.voyUp : Y_UP;   // アポロ: 近くの天体から外向き（ApolloUpWant を滑らかにしたもの）
    s.q = QuatFromBasis(dir, upHint);
    s.local = ShipFwd(s) * -0.032 - ShipRight(s) * 0.009 - ShipUp(s) * 0.005;
    if (app.apolloMission >= 0) {
        // アポロ: 機体の大きさに合わせて離れ、機体の中ほどを見る（ApolloCamFit を UpdateVoyage で滑らかにしたもの）。
        // 地面の近くではカメラを地面より上に保つ
        const Body& b = app.B[app.voyProbe];
        double k = app.voyCamK;
        V3 mid = V3();
        if (app.voyCamMid > 0) {
            int mission, vehicle;
            int model = ApolloVehicleOf(app, app.voyProbe, &mission, &vehicle) ? ApolloModel(APOLLO_MISSIONS[mission], vehicle, app.tDays) : -1;
            if (model >= 0) mid = FromF(Vector3RotateByQuaternion({0, 0, -1}, ApolloPose(app, app.voyProbe, model))) * app.voyCamMid;
        }
        ApolloPieces tv;   // 離れていく部品（第3段・切り離した段）: 機体と部品の間を、両方が入る距離から見る
        if (ApolloPiecesAt(app, app.apolloMission, &tv) && tv.camW > 0 && (tv.pair || app.voyProbe == ApolloBody(app, app.apolloMission, 0))) {
            mid = mid * (1 - tv.camW) + (tv.camTarget - b.pos) * tv.camW;
            k = k * (1 - tv.camW) + tv.camSize * 1.3 / 0.034 * tv.camW;
        }
        s.local = mid + s.local * k;
        double w = app.voyGroundW;
        if (w > 0) {
            // 地面の近く: カメラは地面から 15 m より下には行けない（自機の視点の決まり）ので、少し離れた 16 m 以上の高さから
            // 機体の中ほどを見下ろす（ApolloGroundWant）
            int gb;
            ApolloVel(app, app.voyProbe, app.tDays, &gb);
            const Body& o = app.B[gb];
            V3 up = Norm(b.pos - o.pos);
            double alt = SurfaceDist(o, b.pos);
            V3 h = ShipFwd(s) - up * Dot(ShipFwd(s), up);
            h = Len(h) > 1e-9 ? Norm(h) : Norm(Cross(up, V3(1, 0, 0)));
            V3 side = Norm(Cross(h, up));
            V3 cam = h * (-0.045 * k) - side * (0.010 * k) + up * std::max(0.016 - alt, Dot(mid, up) + 0.004);
            Quaternion q = QuatFromBasis(Norm(mid - cam), up);
            s.q = QuaternionSlerp(s.q, q, (float)w);
            s.local = s.local * (1 - w) + cam * w;
        }
        int fixedBody = ApolloFixedBody(b.d, app.tDays);
        if (fixedBody >= 0) {
            const Body& o = app.B[fixedBody];
            V3 cam = b.pos + s.local, up = Norm(cam - o.pos);
            double alt = SurfaceDist(o, cam), minAlt = fixedBody == B_EARTH ? 0.03 : 0.002;
            if (alt < minAlt) s.local = s.local + up * (minAlt - alt);
        }
    }
}

// アポロ: 画面の上にしたい向き。近くの天体（地球か月）から外向き。見る向き dir がその向きに近い（天体を真下や真上に
// 見る）と上下が決まらず、少し動くだけで画面が半回転してしまうので、近づくにつれて進む向き（真上を見る時はその逆）を
// 上にする方へ連続的に移す（見る向きが天頂を越えても、上にしたい向きは画面の中で同じ側に残る）
static V3 ApolloUpWant(const App& app, V3 dir) {
    int ref;
    V3 v = ApolloVel(app, app.voyProbe, app.tDays, &ref);
    V3 u = ApolloUpHint(app, app.voyProbe, app.tDays);
    V3 h = ApolloFixedBody(app.B[app.voyProbe].d, app.tDays) >= 0 ? ApolloGroundDir(app, app.voyProbe, app.tDays, v, u) : v - u * Dot(v, u);
    if (Len(h) < 1e-9) h = Cross(app.B[ref].pole, u);
    if (Len(h) < 1e-9) return u;
    double c = Len(Cross(dir, u)), w = std::max(0.0, std::min(1.0, c / 0.4));
    double k = 1 - w * w * (3 - 2 * w);
    return Norm(u + Norm(h) * (Dot(dir, u) > 0 ? -k : k));
}

// 今見たい方向: 知らせを出しているできごとの天体、なければ次のできごとの天体、それもなければ進む向き
static V3 ApolloAimDir(const App& app, int* bodyOut);
static V3 VoyageAimDir(const App& app, int* bodyOut = nullptr) {
    if (app.apolloMission >= 0) return ApolloAimDir(app, bodyOut);
    const Body& p = app.B[app.voyProbe];
    int body = -1;
    if (app.voyCard >= 0) body = app.voySteps[app.voyCard].body;
    if (body < 0)
        for (const VoyageStep& st : app.voySteps)
            if (st.t > app.tDays && st.body >= 0) { body = st.body; break; }
    if (bodyOut) *bodyOut = body;
    if (body >= 0) {
        const Body& o = app.B[body];
        V3 d = o.pos - p.pos;
        double r = Len(d);
        // 近くで夜側を通る時は、昼側の縁（三日月形に光る部分）の方を見る。夜側の深さと近さに応じて中心から縁へ
        // 連続的に寄せる（切り替えで画面が飛ばないように）。遠い時や恒星は中心を
        if (body != B_SUN && r > 1e-6 && r < o.d.radius * 8) {
            V3 out = Norm(d * -1.0), toSun = Norm(app.B[B_SUN].pos - o.pos);
            double facing = Dot(out, toSun);
            V3 side = toSun - out * facing;
            if (facing < 0 && Len(side) > 1e-9) {
                auto smooth = [](double x) { x = std::max(0.0, std::min(1.0, x)); return x * x * (3 - 2 * x); };
                double w = smooth(-facing / 0.3) * smooth((o.d.radius * 8 - r) / (o.d.radius * 2));
                d = o.pos + Norm(side) * (o.d.radius * w) - p.pos;
            }
        }
        // 衛星: 時間を速めている間は母天体のまわりを何周も回り、追うとカメラが左右に振れる。画面の上で
        // 1 周が短く（10 秒より短い）、振れの見かけの大きさ（衛星の軌道の見かけの半径）も見て分かる間は
        // 母天体の方を見て、時間が遅くなるにつれて衛星へ寄せる
        if (o.d.parent > 0 && o.d.moonA > 0 && o.d.moonPeriod > 0 && r > 1e-6) {
            auto ramp = [](double x, double lo, double hi) {
                x = std::max(0.0, std::min(1.0, (x - lo) / (hi - lo)));
                return x * x * (3 - 2 * x);
            };
            const Body& host = app.B[o.d.parent];
            double perSec = app.voyRate / (o.d.moonPeriod * 86400);              // 画面の 1 秒あたりの周回数
            double swing = std::atan(std::min(1.0, o.d.moonA / r)) / DEG;        // 振れの見かけの大きさ deg
            double w = 1 - ramp(perSec, 0.02, 0.1) * ramp(swing, 0.02, 0.08);
            d = host.pos + (p.pos + d - host.pos) * w - p.pos;
        }
        if (Len(d) > 1e-6) return Norm(d);
    }
    return Len(p.pos - app.B[B_SUN].pos) > 1e-6 ? Norm(app.voyAim) : V3(1, 0, 0);   // 目標のない巡航は今の向きのまま
}

// アポロ計画の見る向き: 知らせを出しているか次のできごとの向き（天体か、進む向き）。地面の近く（打ち上げ・月面・突入）では
// 水平に近い向きにして、カメラが地面の下に入らないように機体の横から見る
static V3 ApolloAimDir(const App& app, int* bodyOut) {
    const Body& p = app.B[app.voyProbe];
    int look = AP_LOOK_FWD;
    if (app.voyCard >= 0) look = app.voySteps[app.voyCard].body;
    else
        for (const VoyageStep& st : app.voySteps)
            if (st.t > app.tDays) { look = st.body; break; }
    if (bodyOut) *bodyOut = look;
    int ref;
    V3 v = ApolloVel(app, app.voyProbe, app.tDays, &ref);
    V3 up = Norm(p.pos - app.B[ref].pos);
    V3 dir = look >= 0 ? Norm(app.B[look].pos - p.pos) : Len(v) > 1e-9 ? Norm(v) : up;
    double fw = ApolloFixedFade(p.d, app.tDays, 10.0);
    if (fw > 0) {
        // 進む向きを見る時と、見る天体が真上・真下にある時は、地面に対して水平に進む向き（止まっている間も揺れない）。
        // 区間の前後は 10 秒かけて普通の見方と移し合う（軌道に入って真下の地球を見る時に、カメラが一度に振り下ろされないように）
        if (ApolloFixedBody(p.d, app.tDays) < 0) v = v - ApolloSpinVel(app.B[ref], p.pos - app.B[ref].pos);   // 地面に対する速度
        V3 h = look >= 0 ? dir - up * Dot(dir, up) : V3();
        if (Len(h) < 0.2) h = ApolloGroundDir(app, app.voyProbe, app.tDays, v, up);
        if (Len(h) < 1e-9) h = Cross(app.B[ref].pole, up);
        double tilt = std::max(0.0, std::min(1.0, (Len(v) / 86400 - 0.005) / 0.045));   // 止まっている間（月面・発射台）は水平に
        V3 g = Norm(Norm(h) + up * (0.15 * tilt * tilt * (3 - 2 * tilt)));
        V3 r = g * fw + dir * (1 - fw);
        dir = Len(r) > 1e-9 ? Norm(r) : g;
    }
    // 離れていく部品（月着陸船を引き出す手順の第3段、切り離したサターンの段）: 機体と部品を横から（日の当たる側から、
    // 少し後ろ寄りに）見る
    ApolloPieces tv;
    if (ApolloPiecesAt(app, app.apolloMission, &tv) && tv.camW > 0 && (tv.pair || app.voyProbe == ApolloBody(app, app.apolloMission, 0))) {
        V3 a = FromF(Vector3RotateByQuaternion({0, 0, -1}, tv.axis)), sun = Norm(app.B[B_SUN].pos - tv.camTarget);
        V3 side = sun - a * Dot(sun, a);
        if (Len(side) < 0.2) side = Cross(a, Y_UP);
        V3 d = Norm(Norm(side) * -1.0 + a * 0.25);
        // 2 つの向きの間を同じ速さで回して移す（足し合わせると、逆に近い向きどうし（地球を見る向きと横から見る向き）の
        // 間では、半分の所で 1 フレームに何度も振れる）
        double th = std::acos(std::max(-1.0, std::min(1.0, Dot(dir, d)))), sn = std::sin(th);
        if (sn > 1e-3) dir = Norm(dir * (std::sin((1 - tv.camW) * th) / sn) + d * (std::sin(tv.camW * th) / sn));
        else if (th < 1) dir = d;
    }
    return dir;
}

static void PrepareVoyageLocal(App& app, int k);
// 時刻 t に応じた時間の速さと、出す知らせを決める（dt: 前のフレームからの画面の秒。頭出しの時は 0）
static void UpdateVoyageRate(App& app, double dt = 0) {
    const double day = 86400.0;
    double rate = app.voyCruise;
    int card = -1;
    for (size_t k = 0; k < app.voySteps.size(); k++) {
        double delta = (app.tDays - app.voySteps[k].t) * day;
        rate = std::min(rate, std::max(1.0, (std::fabs(delta) - VOY_HOLD_SEC) / VOY_RAMP_SEC));
        if (std::fabs(delta) < VOY_CARD_LEAD_SEC) card = (int)k;
    }
    if (app.apolloMission >= 0) {   // サターンの段間リングを捨てる時も、場面と同じように時間をゆっくりにする
        double tA, tR, tB;
        if (ApolloStagingTimes(APOLLO_MISSIONS[app.apolloMission], &tA, &tR, &tB))
            rate = std::min(rate, std::max(1.0, (std::fabs(app.tDays - tR) * day - VOY_HOLD_SEC) / VOY_RAMP_SEC));
    }
    if (app.apolloMission >= 0 && APOLLO_MISSIONS[app.apolloMission].tde) {
        // 月着陸船を引き出す手順: ドッキング（やり直しの接触も）と引き出しも、場面と同じように時間をゆっくりにする
        const ApolloMission& m = APOLLO_MISSIONS[app.apolloMission];
        const ApolloTde& d = *m.tde;
        auto slow = [&](const char* get) {
            if (get) rate = std::min(rate, std::max(1.0, (std::fabs(app.tDays - ApolloT(m, get)) * day - VOY_HOLD_SEC) / VOY_RAMP_SEC));
        };
        slow(d.dock);
        slow(d.eject);
        for (int k = 0; k < d.keyCount; k++)
            if (d.keys[k].gap == 0) slow(d.keys[k].get);
        // 2 機を入れる見方から戻す間は、戻るのが画面で 2 秒ほどかかる速さまでにする（時間を速めたまま戻すと、カメラが
        // 1 フレームで何百 m も寄り、続けて振り向く）。滑らかな移り方の傾きは最大で平均の 1.5 倍なので、画面の 1 秒あたり
        // 0.75 まで
        const double h = 1.0 / 86400;
        double slope = std::fabs(ApolloTdeCamW(app, app.apolloMission, app.tDays + h) - ApolloTdeCamW(app, app.apolloMission, app.tDays - h)) / 2;
        if (slope > 0) rate = std::min(rate, std::max(1.0, 0.75 / slope));
    }
    if (app.apolloMission >= 0 && APOLLO_MISSIONS[app.apolloMission].lunar) {
        // 月を回る軌道での切り離しとドッキング: 接触（固くつながる前）も場面と同じように時間をゆっくりにし、2 機を入れる見方へ
        // 移る間と戻す間は、月着陸船を引き出す手順と同じく、画面の 1 秒あたり 0.75 までの移り方にする
        const ApolloMission& m = APOLLO_MISSIONS[app.apolloMission];
        if (m.lunar->contact)
            rate = std::min(rate, std::max(1.0, (std::fabs(app.tDays - ApolloT(m, m.lunar->contact)) * day - VOY_HOLD_SEC) / VOY_RAMP_SEC));
        // 2 機を入れる見方の間は、入れる大きさ（2 機の間）が画面の 1 秒あたり半分より速く変わらないようにする
        const double h = 1.0 / 86400;
        ApolloPieces a, b;
        double wa = ApolloRdvView(app, app.apolloMission, app.tDays + h, &a), wb = ApolloRdvView(app, app.apolloMission, app.tDays - h, &b);
        double slope = std::fabs(wa - wb) / 2;
        if (slope > 0) rate = std::min(rate, std::max(1.0, 0.75 / slope));
        double grow = wa > 0 && wb > 0 ? std::fabs(a.camSize - b.camSize) / 2 : 0;
        if (grow > 0) rate = std::min(rate, std::max(1.0, 0.5 * std::min(a.camSize, b.camSize) / grow));
        // ドッキングの前に 2 機が 400 m まで近づく（見方を移し始める）前から、時間をゆっくりにしておく（速めたまま入ると、
        // 1 フレームの間に見方が大きく移る）。そこまでの時間 Δ 秒から、できごとの前と同じように Δ / 1.5 + 20 倍まで
        double ga, gb;
        ApolloRdvView(app, app.apolloMission, app.tDays + h, nullptr, &ga);
        ApolloRdvView(app, app.apolloMission, app.tDays - h, nullptr, &gb);
        double closing = (gb - ga) / 2;   // km/s
        if (ga > 0.4 && gb > 0 && closing > 0) rate = std::min(rate, std::max(1.0, (ga - 0.4) / closing / VOY_RAMP_SEC + 20));
    }
    if (app.apolloMission >= 0) {
        // 機械船の切り離し（13 号は月着陸船の切り離しも）と上昇段の切り離しと子衛星: 2 機を入れる見方へ移る間と戻す間は画面の 1 秒あたり 0.25 まで（横から
        // 見る向きと普段の向きの違いが大きく、0.75 では 1 フレームで 10° 回る）、入れる大きさは 1 秒あたり半分まで。切り離しの向きへ回る間（切り離しの 10 分前から 4 分）は、
        // 噴射の前に向きを変える時と同じように、回り始めと回り終わりの前後で時間をゆっくりにする
        const ApolloMission& m = APOLLO_MISSIONS[app.apolloMission];
        const double h = 1.0 / 86400;
        ApolloPieces a, b;
        double wa = ApolloSepView(app, app.apolloMission, app.tDays + h, &a), wb = ApolloSepView(app, app.apolloMission, app.tDays - h, &b);
        double slope = std::fabs(wa - wb) / 2;
        if (slope > 0) rate = std::min(rate, std::max(1.0, 0.25 / slope));
        double grow = wa > 0 && wb > 0 ? std::fabs(a.camSize - b.camSize) / 2 : 0;
        if (grow > 0) rate = std::min(rate, std::max(1.0, 0.5 * std::min(a.camSize, b.camSize) / grow));
        auto slow = [&](double t) { rate = std::min(rate, std::max(1.0, (std::fabs(app.tDays - t) * day - VOY_HOLD_SEC) / VOY_RAMP_SEC)); };
        bool stack = true;
        double ts = ApolloSmSepT(m, &stack);
        if (!stack) {
            slow(ts - SMSEP_TURN0 / day);
            slow(ts - (SMSEP_TURN0 - SMSEP_TURN) / day);
        }
        // 月を回る軌道での上昇段の切り離し（場面になっていない号が多い）: 切り離しの時と、司令船が回り始める時と回り終わる時
        double ta = ApolloAscJettT(m);
        if (std::isfinite(ta)) {
            slow(ta);
            for (double s : {-AJ_TURN0, -AJ_TURN0 + AJ_TURN, AJ_HOLD, AJ_HOLD + AJ_BACK}) slow(ta + s / day);
        }
        // 子衛星を放つ時と、司令船が回り始める時と回り終わる時
        double tss = ApolloSubsatT(m);
        if (std::isfinite(tss)) {
            double t0 = ApolloSubsatTurnT(m);
            for (double tt : {tss, t0, t0 + SS_TURN / day, tss + SS_HOLD / day, tss + (SS_HOLD + SS_BACK) / day}) slow(tt);
        }
        // 地球へ戻る途中の船外活動: ハッチを開ける時と閉める時、司令船操縦士が外へ出始める時と中へ入り終わる時。その間は 30 倍まで
        // （機器室への行き来の 1〜2 分が画面で数秒）
        if (m.eva) {
            const ApolloEva& e = *m.eva;
            double t0 = ApolloT(m, e.open), t1 = ApolloT(m, e.close);
            slow(t0);
            slow(t1);
            slow(ApolloT(m, e.keys[0].get));
            slow(ApolloT(m, e.keys[e.keyCount - 1].get));
            if (app.tDays > t0 - EVA_FADE / day && app.tDays < t1 + EVA_FADE / day) rate = std::min(rate, EVA_RATE);
        }
    }
    if (app.apolloMission >= 0 && app.voyProbe >= 0) {
        // 主エンジンの噴射（場面になっていない回避や軌道修正も）: 機体は噴射の前後 4 分かけて噴射の向きと回り合うので、
        // 噴射の始まりと終わりの前後は場面と同じように時間をゆっくりにする（速めたままだと 1 フレームで何十度も振り回される）
        const BodyDef& bd = app.B[app.voyProbe].d;
        if (bd.apollo >= 0 && bd.apollo < (int)g_apollo.size())
            for (const ApolloBurn& bn : g_apollo[bd.apollo].burns) {
                if (bn.engine == AP_ENG_RCS || bn.engine == AP_ENG_LM_RCS) continue;
                double delta = std::min(std::fabs(app.tDays - bn.t0), std::fabs(app.tDays - bn.t1)) * day;
                rate = std::min(rate, std::max(1.0, (delta - VOY_HOLD_SEC) / VOY_RAMP_SEC));
            }
    }
    if (app.apolloMission >= 0) {
        // アポロ: できごとが続くので、できごとの 20 秒前から次のできごとまでそのできごとの知らせを出す（30 分たったら消す）
        card = -1;
        for (size_t k = 0; k < app.voySteps.size(); k++) {
            double delta = (app.tDays - app.voySteps[k].t) * day;
            if (delta >= -20) card = delta < 1800 ? (int)k : -1;
        }
    }
    // アポロ: 時間を速める時は画面の 1 秒あたり 4 倍まで（できごとの後に速める普段の速め方は 1 秒あたり 2 倍ほど。2 機を入れる
    // 見方の制限が外れた時などに、1 フレームで何十倍にも跳ばないように）
    if (app.apolloMission >= 0 && dt > 0) rate = std::min(rate, std::max(1.0, app.voyRate) * std::exp(dt * std::log(4.0)));
    if (app.voyDone) rate = 1;
    // 拡大図の軌跡は、場面に近づいた時に一度だけ求める（巡航の速さで 1 日以内）
    for (size_t k = 0; k < app.voySteps.size(); k++)
        if (!app.voyLocal[k].ready && std::fabs(app.tDays - app.voySteps[k].t) < (app.apolloMission >= 0 ? 0.25 : 3.0))
            PrepareVoyageLocal(app, (int)k);
    if (card < 0 && app.voyDone) card = (int)app.voySteps.size() - 1;
    app.voyRate = rate;
    app.voyCard = card;
}

static void StopVoyage(App& app) {
    if (!app.voyageOn) return;
    app.voyageOn = false;
    app.apolloMission = -1;
    app.cockpit = app.voySavedCockpit;
    app.timeIdx = 0;
    app.paused = false;
    app.eventZoom = false;
    if (app.voySavedPhysics) SetPhysics(app, true);
    UpdateReferenceFrame(app);
    Toast(app, T(UI_VOY_STOPPED));
}

// 時刻 t へ移す（場面の頭出し）
static void VoyageJump(App& app, double t) {
    app.tDays = t;
    app.voyDone = false;
    UpdateBodies(app.B, app.tDays);
    UpdateVoyageRate(app);
    if (app.apolloMission >= 0) app.voyProbe = VoyageFocusAt(app, app.tDays);
    app.voyAim = VoyageAimDir(app, &app.voyAimBody);
    app.voyAimPrevWant = app.voyAim;
    if (app.apolloMission >= 0) {
        app.voyUp = app.voyUpPrevWant = ApolloUpWant(app, app.voyAim);
        ApolloCamFit(app, &app.voyCamK, &app.voyCamMid);
        app.voyGroundW = ApolloGroundWant(app);
    }
    PlaceVoyageCamera(app, app.voyAim);
}

// 巡航の速さ（0: ゆっくり, 1: 標準, 2: 速い）
static void VoyageSetSpeed(App& app, int speed) {
    app.voySpeed = std::max(0, std::min(2, speed));
    if (app.voySteps.size() < 2) return;
    double days = app.voySteps.back().t - app.voySteps.front().t;
    app.voyCruise = std::max(app.apolloMission >= 0 ? 0.001 : 0.2, std::min(80.0, days / VOY_CRUISE_WALL_SEC[app.voySpeed])) * 86400.0;
}

// 旅の名前（延長ミッションはその名前）
static std::string VoyageName(const App& app, int i, int leg) {
    if (app.apolloMission >= 0) return TR(APOLLO_MISSIONS[app.apolloMission].jp, APOLLO_MISSIONS[app.apolloMission].en);
    if (const VoyageLeg* l = leg > 1 ? FindVoyageLeg(app.B[i].d, leg) : nullptr) return TR(l->nameJp, l->nameEn);
    return TR(app.B[i].d.jp, app.B[i].d.en);
}

// 探査機 i の区間 leg の旅を始める。scene: 場面の番号（0 が出発、負なら末尾から）。t が有効ならその時刻から
static bool StartVoyage(App& app, int i, int leg, int scene, double t) {
    if (!VoyageCapable(app, i)) return false;
    std::vector<VoyageStep> steps;
    double length;
    BuildVoyage(app, i, leg, &steps, &length);
    if (steps.size() < 2) return false;
    if (app.tourOn) app.tourOn = false;
    app.autopilot = false;
    app.warpT = -1;
    app.voySavedPhysics = app.physics;
    if (app.physics) SetPhysics(app, false);
    app.voySavedCockpit = app.voyageOn ? app.voySavedCockpit : app.cockpit;
    if (app.mouseCaptured) {   // 旅の間は下のタイムラインを押せるよう、マウスを捕まえない（見回すのは左ドラッグ）
        app.mouseCaptured = false;
        EnableCursor();
    }
    app.voyageOn = true;
    app.voyProbe = i;
    app.voyLeg = leg;
    app.voySteps = steps;
    app.voyLength = length;
    app.voyLocal.assign(steps.size(), App::VoyLocal());
    app.voyLookYaw = app.voyLookPitch = app.voyLookHold = 0;
    app.cockpit = true;       // 自機は描かない
    app.paused = false;
    app.timeIdx = 0;
    app.eventZoom = false;
    app.frameLock = false;
    app.target = i;
    SampleVoyageTrack(app);
    double t0 = steps.front().t, t1 = steps.back().t;
    VoyageSetSpeed(app, app.voySpeed);
    int n = (int)steps.size();
    if (scene < 0) scene = n + scene;
    scene = std::max(0, std::min(n - 1, scene));
    double start = std::isfinite(t) && t >= t0 && t <= t1 ? t : scene == 0 ? t0 : std::max(t0, steps[scene].t - VOY_SCENE_LEAD_DAYS);
    // 前のできごとを跨がないよう、直前のできごとの少しあとより前には戻らない
    if (!std::isfinite(t)) {
        if (scene > 0) start = std::max(start, steps[scene - 1].t + VOY_CARD_LEAD_SEC / 86400.0);
        start = std::min(start, steps[scene].t);
    }
    VoyageJump(app, start);
    return true;
}

// アポロ計画の飛行 mission を始める。scene: 場面の番号（0 が打ち上げ、負なら末尾から）。t が有効ならその時刻から
static bool StartApollo(App& app, int mission, int scene, double t) {
    if (mission < 0 || mission >= APOLLO_COUNT) return false;
    const ApolloMission& m = APOLLO_MISSIONS[mission];
    int csm = ApolloBody(app, mission, 0);
    if (csm < 0 || app.B[csm].d.apollo < 0) return false;
    std::vector<VoyageStep> steps;
    for (int k = 0; k < m.eventCount; k++) {
        const ApolloEvent& e = m.events[k];
        VoyageStep st;
        st.kind = VS_APOLLO;
        st.t = ApolloT(m, e.get);
        st.body = e.look;
        st.count = k;
        // 右上の拡大図: 月の影響圏の中なら月のまわり、地球の近く（高度 2,000 km まで）なら地球のまわり
        int b = ApolloBody(app, mission, e.vehicle);
        V3 pe = PosAt(app.B, b, st.t), pm = PosAt(app.B, B_MOON, st.t), pg = PosAt(app.B, B_EARTH, st.t);
        st.mapBody = Len(pe - pm) < app.B[B_MOON].soi ? B_MOON : Len(pe - pg) < app.B[B_EARTH].d.radius + 2000 ? B_EARTH : -1;
        steps.push_back(st);
    }
    // 打ち上げの少し前から（発射台のロケット）
    steps.front().t = std::max(steps.front().t, ApolloLaunch(m));
    if (app.tourOn) app.tourOn = false;
    app.autopilot = false;
    app.warpT = -1;
    app.voySavedPhysics = app.voyageOn ? app.voySavedPhysics : app.physics;
    if (app.physics) SetPhysics(app, false);
    app.voySavedCockpit = app.voyageOn ? app.voySavedCockpit : app.cockpit;
    if (app.mouseCaptured) {
        app.mouseCaptured = false;
        EnableCursor();
    }
    app.voyageOn = true;
    app.apolloMission = mission;
    app.voyProbe = csm;
    app.voyLeg = 1;
    app.voySteps = steps;
    app.voyLength = 0;
    app.voyLocal.assign(steps.size(), App::VoyLocal());
    app.voyLookYaw = app.voyLookPitch = app.voyLookHold = 0;
    app.cockpit = true;
    app.paused = false;
    app.timeIdx = 0;
    app.eventZoom = false;
    app.frameLock = false;
    app.target = csm;
    SampleVoyageTrack(app);
    VoyageSetSpeed(app, app.voySpeed);
    int n = (int)steps.size();
    if (scene < 0) scene = n + scene;
    scene = std::max(0, std::min(n - 1, scene));
    double t0 = steps.front().t - 30.0 / 86400, t1 = steps.back().t;   // 打ち上げの 30 秒前から
    app.voySteps.front().t = steps.front().t;
    double start = std::isfinite(t) && t >= t0 && t <= t1 ? t : scene == 0 ? t0 : steps[scene].t - 60.0 / 86400;
    if (!std::isfinite(t) && scene > 0) start = std::max(start, steps[scene - 1].t + 1.0 / 86400);
    app.voyProbe = VoyageFocusAt(app, start);
    VoyageJump(app, start);
    return true;
}

// URL の voyage= と scene= から始める（起動時に一度）。scene: launch / arrive / 場面の番号（1 から）
static void StartPendingVoyage(App& app) {
    if (!app.pendingApollo.empty()) {   // アポロ計画: apollo=11 など
        int mission = -1;
        for (int k = 0; k < APOLLO_COUNT; k++)
            if ("Apollo " + app.pendingApollo == APOLLO_MISSIONS[k].name || app.pendingApollo == APOLLO_MISSIONS[k].name) mission = k;
        const std::string& sc = app.pendingScene;
        int scene = sc == "end" || sc == "arrive" ? -1 : sc.empty() || sc == "launch" ? 0 : std::max(1, atoi(sc.c_str())) - 1;
        if (mission < 0 || !StartApollo(app, mission, scene, app.pendingVoyageT)) Toast(app, T(UI_VOY_NOT_FOUND));
        app.pendingApollo.clear();
        return;
    }
    if (app.pendingVoyage.empty()) return;
    auto lower = [](std::string v) {
        for (char& ch : v) ch = (char)std::tolower((unsigned char)ch);
        return v;
    };
    int who = -1;
    for (size_t k = 0; k < app.B.size(); k++)
        if (VoyageCapable(app, (int)k) && lower(app.pendingVoyage) == lower(app.B[k].d.en)) who = (int)k;
    int scene = 0;
    const std::string& sc = app.pendingScene;
    if (sc == "arrive" || sc == "end") scene = -1;
    else if (!sc.empty() && sc != "launch") scene = std::max(1, atoi(sc.c_str())) - 1;
    if (who < 0 || !StartVoyage(app, who, std::max(1, app.pendingLeg), scene, app.pendingVoyageT)) Toast(app, T(UI_VOY_NOT_FOUND));
    app.pendingVoyage.clear();
}

// 場面 k へ移る（その場面の 1 日前から。前の場面の知らせが終わった後から）
static void VoyageGoScene(App& app, int k) {
    const std::vector<VoyageStep>& S = app.voySteps;
    if (k < 0 || k >= (int)S.size()) return;
    double t = k == 0 ? S[0].t : std::max(S[0].t, S[k].t - VOY_SCENE_LEAD_DAYS);
    if (k > 0) t = std::min(S[k].t, std::max(t, S[k - 1].t + VOY_CARD_LEAD_SEC / 86400.0));
    VoyageJump(app, t);
}

// 画面下のタイムライン（出発から到着までを時刻で等分した棒）の位置
static Rectangle VoyageTimelineRect(const Cam& c) {
    return {24, c.H - 46, c.W - 48, 10};
}
static float VoyageTimelineX(const App& app, Rectangle r, double t) {
    double t0 = app.voySteps.front().t, t1 = app.voySteps.back().t;
    return r.x + r.width * (float)std::max(0.0, std::min(1.0, (t - t0) / std::max(1e-9, t1 - t0)));
}
// p（2D の表示の座標）にある場面の番号。点から 9 ピクセル以内で一番近いもの。無ければ -1
static int VoyageTimelineScene(const App& app, Rectangle r, Vector2 p) {
    int best = -1;
    float bd = 9;
    for (size_t k = 0; k < app.voySteps.size(); k++) {
        float d = std::fabs(p.x - VoyageTimelineX(app, r, app.voySteps[k].t));
        if (d < bd && std::fabs(p.y - (r.y + r.height / 2)) < 14) { bd = d; best = (int)k; }
    }
    return best;
}
// タイムラインを押した: 場面の点ならその場面へ、ほかの所ならその時刻へ。押したなら true
static bool VoyageTimelineClick(App& app, Vector2 p) {
    Rectangle r = VoyageTimelineRect(UiCam(app.cam));
    if (!CheckCollisionPointRec(p, {r.x - 10, r.y - 12, r.width + 20, r.height + 24})) return false;
    int k = VoyageTimelineScene(app, r, p);
    if (k >= 0) VoyageGoScene(app, k);
    else {
        double t0 = app.voySteps.front().t, t1 = app.voySteps.back().t;
        VoyageJump(app, t0 + (t1 - t0) * std::max(0.0f, std::min(1.0f, (p.x - r.x) / r.width)));
    }
    return true;
}

static void VoyageCardText(const App& app, const VoyageStep& st, std::string* title, std::string* text);
static void DrawVoyageTimeline(const App& app) {
    const Cam& c = app.cam;
    Rectangle r = VoyageTimelineRect(c);
    float cy = r.y + r.height / 2, now = VoyageTimelineX(app, r, app.tDays);
    DrawRectangleRounded({r.x - 8, r.y - 6, r.width + 16, r.height + 12}, 0.5f, 6, {0, 8, 20, 190});
    DrawLineEx({r.x, cy}, {r.x + r.width, cy}, 2.0f, {90, 110, 140, 220});
    DrawLineEx({r.x, cy}, {now, cy}, 3.0f, {255, 196, 120, 230});
    for (size_t k = 0; k < app.voySteps.size(); k++) {
        float x = VoyageTimelineX(app, r, app.voySteps[k].t);
        bool done = app.voySteps[k].t <= app.tDays;
        Color col = (int)k == app.voyCard ? Color{255, 220, 80, 255} : done ? Color{230, 235, 245, 230} : Color{130, 140, 160, 230};
        DrawCircleV({x, cy}, (int)k == app.voyCard ? 5.0f : 3.5f, col);
    }
    DrawCircleV({now, cy}, 4.0f, {120, 255, 170, 255});
    DrawCircleLinesV({now, cy}, 7.0f, {120, 255, 170, 200});
    // マウスを乗せた場面の見出し
    Vector2 m = Vector2Scale(GetMousePosition(), 1 / UiScale(c));
    int k = app.mouseCaptured || app.touchUI ? -1 : VoyageTimelineScene(app, r, m);
    if (k >= 0) {
        std::string title, text;
        VoyageCardText(app, app.voySteps[k], &title, &text);
        std::string s = FmtDay(app.voySteps[k].t) + "  " + title;
        float w = TxtW(s.c_str(), 14) + 16, x = std::max(4.0f, std::min(c.W - w - 4, VoyageTimelineX(app, r, app.voySteps[k].t) - w / 2));
        DrawRectangleRounded({x, r.y - 34, w, 22}, 0.3f, 6, {0, 8, 20, 235});
        Txt(s.c_str(), x + 8, r.y - 31, 14, {255, 230, 170, 255});
    }
}

// 次・前の場面へ
static void VoyageSkip(App& app, int dir) {
    int n = (int)app.voySteps.size();
    if (dir > 0) {
        for (int k = 0; k < n; k++)
            if (app.voySteps[k].t > app.tDays + VOY_HOLD_SEC / 86400.0) {
                // ふつうは 1 日前へ。もう 1 日を切っていたら、等倍になる少し前（実時間で数秒）まで進める
                double t = app.voySteps[k].t - VOY_SCENE_LEAD_DAYS;
                if (t <= app.tDays) t = app.voySteps[k].t - 2 * VOY_HOLD_SEC / 86400.0;
                VoyageJump(app, std::max(app.tDays, t));
                return;
            }
        return;
    }
    for (int k = n - 1; k >= 0; k--)
        if (app.voySteps[k].t < app.tDays - 30.0 / 86400.0) {
            VoyageJump(app, k == 0 ? app.voySteps[0].t : std::max(app.voySteps[0].t, app.voySteps[k].t - VOY_SCENE_LEAD_DAYS));
            return;
        }
}

// 毎フレーム: 時間の速さを決め、カメラを探査機に付ける
static void UpdateVoyage(App& app, double dt) {
    if (!app.voyageOn) return;
    int prevProbe = app.voyProbe;
    if (app.apolloMission >= 0) app.voyProbe = VoyageFocusAt(app, app.tDays);   // 追う機体（司令船か月着陸船）
    // 見回した向きは、操作をやめて 3 秒たつと滑らかに自動の向きへ戻る
    if (app.voyLookHold > 0) app.voyLookHold -= dt;
    else {
        double k = std::exp(-dt * 2.0);
        app.voyLookYaw *= k;
        app.voyLookPitch *= k;
        if (std::fabs(app.voyLookYaw) < 1e-4 && std::fabs(app.voyLookPitch) < 1e-4) app.voyLookYaw = app.voyLookPitch = 0;
    }
    double tEnd = app.voySteps.back().t;
    if (!app.voyDone && app.tDays >= tEnd) {
        // データの終わりで探査機が消える時は、その場で時間を止める。飛び続ける時は少し見せてから完了に
        std::vector<Body> B = app.B;
        for (Body& b : B) b.orbit.clear();
        UpdateBodies(B, tEnd + VOY_HOLD_SEC / 86400.0);
        bool ends = !B[app.voyProbe].active;
        if (ends || app.tDays >= tEnd + VOY_HOLD_SEC / 86400.0) {
            app.voyDone = true;
            if (ends) {
                app.paused = true;
                app.tDays = tEnd;
                UpdateBodies(app.B, app.tDays);
            }
        }
    }
    UpdateVoyageRate(app, dt);
    int aimBody;
    V3 want = VoyageAimDir(app, &aimBody);
    // カメラの向き: 同じ天体を追っている間は、その天体の見かけの動きの分だけ回す（毎秒 20° まで。通過の直後に
    // 時間が速まって天体が後ろへ飛び去る時に、振り回されないように）。残りのずれは 0.4 秒ほどで滑らかに詰める
    // （毎秒 40° まで）。見る天体が変わった時も、この詰め方で振り向く
    auto rotateToward = [](V3 v, V3 to, double maxAngle) {
        double c = std::max(-1.0, std::min(1.0, Dot(v, to)));
        double ang = std::acos(c);
        if (ang < 1e-9) return v;
        V3 perp = to - v * c;
        if (Len(perp) < 1e-12) return v;
        double step = std::min(ang, maxAngle);
        return Norm(v * std::cos(step) + Norm(perp) * std::sin(step));
    };
    V3 aim = app.voyAim;
    if (aimBody == app.voyAimBody && aimBody >= 0) {
        double moved = std::acos(std::max(-1.0, std::min(1.0, Dot(app.voyAimPrevWant, want))));
        if (moved > 1e-9) {   // 前のフレームの見たい向きから今の見たい向きへの回転を、今の向きにも掛ける
            V3 axis = Cross(app.voyAimPrevWant, want);
            if (Len(axis) > 1e-12) {
                axis = Norm(axis);
                // アポロで回っている天体（地球か月）を見ている時は、周回の動き（1 フレームで 10° まで）を制限しない（時間を
                // 速めると毎秒 100° 以上動き、追いつけないと見る向きが真下を外れて、画面が回ってしまう）。打ち上げ・着陸の後で
                // 見方が変わる時の飛びは、下で滑らかに詰める
                int ref = -1;
                if (app.apolloMission >= 0) ApolloVel(app, app.voyProbe, app.tDays, &ref);
                double a = aimBody == ref && moved < 10 * DEG ? moved : std::min(moved, 20 * DEG * dt);
                aim = aim * std::cos(a) + Cross(axis, aim) * std::sin(a) + axis * (Dot(axis, aim) * (1 - std::cos(a)));
                aim = Norm(aim);
            }
        }
    }
    double gap = std::acos(std::max(-1.0, std::min(1.0, Dot(aim, want))));
    app.voyAim = rotateToward(aim, want, std::min(gap * (1 - std::exp(-dt / 0.4)), 40 * DEG * dt));
    app.voyAimBody = aimBody;
    app.voyAimPrevWant = want;
    if (app.apolloMission >= 0) {
        // 画面の上: 見る向きのまわりの回転（傾き）として、近くの天体から外向き（離れると黄道の北、ApolloUpHint）へ寄せる。周回で回る分（1 フレームで
        // 10° まで）はそのまま追い、基準天体や追う機体が変わった時の飛びは 0.5 秒ほどで詰める。天体を真下や真上に見る間は
        // 外向きの向きが画面の上で定まらないので、近づくにつれて寄せるのを弱め、今の傾きのままにする
        V3 d = app.voyAim;
        auto perp = [&](V3 v) { V3 p = v - d * Dot(v, d); return Len(p) > 1e-9 ? Norm(p) : V3(); };
        auto roll = [&](V3 a, V3 b) { return std::atan2(Dot(Cross(a, b), d), Dot(a, b)); };   // a から b へ、d のまわりの角度
        auto rot = [&](V3 v, double a) { return Norm(v * std::cos(a) + Cross(d, v) * std::sin(a)); };
        V3 u = ApolloUpHint(app, app.voyProbe, app.tDays);
        double w = ApolloSmooth(Len(Cross(d, u)) / 0.4);
        V3 up = perp(u), prev = perp(app.voyUpPrevWant), cur = perp(app.voyUp);
        if (Len(cur) == 0) cur = perp(ApolloUpWant(app, d));
        if (w > 0 && Len(up) > 0) {
            if (Len(prev) > 0 && std::fabs(roll(prev, up)) < 10 * DEG) cur = rot(cur, roll(prev, up) * w);
            cur = rot(cur, roll(cur, up) * (1 - std::exp(-dt / 0.5)) * w);
        }
        app.voyUp = cur;
        app.voyUpPrevWant = u;
        // 機体の形が変わった時（段の切り離し・ドッキングなど）、離れ方と見る点を 1 秒ほどかけて移す
        double k, mid, f = 1 - std::exp(-dt / 0.6);
        ApolloCamFit(app, &k, &mid);
        app.voyCamK += (k - app.voyCamK) * f;
        app.voyCamMid += (mid - app.voyCamMid) * f;
        // 追う機体が変わった時（月面の月着陸船から上空の司令船へなど）は、すぐその機体の見方にする。地面の近くの見方を
        // 上空の機体に混ぜると、真下の月を見ていて水平の向きが定まらず、カメラがフレームごとに飛ぶ
        double gw = ApolloGroundWant(app);
        app.voyGroundW = app.voyProbe != prevProbe ? gw : app.voyGroundW + (gw - app.voyGroundW) * (1 - std::exp(-dt / 1.0));
    }
    app.target = app.voyCard >= 0 && app.voySteps[app.voyCard].body >= 0 ? app.voySteps[app.voyCard].body : app.voyProbe;
    PlaceVoyageCamera(app, app.voyAim);
    // 小惑星のそば: 小さくて普通の視野では点にしか見えないので、画面の 4 分の 1 ほどの大きさになるまでズームする
    app.eventZoom = false;
    if (app.voyCard >= 0) {
        const VoyageStep& st = app.voySteps[app.voyCard];
        if (st.body > B_PLUTO && (st.kind == VS_FLYBY || st.kind == VS_ARRIVE)) {
            const Body& o = app.B[st.body];
            double d = Len(o.pos - app.B[app.voyProbe].pos);
            double ang = 2 * std::atan(std::max(o.d.radius, o.outer) / std::max(d, 1e-6)) / DEG;
            app.eventZoom = true;
            app.eventZoomFov = std::max(0.5, std::min(60.0, ang * 4));
        }
    }
}

// できごとの見出しと説明
static void VoyageCardText(const App& app, const VoyageStep& st, std::string* title, std::string* text) {
    char b[320];
    auto name = [&](int i) { return std::string(TR(app.B[i].d.jp, app.B[i].d.en)); };
    const BodyDef& pr = app.B[app.voyProbe].d;
    *text = "";
    switch (st.kind) {
        case VS_APOLLO: {
            const ApolloEvent& e = APOLLO_MISSIONS[app.apolloMission].events[st.count];
            *title = TR(e.jp, e.en);
            *text = TR(e.textJp, e.textEn);
            return;
        }
        case VS_LAUNCH:
            if (const VoyageLeg* l = FindVoyageLeg(pr, app.voyLeg)) {
                *title = TR(l->startTitleJp, l->startTitleEn);
                *text = TR(l->startTextJp, l->startTextEn);
                break;
            }
            *title = T(UI_VOY_LAUNCH);
            snprintf(b, sizeof b, T(UI_VOY_LAUNCH_D), TR(pr.jp, pr.en));
            *text = b;
            break;
        case VS_ESCAPE:
            *title = T(UI_VOY_ESCAPE);
            snprintf(b, sizeof b, T(UI_VOY_ESCAPE_D), FmtDist(st.dist).c_str(), st.vOut);
            *text = b;
            break;
        case VS_SWINGBY:
            snprintf(b, sizeof b, T(UI_VOY_SWINGBY), name(st.body).c_str());
            *title = b;
            snprintf(b, sizeof b, T(UI_VOY_SWINGBY_D), FmtDist(st.dist - app.B[st.body].d.radius).c_str(), st.vIn, st.vOut);
            *text = b;
            break;
        case VS_FLYBY:
            snprintf(b, sizeof b, T(UI_VOY_FLYBY), name(st.body).c_str());
            *title = b;
            snprintf(b, sizeof b, T(UI_VOY_FLYBY_D), FmtDist(st.dist).c_str(), app.B[st.body].d.radius * 2);
            *text = b;
            break;
        case VS_TARGET_EARTH:
            snprintf(b, sizeof b, T(UI_VOY_TARGET_EARTH), name(st.body).c_str());
            *title = b;
            snprintf(b, sizeof b, T(UI_VOY_TARGET_EARTH_D), FmtDist(st.dist).c_str(), FmtDist(st.vIn).c_str());
            *text = b;
            break;
        case VS_NEAR:
            snprintf(b, sizeof b, T(UI_VOY_NEAR), name(st.body).c_str());
            *title = b;
            snprintf(b, sizeof b, T(UI_VOY_NEAR_D), FmtDist(st.dist).c_str());
            *text = b;
            break;
        default: {
            const VoyageGoal* g = app.voyLeg <= 1 ? FindVoyageGoal(pr) : nullptr;
            if (const VoyageLeg* l = FindVoyageLeg(pr, app.voyLeg)) {
                *title = TR(l->endTitleJp, l->endTitleEn);
                *text = TR(l->endTextJp, l->endTextEn);
            } else if (g) {
                *title = TR(g->titleJp, g->titleEn);
                *text = TR(g->textJp, g->textEn);
            } else {
                UiId id = st.evKind == EV_IMPACT ? UI_VOY_IMPACT : st.evKind == EV_FLYBY ? UI_VOY_REACHED : UI_VOY_ARRIVED;
                snprintf(b, sizeof b, T(id), name(st.body).c_str());
                *title = b;
                for (const TourStop& ts : TOUR)
                    if (ts.page == 2 && std::strcmp(ts.who, pr.en) == 0 && ts.body && std::strcmp(ts.body, app.B[st.body].d.en) == 0 &&
                        ts.kind == st.evKind)
                        *text = TR(ts.jp, ts.en);
                for (const auto& a : VOYAGE_ARRIVAL_TEXT)
                    if (text->empty() && std::strcmp(a.probe, pr.en) == 0) *text = TR(a.jp, a.en);
            }
            double years = (st.t - app.voySteps.front().t) / 365.25;
            snprintf(b, sizeof b, T(UI_VOY_SUMMARY), years, st.count, app.voyLength / AU_KM);
            *text += (text->empty() ? "" : "\n") + std::string(b);
            break;
        }
    }
    if (st.kind != VS_LAUNCH && st.t > NowDaysSinceJ2000()) *title += T(UI_VOY_PLANNED);   // まだ起きていない（予定の軌道）
}

static std::vector<std::string> SplitLines(const std::string& s) {
    std::vector<std::string> lines;
    for (size_t a = 0; a <= s.size() && !s.empty();) {
        size_t e = std::min(s.find('\n', a), s.size());
        lines.push_back(s.substr(a, e - a));
        a = e + 1;
    }
    return lines;
}

// 時間の速さの表示（x1 / 1 秒あたり何日）
static std::string VoyageRateText(const App& app) {
    if (app.paused) return T(UI_PAUSED);
    if (app.voyRate < 1.5) return std::string("x1 (") + T(UI_REALTIME) + ")";
    char b[64];
    if (app.voyRate < 600) snprintf(b, sizeof b, "x%.0f", app.voyRate);
    else if (app.voyRate < 3600) snprintf(b, sizeof b, T(UI_VOY_RATE_MIN), app.voyRate / 60);
    else if (app.voyRate < 86400) snprintf(b, sizeof b, T(UI_VOY_RATE_H), app.voyRate / 3600);
    else snprintf(b, sizeof b, T(UI_VOY_RATE_D), app.voyRate / 86400);
    return b;
}

// 右上の小さな全体図: 太陽系を真上から見て（右が春分点の方向）、全行程が収まる縮尺で、惑星の軌道と位置、
// 探査機の通ってきた道（明るく）とこれから通る道（暗く）、場面の場所、今の位置を描く
static Rectangle VoyageMapRect(const Cam& c) {
    float s = std::max(150.0f, std::min(280.0f, std::min(c.W * 0.26f, c.H * 0.42f)));
    return {c.W - s - 10, 10, s, s};
}

// 天体の近くの拡大図に切り替える場面（太陽と、天体のない場面は除く）。場面の前後 span 日の間
static int VoyageLocalStep(const App& app) {
    for (size_t k = 0; k < app.voySteps.size(); k++) {
        const VoyageStep& st = app.voySteps[k];
        if ((st.mapBody >= 0 ? st.mapBody : st.body) <= B_SUN || !app.voyLocal[k].ready) continue;
        if (std::fabs(app.tDays - st.t) < app.voyLocal[k].span) return (int)k;
    }
    return -1;
}

// 場面 k の拡大図の道を求める: 最接近の距離の数倍まで離れる時間の範囲で、探査機と天体の位置。
// 全体図と同じく太陽に対して止まった見方で描くので、天体も図の中を動き、探査機の進む向きは全体図と同じになる
static void PrepareVoyageLocal(App& app, int k) {
    App::VoyLocal& L = app.voyLocal[k];
    if (L.ready) return;
    L.ready = true;
    const VoyageStep& st = app.voySteps[k];
    int body = st.mapBody >= 0 ? st.mapBody : st.body, o = VoyageOrigin(app);
    if (body <= B_SUN) return;
    std::vector<Body> B = app.B;
    for (Body& b : B) b.orbit.clear();
    auto relAt = [&](double t) {
        UpdateBodies(B, t);
        return B[VoyageFocusAt(app, t)].pos - B[body].pos;
    };
    // 最接近の距離の 4 倍（地球の出発・脱出は重力圏の半径まで）離れるまでの時間を、前後それぞれ探す
    double d0 = std::max(Len(relAt(st.t)), app.B[body].d.radius * 2);
    double reach = st.kind == VS_LAUNCH || st.kind == VS_ESCAPE ? app.B[B_EARTH].soi * 1.3 : d0 * 4;
    double t0 = app.voySteps.front().t, t1 = app.voySteps.back().t;
    auto edge = [&](double dir) {
        double h = 1.0 / 1440, hmax = app.apolloMission >= 0 ? 2.0 / 24 : 60.0;   // 1 分から倍々に（アポロは前後 2 時間まで）
        while (h < hmax && Len(relAt(std::max(t0, std::min(t1, st.t + dir * h)))) < reach) h *= 2;
        return std::min(h, hmax);
    };
    double before = std::min(edge(-1), st.t - t0), after = std::min(edge(1), t1 - st.t);
    if (st.kind == VS_LAUNCH) before = 0;
    L.span = std::max(before, after);
    relAt(st.t);
    L.center = B[body].pos - B[o].pos;
    // 月を回るアポロは、月も動くと周回の輪がずれて重なり螺旋のようになるので、月から見た道にする
    L.follow = app.apolloMission >= 0 && body == B_MOON;
    const int N = 400;
    for (int j = 0; j <= N; j++) {
        double t = st.t - before + (before + after) * j / N;
        relAt(t);
        V3 c = L.follow ? B[body].pos - B[o].pos : L.center;
        L.t.push_back(t);
        L.rel.push_back(B[VoyageFocusAt(app, t)].pos - B[o].pos - c);
        L.body.push_back(B[body].pos - B[o].pos - c);
    }
}

// 色の透明度に掛ける（右上の図の切り替えのフェード）
static Color Alpha(Color c, float a) {
    c.a = (unsigned char)(c.a * std::max(0.0f, std::min(1.0f, a)));
    return c;
}

// 天体の近くの拡大図（場面 ls の天体のあたりを、全体図と同じ向き・同じ見方で拡大）。fa は透明度
static void DrawVoyageMapLocal(const App& app, Rectangle r, int ls, float fa) {
    const App::VoyLocal& L = app.voyLocal[ls];
    const VoyageStep& st = app.voySteps[ls];
    const Body& o = app.B[st.mapBody >= 0 ? st.mapBody : st.body];
    // 探査機と天体の道がちょうど収まる正方形を、図の中心に置く
    double x0 = -o.d.radius, x1 = o.d.radius, z0 = -o.d.radius, z1 = o.d.radius;
    for (const std::vector<V3>* v : {&L.rel, &L.body})
        for (const V3& p : *v) {
            x0 = std::min(x0, p.x), x1 = std::max(x1, p.x);
            z0 = std::min(z0, p.z), z1 = std::max(z1, p.z);
        }
    double range = std::max(x1 - x0, z1 - z0) * 0.5 * 1.08, mx = (x0 + x1) * 0.5, mz = (z0 + z1) * 0.5;
    float cx = r.x + r.width * 0.5f, cy = r.y + r.height * 0.5f + 6, R = r.width * 0.5f - 10;
    double k = R / range;
    auto proj = [&](V3 p) { return Vector2{(float)(cx + (p.x - mx) * k), (float)(cy + (p.z - mz) * k)}; };
    Rectangle in = {r.x + 2, r.y + 2, r.width - 4, r.height - 4};
    const V3 sun = L.follow ? o.pos - L.center : app.B[VoyageOrigin(app)].pos;   // 天体から見た道では、今の天体が中心
    // 天体の通る道（細く）と、今の天体
    Color oc = BodyColor(o, 0.9f);
    Color ol = oc;
    ol.a = 90;
    for (size_t j = 1; j < L.body.size(); j++) {
        Vector2 a = proj(L.body[j - 1]), b = proj(L.body[j]);
        if (CheckCollisionPointRec(a, in) && CheckCollisionPointRec(b, in)) DrawLineEx(a, b, 1.0f, Alpha(ol, fa));
    }
    Vector2 oc2 = proj(o.pos - sun - L.center);
    // 天体の点の大きさは、天体から見た道が収まる縮尺での大きさ（実際の縮尺では小さすぎて見えないので）
    double near = o.d.radius * 1.5;
    for (size_t j = 0; j < L.rel.size(); j++) {
        V3 d = L.rel[j] - L.body[j];
        near = std::max(near, std::sqrt(d.x * d.x + d.z * d.z));
    }
    if (CheckCollisionPointRec(oc2, in)) DrawCircleV(oc2, std::max(2.5f, (float)(o.d.radius * R / (near * 1.08))), Alpha(oc, fa));
    for (size_t j = 1; j < L.rel.size(); j++) {
        Vector2 a = proj(L.rel[j - 1]), b = proj(L.rel[j]);
        if (!CheckCollisionPointRec(a, in) || !CheckCollisionPointRec(b, in)) continue;
        bool past = L.t[j] <= app.tDays;
        DrawLineEx(a, b, past ? 1.8f : 1.0f, past ? Alpha({255, 196, 120, 230}, fa) : Alpha({255, 196, 120, 70}, fa));
    }
    Vector2 me = proj(app.B[app.voyProbe].pos - sun - L.center);
    if (CheckCollisionPointRec(me, in)) {
        DrawCircleV(me, 3.0f, Alpha({120, 255, 170, 255}, fa));
        DrawCircleLinesV(me, 4.0f + 2.0f * (float)std::fabs(std::sin(GetTime() * 3)), Alpha({120, 255, 170, 220}, fa));
    }
    char b[96];
    snprintf(b, sizeof b, T(L.follow ? UI_VOY_MAP_FROM : UI_VOY_MAP_NEAR), TR(o.d.jp, o.d.en));
    Txt(b, r.x + 8, r.y + 5, 13, Alpha({170, 200, 230, 255}, fa));
    double unit = std::pow(10.0, std::floor(std::log10(range * 0.6)));
    float bw = (float)(unit * k);
    DrawLineEx({r.x + 10, r.y + r.height - 10}, {r.x + 10 + bw, r.y + r.height - 10}, 1.5f, Alpha({170, 200, 230, 220}, fa));
    Txt(FmtDist(unit).c_str(), r.x + 14 + bw, r.y + r.height - 18, 12, Alpha({170, 200, 230, 220}, fa));
}

// 全行程の全体図。fa は透明度
static void DrawVoyageMapWhole(const App& app, Rectangle r, float fa) {
    double range = 0;
    for (const V3& p : app.voyTrack) range = std::max(range, std::sqrt(p.x * p.x + p.z * p.z));
    range = std::max(range * 1.08, app.apolloMission >= 0 ? 20000.0 : 0.05 * AU_KM);
    float cx = r.x + r.width * 0.5f, cy = r.y + r.height * 0.5f + 6, R = r.width * 0.5f - 10;
    double k = R / range;
    auto proj = [&](V3 p) { return Vector2{(float)(cx + p.x * k), (float)(cy + p.z * k)}; };
    // 枠の外にはみ出す線や点は描かない（切り抜きは HUD の拡大と座標が合わないので使わない）
    Rectangle in = {r.x + 2, r.y + 2, r.width - 4, r.height - 4};
    auto line = [&](Vector2 a, Vector2 b, float w, Color col) {
        if (CheckCollisionPointRec(a, in) && CheckCollisionPointRec(b, in)) DrawLineEx(a, b, w, col);
    };
    auto dot = [&](Vector2 a, float rad, Color col) {
        if (CheckCollisionPointRec(a, in)) DrawCircleV(a, rad, col);
    };
    const V3 sun = app.B[VoyageOrigin(app)].pos;
    if (app.apolloMission >= 0) {   // 地球中心: 月の通る道（細く）と今の月、中心の地球
        const Body& moon = app.B[B_MOON];
        Color col = BodyColor(moon, 0.8f);
        col.a = 90;
        for (size_t j = 1; j < app.voyMoonTrack.size(); j++) line(proj(app.voyMoonTrack[j - 1]), proj(app.voyMoonTrack[j]), 1.0f, Alpha(col, fa));
        col.a = 230;
        dot(proj(moon.pos - sun), 3.0f, Alpha(col, fa));
    }
    // 惑星の軌道と今の位置（全体図の外まで広がる軌道も、図の中の部分は描く）
    for (int i = 1; i <= B_PLUTO && app.apolloMission < 0; i++) {
        const Body& b = app.B[i];
        if (b.orbit.size() < 2 || b.d.a_au * AU_KM * (1 - 0.25) > range * 1.5) continue;
        Color col = BodyColor(b, 0.8f);
        col.a = 90;
        Vector2 prev = proj(b.orbit[0]);
        for (size_t j = 1; j < b.orbit.size(); j++) {
            Vector2 q = proj(b.orbit[j]);
            line(prev, q, 1.0f, Alpha(col, fa));
            prev = q;
        }
        col.a = 230;
        dot(proj(b.pos - sun), 2.5f, Alpha(col, fa));
    }
    DrawCircleV({cx, cy}, 3.5f, app.apolloMission >= 0 ? Alpha(BodyColor(app.B[B_EARTH], 1.0f), fa) : Alpha({255, 220, 120, 255}, fa));
    // 探査機の道
    size_t now = 0;
    while (now + 1 < app.voyTrackT.size() && app.voyTrackT[now + 1] <= app.tDays) now++;
    for (size_t j = 1; j < app.voyTrack.size(); j++) {
        bool past = j <= now;
        line(proj(app.voyTrack[j - 1]), proj(app.voyTrack[j]), past ? 1.8f : 1.0f,
                   past ? Alpha({255, 196, 120, 230}, fa) : Alpha({255, 196, 120, 70}, fa));
    }
    // 場面の場所（済んだものは白、これからのものは灰色。知らせを出している場面は黄色）
    for (size_t j = 0; j < app.voyStepPos.size(); j++) {
        bool done = app.voySteps[j].t <= app.tDays;
        Color col = (int)j == app.voyCard ? Color{255, 220, 80, 255} : done ? Color{230, 235, 245, 220} : Color{130, 140, 160, 200};
        dot(proj(app.voyStepPos[j]), (int)j == app.voyCard ? 3.5f : 2.2f, Alpha(col, fa));
    }
    // 今の位置（点滅する輪）
    Vector2 me = proj(app.B[app.voyProbe].pos - sun);
    float pulse = 4.0f + 2.0f * (float)std::fabs(std::sin(GetTime() * 3));
    dot(me, 3.0f, Alpha({120, 255, 170, 255}, fa));
    if (CheckCollisionPointRec(me, in)) DrawCircleLinesV(me, pulse, Alpha({120, 255, 170, 220}, fa));
    // 見出しと縮尺（1 / 10 AU の棒）
    Txt(T(UI_VOY_MAP), r.x + 8, r.y + 5, 13, Alpha({170, 200, 230, 255}, fa));
    double unit = range > 8 * AU_KM ? 10 * AU_KM : range > 0.8 * AU_KM ? AU_KM : 0.1 * AU_KM;
    float bw = (float)(unit * k);
    char b[32];
    snprintf(b, sizeof b, "%g AU", unit / AU_KM);
    if (app.apolloMission >= 0) {   // 地球と月のまわりは km で
        unit = std::pow(10.0, std::floor(std::log10(range * 0.6)));
        bw = (float)(unit * k);
        snprintf(b, sizeof b, "%s", FmtDist(unit).c_str());
    }
    DrawLineEx({r.x + 10, r.y + r.height - 10}, {r.x + 10 + bw, r.y + r.height - 10}, 1.5f, Alpha({170, 200, 230, 220}, fa));
    Txt(b, r.x + 14 + bw, r.y + r.height - 18, 12, Alpha({170, 200, 230, 220}, fa));
}

// 右上の小さな図。全体図と拡大図が切り替わる時は 0.6 秒かけて重ねて入れ替える
static void DrawVoyageMap(const App& app) {
    if (!app.voyMap || app.voyTrack.empty()) return;
    Rectangle r = VoyageMapRect(app.cam);
    DrawRectangleRounded(r, 0.05f, 6, {0, 8, 20, 250});
    DrawRectangleRoundedLinesEx(r, 0.05f, 6, 1.2f, {90, 130, 180, 200});
    // 表示中の図（-1: 全体図, 0 以上: その場面の拡大図）と、フェードで消えていく図。旅が変わったら最初から
    static int shown = -2, fading = -2, key = -1;
    static double fadeStart = -1e9;
    int mode = VoyageLocalStep(app);
    int k = app.voyProbe * 16 + app.voyLeg;
    if (key != k || shown == -2) {
        key = k;
        shown = fading = mode;
        fadeStart = -1e9;
    }
    if (mode != shown) {
        fading = shown;
        shown = mode;
        fadeStart = GetTime();
    }
    float f = (float)std::min(1.0, (GetTime() - fadeStart) / 0.6);
    f = f * f * (3 - 2 * f);
    auto draw = [&](int m, float a) {
        if (m >= 0 && m < (int)app.voySteps.size()) DrawVoyageMapLocal(app, r, m, a);
        else DrawVoyageMapWhole(app, r, a);
    };
    if (f < 1) draw(fading, 1 - f);
    draw(shown, f < 1 ? f : 1.0f);
}

// 巡航中のひとこと（できごとの知らせと同じ左下に）。1 つを VOY_TIP_SEC 秒ずつ順に出し、知らせの間は消える。
// 止めている間は進めない。知らせで途中までしか出せなかったひとことは、次の巡航でもう一度出す
static const double VOY_TIP_SEC = 14.0;
// 幅に収まらない行を折り返す（英語は空白で、日本語は句読点の後を優先して文字の間で）
static std::vector<std::string> WrapLine(const std::string& s, float size, float maxw) {
    std::vector<std::string> out;
    size_t start = 0;
    while (start < s.size()) {
        if (TxtW(s.substr(start).c_str(), size) <= maxw) {
            out.push_back(s.substr(start));
            break;
        }
        size_t fit = start, brk = 0, i = start;
        while (i < s.size()) {
            size_t n = i + 1;
            while (n < s.size() && (s[n] & 0xC0) == 0x80) n++;
            if (TxtW(s.substr(start, n - start).c_str(), size) > maxw) break;
            if (s[i] == ' ' && !g_jp) brk = i;
            else if (s.compare(i, 3, "、") == 0 || s.compare(i, 3, "。") == 0) brk = n;
            fit = i = n;
        }
        size_t end = brk > start ? brk : fit;
        // 日本語の「2014 年 1 月」のような数字と単位の間では切らない
        auto ascii = [&](size_t k) { return k < s.size() && (unsigned char)s[k] < 0x80; };
        if (g_jp && brk <= start)
            while (end > start + 1 && (ascii(end) || ascii(end - 1)))
                do end--; while (end > start && (s[end] & 0xC0) == 0x80);
        if (end <= start) end = std::max(fit, start + 1);
        out.push_back(s.substr(start, end - start));
        start = end;
        while (start < s.size() && s[start] == ' ') start++;
    }
    return out;
}

static void DrawVoyageTip(const App& app, bool want, float px, float w) {
    std::vector<const VoyageTip*> tips;
    const BodyDef& pr = app.B[app.voyProbe].d;
    for (const VoyageTip& t : VOYAGE_TIPS)
        if (t.leg == app.voyLeg && std::strcmp(t.probe, pr.en) == 0) tips.push_back(&t);
    static int key = -1, idx = 0;
    static double shown = 0;
    static float vis = 0;
    int k = app.voyProbe * 16 + app.voyLeg;
    if (key != k) {
        key = k;
        idx = 0;
        shown = 0;
        vis = 0;
    }
    if (tips.empty()) return;
    float dt = GetFrameTime();
    if (want && !app.paused) shown += dt;
    if (!want && shown > VOY_TIP_SEC * 0.5) {   // ほぼ読めたものは済んだことにする
        idx++;
        shown = 0;
    }
    if (shown >= VOY_TIP_SEC) {
        idx++;
        shown = 0;
    }
    vis = want ? std::min(1.0f, vis + dt * 3) : std::max(0.0f, vis - dt * 5);
    if (vis <= 0) return;
    // 1 つずつの入れ替わりもフェードで
    float a = vis * (float)std::max(0.0, std::min(1.0, std::min(shown / 0.5, (VOY_TIP_SEC - shown) / 0.5)));
    if (!want) a = vis;
    const VoyageTip& t = *tips[idx % tips.size()];
    // 狭い画面（スマホのブラウザなど）で収まらない時は、改行をつないで枠の幅で折り返し直す
    std::vector<std::string> lines = SplitLines(TR(t.jp, t.en));
    bool fits = true;
    for (const std::string& l : lines) fits = fits && TxtW(l.c_str(), 16) <= w - 36;
    if (!fits) {
        std::string all;
        for (const std::string& l : lines) all += (all.empty() || g_jp ? "" : " ") + l;
        lines = WrapLine(all, 16, w - 36);
    }
    const Cam& c = app.cam;
    char b[96];
    snprintf(b, sizeof b, T(UI_VOY_TIP), TR(pr.jp, pr.en), idx % (int)tips.size() + 1, (int)tips.size());
    float tw = TxtW(b, 14);
    for (const std::string& l : lines) tw = std::max(tw, TxtW(l.c_str(), 16));
    w = std::min(w, tw + 40);   // 短いひとことは枠も狭く
    float h = 44 + lines.size() * 22.0f, py = c.H - 60 - h;
    DrawRectangleRounded({px, py, w, h}, 0.08f, 6, Alpha({0, 8, 20, 190}, vis));
    DrawRectangleRoundedLinesEx({px, py, w, h}, 0.08f, 6, 1.2f, Alpha({70, 100, 140, 220}, vis));
    Txt(b, px + 18, py + 10, 14, Alpha({150, 185, 225, 255}, a));
    for (size_t i = 0; i < lines.size(); i++) Txt(lines[i].c_str(), px + 18, py + 34 + i * 22.0f, 16, Alpha({215, 228, 245, 255}, a));
}

static void DrawVoyageHud(const App& app) {
    const Cam& c = app.cam;
    const Body& p = app.B[app.voyProbe];
    char b[320];
    float fs = 16, lh = 22;
    int nextIdx = -1;
    for (size_t k = 0; k < app.voySteps.size(); k++)
        if (app.voySteps[k].t > app.tDays) { nextIdx = (int)k; break; }
    float panelH = lh * (nextIdx >= 0 ? 5 : 4) + 34;
    DrawRectangleRounded({6, 6, 460, panelH}, 0.06f, 6, {0, 10, 25, 170});
    float x = 16, y = 12;
    snprintf(b, sizeof b, T(app.apolloMission >= 0 ? UI_APOLLO_HEAD : UI_VOY_HEAD), VoyageName(app, app.voyProbe, app.voyLeg).c_str());
    Txt(b, x, y, fs + 3, {255, 220, 120, 255}); y += lh + 6;
    double elapsed = std::max(0.0, app.tDays - app.voySteps.front().t);
    snprintf(b, sizeof b, T(UI_VOY_DATE), FmtDate(app.tDays).c_str());
    Txt(b, x, y, fs, WHITE); y += lh;
    if (app.apolloMission >= 0) {   // 打ち上げからの経過時間（時:分:秒）
        double get = (app.tDays - ApolloLaunch(APOLLO_MISSIONS[app.apolloMission])) * 86400;
        int gs = (int)std::floor(std::fabs(get));
        snprintf(b, sizeof b, T(UI_APOLLO_GET), get < 0 ? "-" : "", gs / 3600, gs / 60 % 60, gs % 60, TR(p.d.jp, p.d.en));
    } else {
        int ey = (int)(elapsed / 365.25);
        snprintf(b, sizeof b, T(UI_VOY_ELAPSED), ey, (int)(elapsed - ey * 365.25));
    }
    Txt(b, x, y, fs, {200, 220, 240, 255}); y += lh;
    std::string rate = VoyageRateText(app);
    static const UiId SPEED[3] = {UI_VOY_SPEED_SLOW, UI_VOY_SPEED_NORMAL, UI_VOY_SPEED_FAST};
    snprintf(b, sizeof b, "%s %s   [%s]", T(UI_TSCALE), rate.c_str(), T(SPEED[app.voySpeed]));
    Txt(b, x, y, fs, app.paused ? Color{255, 160, 80, 255} : app.voyRate < 1.5 ? Color{140, 230, 160, 255} : Color{255, 210, 120, 255}); y += lh;
    if (nextIdx >= 0) {
        std::string t, d;
        VoyageCardText(app, app.voySteps[nextIdx], &t, &d);
        double left = app.voySteps[nextIdx].t - app.tDays;
        std::string when = left > 1 ? std::to_string((int)left) + T(UI_VOY_DAYS) : FmtDuration(left * 86400);
        snprintf(b, sizeof b, T(UI_VOY_NEXT), t.c_str(), when.c_str());
        Txt(b, x, y, fs - 1, {170, 200, 230, 255});
    }

    DrawVoyageMap(app);

    // できごとの知らせ（右下の空いた所まで使えるよう、左下に）
    bool showCard = app.voyCard >= 0;
    float w = std::min(c.W - 24, 880.0f);
    float px = 12;
    if (showCard) {
        const VoyageStep& st = app.voySteps[app.voyCard];
        std::string title, text;
        VoyageCardText(app, st, &title, &text);
        std::vector<std::string> lines = SplitLines(text);
        float h = 72 + lines.size() * 22.0f, py = c.H - 60 - h;
        DrawRectangleRounded({px, py, w, h}, 0.08f, 6, {0, 8, 20, 215});
        DrawRectangleRoundedLinesEx({px, py, w, h}, 0.08f, 6, 1.5f, {90, 130, 180, 255});
        snprintf(b, sizeof b, T(UI_VOY_STEP), app.voyCard + 1, (int)app.voySteps.size());
        Txt((std::string(b) + "    " + FmtMinute(st.t) + " UTC").c_str(), px + 18, py + 10, 15, {170, 200, 230, 255});
        Txt(title.c_str(), px + 18, py + 32, 22, {255, 220, 120, 255});
        for (size_t i = 0; i < lines.size(); i++) Txt(lines[i].c_str(), px + 18, py + 64 + i * 22.0f, 16, {215, 228, 245, 255});
    }
    DrawVoyageTip(app, !showCard && !app.voyDone, px, w);
    DrawVoyageTimeline(app);
    const char* keys = T(app.voyDone ? UI_VOY_KEYS_DONE : UI_VOY_KEYS);
    Txt(keys, 14, c.H - 22, 13, {150, 170, 200, 230});
#if defined(__EMSCRIPTEN__)
    if (int pend = DeferredAssetsPending(); pend > 0) {
        snprintf(b, sizeof b, T(UI_LOADING), pend);
        Txt(b, 14, c.H - 70, 14, {255, 210, 120, 230});
    }
#endif
}

// 旅の間のキー入力。自機の操縦は受け付けない
static void HandleVoyageInput(App& app, bool next, bool prev, bool quit) {
    if (quit) { StopVoyage(app); return; }
    if (next) {
        if (app.voyDone) StopVoyage(app);
        else VoyageSkip(app, 1);
    }
    if (prev) VoyageSkip(app, -1);
}
