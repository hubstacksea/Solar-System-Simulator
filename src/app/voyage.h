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

    {"Pioneer 11", 1, "パイオニア10号の双子の探査機。同じ形で、パイオニア銘板も同じものを積んでいる",
     "The twin of Pioneer 10, with the same design and the same plaque"},
    {"Pioneer 11", 1, "木星の重力で向きを変えて土星へ向かった、初めての探査機。土星では細い F 環を見つけた",
     "The first spacecraft to use Jupiter's gravity to head for Saturn, where it discovered the narrow F ring"},
    {"Pioneer 11", 1, "1995 年に電力が尽きて運用を終えた。今はわし座の方向へ進んでいる",
     "Its power ran out in 1995. It is now heading toward the constellation Aquila"},

    {"Voyager 2", 1, "木星・土星・天王星・海王星が同じ側に並ぶ、約 175 年に一度の配置を使った「グランドツアー」。\n天王星と海王星を訪れた探査機は今もボイジャー2号だけ",
     "Its \"Grand Tour\" used an alignment of Jupiter, Saturn, Uranus and Neptune that comes about once in 175 years.\nIt is still the only spacecraft to have visited Uranus and Neptune"},
    {"Voyager 2", 1, "打ち上げはボイジャー1号より 16 日早い。ボイジャー1号の方が速い軌道で、先に木星に着いた",
     "It launched 16 days before Voyager 1, which took a faster path and reached Jupiter first"},
    {"Voyager 2", 1, "地球の音や画像、55 の言語のあいさつ、世界の音楽を収めた金色のレコード（ゴールデンレコード）を積んでいる",
     "It carries the Golden Record: sounds and images of Earth, greetings in 55 languages and music from around the world"},
    {"Voyager 2", 1, "2018 年 11 月、太陽風の届く範囲（太陽圏）を出て恒星間空間に入った",
     "In November 2018 it left the heliosphere, the bubble of the solar wind, and entered interstellar space"},

    {"Voyager 1", 1, "土星では衛星タイタンのそばを通ることを選び、その重力で惑星の並ぶ面から北へはじき出された。\nそのため土星が最後の惑星になった",
     "At Saturn it was sent close to Titan, whose gravity flung it north out of the plane of the planets,\nso Saturn was its last planet"},
    {"Voyager 1", 1, "1990 年 2 月、約 60 億 km 先から振り返って地球を撮った。光の筋の中の小さな点の写真は\n「ペイル・ブルー・ドット」と呼ばれる",
     "In February 1990 it looked back from about 6 billion km and photographed Earth as a tiny dot in a sunbeam,\nthe \"Pale Blue Dot\""},
    {"Voyager 1", 1, "2012 年 8 月、人工物で初めて恒星間空間に入った。地球から最も遠くにある人工物で、今も信号を送っている",
     "In August 2012 it became the first human-made object in interstellar space. It is the most distant one\nand still sends signals home"},

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

    {"Cassini", 1, "NASA・ESA・イタリア宇宙機関の共同の探査機。燃料を入れると約 5.7 トンで、惑星探査機でも最大級",
     "A joint mission of NASA, ESA and the Italian Space Agency. Fully fuelled it weighed about 5.7 tonnes,\none of the largest planetary probes"},
    {"Cassini", 1, "ESA の着陸機ホイヘンスを積んでいて、2005 年 1 月にタイタンに降ろした。地球の月以外の衛星への初めての着陸",
     "It carried ESA's Huygens probe, which landed on Titan in January 2005, the first landing on a moon other than ours"},
    {"Cassini", 1, "木星のそばを通った時は、木星を回っていたガリレオと同時に木星を観測した",
     "Passing Jupiter, it observed the planet together with Galileo, which was orbiting there"},
    {"Cassini", 1, "燃料が尽きる前の 2017 年 9 月、衛星を汚さないよう土星の大気に飛び込んで運用を終えた（グランドフィナーレ）",
     "In September 2017, before its fuel ran out, it dived into Saturn's atmosphere so it could never contaminate\nthe moons (the Grand Finale)"},

    {"Rosetta", 1, "名前は、古代エジプトの文字を解読する手がかりになったロゼッタ・ストーンから。\n着陸機フィラエの名前は、解読を助けたオベリスクが見つかったナイル川のフィラエ島から",
     "It is named after the Rosetta Stone, the key to Egyptian hieroglyphs. Its lander Philae is named after\nthe Nile island whose obelisk also helped the decipherment"},
    {"Rosetta", 1, "電気は太陽電池でつくるので、太陽から遠く離れて電気が足りなくなる間は、ほとんどの機器を止めて眠った。\n眠っていたのは 2011 年 6 月から 2014 年 1 月までの 31 か月。目覚めの合図が届いた時、管制室は歓声に包まれた",
     "It ran on solar power, so while it was too far from the Sun it shut nearly everything down and slept for\n31 months, June 2011 to January 2014. The control room cheered when its wake-up signal arrived"},
    {"Rosetta", 1, "途中で小惑星シュテインス（2008 年）とルテティア（2010 年）のそばを通って撮影した",
     "On the way it flew past and imaged the asteroids Steins (2008) and Lutetia (2010)"},
    {"Rosetta", 1, "2016 年 9 月、2 年あまり回った彗星の核の表面にゆっくり降りて運用を終えた",
     "In September 2016, after more than two years in orbit, it was set down gently on the comet's nucleus"},

    {"New Horizons", 1, "地球を出る時の速さは、打ち上げた探査機で最も速い。月の軌道を 9 時間ほどで横切った",
     "It left Earth faster than any other spacecraft launched, crossing the Moon's orbit in about nine hours"},
    {"New Horizons", 1, "冥王星を見つけたクライド・トンボーの遺灰の一部を積んでいる",
     "It carries some of the ashes of Clyde Tombaugh, who discovered Pluto"},
    {"New Horizons", 1, "打ち上げから 7 か月後の 2006 年 8 月、冥王星は惑星から準惑星に分け直された",
     "Seven months after launch, in August 2006, Pluto was reclassified from planet to dwarf planet"},
    {"New Horizons", 1, "木星のスイングバイで冥王星までの旅が約 3 年短くなった。冥王星のあと、2019 年 1 月 1 日に\nカイパーベルト天体アロコスのそばも通った",
     "The Jupiter flyby cut about three years off the trip. After Pluto it flew past the Kuiper belt object Arrokoth\non 1 January 2019"},

    {"Juno", 1, "木星まで行った探査機で初めて、太陽電池だけで動く。長さ約 9 m の太陽電池パネルが 3 枚",
     "The first spacecraft to work at Jupiter on solar power alone, with three solar arrays about 9 m long"},
    {"Juno", 1, "木星の強い放射線から電子機器を守るため、壁の厚さ約 1 cm のチタンの箱に入れている",
     "Its electronics sit in a titanium vault with walls about 1 cm thick to survive Jupiter's radiation"},
    {"Juno", 1, "木星の極の上を通る細長い軌道を回り、雲の下の構造や重力、磁場を調べる。\n機体にはガリレオ・ガリレイ、ユーピテル、ユーノーの 3 体のレゴ人形が乗っている",
     "From a long orbit over Jupiter's poles it studies the planet's interior, gravity and magnetic field.\nIt also carries three LEGO figures: Galileo Galilei, Jupiter and Juno"},

    {"Hayabusa2", 1, "イオンエンジンで小惑星リュウグウへ向かった。初代はやぶさのトラブルの経験を生かして作られた",
     "Ion engines carried it to the asteroid Ryugu. It was built on the lessons of the troubled first Hayabusa"},
    {"Hayabusa2", 1, "2018 年、小型ローバー MINERVA-II1 の 2 台が、小惑星の表面を初めて跳ねて移動した",
     "In 2018 its two MINERVA-II1 rovers became the first to hop across the surface of an asteroid"},
    {"Hayabusa2", 1, "2019 年、2 kg の銅の塊を秒速 2 km で撃ち込んで人工のクレーターを作り、その近くに降りて\n地下から飛び出した物質も集めた",
     "In 2019 it fired a 2 kg copper projectile at 2 km/s to make an artificial crater, then touched down nearby\nto collect material thrown up from below the surface"},
    {"Hayabusa2", 1, "地球に届けた試料は約 5.4 g。アミノ酸などの有機物が見つかった",
     "It delivered about 5.4 g of samples, in which amino acids and other organic molecules were found"},
    {"Hayabusa2", 2, "延長ミッションの最後の目標、小惑星 1998 KY26 は 10 分ほどで 1 回転する、とても速く自転する小さな小惑星",
     "The final target of the extended mission, 1998 KY26, is a small asteroid that spins once about every 10 minutes"},
    {"Hayabusa2", 2, "1998 KY26 に着くまでに、地球のスイングバイを 2 回重ねる",
     "It will swing past Earth twice more before reaching 1998 KY26"},

    {"OSIRIS-REx", 1, "目標のベンヌは、22 世紀後半に地球にぶつかる可能性がわずかにある小惑星",
     "Its target Bennu is an asteroid with a small chance of hitting Earth in the late 22nd century"},
    {"OSIRIS-REx", 1, "2020 年 10 月の採取で腕の先を表面に押し当てると、表面は思ったよりずっと柔らかく、\nそのまま沈み込みかけた。ベンヌの表面は固まっていない石の山だった",
     "When it pressed its sampling head to the surface in October 2020, the ground was far softer than expected\nand it began to sink in. Bennu's surface is loosely packed rubble"},
    {"OSIRIS-REx", 1, "試料が多く取れすぎてふたがきちんと閉まらず、少しこぼれていた。地球に届いたのは約 122 g",
     "It collected so much that the lid could not close properly and some escaped. About 122 g reached Earth"},
    {"OSIRIS-REx", 2, "アポフィスは 2004 年の発見の直後、2029 年に地球にぶつかる確率が 2.7% と計算されて注目された\n（今は衝突しないことがわかっている）",
     "Soon after its discovery in 2004, Apophis was briefly given a 2.7% chance of hitting Earth in 2029\n(it is now known it will not)"},
    {"OSIRIS-REx", 2, "2029 年 4 月 13 日の接近では、アポフィスはヨーロッパやアフリカから肉眼で見えるほど明るくなる",
     "During the 13 April 2029 approach, Apophis will be visible to the naked eye from Europe and Africa"},

    {"Parker Solar Probe", 1, "太陽側に厚さ約 11 cm の炭素の熱シールドがあり、表面が 1,000℃ を超えても、陰の機器は室温ほどに保たれる",
     "A carbon heat shield about 11 cm thick faces the Sun. Its front exceeds 1,000 °C while the instruments\nbehind it stay near room temperature"},
    {"Parker Solar Probe", 1, "名前は太陽風を予言した物理学者ユージン・パーカーから。存命中の人の名前が付いた初めての NASA の探査機",
     "It is named after Eugene Parker, who predicted the solar wind, the first NASA spacecraft named after a living person"},
    {"Parker Solar Probe", 1, "2021 年 4 月、初めて太陽のコロナの中に入った。最も近い所では秒速 190 km を超える",
     "In April 2021 it became the first spacecraft to fly through the Sun's corona. At its closest it moves\nat more than 190 km/s"},

    {"BepiColombo", 1, "ESA の MPO と JAXA の「みお」の 2 機の水星探査機を重ねて運び、水星に着いてから分かれる",
     "It carries two Mercury orbiters stacked together, ESA's MPO and JAXA's Mio, which separate on arrival"},
    {"BepiColombo", 1, "太陽に向かって落ちると速くなりすぎるので、地球 1 回・金星 2 回・水星 6 回のスイングバイで\n少しずつブレーキをかける",
     "Falling toward the Sun makes it too fast, so it brakes with gravity assists: one at Earth, two at Venus\nand six at Mercury"},
    {"BepiColombo", 1, "名前は、水星の自転と公転の関係を解き明かし、マリナー10号が水星に何度も近づける軌道を考えたジュゼッペ・コロンボから",
     "It is named after Giuseppe Colombo, who explained Mercury's spin-orbit resonance and devised\nthe orbit that let Mariner 10 return to Mercury again and again"},
    {"BepiColombo", 1, "電気推進のエンジンの不調で、水星を回る軌道に入るのは 2025 年 12 月から 2026 年 11 月に延びた",
     "A problem with its electric propulsion delayed its arrival in Mercury orbit from December 2025 to November 2026"},

    {"Lucy", 1, "名前は約 320 万年前の人類の祖先の化石「ルーシー」から。トロヤ群小惑星は惑星ができた頃の\n「化石」と考えられている",
     "It is named after \"Lucy\", the 3.2-million-year-old fossil of a human ancestor; the Trojan asteroids\nare thought to be fossils of planet formation"},
    {"Lucy", 1, "直径 7 m あまりの円い太陽電池パネルを 2 枚、扇のように広げている",
     "It unfurled two round solar arrays, each more than 7 m across, like folding fans"},
    {"Lucy", 1, "2023 年 11 月、小惑星ディンキネシュに小さな衛星セラムを見つけた。セラムは 2 つの塊がくっついた形だった",
     "In November 2023 it found that the asteroid Dinkinesh has a small moon, Selam, which turned out\nto be two lobes stuck together"},
    {"Lucy", 1, "1 機でこれまでで最も多くの小惑星を訪れる。木星の前と後ろの 2 つのトロヤ群を両方巡る",
     "It will visit more asteroids than any other mission, touring both Trojan swarms ahead of and behind Jupiter"},

    {"DART", 1, "小惑星の軌道を体当たりで変えられるかを確かめる、初めての惑星防衛の実験",
     "The first planetary defence test: can an impact change an asteroid's orbit?"},
    {"DART", 1, "衝突の様子は、15 日前に切り離したイタリアの小型衛星 LICIACube が撮影した",
     "The Italian CubeSat LICIACube, released 15 days earlier, photographed the impact"},
    {"DART", 1, "2022 年 9 月 26 日の衝突で、ディモルフォスがディディモスを回る周期は約 33 分短くなった",
     "The impact on 26 September 2022 shortened Dimorphos's orbit around Didymos by about 33 minutes"},

    {"James Webb Space Telescope", 1, "主鏡は金でおおった 18 枚の六角形の鏡を合わせた直径 6.5 m。ロケットに入るよう折りたたんで打ち上げた",
     "Its 6.5 m mirror is made of 18 gold-coated hexagonal segments, folded to fit inside the rocket"},
    {"James Webb Space Telescope", 1, "テニスコートほどの 5 層の日よけで太陽・地球・月の光と熱をさえぎり、赤外線で遠い宇宙の\n初期の銀河を見る",
     "A five-layer sunshield the size of a tennis court blocks the light and heat of the Sun, Earth and Moon,\nso it can see the earliest galaxies in infrared"},
    {"James Webb Space Telescope", 1, "2021 年 12 月 25 日にアリアン 5 で打ち上げた。軌道がとても正確で燃料が節約でき、\n20 年以上観測できる見込み",
     "It launched on an Ariane 5 on 25 December 2021. The launch was so accurate that the saved fuel\nshould let it work for more than 20 years"},

    {"JUICE", 1, "ESA の木星氷衛星探査機。最後はガニメデを回る、地球の月以外の衛星を回る初めての探査機になる",
     "ESA's Jupiter Icy Moons Explorer. It will end up orbiting Ganymede, the first spacecraft to orbit a moon\nother than ours"},
    {"JUICE", 1, "2024 年 8 月、月と地球のそばを 1 日おいて続けて通る、初めての「月・地球スイングバイ」をした",
     "In August 2024 it made the first lunar-Earth flyby, passing the Moon and then Earth a day later"},
    {"JUICE", 1, "打ち上げ後、長さ 16 m のレーダーのアンテナが引っかかって開かず、機体を揺らすなどして 1 か月後に開いた",
     "After launch its 16 m radar antenna was stuck; it was freed a month later, in part by jolting the spacecraft"},

    {"Psyche", 1, "目標の小惑星プシケは、惑星になりかけた天体の金属の核がむき出しになったものかもしれない",
     "Its target, asteroid Psyche, may be the exposed metal core of a planet-building body"},
    {"Psyche", 1, "ホール効果スラスターという電気推進で進む。キセノンを使い、青い光を出す",
     "It is driven by Hall-effect thrusters, electric engines that use xenon and glow blue"},
    {"Psyche", 1, "レーザー光で地球と通信する実験（深宇宙光通信）も載せていて、2023 年 12 月には約 3,100 万 km 先から猫の動画を送った",
     "It also carries a laser communication experiment (Deep Space Optical Communications) that in December 2023\nsent a cat video from about 31 million km away"},

    {"Hera", 1, "ESA の初めての惑星防衛のミッション。名前はギリシア神話の女神ヘラから",
     "ESA's first planetary defence mission, named after the Greek goddess Hera"},
    {"Hera", 1, "2025 年 3 月の火星スイングバイで、火星の小さな衛星ダイモスを撮影した",
     "During its March 2025 Mars flyby it imaged the small Martian moon Deimos"},
    {"Hera", 1, "DART の衝突で、ディモルフォスがどれほど重く、どれほど動いたかを測り、体当たりの効果を確かめる",
     "It will measure Dimorphos's mass and how much it moved, to work out exactly how effective DART's impact was"},

    {"Europa Clipper", 1, "NASA が作った惑星探査機で最大。太陽電池パネルを広げると端から端まで約 30 m",
     "The largest planetary spacecraft NASA has built: about 30 m across with its solar arrays deployed"},
    {"Europa Clipper", 1, "エウロパの氷の下の海が生命のすめる環境かを調べる。木星を回りながらエウロパのそばを約 50 回通る",
     "It will investigate whether the ocean under Europa's ice could support life, flying past Europa about 50 times\nwhile orbiting Jupiter"},
    {"Europa Clipper", 1, "機体には詩人エイダ・リモンが書いた詩と、世界中から寄せられた 260 万人あまりの名前が載っている",
     "It carries a poem by Ada Limón and the names of more than 2.6 million people from around the world"},
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

// 右上の小さな全体図のために、全行程の太陽中心の位置を等間隔の時刻で求めておく
static void SampleVoyageTrack(App& app) {
    const int N = 1500;
    std::vector<Body> B = app.B;
    for (Body& b : B) b.orbit.clear();
    double t0 = app.voySteps.front().t, t1 = app.voySteps.back().t;
    app.voyTrack.clear();
    app.voyTrackT.clear();
    app.voyStepPos.clear();
    for (int k = 0; k <= N; k++) {
        double t = t0 + (t1 - t0) * k / N;
        UpdateBodies(B, t);
        app.voyTrack.push_back(B[app.voyProbe].pos - B[B_SUN].pos);
        app.voyTrackT.push_back(t);
    }
    for (const VoyageStep& st : app.voySteps) {
        UpdateBodies(B, st.t);
        app.voyStepPos.push_back(B[app.voyProbe].pos - B[B_SUN].pos);
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
    s.q = QuatFromBasis(dir, Y_UP);
    s.local = ShipFwd(s) * -0.032 - ShipRight(s) * 0.009 - ShipUp(s) * 0.005;
}

// 今見たい方向: 知らせを出しているできごとの天体、なければ次のできごとの天体、それもなければ進む向き
static V3 VoyageAimDir(const App& app, int* bodyOut = nullptr) {
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

static void PrepareVoyageLocal(App& app, int k);
// 時刻 t に応じた時間の速さと、出す知らせを決める
static void UpdateVoyageRate(App& app) {
    const double day = 86400.0;
    double rate = app.voyCruise;
    int card = -1;
    for (size_t k = 0; k < app.voySteps.size(); k++) {
        double delta = (app.tDays - app.voySteps[k].t) * day;
        rate = std::min(rate, std::max(1.0, (std::fabs(delta) - VOY_HOLD_SEC) / VOY_RAMP_SEC));
        if (std::fabs(delta) < VOY_CARD_LEAD_SEC) card = (int)k;
    }
    if (app.voyDone) rate = 1;
    // 拡大図の軌跡は、場面に近づいた時に一度だけ求める（巡航の速さで 1 日以内）
    for (size_t k = 0; k < app.voySteps.size(); k++)
        if (!app.voyLocal[k].ready && std::fabs(app.tDays - app.voySteps[k].t) < 3.0) PrepareVoyageLocal(app, (int)k);
    if (card < 0 && app.voyDone) card = (int)app.voySteps.size() - 1;
    app.voyRate = rate;
    app.voyCard = card;
}

static void StopVoyage(App& app) {
    if (!app.voyageOn) return;
    app.voyageOn = false;
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
    app.voyAim = VoyageAimDir(app, &app.voyAimBody);
    app.voyAimPrevWant = app.voyAim;
    PlaceVoyageCamera(app, app.voyAim);
}

// 巡航の速さ（0: ゆっくり, 1: 標準, 2: 速い）
static void VoyageSetSpeed(App& app, int speed) {
    app.voySpeed = std::max(0, std::min(2, speed));
    if (app.voySteps.size() < 2) return;
    double days = app.voySteps.back().t - app.voySteps.front().t;
    app.voyCruise = std::max(0.2, std::min(80.0, days / VOY_CRUISE_WALL_SEC[app.voySpeed])) * 86400.0;
}

// 旅の名前（延長ミッションはその名前）
static std::string VoyageName(const App& app, int i, int leg) {
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

// URL の voyage= と scene= から始める（起動時に一度）。scene: launch / arrive / 場面の番号（1 から）
static void StartPendingVoyage(App& app) {
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
    UpdateVoyageRate(app);
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
                double a = std::min(moved, 20 * DEG * dt);
                aim = aim * std::cos(a) + Cross(axis, aim) * std::sin(a) + axis * (Dot(axis, aim) * (1 - std::cos(a)));
                aim = Norm(aim);
            }
        }
    }
    double gap = std::acos(std::max(-1.0, std::min(1.0, Dot(aim, want))));
    app.voyAim = rotateToward(aim, want, std::min(gap * (1 - std::exp(-dt / 0.4)), 40 * DEG * dt));
    app.voyAimBody = aimBody;
    app.voyAimPrevWant = want;
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
        if (st.body <= B_SUN || !app.voyLocal[k].ready) continue;
        if (std::fabs(app.tDays - st.t) < app.voyLocal[k].span) return (int)k;
    }
    return -1;
}

// 場面 k の拡大図の軌跡を求める: 最接近の距離の数倍まで離れる時間の範囲で、天体から見た探査機の位置
static void PrepareVoyageLocal(App& app, int k) {
    App::VoyLocal& L = app.voyLocal[k];
    if (L.ready) return;
    L.ready = true;
    const VoyageStep& st = app.voySteps[k];
    if (st.body <= B_SUN) return;
    std::vector<Body> B = app.B;
    for (Body& b : B) b.orbit.clear();
    auto relAt = [&](double t) {
        UpdateBodies(B, t);
        return B[app.voyProbe].pos - B[st.body].pos;
    };
    // 最接近の距離の 4 倍（地球の出発・脱出は重力圏の半径まで）離れるまでの時間を、前後それぞれ探す
    double d0 = std::max(Len(relAt(st.t)), app.B[st.body].d.radius * 2);
    double reach = st.kind == VS_LAUNCH || st.kind == VS_ESCAPE ? app.B[B_EARTH].soi * 1.3 : d0 * 4;
    double t0 = app.voySteps.front().t, t1 = app.voySteps.back().t;
    auto edge = [&](double dir) {
        double h = 1.0 / 1440;   // 1 分から倍々に
        while (h < 60 && Len(relAt(std::max(t0, std::min(t1, st.t + dir * h)))) < reach) h *= 2;
        return std::min(h, 60.0);
    };
    double before = std::min(edge(-1), st.t - t0), after = std::min(edge(1), t1 - st.t);
    if (st.kind == VS_LAUNCH) before = 0;
    L.span = std::max(before, after);
    const int N = 400;
    for (int j = 0; j <= N; j++) {
        double t = st.t - before + (before + after) * j / N;
        L.t.push_back(t);
        L.rel.push_back(relAt(t));
    }
}

// 色の透明度に掛ける（右上の図の切り替えのフェード）
static Color Alpha(Color c, float a) {
    c.a = (unsigned char)(c.a * std::max(0.0f, std::min(1.0f, a)));
    return c;
}

// 天体の近くの拡大図（場面 ls の天体を中心に）。fa は透明度
static void DrawVoyageMapLocal(const App& app, Rectangle r, int ls, float fa) {
    const App::VoyLocal& L = app.voyLocal[ls];
    const Body& o = app.B[app.voySteps[ls].body];
    double range = o.d.radius * 1.5;
    for (const V3& p : L.rel) range = std::max(range, std::sqrt(p.x * p.x + p.z * p.z));
    range *= 1.08;
    float cx = r.x + r.width * 0.5f, cy = r.y + r.height * 0.5f + 6, R = r.width * 0.5f - 10;
    double k = R / range;
    auto proj = [&](V3 p) { return Vector2{(float)(cx + p.x * k), (float)(cy + p.z * k)}; };
    Rectangle in = {r.x + 2, r.y + 2, r.width - 4, r.height - 4};
    DrawCircleV({cx, cy}, std::max(2.5f, (float)(o.d.radius * k)), Alpha(BodyColor(o, 0.9f), fa));
    for (size_t j = 1; j < L.rel.size(); j++) {
        Vector2 a = proj(L.rel[j - 1]), b = proj(L.rel[j]);
        if (!CheckCollisionPointRec(a, in) || !CheckCollisionPointRec(b, in)) continue;
        bool past = L.t[j] <= app.tDays;
        DrawLineEx(a, b, past ? 1.8f : 1.0f, past ? Alpha({255, 196, 120, 230}, fa) : Alpha({255, 196, 120, 70}, fa));
    }
    Vector2 me = proj(app.B[app.voyProbe].pos - o.pos);
    if (CheckCollisionPointRec(me, in)) {
        DrawCircleV(me, 3.0f, Alpha({120, 255, 170, 255}, fa));
        DrawCircleLinesV(me, 4.0f + 2.0f * (float)std::fabs(std::sin(GetTime() * 3)), Alpha({120, 255, 170, 220}, fa));
    }
    char b[96];
    snprintf(b, sizeof b, T(UI_VOY_MAP_NEAR), TR(o.d.jp, o.d.en));
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
    range = std::max(range * 1.08, 0.05 * AU_KM);
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
    const V3 sun = app.B[B_SUN].pos;
    // 惑星の軌道と今の位置（全体図の外まで広がる軌道も、図の中の部分は描く）
    for (int i = 1; i <= B_PLUTO; i++) {
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
    DrawCircleV({cx, cy}, 3.5f, Alpha({255, 220, 120, 255}, fa));
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
    snprintf(b, sizeof b, T(UI_VOY_HEAD), VoyageName(app, app.voyProbe, app.voyLeg).c_str());
    Txt(b, x, y, fs + 3, {255, 220, 120, 255}); y += lh + 6;
    double elapsed = std::max(0.0, app.tDays - app.voySteps.front().t);
    snprintf(b, sizeof b, T(UI_VOY_DATE), FmtDate(app.tDays).c_str());
    Txt(b, x, y, fs, WHITE); y += lh;
    int ey = (int)(elapsed / 365.25);
    snprintf(b, sizeof b, T(UI_VOY_ELAPSED), ey, (int)(elapsed - ey * 365.25));
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
