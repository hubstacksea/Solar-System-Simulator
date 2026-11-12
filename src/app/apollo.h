// Solar System Voyager — Copyright (c) 2026 hubstacksea
// MIT License (LICENSE を参照) — https://github.com/hubstacksea/Solar-System-Simulator
// アポロ計画: 打ち上げから月への到着・着陸・帰還・着水までを、宇宙船のそばから追う（探査機の旅と同じ見せ方）。
// 宇宙船の位置は assets/apollo.bin（tools/make_apollo_assets.py が NASA の飛行記録の位置と速度から計算）。
// できごとの時刻と数値は NASA の記録（Apollo by the Numbers, SP-2000-4029 と各号の Mission Report）による。
// main.cpp の一部で、main.cpp が決まった順に読み込む（単体ではコンパイルしない）

// 見る向き（できごとの look）: 天体の番号、または進む向き
static const int AP_LOOK_FWD = -1;

struct ApolloEvent {
    const char* get;        // 打ち上げからの経過時間（GET）"hhh:mm:ss.s"
    int vehicle;            // そのできごとで追う機体（0: 司令船とそれにつながった機体, 1: 月着陸船）
    int look;               // 見る向き（AP_LOOK_FWD か天体の番号）
    const char *jp, *en, *textJp, *textEn;
};
// 機体の形の切り替え（その時刻から）
struct ApolloShape {
    const char* get;
    int vehicle;
    int model;              // ProbeModel（-1: 描かない。司令船とつながっている間の月着陸船など）
};
// 第3段から離れ、向きを変えて月着陸船とドッキングし、引き出す（transposition, docking and extraction）。
// 記録の軌跡は司令船の 1 本だけで、数十 m の動きは入っていないので、各号の Mission Report と Apollo Flight Journal の
// 値から、第3段に対する司令船の動きを組み立てる（数分間・数十 m の相対の動きなので、重力の差は無視できて直線の動き）
// ドッキングをやり直した時の、決まった時刻の 2 機の間（ドッキングした位置からの距離 m。0 は接触）
struct ApolloTdeKey {
    const char* get;
    double gap;
};
struct ApolloTde {
    const char *sep, *dock, *eject, *end;   // 離れる・ドッキング・引き出す時刻（月着陸船を積まない 8 号は dock と eject が
                                            // nullptr）。end: 第3段を描くのをやめる時刻（第3段が自分で向きを変えたり噴射したり
                                            // し始める時。その後の第3段の動きは記録がない）
    double vSep;            // 離れる速さ m/s
    double dMax;            // 振り向いて戻り始める時の、司令船が離れた距離 m（0: 記録がないので、離れる速さで離れ始めて
                            // 向きを変え終わる時に止まる距離）
    double turnStart;       // 離れてから向きを変え始めるまでの秒
    double turnRate;        // 向きを変える速さ deg/s（180° 回る）
    double vContact;        // ドッキングの時の近づく速さ m/s
    double vEject;          // 引き出す時に離れる速さ m/s
    const ApolloTdeKey* keys = nullptr;   // ドッキングの前の接触とやり直し（最初の接触から。無ければ dock で接触）
    int keyCount = 0;
    const char* kickGet = nullptr;        // 記録の軌跡に入っていない司令船の噴射（地球から外向き）の時刻と速さ m/s
    double kickDv = 0;
};
// 月を回る軌道での月着陸船の切り離しと、月から戻った上昇段のドッキング。記録の軌跡は数 km の精度で、切り離した後は
// 司令船の分離の噴射まで同じ道（10〜12 号）か、すぐに秒速 1〜3 m で離れていき（14〜17 号）、ドッキングの前は上昇段が減速せずに
// 秒速 7 m ほどで司令船の位置へ入っていく。数十 m の動きは各号の Apollo Flight Journal（乗員の報告と Technical debrief）の値から
// 組み立てる（数分間・数十 m の相対の動きなので、重力の差は無視できて直線の動き）
struct ApolloLunarDock {
    const char* gear;       // 月着陸船の脚を開いた時刻（ドッキングした形も脚を開いた形にする）
    const char* undock;     // 切り離し
    const char* undockTurn; // 切り離した後、月着陸船が司令船に脚を見せる向きへ回り始める時刻（nullptr は切り離しの 60 秒後）
    const char* dock;       // ドッキング（ドッキングした形になる時刻。記録の多くは固くつながった時刻）
    const char* contact;    // 接触して捕まえた時刻（nullptr は dock）
    const char* turn;       // 上昇段がドッキングの向きへ回り始める時刻（nullptr は最後に近づき始める 150 秒前）
    const ApolloTdeKey* keys = nullptr;   // ドッキングの前の 2 機の間（ドッキングした位置からの距離 m。無ければ手順書の
    int keyCount = 0;                     // 40 ft で 5 分待ち、0.1 ft/s で近づいて接触）
};
struct ApolloMission {
    const char* name;       // assets/apollo.bin の飛行の名前
    const char *jp, *en;
    int y, mo, d, h, mi;    // 打ち上げ（GET 0）の UTC
    double sec;
    const char* vehicles[2];        // 機体の天体の英語名
    const char *noteJp, *noteEn;    // 一覧に出す短い説明
    const ApolloEvent* events;
    int eventCount;
    const ApolloShape* shapes;
    int shapeCount;
    double siteLat, siteLon;        // 着陸地点（下降段が残る所）。着陸しなかった飛行は NaN
    const char* liftoff;            // 月面を離れた時刻（下降段を月面に描き始める）
    const ApolloTde* tde;           // 月着陸船を引き出す手順（無ければ nullptr）
    int sicRetros = 8;              // 第1段の逆噴射のロケットの数（15 号は 8 基のうち 4 基を外した。Flight Evaluation Report）
    const ApolloLunarDock* lunar = nullptr;   // 月を回る軌道での切り離しとドッキング（無ければ nullptr）
    // 大気に入る前に切り離した機械船: 質量 lb（各号の Mission Report の質量の表。8・10 号は切り離す前の司令・機械船と、切り離した
    // 後の司令船の差）、後ろへ離れる噴射（-X の 4 基）の秒、回す噴射（ロールの 4 基、切り離して 2 秒後から）の秒。0 は噴射しない
    // （13 号: 機械船は電源が無く、月着陸船の噴射で離れた）
    double smLb = 0, smBurn = 0, smRoll = 0;
};

static const ApolloEvent APOLLO11_EVENTS[] = {
    {"0:00:00.63", 0, AP_LOOK_FWD, "打ち上げ", "Liftoff",
     "ケネディ宇宙センター 39A 発射台から、サターン V ロケットで打ち上げ。乗員はニール・アームストロング船長、\nマイケル・コリンズ司令船操縦士、バズ・オルドリン月着陸船操縦士",
     "Launched on a Saturn V from Launch Complex 39A at Kennedy Space Center. Crew: Commander Neil Armstrong,\nCommand Module Pilot Michael Collins and Lunar Module Pilot Buzz Aldrin"},
    {"0:02:42.30", 0, AP_LOOK_FWD, "第1段を切り離す", "First stage separation",
     "高度 約 67 km、秒速 2.8 km（地球の中心に対する速さ）で第1段 S-IC を切り離し、第2段 S-II に点火",
     "At about 67 km altitude and 2.8 km/s (relative to Earth's center) the S-IC first stage separated and the S-II second stage ignited"},
    {"0:09:09.00", 0, AP_LOOK_FWD, "第2段を切り離す", "Second stage separation",
     "高度 約 187 km、秒速 6.9 km で第2段を切り離し、第3段 S-IVB に点火",
     "At about 187 km altitude and 6.9 km/s the second stage separated and the S-IVB third stage ignited"},
    {"0:11:49.33", 0, B_EARTH, "地球を回る軌道に入る", "Earth orbit insertion",
     "高度 約 183〜186 km、1 周 88 分の軌道（赤道に対する傾き 32.5°）。ここで宇宙船とロケットの状態を確かめる",
     "An orbit about 183-186 km high, 88 minutes per revolution, inclined 32.5° to the equator.\nThe crew checked out the spacecraft and the rocket here"},
    {"2:44:16.2", 0, B_MOON, "月へ向かう噴射（TLI）", "Translunar injection",
     "第3段 S-IVB を再び 5 分 47 秒噴射し、秒速 3.05 km 速めて秒速 10.8 km に。月へ向かう軌道に乗った",
     "The S-IVB fired again for 5 min 47 s, gaining 3.05 km/s to reach 10.8 km/s on a course to the Moon"},
    {"3:17:04.6", 0, AP_LOOK_FWD, "第3段から離れる", "Separation from the S-IVB",
     "司令・機械船コロンビアが第3段から離れて向きを変え、第3段の上に収まっている月着陸船イーグルへ向かう",
     "The command and service module Columbia separated from the S-IVB and turned around toward the lunar module Eagle,\nstowed atop the stage"},
    {"3:24:03.1", 0, AP_LOOK_FWD, "月着陸船とドッキング", "Docked with the lunar module",
     "コロンビアの先端をイーグルの上の口につないだ", "Columbia's nose was docked to Eagle's upper hatch"},
    {"4:16:59.1", 0, B_MOON, "月着陸船を引き出す", "Lunar module extracted",
     "ドッキングしたイーグルを第3段から引き出した。第3段はこの後、月の近くを通って太陽を回る軌道へ",
     "The docked Eagle was pulled out of the S-IVB. The stage later passed the Moon into an orbit around the Sun"},
    {"26:44:58.7", 0, B_MOON, "軌道の修正", "Midcourse correction",
     "主エンジンを 3 秒噴射し、秒速 6.4 m だけ速さを変えた。月へ向かう途中の軌道修正はこの 1 回だけ",
     "A 3-second burn of the main engine changed the speed by 6.4 m/s, the only course correction on the way to the Moon"},
    {"75:49:50.4", 0, B_MOON, "月を回る軌道に入る", "Lunar orbit insertion",
     "月の裏側で主エンジンを 5 分 58 秒噴射して秒速 889 m 減速し、高度 約 111〜314 km の月を回る軌道に入った。\n地球からは見えない側なので、交信が途絶えている間の噴射",
     "Behind the Moon the main engine fired for 5 min 58 s, slowing by 889 m/s into a lunar orbit about 111-314 km high.\nThe burn took place out of contact with Earth"},
    {"80:11:36.8", 0, B_MOON, "軌道を円に近づける", "Orbit circularization",
     "17 秒の噴射で、高度 約 101〜122 km の軌道に", "A 17-second burn lowered the orbit to about 101-122 km"},
    {"100:12:00.0", 1, B_MOON, "イーグルが切り離される", "Eagle undocks",
     "アームストロングとオルドリンの乗った月着陸船イーグルが、コリンズの残る司令船コロンビアから離れた",
     "The lunar module Eagle, with Armstrong and Aldrin aboard, undocked from Columbia, where Collins stayed"},
    {"101:36:14.0", 1, B_MOON, "降下軌道へ", "Descent orbit insertion",
     "月の裏側で下降エンジンを 30 秒噴射し、最も低い所が高度 約 14 km の軌道へ",
     "Behind the Moon the descent engine fired for 30 s, dropping the low point of the orbit to about 14 km"},
    {"102:33:05.0", 1, AP_LOOK_FWD, "動力降下の開始", "Powered descent",
     "下降エンジンを噴射し続け、12 分 36 秒かけて月面へ降りる", "The descent engine fired continuously for 12 min 36 s down to the surface"},
    {"102:38:22", 1, AP_LOOK_FWD, "コンピューターの警報", "Computer alarm",
     "誘導コンピューターの処理が追いつかないことを示す警報 1202 が出た。管制センターは降下を続けてよいと判断した\n（この後も 1201・1202 の警報が出て、合わせて 5 回）",
     "The guidance computer raised program alarm 1202: it was overloaded. Mission Control judged it safe to continue\n(five 1201/1202 alarms in all)"},
    {"102:41:32", 1, AP_LOOK_FWD, "最終進入", "Final approach",
     "高度 約 2,170 m（7,129 フィート）で最終進入のプログラムに切り替わり、機体を起こして前方の月面が見えるようになった",
     "At about 2,170 m (7,129 ft) the final approach program took over and the LM pitched up so the crew could see the surface ahead"},
    {"102:43:13", 1, AP_LOOK_FWD, "手動の操縦へ", "Manual control",
     "向かう先が大きなクレーターだったため、アームストロングが手動の操縦に切り替え、\n予定より約 340 m（1,100 フィート）先まで飛んで降りる場所を探した",
     "The approach path led into a large crater, so Armstrong took manual control and flew about 340 m (1,100 ft)\nbeyond the planned point to find a place to land"},
    {"102:45:39.9", 1, B_EARTH, "月面に着陸", "Landed on the Moon",
     "静かの海（北緯 0.674°・東経 23.473°）に着陸。「ヒューストン、こちら静かの基地。イーグルは着陸した」",
     "Touchdown in the Sea of Tranquility (0.674° N, 23.473° E). \"Houston, Tranquility Base here. The Eagle has landed.\""},
    {"109:24:15", 1, B_EARTH, "人類が初めて月面に立つ", "First step on the Moon",
     "アームストロングが月面に降り立った。「これは一人の人間にとっては小さな一歩だが、人類にとっては偉大な飛躍だ」。\nオルドリンも続き、2 人は 2 時間半ほど月面で活動した",
     "Armstrong stepped onto the surface: \"That's one small step for a man, one giant leap for mankind.\"\nAldrin followed, and the two worked outside for about two and a half hours"},
    {"124:22:00.79", 1, AP_LOOK_FWD, "月面を離れる", "Lunar liftoff",
     "21 時間 36 分の月面滞在を終え、上昇段が飛び立った。下降段は発射台として月面に残った",
     "After 21 h 36 min on the Moon the ascent stage lifted off. The descent stage stayed behind as its launch pad"},
    {"124:29:15.67", 1, B_MOON, "月を回る軌道へ", "Into lunar orbit",
     "7 分 15 秒の噴射で、高度 約 17〜89 km の軌道に入った", "A 7 min 15 s burn put the ascent stage into an orbit about 17-89 km high"},
    {"125:19:35.0", 1, B_MOON, "ランデブーの開始", "Rendezvous begins",
     "司令船の軌道に追いつくための軌道の調整を始めた", "The first of the maneuvers to catch up with Columbia"},
    {"127:03:51.8", 1, B_MOON, "司令船へ向かう", "Terminal phase initiation",
     "司令船へまっすぐ近づいていく噴射", "The burn that set the ascent stage on its final approach to Columbia"},
    {"128:03:00.0", 0, B_MOON, "ドッキング", "Docking",
     "イーグルの上昇段がコロンビアとドッキング。アームストロングとオルドリンが月の石など約 22 kg の試料を持って司令船に戻った",
     "Eagle's ascent stage docked with Columbia. Armstrong and Aldrin returned to the command module with about 22 kg\nof lunar samples"},
    {"130:09:31.2", 0, B_MOON, "上昇段を切り離す", "Ascent stage jettisoned",
     "役目を終えたイーグルの上昇段を切り離した。上昇段は月を回る軌道に残された",
     "Eagle's ascent stage, its work done, was jettisoned and left in lunar orbit"},
    {"135:23:42.28", 0, B_EARTH, "地球へ向かう噴射", "Transearth injection",
     "月の裏側で主エンジンを 2 分 31 秒噴射し、秒速 1.0 km 速めて地球へ向かう軌道に",
     "Behind the Moon the main engine fired for 2 min 31 s, gaining 1.0 km/s, onto a course for Earth"},
    {"150:29:57.4", 0, B_EARTH, "軌道の修正", "Midcourse correction",
     "10 秒の噴射で、秒速 1.5 m だけ速さを変えた", "A 10-second burn changed the speed by 1.5 m/s"},
    {"194:49:12.7", 0, B_EARTH, "機械船を切り離す", "Service module jettisoned",
     "司令船だけで大気に入るため、機械船を切り離した", "The service module was jettisoned so the command module could enter the atmosphere alone"},
    {"195:03:05.7", 0, AP_LOOK_FWD, "大気圏に突入", "Atmospheric entry",
     "高度 約 122 km、秒速 11.0 km で大気に突入。空気との摩擦で減速し、最大で 6.5 G がかかった",
     "Entering the atmosphere at about 122 km altitude and 11.0 km/s. Air drag slowed the capsule, peaking at 6.5 g"},
    {"195:12:06.9", 0, AP_LOOK_FWD, "パラシュートを開く", "Parachutes",
     "減速用のドローグ・パラシュートを開き、続けて 3 つの主パラシュートを開いた",
     "The drogue parachutes opened, followed by the three main parachutes"},
    {"195:18:35", 0, AP_LOOK_FWD, "着水", "Splashdown",
     "ハワイの南西の太平洋（北緯 13.30°・西経 169.15°）に着水し、空母ホーネットが回収した。\n打ち上げから 8 日 3 時間 18 分の飛行",
     "Splashdown in the Pacific southwest of Hawaii (13.30° N, 169.15° W); the carrier USS Hornet recovered the crew.\nThe flight lasted 8 days 3 hours 18 minutes"},
};

// サターンの段間リングは、第1段を切り離した 30 秒後に捨てる（Flight Evaluation Report の 8 号 154.5 秒と 184.5 秒、10 号と
// 11 号 162.3 秒と 192.3 秒。16 号はリングが第2段のエンジンの下を抜けたのが 194.6 秒で、163.5 秒の 30 秒後の指令と合う）
static const ApolloShape APOLLO11_SHAPES[] = {
    {"-3:00:00", 0, PM_SATURN_V}, {"0:02:42.30", 0, PM_SATURN_SII_RING}, {"0:03:12.30", 0, PM_SATURN_SII_LES}, {"0:03:17.90", 0, PM_SATURN_SII},
    {"0:09:09.00", 0, PM_SATURN_SIVB}, {"3:17:04.6", 0, PM_APOLLO_CSM}, {"3:24:03.1", 0, PM_APOLLO_CSM_LM},
    {"98:14:35", 0, PM_APOLLO_CSM_LMG}, {"100:12:00.0", 0, PM_APOLLO_CSM}, {"128:03:00.0", 0, PM_APOLLO_CSM_LMA}, {"130:09:31.2", 0, PM_APOLLO_CSM},
    {"194:49:12.7", 0, PM_APOLLO_CM}, {"195:12:06.9", 0, PM_CM_DROGUE}, {"195:12:54", 0, PM_CM_MAIN}, {"195:18:35", 0, PM_APOLLO_CM},
    {"100:12:00.0", 1, PM_APOLLO_LM}, {"124:22:00.79", 1, PM_LM_ASCENT}, {"128:03:00.0", 1, -1}, {"130:09:31.2", 1, PM_LM_ASCENT},
};

static const ApolloEvent APOLLO8_EVENTS[] = {
    {"0:00:00.67", 0, AP_LOOK_FWD, "打ち上げ", "Liftoff",
     "ケネディ宇宙センター 39A 発射台から、人を乗せた初めてのサターン V で打ち上げ。乗員はフランク・ボーマン船長、\nジェームズ・ラベル司令船操縦士、ウィリアム・アンダース月着陸船操縦士（月着陸船は積んでいない）",
     "The first crewed Saturn V lifted off from Launch Complex 39A. Crew: Commander Frank Borman, Command Module Pilot\nJames Lovell and Lunar Module Pilot William Anders (no lunar module was carried)"},
    {"0:02:34.47", 0, AP_LOOK_FWD, "第1段を切り離す", "First stage separation",
     "高度 約 66 km、秒速 2.7 km（地球の中心に対する速さ）で第1段 S-IC を切り離し、第2段 S-II に点火",
     "At about 66 km altitude and 2.7 km/s (relative to Earth's center) the S-IC first stage separated and the S-II second stage ignited"},
    {"0:08:44.90", 0, AP_LOOK_FWD, "第2段を切り離す", "Second stage separation",
     "高度 約 192 km、秒速 6.8 km で第2段を切り離し、第3段 S-IVB に点火",
     "At about 192 km altitude and 6.8 km/s the second stage separated and the S-IVB third stage ignited"},
    {"0:11:34.98", 0, B_EARTH, "地球を回る軌道に入る", "Earth orbit insertion",
     "高度 約 184〜185 km、1 周 88 分の軌道（赤道に対する傾き 32.5°）", "An orbit about 184-185 km high, 88 minutes per revolution, inclined 32.5°"},
    {"2:50:37.79", 0, B_MOON, "月へ向かう噴射（TLI）", "Translunar injection",
     "第3段を 5 分 18 秒噴射し、秒速 3.04 km 速めて月へ向かう軌道に。人類が初めて地球を回る軌道を離れた",
     "The S-IVB fired for 5 min 18 s, gaining 3.04 km/s, onto a course to the Moon. Humans left Earth orbit for the first time"},
    {"3:20:59", 0, B_MOON, "第3段から離れる", "Separation from the S-IVB",
     "司令・機械船が第3段から離れた", "The command and service module separated from the S-IVB"},
    {"10:59:59.5", 0, B_MOON, "軌道の修正", "Midcourse correction",
     "主エンジンを 2.4 秒噴射し、秒速 6.2 m だけ速さを変えた", "A 2.4-second burn of the main engine changed the speed by 6.2 m/s"},
    {"60:59:55", 0, B_MOON, "軌道の修正", "Midcourse correction",
     "月の重力圏に入った後の小さな修正（12 秒、秒速 0.4 m）", "A small correction after entering the Moon's sphere of influence (12 s, 0.4 m/s)"},
    {"69:08:20.4", 0, B_MOON, "月を回る軌道に入る", "Lunar orbit insertion",
     "月の裏側で主エンジンを 4 分 7 秒噴射して秒速 913 m 減速し、高度 約 111〜312 km の月を回る軌道に入った。\n人類が初めて月を回った",
     "Behind the Moon the main engine fired for 4 min 7 s, slowing by 913 m/s into a lunar orbit about 111-312 km high.\nHumans orbited the Moon for the first time"},
    {"73:35:07", 0, B_MOON, "軌道を円に近づける", "Orbit circularization",
     "10 秒の噴射で、高度 約 111〜112 km の軌道に", "A 10-second burn put the spacecraft into an orbit about 111-112 km high"},
    {"86:06:56", 0, B_EARTH, "月からのクリスマス・イブの放送", "Christmas Eve broadcast",
     "月を回りながらのテレビの生放送で、3 人が旧約聖書の創世記の冒頭を読み上げた（アメリカでは 12 月 24 日の夜）",
     "In a live television broadcast from lunar orbit, the three astronauts read the opening of the Book of Genesis\n(Christmas Eve in the United States)"},
    {"89:19:16.6", 0, B_EARTH, "地球へ向かう噴射", "Transearth injection",
     "月を 10 周した後、月の裏側で主エンジンを 3 分 24 秒噴射し、秒速 1.07 km 速めて地球へ向かう軌道に",
     "After ten lunar orbits the main engine fired behind the Moon for 3 min 24 s, gaining 1.07 km/s, onto a course for Earth"},
    {"103:59:54", 0, B_EARTH, "軌道の修正", "Midcourse correction",
     "15 秒の噴射で、秒速 1.5 m だけ速さを変えた", "A 15-second burn changed the speed by 1.5 m/s"},
    {"146:28:48.0", 0, B_EARTH, "機械船を切り離す", "Service module jettisoned",
     "司令船だけで大気に入るため、機械船を切り離した", "The service module was jettisoned so the command module could enter the atmosphere alone"},
    {"146:46:12.8", 0, AP_LOOK_FWD, "大気圏に突入", "Atmospheric entry",
     "高度 約 122 km、秒速 11.0 km で大気に突入。月から戻った宇宙船の初めての突入で、最大で 6.8 G がかかった",
     "Entering the atmosphere at about 122 km altitude and 11.0 km/s, the first return from the Moon, peaking at 6.8 g"},
    {"146:54:47.8", 0, AP_LOOK_FWD, "パラシュートを開く", "Parachutes",
     "減速用のドローグ・パラシュートを開き、続けて 3 つの主パラシュートを開いた",
     "The drogue parachutes opened, followed by the three main parachutes"},
    {"147:00:42", 0, AP_LOOK_FWD, "着水", "Splashdown",
     "夜明け前の太平洋（北緯 8.10°・西経 165.00°）に着水し、空母ヨークタウンが回収した。打ち上げから 6 日 3 時間の飛行",
     "Splashdown before dawn in the Pacific (8.10° N, 165.00° W); the carrier USS Yorktown recovered the crew.\nThe flight lasted 6 days 3 hours"},
};

static const ApolloShape APOLLO8_SHAPES[] = {
    {"-3:00:00", 0, PM_SATURN_V}, {"0:02:34.47", 0, PM_SATURN_SII_RING}, {"0:03:04.47", 0, PM_SATURN_SII_LES}, {"0:03:08.6", 0, PM_SATURN_SII},
    {"0:08:44.90", 0, PM_SATURN_SIVB}, {"3:20:59", 0, PM_APOLLO_CSM}, {"146:28:48.0", 0, PM_APOLLO_CM},
    {"146:54:47.8", 0, PM_CM_DROGUE}, {"146:55:38.9", 0, PM_CM_MAIN}, {"147:00:42", 0, PM_APOLLO_CM},
};

static const ApolloEvent APOLLO10_EVENTS[] = {
    {"0:00:00.58", 0, AP_LOOK_FWD, "打ち上げ", "Liftoff",
     "ケネディ宇宙センター 39B 発射台から打ち上げ（39B から打ち上げた唯一のサターン V）。乗員はトーマス・スタッフォード船長、\nジョン・ヤング司令船操縦士、ユージン・サーナン月着陸船操縦士。着陸の手前までを確かめる予行演習",
     "Liftoff from Launch Complex 39B, the only Saturn V launched from that pad. Crew: Commander Thomas Stafford,\nCommand Module Pilot John Young and Lunar Module Pilot Eugene Cernan, on a dress rehearsal for the landing"},
    {"0:02:42.31", 0, AP_LOOK_FWD, "第1段を切り離す", "First stage separation",
     "高度 約 66 km、秒速 2.8 km で第1段 S-IC を切り離し、第2段 S-II に点火", "At about 66 km altitude and 2.8 km/s the S-IC separated and the S-II ignited"},
    {"0:09:13.50", 0, AP_LOOK_FWD, "第2段を切り離す", "Second stage separation",
     "高度 約 188 km、秒速 6.9 km で第2段を切り離し、第3段 S-IVB に点火", "At about 188 km altitude and 6.9 km/s the S-II separated and the S-IVB ignited"},
    {"0:11:53.76", 0, B_EARTH, "地球を回る軌道に入る", "Earth orbit insertion",
     "高度 約 185〜186 km、1 周 88 分の軌道（赤道に対する傾き 32.5°）", "An orbit about 185-186 km high, 88 minutes per revolution, inclined 32.5°"},
    {"2:33:27.6", 0, B_MOON, "月へ向かう噴射（TLI）", "Translunar injection",
     "第3段を 5 分 43 秒噴射し、秒速 10.8 km で月へ向かう軌道に", "The S-IVB fired for 5 min 43 s, reaching 10.8 km/s on a course to the Moon"},
    {"3:02:42.4", 0, AP_LOOK_FWD, "第3段から離れる", "Separation from the S-IVB",
     "司令・機械船チャーリー・ブラウンが第3段から離れて向きを変え、月着陸船スヌーピーとドッキングして引き出した",
     "The command and service module Charlie Brown separated, turned around, docked with the lunar module Snoopy and pulled it free"},
    {"26:32:56.8", 0, B_MOON, "軌道の修正", "Midcourse correction",
     "7 秒の噴射で、秒速 15 m だけ速さを変えた", "A 7-second burn changed the speed by 15 m/s"},
    {"75:55:54.0", 0, B_MOON, "月を回る軌道に入る", "Lunar orbit insertion",
     "月の裏側で主エンジンを 5 分 56 秒噴射して秒速 909 m 減速し、高度 約 111〜315 km の月を回る軌道に入った",
     "Behind the Moon the main engine fired for 5 min 56 s, slowing by 909 m/s into a lunar orbit about 111-315 km high"},
    {"80:25:08.1", 0, B_MOON, "軌道を円に近づける", "Orbit circularization",
     "14 秒の噴射で、高度 約 110〜113 km の軌道に", "A 14-second burn put the spacecraft into an orbit about 110-113 km high"},
    {"98:11:57", 1, B_MOON, "スヌーピーが切り離される", "Snoopy undocks",
     "スタッフォードとサーナンの乗った月着陸船スヌーピーが、ヤングの残る司令船から離れた",
     "The lunar module Snoopy, with Stafford and Cernan aboard, undocked from the command module, where Young stayed"},
    {"99:46:01.6", 1, B_MOON, "降下軌道へ", "Descent orbit insertion",
     "下降エンジンを 27 秒噴射し、最も低い所が高度 約 16 km の軌道へ", "The descent engine fired for 27 s, dropping the low point of the orbit to about 16 km"},
    {"100:41:43", 1, AP_LOOK_FWD, "月面に最も近づく", "Closest approach to the surface",
     "高度 約 14 km（7.8 海里）まで降り、11 号の着陸予定地の静かの海を見下ろした。着陸はしない",
     "Snoopy came down to about 14 km (7.8 nautical miles), looking over Apollo 11's planned landing area in the Sea of Tranquility.\nIt did not land"},
    {"100:58:25.9", 1, B_MOON, "位相合わせの噴射", "Phasing burn",
     "下降エンジンを 40 秒噴射し、高度 約 22〜352 km の軌道へ。司令船との位置関係を、月面から離陸した時と同じにする",
     "A 40-second descent engine burn put Snoopy into an orbit about 22-352 km high, setting up the same geometry\nas a liftoff from the surface"},
    {"102:45:16.9", 1, AP_LOOK_FWD, "下降段を切り離す", "Descent stage jettisoned",
     "下降段を切り離した。姿勢制御のスイッチの設定の誤りで、上昇段がしばらく激しく回った",
     "The descent stage was jettisoned. A switch setting error sent the ascent stage gyrating wildly for a few moments"},
    {"102:55:02.1", 1, B_MOON, "上昇段の軌道投入", "Ascent stage insertion",
     "上昇エンジンを 16 秒噴射し、高度 約 20〜86 km の軌道へ", "A 16-second ascent engine burn put the stage into an orbit about 20-86 km high"},
    {"105:22:55.6", 1, B_MOON, "司令船へ向かう", "Terminal phase initiation",
     "司令船へまっすぐ近づいていく噴射", "The burn that set Snoopy on its final approach to the command module"},
    {"106:22:02", 0, B_MOON, "ドッキング", "Docking",
     "スヌーピーの上昇段がチャーリー・ブラウンとドッキングし、2 人が司令船に戻った",
     "Snoopy's ascent stage docked with Charlie Brown and the two astronauts returned to the command module"},
    {"108:52:05.5", 1, AP_LOOK_FWD, "上昇段を太陽を回る軌道へ", "Ascent stage sent into solar orbit",
     "切り離した上昇段のエンジンを燃料が尽きるまで噴射し、月を離れて太陽を回る軌道へ送った（今も太陽を回っている）",
     "The jettisoned ascent stage fired its engine to depletion, leaving the Moon for an orbit around the Sun (where it still is)"},
    {"137:36:28.9", 0, B_EARTH, "地球へ向かう噴射", "Transearth injection",
     "月の裏側で主エンジンを 2 分 45 秒噴射し、秒速 1.12 km 速めて地球へ向かう軌道に",
     "Behind the Moon the main engine fired for 2 min 45 s, gaining 1.12 km/s, onto a course for Earth"},
    {"188:49:58", 0, B_EARTH, "軌道の修正", "Midcourse correction",
     "7 秒の噴射で、秒速 0.7 m だけ速さを変えた", "A 7-second burn changed the speed by 0.7 m/s"},
    {"191:33:26", 0, B_EARTH, "機械船を切り離す", "Service module jettisoned",
     "司令船だけで大気に入るため、機械船を切り離した", "The service module was jettisoned so the command module could enter the atmosphere alone"},
    {"191:48:54.5", 0, AP_LOOK_FWD, "大気圏に突入", "Atmospheric entry",
     "高度 約 122 km、秒速 11.1 km で大気に突入。最大で 6.8 G がかかった", "Entering the atmosphere at about 122 km altitude and 11.1 km/s, peaking at 6.8 g"},
    {"191:57:18.0", 0, AP_LOOK_FWD, "パラシュートを開く", "Parachutes",
     "減速用のドローグ・パラシュートを開き、続けて 3 つの主パラシュートを開いた", "The drogue parachutes opened, followed by the three main parachutes"},
    {"192:03:23", 0, AP_LOOK_FWD, "着水", "Splashdown",
     "サモアの東の太平洋（南緯 15.07°・西経 164.65°）に着水し、強襲揚陸艦プリンストンが回収した。打ち上げから 8 日 0 時間 3 分の飛行",
     "Splashdown in the Pacific east of Samoa (15.07° S, 164.65° W); the USS Princeton recovered the crew.\nThe flight lasted 8 days 0 hours 3 minutes"},
};
static const ApolloShape APOLLO10_SHAPES[] = {
    {"-3:00:00", 0, PM_SATURN_V}, {"0:02:42.31", 0, PM_SATURN_SII_RING}, {"0:03:12.31", 0, PM_SATURN_SII_LES}, {"0:03:17.8", 0, PM_SATURN_SII},
    {"0:09:13.50", 0, PM_SATURN_SIVB}, {"3:02:42.4", 0, PM_APOLLO_CSM}, {"3:17:03", 0, PM_APOLLO_CSM_LM}, {"97:45:03", 0, PM_APOLLO_CSM_LMG}, {"98:11:57", 0, PM_APOLLO_CSM},
    {"106:22:02", 0, PM_APOLLO_CSM_LMA}, {"108:24:36", 0, PM_APOLLO_CSM}, {"191:33:26", 0, PM_APOLLO_CM},
    {"191:57:18.0", 0, PM_CM_DROGUE}, {"191:58:05", 0, PM_CM_MAIN}, {"192:03:23", 0, PM_APOLLO_CM},
    {"98:11:57", 1, PM_APOLLO_LM}, {"102:45:16.9", 1, PM_LM_ASCENT}, {"106:22:02", 1, -1}, {"108:24:36", 1, PM_LM_ASCENT},
};

static const ApolloEvent APOLLO12_EVENTS[] = {
    {"0:00:00.68", 0, AP_LOOK_FWD, "打ち上げ", "Liftoff",
     "雨雲の下、ケネディ宇宙センター 39A 発射台から打ち上げ。乗員はチャールズ・コンラッド船長、リチャード・ゴードン司令船操縦士、\nアラン・ビーン月着陸船操縦士",
     "Liftoff under rain clouds from Launch Complex 39A. Crew: Commander Charles Conrad, Command Module Pilot Richard Gordon\nand Lunar Module Pilot Alan Bean"},
    {"0:00:36.5", 0, AP_LOOK_FWD, "落雷", "Lightning strikes",
     "打ち上げの 36.5 秒後と 52 秒後に、ロケットが雷を 2 度受けた。宇宙船の電源が一時切れて計器が乱れたが、乗員が回復させて飛行を続けた",
     "36.5 and 52 seconds after liftoff the rocket was struck by lightning twice. The spacecraft lost power and its instruments\nscrambled, but the crew restored them and flew on"},
    {"0:02:42.4", 0, AP_LOOK_FWD, "第1段を切り離す", "First stage separation",
     "高度 約 69 km、秒速 2.8 km で第1段 S-IC を切り離し、第2段 S-II に点火", "At about 69 km altitude and 2.8 km/s the S-IC separated and the S-II ignited"},
    {"0:09:13.20", 0, AP_LOOK_FWD, "第2段を切り離す", "Second stage separation",
     "高度 約 190 km、秒速 7.0 km で第2段を切り離し、第3段 S-IVB に点火", "At about 190 km altitude and 7.0 km/s the S-II separated and the S-IVB ignited"},
    {"0:11:43.91", 0, B_EARTH, "地球を回る軌道に入る", "Earth orbit insertion",
     "高度 約 181〜185 km、1 周 88 分の軌道（赤道に対する傾き 32.5°）", "An orbit about 181-185 km high, 88 minutes per revolution, inclined 32.5°"},
    {"2:47:22.8", 0, B_MOON, "月へ向かう噴射（TLI）", "Translunar injection",
     "第3段を 5 分 41 秒噴射し、秒速 10.8 km で月へ向かう軌道に", "The S-IVB fired for 5 min 41 s, reaching 10.8 km/s on a course to the Moon"},
    {"3:18:04.9", 0, AP_LOOK_FWD, "第3段から離れる", "Separation from the S-IVB",
     "司令・機械船ヤンキー・クリッパーが第3段から離れて向きを変え、月着陸船イントレピッドとドッキングして引き出した",
     "The command and service module Yankee Clipper separated, turned around, docked with the lunar module Intrepid and pulled it free"},
    {"30:52:44.4", 0, B_MOON, "自由帰還軌道を離れる", "Leaving the free-return trajectory",
     "9 秒の噴射（秒速 19 m）で、何もしなくても地球へ戻る軌道を離れ、着陸地点に合う軌道に乗り換えた（初めての試み）",
     "A 9-second burn (19 m/s) left the free-return trajectory for one suited to the landing site, the first time this was done"},
    {"83:25:23.4", 0, B_MOON, "月を回る軌道に入る", "Lunar orbit insertion",
     "月の裏側で主エンジンを噴射し、月を回る軌道に入った", "Behind the Moon the main engine fired to enter lunar orbit"},
    {"87:48:48.1", 0, B_MOON, "軌道を円に近づける", "Orbit circularization",
     "17 秒の噴射で、円に近い軌道に", "A 17-second burn made the orbit nearly circular"},
    {"107:54:02.3", 1, B_MOON, "イントレピッドが切り離される", "Intrepid undocks",
     "コンラッドとビーンの乗った月着陸船イントレピッドが、ゴードンの残る司令船から離れた",
     "The lunar module Intrepid, with Conrad and Bean aboard, undocked from the command module, where Gordon stayed"},
    {"109:23:39.9", 1, B_MOON, "降下軌道へ", "Descent orbit insertion",
     "下降エンジンを 29 秒噴射し、最も低い所が高度 約 15 km の軌道へ", "The descent engine fired for 29 s, dropping the low point of the orbit to about 15 km"},
    {"110:20:38.1", 1, AP_LOOK_FWD, "動力降下の開始", "Powered descent",
     "下降エンジンを噴射し続けて月面へ降りる", "The descent engine fired continuously down to the surface"},
    {"110:32:36.2", 1, B_EARTH, "月面に着陸", "Landed on the Moon",
     "嵐の大洋（南緯 3.012°・西経 23.422°）に着陸。2 年 7 か月前に着陸した無人探査機サーベイヤー3号から約 160 m の所に降りる、\n初めてのピンポイント着陸",
     "Touchdown in the Ocean of Storms (3.012° S, 23.422° W), about 160 m from the robotic Surveyor 3 that had landed\n2 years 7 months earlier, the first pinpoint landing"},
    {"115:22:22", 1, B_EARTH, "月面に立つ", "On the surface",
     "コンラッドが月面に降り立ち、ビーンも続いた。1 回目の月面活動で観測装置を置いた",
     "Conrad stepped onto the Moon, followed by Bean. On the first moonwalk they set up the experiment package"},
    {"119:47:13.2", 0, B_MOON, "司令船の軌道面の変更", "CSM plane change",
     "月着陸船の離陸に合わせて、司令船が軌道面を変えた", "The command module changed its orbital plane to match the lunar module's liftoff"},
    {"131:32:45", 1, B_EARTH, "サーベイヤー3号へ", "To Surveyor 3",
     "2 回目の月面活動で、サーベイヤー3号まで歩いて調べ、カメラなどの部品を外して持ち帰った",
     "On the second moonwalk they walked to Surveyor 3 and removed its camera and other parts to bring home"},
    {"142:03:47.78", 1, AP_LOOK_FWD, "月面を離れる", "Lunar liftoff",
     "31 時間 31 分の月面滞在を終え、上昇段が飛び立った", "After 31 h 31 min on the Moon the ascent stage lifted off"},
    {"143:01:51", 1, B_MOON, "ランデブーの開始", "Rendezvous begins",
     "司令船の軌道に追いつくための軌道の調整を始めた", "The first of the maneuvers to catch up with the command module"},
    {"144:36:26", 1, B_MOON, "司令船へ向かう", "Terminal phase initiation",
     "司令船へまっすぐ近づいていく噴射", "The burn that set the ascent stage on its final approach"},
    {"145:36:20.2", 0, B_MOON, "ドッキング", "Docking",
     "イントレピッドの上昇段がヤンキー・クリッパーとドッキングした", "Intrepid's ascent stage docked with Yankee Clipper"},
    {"149:55:16.4", 1, B_MOON, "上昇段を月面に落とす", "Ascent stage impact",
     "切り離した上昇段を月面（南緯 3.94°・西経 21.20°）に落とした。置いてきた地震計は、揺れが 50 分以上続くのを記録した",
     "The jettisoned ascent stage was crashed onto the Moon (3.94° S, 21.20° W). The seismometer left behind recorded shaking\nthat went on for more than 50 minutes"},
    {"159:04:45.5", 0, B_MOON, "写真撮影のための軌道面の変更", "Plane change for photography",
     "この後の着陸候補地を撮影するため、軌道面を変えた", "The orbital plane was changed to photograph candidate landing sites for later missions"},
    {"172:27:16.8", 0, B_EARTH, "地球へ向かう噴射", "Transearth injection",
     "月の裏側で主エンジンを 2 分 10 秒噴射し、地球へ向かう軌道に", "Behind the Moon the main engine fired for 2 min 10 s onto a course for Earth"},
    {"188:27:15.8", 0, B_EARTH, "軌道の修正", "Midcourse correction", "4 秒の小さな修正", "A 4-second correction"},
    {"241:21:59.7", 0, B_EARTH, "軌道の修正", "Midcourse correction", "6 秒の小さな修正", "A 6-second correction"},
    {"244:07:20.1", 0, B_EARTH, "機械船を切り離す", "Service module jettisoned",
     "司令船だけで大気に入るため、機械船を切り離した", "The service module was jettisoned so the command module could enter the atmosphere alone"},
    {"244:22:19.1", 0, AP_LOOK_FWD, "大気圏に突入", "Atmospheric entry",
     "高度 約 122 km、秒速 11.0 km で大気に突入。最大で 6.6 G がかかった", "Entering the atmosphere at about 122 km altitude and 11.0 km/s, peaking at 6.6 g"},
    {"244:30:39.7", 0, AP_LOOK_FWD, "パラシュートを開く", "Parachutes",
     "減速用のドローグ・パラシュートを開き、続けて 3 つの主パラシュートを開いた", "The drogue parachutes opened, followed by the three main parachutes"},
    {"244:36:25", 0, AP_LOOK_FWD, "着水", "Splashdown",
     "サモアの東の太平洋（南緯 15.78°・西経 165.15°）に着水し、空母ホーネットが回収した。打ち上げから 10 日 4 時間 36 分の飛行",
     "Splashdown in the Pacific east of Samoa (15.78° S, 165.15° W); the carrier USS Hornet recovered the crew.\nThe flight lasted 10 days 4 hours 36 minutes"},
};
static const ApolloShape APOLLO12_SHAPES[] = {
    {"-3:00:00", 0, PM_SATURN_V}, {"0:02:42.4", 0, PM_SATURN_SII_RING}, {"0:03:12.40", 0, PM_SATURN_SII_LES}, {"0:03:17.9", 0, PM_SATURN_SII},
    {"0:09:13.20", 0, PM_SATURN_SIVB}, {"3:18:04.9", 0, PM_APOLLO_CSM}, {"3:26:53.3", 0, PM_APOLLO_CSM_LM},
    {"104:47:54", 0, PM_APOLLO_CSM_LMG}, {"107:54:02.3", 0, PM_APOLLO_CSM}, {"145:36:20.2", 0, PM_APOLLO_CSM_LMA}, {"148:04:30.9", 0, PM_APOLLO_CSM},
    {"244:07:20.1", 0, PM_APOLLO_CM}, {"244:30:39.7", 0, PM_CM_DROGUE}, {"244:31:30.2", 0, PM_CM_MAIN}, {"244:36:25", 0, PM_APOLLO_CM},
    {"107:54:02.3", 1, PM_APOLLO_LM}, {"142:03:47.78", 1, PM_LM_ASCENT}, {"145:36:20.2", 1, -1}, {"148:04:30.9", 1, PM_LM_ASCENT},
};

static const ApolloEvent APOLLO13_EVENTS[] = {
    {"0:00:00.61", 0, AP_LOOK_FWD, "打ち上げ", "Liftoff",
     "ケネディ宇宙センター 39A 発射台から打ち上げ。乗員はジェームズ・ラベル船長、ジョン・スワイガート司令船操縦士、\nフレッド・ヘイズ月着陸船操縦士（スワイガートは、風疹にかかるおそれのあったケン・マッティングリーの代わりに 3 日前に決まった）",
     "Liftoff from Launch Complex 39A. Crew: Commander James Lovell, Command Module Pilot John Swigert and Lunar Module Pilot\nFred Haise (Swigert replaced Ken Mattingly, exposed to German measles, three days before launch)"},
    {"0:02:44.3", 0, AP_LOOK_FWD, "第1段を切り離す", "First stage separation",
     "高度 約 68 km、秒速 2.8 km で第1段 S-IC を切り離し、第2段 S-II に点火", "At about 68 km altitude and 2.8 km/s the S-IC separated and the S-II ignited"},
    {"0:05:30.64", 0, AP_LOOK_FWD, "第2段の中央エンジンが止まる", "S-II center engine shutdown",
     "第2段の 5 基のエンジンのうち中央の 1 基が、激しい振動のため予定より 2 分 12 秒早く止まった。残りのエンジンと第3段を長く噴射して補った",
     "Severe vibration shut down the S-II's center engine 2 min 12 s early. The other engines and the third stage burned longer to make up for it"},
    {"0:09:53.50", 0, AP_LOOK_FWD, "第2段を切り離す", "Second stage separation",
     "高度 約 189 km、秒速 6.9 km で第2段を切り離し、第3段 S-IVB に点火", "At about 189 km altitude and 6.9 km/s the S-II separated and the S-IVB ignited"},
    {"0:12:39.83", 0, B_EARTH, "地球を回る軌道に入る", "Earth orbit insertion",
     "高度 約 181〜186 km の軌道", "An orbit about 181-186 km high"},
    {"2:35:46.4", 0, B_MOON, "月へ向かう噴射（TLI）", "Translunar injection",
     "第3段を 5 分 51 秒噴射し、秒速 10.8 km で月へ向かう軌道に", "The S-IVB fired for 5 min 51 s, reaching 10.8 km/s on a course to the Moon"},
    {"3:06:38.9", 0, AP_LOOK_FWD, "第3段から離れる", "Separation from the S-IVB",
     "司令・機械船オデッセイが第3段から離れて向きを変え、月着陸船アクエリアスとドッキングして引き出した",
     "The command and service module Odyssey separated, turned around, docked with the lunar module Aquarius and pulled it free"},
    {"30:40:49.6", 0, B_MOON, "自由帰還軌道を離れる", "Leaving the free-return trajectory",
     "3.5 秒の噴射（秒速 7 m）で、着陸地点に合う軌道に乗り換えた", "A 3.5-second burn (7 m/s) changed to a trajectory suited to the landing site"},
    {"55:54:53", 0, B_MOON, "酸素タンクの爆発", "Oxygen tank explosion",
     "機械船の酸素タンク 2 号が破裂し、酸素と電力のほとんどを失った。「ヒューストン、問題が起きた」。着陸を中止し、\n月着陸船を救命ボートにして地球へ戻ることになった（地球から約 32 万 km の所）",
     "Oxygen tank 2 in the service module ruptured, taking most of the oxygen and power with it: \"Houston, we've had a problem.\"\nThe landing was called off and the lunar module became a lifeboat for the trip home (about 320,000 km from Earth)"},
    {"61:29:43.5", 0, B_MOON, "自由帰還軌道に戻る", "Back to free return",
     "月着陸船の下降エンジンを 34 秒噴射し、月を回って自然に地球へ戻る軌道に戻した", "The lunar module's descent engine fired for 34 s to put the spacecraft back on a free-return course around the Moon"},
    {"77:08:35", 0, B_MOON, "月の裏側へ", "Behind the Moon",
     "月の裏側の上空 約 250 km を回った。地球から約 40 万 km 離れ、人類が地球から最も遠くへ行った記録になった",
     "The spacecraft swung about 250 km above the far side of the Moon. About 400,000 km from Earth, the crew set the record\nfor the farthest humans have travelled"},
    {"77:56:39.7", 0, B_MOON, "第3段が月面に衝突", "S-IVB impact",
     "月の地震計で調べるため、切り離した第3段を月面に衝突させた（12 号の置いた地震計が揺れを記録した）",
     "The spent S-IVB was crashed into the Moon so that the seismometer left by Apollo 12 could record the impact"},
    {"79:27:39.0", 0, B_EARTH, "帰りを早める噴射（PC+2）", "PC+2 burn",
     "月の裏を回った 2 時間後、下降エンジンを 4 分 24 秒噴射して秒速 262 m 速め、帰りを約 9 時間早めた。着水地点もインド洋から太平洋へ",
     "Two hours after passing the Moon the descent engine fired for 4 min 24 s, gaining 262 m/s, cutting about 9 hours off the trip\nand moving the splashdown from the Indian Ocean to the Pacific"},
    {"105:18:28.0", 0, B_EARTH, "軌道の修正", "Midcourse correction",
     "下降エンジンを 14 秒噴射。誘導の計器を使えないので、地球の明暗の境目を窓の印に合わせて向きを保った",
     "A 14-second descent engine burn, steered by keeping Earth's terminator lined up in the window because the guidance\nplatform was powered down"},
    {"137:39:51.5", 0, B_EARTH, "最後の軌道の修正", "Final midcourse correction",
     "月着陸船の姿勢制御の噴射口で、22 秒の小さな修正", "A 22-second correction with the lunar module's thrusters"},
    {"138:01:48.0", 0, B_EARTH, "機械船を切り離す", "Service module jettisoned",
     "機械船を切り離し、乗員は初めて損傷を見た。側面のパネルが 1 枚まるごとなくなっていた",
     "The service module was jettisoned and the crew saw the damage for the first time: an entire side panel was missing"},
    {"141:30:00.2", 0, B_EARTH, "月着陸船を切り離す", "Lunar module jettisoned",
     "4 日間救命ボートになった月着陸船アクエリアスを切り離した。アクエリアスは大気に入って燃え尽きた",
     "Aquarius, the lifeboat for four days, was jettisoned and burned up in the atmosphere"},
    {"142:40:45.7", 0, AP_LOOK_FWD, "大気圏に突入", "Atmospheric entry",
     "高度 約 122 km、秒速 11.0 km で大気に突入", "Entering the atmosphere at about 122 km altitude and 11.0 km/s"},
    {"142:54:41", 0, AP_LOOK_FWD, "着水", "Splashdown",
     "サモアの南東の太平洋（南緯 21.63°・西経 165.37°）に着水し、強襲揚陸艦イオー・ジマが回収した。打ち上げから 5 日 22 時間 55 分の飛行",
     "Splashdown in the Pacific southeast of Samoa (21.63° S, 165.37° W); the USS Iwo Jima recovered the crew.\nThe flight lasted 5 days 22 hours 55 minutes"},
};
static const ApolloShape APOLLO13_SHAPES[] = {
    {"-3:00:00", 0, PM_SATURN_V}, {"0:02:44.3", 0, PM_SATURN_SII_RING}, {"0:03:14.30", 0, PM_SATURN_SII_LES}, {"0:03:21", 0, PM_SATURN_SII},
    {"0:09:53.50", 0, PM_SATURN_SIVB}, {"3:06:38.9", 0, PM_APOLLO_CSM}, {"3:19:08.8", 0, PM_APOLLO_CSM_LM},
    {"138:01:48.0", 0, PM_APOLLO_CM_LM}, {"141:30:00.2", 0, PM_APOLLO_CM},
    {"142:48:41", 0, PM_CM_DROGUE}, {"142:49:31", 0, PM_CM_MAIN}, {"142:54:41", 0, PM_APOLLO_CM},
    {"141:30:00.2", 1, PM_LM_STOWED},
};

static const ApolloEvent APOLLO14_EVENTS[] = {
    {"0:00:00.57", 0, AP_LOOK_FWD, "打ち上げ", "Liftoff",
     "雨雲のため 40 分遅れて、ケネディ宇宙センター 39A 発射台から打ち上げ。乗員はアラン・シェパード船長、スチュアート・ルーサ司令船操縦士、\nエドガー・ミッチェル月着陸船操縦士。シェパードは 1961 年にアメリカ人で初めて宇宙へ行った飛行士",
     "Liftoff from Launch Complex 39A after a 40-minute weather hold. Crew: Commander Alan Shepard, Command Module Pilot Stuart Roosa\nand Lunar Module Pilot Edgar Mitchell. Shepard had been the first American in space, in 1961"},
    {"0:02:44.8", 0, AP_LOOK_FWD, "第1段を切り離す", "First stage separation",
     "高度 約 68 km、秒速 2.7 km で第1段 S-IC を切り離し、第2段 S-II に点火", "At about 68 km altitude and 2.7 km/s the S-IC separated and the S-II ignited"},
    {"0:09:20.00", 0, AP_LOOK_FWD, "第2段を切り離す", "Second stage separation",
     "高度 約 188 km、秒速 7.0 km で第2段を切り離し、第3段 S-IVB に点火", "At about 188 km altitude and 7.0 km/s the S-II separated and the S-IVB ignited"},
    {"0:11:50.56", 0, B_EARTH, "地球を回る軌道に入る", "Earth orbit insertion",
     "高度 約 183〜185 km、1 周 88 分の軌道", "An orbit about 183-185 km high, 88 minutes per revolution"},
    {"2:28:32.4", 0, B_MOON, "月へ向かう噴射（TLI）", "Translunar injection",
     "第3段を 5 分 51 秒噴射し、秒速 10.8 km で月へ向かう軌道に", "The S-IVB fired for 5 min 51 s, reaching 10.8 km/s on a course to the Moon"},
    {"3:02:29.4", 0, AP_LOOK_FWD, "ドッキングに手間取る", "Docking trouble",
     "司令・機械船キティホークが第3段から離れて向きを変えたが、月着陸船アンタレスとのドッキングの留め金がかからず、\n6 回目でやっとドッキングできた",
     "The command and service module Kitty Hawk separated and turned around, but its docking latches would not catch on\nthe lunar module Antares; it finally docked on the sixth attempt"},
    {"30:36:07.9", 0, B_MOON, "自由帰還軌道を離れる", "Leaving the free-return trajectory",
     "10 秒の噴射で、着陸地点に合う軌道に乗り換えた", "A 10-second burn changed to a trajectory suited to the landing site"},
    {"81:56:40.7", 0, B_MOON, "月を回る軌道に入る", "Lunar orbit insertion",
     "月の裏側で主エンジンを 6 分 11 秒噴射して秒速 921 m 減速し、高度 約 108〜313 km の月を回る軌道に入った",
     "Behind the Moon the main engine fired for 6 min 11 s, slowing by 921 m/s into a lunar orbit about 108-313 km high"},
    {"86:10:53.0", 0, B_MOON, "つながったまま降下軌道へ", "Descent orbit insertion, docked",
     "司令船の主エンジンで、月着陸船とつながったまま最も低い所が高度 約 17 km の軌道に下げた（月着陸船の燃料を着陸のために残す初めての方法）",
     "The command module's engine lowered the docked spacecraft to an orbit with a low point of about 17 km, saving the lunar\nmodule's fuel for landing (done this way for the first time)"},
    {"103:47:41.6", 1, B_MOON, "アンタレスが離れる", "Antares undocks",
     "シェパードとミッチェルの乗った月着陸船アンタレスが、ルーサの残る司令船から離れた",
     "The lunar module Antares, with Shepard and Mitchell aboard, undocked from the command module, where Roosa stayed"},
    {"108:02:26.5", 1, AP_LOOK_FWD, "動力降下の開始", "Powered descent",
     "直前に、誤って出ていた中止の信号を無視するよう誘導コンピューターの手順を書き換えて、降下を始めた。12 分 45 秒かけて月面へ",
     "Just before descent the guidance computer's procedures were changed to ignore a false abort signal. The descent took 12 min 45 s"},
    {"108:15:09.3", 1, B_EARTH, "月面に着陸", "Landed on the Moon",
     "フラ・マウロ高地（南緯 3.645°・西経 17.471°）に着陸。13 号が目指していた地域", "Touchdown in the Fra Mauro highlands (3.645° S, 17.471° W), the area Apollo 13 had been bound for"},
    {"113:39:11", 1, B_EARTH, "月面での活動", "Moonwalks",
     "2 回の月面活動で観測装置を置き、手押しの運搬車を使って岩石を集めた。2 回目の終わりに、シェパードがゴルフボールを打った",
     "On two moonwalks they set up experiments and gathered rocks with a hand-pulled cart. At the end of the second, Shepard hit\ngolf balls"},
    {"117:29:33.1", 0, B_MOON, "司令船の軌道面の変更", "CSM plane change",
     "月着陸船の離陸に合わせて、司令船が軌道面を変えた", "The command module changed its orbital plane to match the lunar module's liftoff"},
    {"141:45:40", 1, AP_LOOK_FWD, "月面を離れる", "Lunar liftoff",
     "33 時間 30 分の月面滞在を終え、上昇段が飛び立った", "After 33 h 30 min on the Moon the ascent stage lifted off"},
    {"141:52:52.1", 1, B_MOON, "月を回る軌道へ", "Into lunar orbit",
     "7 分 12 秒の噴射で、高度 約 16〜96 km の軌道に", "A 7 min 12 s burn put the ascent stage into an orbit about 16-96 km high"},
    {"142:30:51.1", 1, B_MOON, "司令船へ向かう", "Terminal phase initiation",
     "1 周目で司令船に追いつく、初めての直接ランデブー", "The first direct rendezvous, catching up with the command module on the first orbit"},
    {"143:32:50.5", 0, B_MOON, "ドッキング", "Docking",
     "離陸から 1 時間 47 分で、アンタレスの上昇段がキティホークとドッキングした", "Antares's ascent stage docked with Kitty Hawk 1 h 47 min after liftoff"},
    {"147:14:16.9", 1, B_MOON, "上昇段を月面に落とす", "Ascent stage deorbit",
     "切り離した上昇段を、月の地震計で調べるため月面に落とした", "The jettisoned ascent stage was crashed into the Moon for the seismometers"},
    {"148:36:02.3", 0, B_EARTH, "地球へ向かう噴射", "Transearth injection",
     "月の裏側で主エンジンを 2 分 29 秒噴射し、秒速 1.05 km 速めて地球へ向かう軌道に",
     "Behind the Moon the main engine fired for 2 min 29 s, gaining 1.05 km/s, onto a course for Earth"},
    {"165:34:56.7", 0, B_EARTH, "軌道の修正", "Midcourse correction", "3 秒の小さな修正", "A 3-second correction"},
    {"215:32:42.2", 0, B_EARTH, "機械船を切り離す", "Service module jettisoned",
     "司令船だけで大気に入るため、機械船を切り離した", "The service module was jettisoned so the command module could enter the atmosphere alone"},
    {"215:47:45.3", 0, AP_LOOK_FWD, "大気圏に突入", "Atmospheric entry",
     "高度 約 122 km、秒速 11.0 km で大気に突入", "Entering the atmosphere at about 122 km altitude and 11.0 km/s"},
    {"216:01:58.1", 0, AP_LOOK_FWD, "着水", "Splashdown",
     "サモアの南の太平洋（南緯 27.02°・西経 172.67°）に着水し、強襲揚陸艦ニューオーリンズが回収した。打ち上げから 9 日 0 時間 2 分の飛行",
     "Splashdown in the Pacific south of Samoa (27.02° S, 172.67° W); the USS New Orleans recovered the crew.\nThe flight lasted 9 days 0 hours 2 minutes"},
};
static const ApolloShape APOLLO14_SHAPES[] = {
    {"-3:00:00", 0, PM_SATURN_V}, {"0:02:44.8", 0, PM_SATURN_SII_RING}, {"0:03:14.80", 0, PM_SATURN_SII_LES}, {"0:03:20.7", 0, PM_SATURN_SII},
    {"0:09:20.00", 0, PM_SATURN_SIVB}, {"3:02:29.4", 0, PM_APOLLO_CSM}, {"4:56:56.7", 0, PM_APOLLO_CSM_LM},
    {"102:59:12", 0, PM_APOLLO_CSM_LMG}, {"103:47:41.6", 0, PM_APOLLO_CSM}, {"143:32:50.5", 0, PM_APOLLO_CSM_LMA}, {"145:44:58.0", 0, PM_APOLLO_CSM},
    {"215:32:42.2", 0, PM_APOLLO_CM}, {"215:55:58.1", 0, PM_CM_DROGUE}, {"215:56:48.1", 0, PM_CM_MAIN}, {"216:01:58.1", 0, PM_APOLLO_CM},
    {"103:47:41.6", 1, PM_APOLLO_LM}, {"141:45:40", 1, PM_LM_ASCENT}, {"143:32:50.5", 1, -1}, {"145:44:58.0", 1, PM_LM_ASCENT},
};

static const ApolloEvent APOLLO15_EVENTS[] = {
    {"0:00:00.58", 0, AP_LOOK_FWD, "打ち上げ", "Liftoff",
     "ケネディ宇宙センター 39A 発射台から打ち上げ。乗員はデビッド・スコット船長、アルフレッド・ウォーデン司令船操縦士、\nジェームズ・アーウィン月着陸船操縦士。月面に長くとどまり、月面車で遠くまで調べる初めての飛行",
     "Liftoff from Launch Complex 39A. Crew: Commander David Scott, Command Module Pilot Alfred Worden and Lunar Module Pilot\nJames Irwin, on the first mission with a longer stay and a lunar rover"},
    {"0:02:41.2", 0, AP_LOOK_FWD, "第1段を切り離す", "First stage separation",
     "高度 約 70 km、秒速 2.8 km で第1段 S-IC を切り離し、第2段 S-II に点火", "At about 70 km altitude and 2.8 km/s the S-IC separated and the S-II ignited"},
    {"0:09:10.1", 0, AP_LOOK_FWD, "第2段を切り離す", "Second stage separation",
     "高度 約 176 km、秒速 7.0 km で第2段を切り離し、第3段 S-IVB に点火", "At about 176 km altitude and 7.0 km/s the S-II separated and the S-IVB ignited"},
    {"0:11:44.67", 0, B_EARTH, "地球を回る軌道に入る", "Earth orbit insertion",
     "高度 約 166〜170 km、1 周 88 分の軌道（赤道に対する傾き 29.7°）", "An orbit about 166-170 km high, 88 minutes per revolution, inclined 29.7°"},
    {"2:50:02.9", 0, B_MOON, "月へ向かう噴射（TLI）", "Translunar injection",
     "第3段を 5 分 51 秒噴射し、秒速 10.8 km で月へ向かう軌道に", "The S-IVB fired for 5 min 51 s, reaching 10.8 km/s on a course to the Moon"},
    {"3:22:27.2", 0, AP_LOOK_FWD, "第3段から離れる", "Separation from the S-IVB",
     "司令・機械船エンデバーが第3段から離れて向きを変え、月着陸船ファルコンとドッキングして引き出した",
     "The command and service module Endeavour separated, turned around, docked with the lunar module Falcon and pulled it free"},
    {"74:06:47.1", 0, B_MOON, "科学機器室の扉を捨てる", "SIM bay door jettisoned",
     "月を回りながら観測するため、機械船の科学機器室の扉を捨てた", "The door of the service module's scientific instrument bay was jettisoned so the instruments could observe from orbit"},
    {"78:31:46.7", 0, B_MOON, "月を回る軌道に入る", "Lunar orbit insertion",
     "月の裏側で主エンジンを 6 分 38 秒噴射して秒速 914 m 減速し、高度 約 107〜315 km の月を回る軌道に入った",
     "Behind the Moon the main engine fired for 6 min 38 s, slowing by 914 m/s into a lunar orbit about 107-315 km high"},
    {"82:39:49.1", 0, B_MOON, "つながったまま降下軌道へ", "Descent orbit insertion, docked",
     "司令船の主エンジンで、月着陸船とつながったまま最も低い所が高度 約 18 km の軌道に下げた",
     "The command module's engine lowered the docked spacecraft to an orbit with a low point of about 18 km"},
    {"100:39:16.2", 1, B_MOON, "ファルコンが離れる", "Falcon undocks",
     "スコットとアーウィンの乗った月着陸船ファルコンが、ウォーデンの残る司令船から離れた",
     "The lunar module Falcon, with Scott and Irwin aboard, undocked from the command module, where Worden stayed"},
    {"104:30:09.4", 1, AP_LOOK_FWD, "動力降下の開始", "Powered descent",
     "アペニン山脈を越えて、12 分 19 秒かけて急な角度で月面へ降りる", "Coming in over the Apennine Mountains, a steep 12 min 19 s descent to the surface"},
    {"104:42:29.3", 1, B_EARTH, "月面に着陸", "Landed on the Moon",
     "ハドリー谷のそば（北緯 26.132°・東経 3.634°）に着陸。高さ 4,000 m を超える山々に囲まれた谷", "Touchdown beside Hadley Rille (26.132° N, 3.634° E), in a valley ringed by mountains over 4,000 m high"},
    {"119:39:17", 1, B_EARTH, "月面車で走る", "Driving the rover",
     "初めて月面車（LRV）を使った。3 回の月面活動で合わせて約 28 km を走った", "The lunar rover was used for the first time; over three moonwalks it covered about 28 km"},
    {"142:14:48", 1, B_EARTH, "創世記の石", "The Genesis Rock",
     "2 回目の月面活動で、月の初めの頃にできた白い斜長岩を見つけた。のちに「創世記の石」と呼ばれた",
     "On the second moonwalk they found a white anorthosite from the Moon's early crust, later called the Genesis Rock"},
    {"163:18:14", 1, B_EARTH, "ハンマーと羽根", "Hammer and feather",
     "3 回目の月面活動の終わりに、スコットがハンマーと羽根を同時に落とし、空気のない所では同時に落ちることを見せた",
     "At the end of the third moonwalk Scott dropped a hammer and a feather together; with no air, they landed at the same time"},
    {"165:11:32.7", 0, B_MOON, "司令船の軌道面の変更", "CSM plane change",
     "月着陸船の離陸に合わせて、司令船が軌道面を変えた", "The command module changed its orbital plane to match the lunar module's liftoff"},
    {"171:37:23.2", 1, AP_LOOK_FWD, "月面を離れる", "Lunar liftoff",
     "66 時間 55 分の月面滞在を終え、上昇段が飛び立った。月面車に置いたテレビカメラが離陸を映した",
     "After 66 h 55 min on the Moon the ascent stage lifted off, televised by the camera left on the rover"},
    {"171:44:34.2", 1, B_MOON, "月を回る軌道へ", "Into lunar orbit",
     "7 分 11 秒の噴射で、高度 約 17〜79 km の軌道に", "A 7 min 11 s burn put the ascent stage into an orbit about 17-79 km high"},
    {"172:29:40.0", 1, B_MOON, "司令船へ向かう", "Terminal phase initiation",
     "1 周目で司令船に追いつく直接ランデブー", "A direct rendezvous on the first orbit"},
    {"173:36:25.5", 0, B_MOON, "ドッキング", "Docking",
     "ファルコンの上昇段がエンデバーとドッキングした", "Falcon's ascent stage docked with Endeavour"},
    {"181:04:19.8", 1, B_MOON, "上昇段を月面に落とす", "Ascent stage deorbit",
     "切り離した上昇段を、月の地震計で調べるため月面に落とした", "The jettisoned ascent stage was crashed into the Moon for the seismometers"},
    {"222:39:29.1", 0, B_MOON, "子衛星を放つ", "Subsatellite released",
     "月を回って磁場や粒子を測る小さな子衛星を、機械船から放った", "A small subsatellite to measure magnetic fields and particles in lunar orbit was released from the service module"},
    {"223:48:45.8", 0, B_EARTH, "地球へ向かう噴射", "Transearth injection",
     "月の裏側で主エンジンを 2 分 21 秒噴射し、地球へ向かう軌道に", "Behind the Moon the main engine fired for 2 min 21 s onto a course for Earth"},
    {"241:57:12", 0, B_EARTH, "深宇宙での船外活動", "Deep-space spacewalk",
     "ウォーデンが宇宙船の外に出て、科学機器室からフィルムを回収した。地球から約 32 万 km での、初めての深宇宙での船外活動",
     "Worden went outside to retrieve film from the instrument bay, the first spacewalk in deep space, about 320,000 km from Earth"},
    {"291:56:49.9", 0, B_EARTH, "軌道の修正", "Midcourse correction", "22 秒の修正", "A 22-second correction"},
    {"294:43:55.2", 0, B_EARTH, "機械船を切り離す", "Service module jettisoned",
     "司令船だけで大気に入るため、機械船を切り離した", "The service module was jettisoned so the command module could enter the atmosphere alone"},
    {"294:58:54.7", 0, AP_LOOK_FWD, "大気圏に突入", "Atmospheric entry",
     "高度 約 122 km、秒速 11.0 km で大気に突入。最大で 6.2 G がかかった", "Entering the atmosphere at about 122 km altitude and 11.0 km/s, peaking at 6.2 g"},
    {"295:11:53.0", 0, AP_LOOK_FWD, "着水", "Splashdown",
     "3 つの主パラシュートの 1 つが開かず、2 つで少し速く着水した（ハワイの北の太平洋、北緯 26.13°・西経 158.13°）。\n強襲揚陸艦オキナワが回収した。打ち上げから 12 日 7 時間 12 分の飛行",
     "One of the three main parachutes failed, so the capsule came down a little faster on two (Pacific north of Hawaii, 26.13° N,\n158.13° W); the USS Okinawa recovered the crew. The flight lasted 12 days 7 hours 12 minutes"},
};
// 打ち上げ脱出用のロケットの切り離しは記録が見当たらないので、S-IC 切り離しの約 36 秒後（11〜14 号と同じ間隔）
static const ApolloShape APOLLO15_SHAPES[] = {
    {"-3:00:00", 0, PM_SATURN_V}, {"0:02:41.2", 0, PM_SATURN_SII_RING}, {"0:03:11.20", 0, PM_SATURN_SII_LES}, {"0:03:17", 0, PM_SATURN_SII},
    {"0:09:10.1", 0, PM_SATURN_SIVB}, {"3:22:27.2", 0, PM_APOLLO_CSM}, {"3:33:49.5", 0, PM_APOLLO_CSM_LM},
    {"98:40:01", 0, PM_APOLLO_CSM_LMG}, {"100:39:16.2", 0, PM_APOLLO_CSM}, {"173:36:25.5", 0, PM_APOLLO_CSM_LMA}, {"179:30:01.4", 0, PM_APOLLO_CSM},
    {"294:43:55.2", 0, PM_APOLLO_CM}, {"295:05:53.0", 0, PM_CM_DROGUE}, {"295:06:43.0", 0, PM_CM_MAIN}, {"295:11:53.0", 0, PM_APOLLO_CM},
    {"100:39:16.2", 1, PM_APOLLO_LM}, {"171:37:23.2", 1, PM_LM_ASCENT}, {"173:36:25.5", 1, -1}, {"179:30:01.4", 1, PM_LM_ASCENT},
};

static const ApolloEvent APOLLO16_EVENTS[] = {
    {"0:00:00.59", 0, AP_LOOK_FWD, "打ち上げ", "Liftoff",
     "ケネディ宇宙センター 39A 発射台から打ち上げ。乗員はジョン・ヤング船長、トーマス・マッティングリー司令船操縦士、\nチャールズ・デューク月着陸船操縦士。初めて月の高地に降りる",
     "Liftoff from Launch Complex 39A. Crew: Commander John Young, Command Module Pilot Thomas Mattingly and Lunar Module Pilot\nCharles Duke, bound for the first landing in the lunar highlands"},
    {"0:02:43.5", 0, AP_LOOK_FWD, "第1段を切り離す", "First stage separation",
     "高度 約 68 km、秒速 2.7 km で第1段 S-IC を切り離し、第2段 S-II に点火", "At about 68 km altitude and 2.7 km/s the S-IC separated and the S-II ignited"},
    {"0:09:20.5", 0, AP_LOOK_FWD, "第2段を切り離す", "Second stage separation",
     "高度 約 173 km、秒速 7.0 km で第2段を切り離し、第3段 S-IVB に点火", "At about 173 km altitude and 7.0 km/s the S-II separated and the S-IVB ignited"},
    {"0:11:56.21", 0, B_EARTH, "地球を回る軌道に入る", "Earth orbit insertion",
     "高度 約 167〜169 km、1 周 88 分の軌道（赤道に対する傾き 32.5°）", "An orbit about 167-169 km high, 88 minutes per revolution, inclined 32.5°"},
    {"2:33:36.5", 0, B_MOON, "月へ向かう噴射（TLI）", "Translunar injection",
     "第3段を再び噴射し、秒速 10.8 km で月へ向かう軌道に", "The S-IVB fired again, reaching 10.8 km/s on a course to the Moon"},
    {"3:04:59.0", 0, AP_LOOK_FWD, "第3段から離れる", "Separation from the S-IVB",
     "司令・機械船キャスパーが第3段から離れて向きを変え、月着陸船オリオンとドッキングして引き出した",
     "The command and service module Casper separated, turned around, docked with the lunar module Orion and pulled it free"},
    {"30:39:01", 0, B_MOON, "軌道の修正", "Midcourse correction", "2 秒の噴射で、秒速 3.8 m だけ速さを変えた", "A 2-second burn changed the speed by 3.8 m/s"},
    {"69:59:01", 0, B_MOON, "科学機器室の扉を捨てる", "SIM bay door jettisoned",
     "月を回りながら観測するため、機械船の科学機器室の扉を捨てた", "The door of the service module's scientific instrument bay was jettisoned"},
    {"74:28:28", 0, B_MOON, "月を回る軌道に入る", "Lunar orbit insertion",
     "月の裏側で主エンジンを 6 分 15 秒噴射し、高度 約 108〜315 km の月を回る軌道に入った",
     "Behind the Moon the main engine fired for 6 min 15 s into a lunar orbit about 108-315 km high"},
    {"78:33:45", 0, B_MOON, "つながったまま降下軌道へ", "Descent orbit insertion, docked",
     "司令船の主エンジンで、月着陸船とつながったまま降下軌道に下げた", "The command module's engine lowered the docked spacecraft into the descent orbit"},
    {"96:13:31", 1, B_MOON, "オリオンが離れる", "Orion undocks",
     "ヤングとデュークの乗った月着陸船オリオンが、マッティングリーの残る司令船から離れた",
     "The lunar module Orion, with Young and Duke aboard, undocked from the command module, where Mattingly stayed"},
    {"103:21:43", 0, B_MOON, "司令船の主エンジンの不具合", "CSM engine problem",
     "司令船の主エンジンの首振り装置に不具合が見つかり、確かめるため着陸が約 6 時間遅れた。使えると判断され、円軌道への噴射を行った",
     "A fault in the command module's engine gimbal delayed the landing by about 6 hours while it was checked. It was judged usable\nand the circularization burn went ahead"},
    {"104:17:25", 1, AP_LOOK_FWD, "動力降下の開始", "Powered descent",
     "下降エンジンを噴射し続けて月面へ降りる", "The descent engine fired continuously down to the surface"},
    {"104:29:35", 1, B_EARTH, "月面に着陸", "Landed on the Moon",
     "デカルト高地（南緯 8.973°・東経 15.500°）に着陸", "Touchdown in the Descartes highlands (8.973° S, 15.500° E)"},
    {"165:31:28", 1, B_EARTH, "月面での活動", "Moonwalks",
     "3 回の月面活動は合わせて約 20 時間、月面車で約 27 km を走った。最後は北レイ・クレーターの縁まで行った",
     "Three moonwalks totalled about 20 hours and some 27 km of driving, the last reaching the rim of North Ray crater"},
    {"169:05:52", 0, B_MOON, "司令船の軌道面の変更", "CSM plane change",
     "月着陸船の離陸に合わせて、司令船が軌道面を変えた", "The command module changed its orbital plane to match the lunar module's liftoff"},
    {"175:31:47.9", 1, AP_LOOK_FWD, "月面を離れる", "Lunar liftoff",
     "71 時間 2 分の月面滞在を終え、上昇段が飛び立った", "After 71 h 2 min on the Moon the ascent stage lifted off"},
    {"175:38:56", 1, B_MOON, "月を回る軌道へ", "Into lunar orbit", "上昇段が月を回る軌道に入った", "The ascent stage reached lunar orbit"},
    {"176:26:05", 1, B_MOON, "司令船へ向かう", "Terminal phase initiation", "司令船へまっすぐ近づいていく噴射", "The burn that set the ascent stage on its final approach"},
    {"177:41:18", 0, B_MOON, "ドッキング", "Docking", "オリオンの上昇段がキャスパーとドッキングした", "Orion's ascent stage docked with Casper"},
    {"195:00:12", 0, B_MOON, "上昇段を切り離す", "Ascent stage jettisoned",
     "切り離した上昇段は姿勢を保てなくなり、月面に落とす噴射ができなかった。上昇段はしばらく月を回る軌道に残った",
     "After jettison the ascent stage lost attitude control and could not be deorbited; it stayed in lunar orbit for a time"},
    {"196:02:09", 0, B_MOON, "子衛星を放つ", "Subsatellite released",
     "機械船から子衛星を放った。予定より低い軌道に入り、約 1 か月後に月面に落ちた",
     "A subsatellite was released from the service module. It went into a lower orbit than planned and hit the Moon about a month later"},
    {"200:21:33", 0, B_EARTH, "地球へ向かう噴射", "Transearth injection",
     "月の裏側で主エンジンを 2 分 42 秒噴射し、地球へ向かう軌道に", "Behind the Moon the main engine fired for 2 min 42 s onto a course for Earth"},
    {"214:35:03", 0, B_EARTH, "軌道の修正", "Midcourse correction", "22 秒の小さな修正", "A 22-second correction"},
    {"218:39:46", 0, B_EARTH, "深宇宙での船外活動", "Deep-space spacewalk",
     "マッティングリーが宇宙船の外に出て、科学機器室からフィルムを回収した", "Mattingly went outside to retrieve film from the scientific instrument bay"},
    {"262:37:21", 0, B_EARTH, "軌道の修正", "Midcourse correction", "6 秒の小さな修正", "A 6-second correction"},
    {"265:22:23", 0, B_EARTH, "機械船を切り離す", "Service module jettisoned",
     "司令船だけで大気に入るため、機械船を切り離した", "The service module was jettisoned so the command module could enter the atmosphere alone"},
    {"265:37:31", 0, AP_LOOK_FWD, "大気圏に突入", "Atmospheric entry",
     "高度 約 122 km、秒速 11.0 km で大気に突入。最大で 7.2 G がかかった", "Entering the atmosphere at about 122 km altitude and 11.0 km/s, peaking at 7.2 g"},
    {"265:45:26", 0, AP_LOOK_FWD, "パラシュートを開く", "Parachutes",
     "減速用のドローグ・パラシュートを開き、続けて 3 つの主パラシュートを開いた", "The drogue parachutes opened, followed by the three main parachutes"},
    {"265:51:05", 0, AP_LOOK_FWD, "着水", "Splashdown",
     "クリスマス島の近くの太平洋（南緯 0.70°・西経 156.22°）に着水し、空母タイコンデロガが回収した。打ち上げから 11 日 1 時間 51 分の飛行",
     "Splashdown in the Pacific near Christmas Island (0.70° S, 156.22° W); the carrier USS Ticonderoga recovered the crew.\nThe flight lasted 11 days 1 hour 51 minutes"},
};
static const ApolloShape APOLLO16_SHAPES[] = {
    {"-3:00:00", 0, PM_SATURN_V}, {"0:02:43.5", 0, PM_SATURN_SII_RING}, {"0:03:13.50", 0, PM_SATURN_SII_LES}, {"0:03:19.8", 0, PM_SATURN_SII},
    {"0:09:20.5", 0, PM_SATURN_SIVB}, {"3:04:59.0", 0, PM_APOLLO_CSM}, {"3:15:23", 0, PM_APOLLO_CSM_LM},
    {"94:00:09", 0, PM_APOLLO_CSM_LMG}, {"96:13:31", 0, PM_APOLLO_CSM}, {"177:41:18", 0, PM_APOLLO_CSM_LMA}, {"195:00:12", 0, PM_APOLLO_CSM},
    {"265:22:23", 0, PM_APOLLO_CM}, {"265:45:26", 0, PM_CM_DROGUE}, {"265:46:16", 0, PM_CM_MAIN}, {"265:51:05", 0, PM_APOLLO_CM},
    {"96:13:31", 1, PM_APOLLO_LM}, {"175:31:47.9", 1, PM_LM_ASCENT}, {"177:41:18", 1, -1}, {"195:00:12", 1, PM_LM_ASCENT},
};

static const ApolloEvent APOLLO17_EVENTS[] = {
    {"0:00:00.63", 0, AP_LOOK_FWD, "夜の打ち上げ", "Night launch",
     "サターン V で初めての夜の打ち上げ（発射の手順の装置の不具合で 2 時間 40 分遅れた）。乗員はユージン・サーナン船長、\nロナルド・エバンス司令船操縦士、地質学者のハリソン・シュミット月着陸船操縦士",
     "The first night launch of a Saturn V, 2 h 40 min late after a launch sequencer fault. Crew: Commander Eugene Cernan,\nCommand Module Pilot Ronald Evans and Lunar Module Pilot Harrison Schmitt, a geologist"},
    {"0:02:42.9", 0, AP_LOOK_FWD, "第1段を切り離す", "First stage separation",
     "高度 約 68 km、秒速 2.8 km で第1段 S-IC を切り離し、第2段 S-II に点火", "At about 68 km altitude and 2.8 km/s the S-IC separated and the S-II ignited"},
    {"0:09:20.6", 0, AP_LOOK_FWD, "第2段を切り離す", "Second stage separation",
     "高度 約 173 km、秒速 7.0 km で第2段を切り離し、第3段 S-IVB に点火", "At about 173 km altitude and 7.0 km/s the S-II separated and the S-IVB ignited"},
    {"0:11:52.65", 0, B_EARTH, "地球を回る軌道に入る", "Earth orbit insertion", "地球を回る軌道に入った", "The spacecraft reached Earth orbit"},
    {"3:12:36.6", 0, B_MOON, "月へ向かう噴射（TLI）", "Translunar injection",
     "第3段を 5 分 51 秒噴射し、秒速 10.8 km で月へ向かう軌道に", "The S-IVB fired for 5 min 51 s, reaching 10.8 km/s on a course to the Moon"},
    {"3:42:27.6", 0, AP_LOOK_FWD, "第3段から離れる", "Separation from the S-IVB",
     "司令・機械船アメリカが第3段から離れて向きを変え、月着陸船チャレンジャーとドッキングして引き出した",
     "The command and service module America separated, turned around, docked with the lunar module Challenger and pulled it free"},
    {"5:06:00", 0, B_EARTH, "ブルー・マーブル", "The Blue Marble",
     "地球から約 2 万 9 千 km の所で、昼の側の地球全体が写った写真「ブルー・マーブル」を撮った", "About 29,000 km from Earth the crew photographed the whole sunlit Earth: the Blue Marble"},
    {"35:30:00", 0, B_MOON, "軌道の修正", "Midcourse correction", "2 秒の小さな修正", "A 2-second correction"},
    {"81:32:40", 0, B_MOON, "科学機器室の扉を捨てる", "SIM bay door jettisoned",
     "月を回りながら観測するため、機械船の科学機器室の扉を捨てた", "The door of the service module's scientific instrument bay was jettisoned"},
    {"86:14:23", 0, B_MOON, "月を回る軌道に入る", "Lunar orbit insertion",
     "月の裏側で主エンジンを 6 分 33 秒噴射し、月を回る軌道に入った", "Behind the Moon the main engine fired for 6 min 33 s to enter lunar orbit"},
    {"90:31:37", 0, B_MOON, "つながったまま降下軌道へ", "Descent orbit insertion, docked",
     "司令船の主エンジンで、月着陸船とつながったまま降下軌道に下げた", "The command module's engine lowered the docked spacecraft into the descent orbit"},
    {"107:47:56", 1, B_MOON, "チャレンジャーが離れる", "Challenger undocks",
     "サーナンとシュミットの乗った月着陸船チャレンジャーが、エバンスの残る司令船から離れた",
     "The lunar module Challenger, with Cernan and Schmitt aboard, undocked from the command module, where Evans stayed"},
    {"109:22:42", 1, B_MOON, "2 回目の降下軌道への噴射", "Second descent orbit insertion",
     "月着陸船の下降エンジンで、着陸に向けて軌道をさらに下げた", "The lunar module's descent engine lowered the orbit further for the landing"},
    {"110:09:53", 1, AP_LOOK_FWD, "動力降下の開始", "Powered descent", "下降エンジンを噴射し続けて月面へ降りる", "The descent engine fired continuously down to the surface"},
    {"110:21:58", 1, B_EARTH, "月面に着陸", "Landed on the Moon",
     "タウルス・リットロウ谷（北緯 20.191°・東経 30.772°）に着陸。山々に囲まれた深い谷", "Touchdown in the Taurus-Littrow valley (20.191° N, 30.772° E), a deep valley ringed by mountains"},
    {"114:21:49", 1, B_EARTH, "月面での活動", "Moonwalks",
     "3 回の月面活動は合わせて約 22 時間、月面車で約 36 km を走った。2 回目にはショーティ・クレーターでオレンジ色の土を見つけた",
     "Three moonwalks totalled about 22 hours and some 36 km of driving. On the second they found orange soil at Shorty crater"},
    {"160:52:48", 1, B_EARTH, "最後の月面活動", "Last moonwalk",
     "3 回目の月面活動。最後に月面を離れたのはサーナンで、これまでで最後の有人の月面着陸になっている",
     "The third moonwalk. Cernan was the last to leave the surface; this remains the most recent crewed landing on the Moon"},
    {"179:54:14", 0, B_MOON, "司令船の軌道面の変更", "CSM plane change",
     "月着陸船の離陸に合わせて、司令船が軌道面を変えた", "The command module changed its orbital plane to match the lunar module's liftoff"},
    {"185:21:37", 1, AP_LOOK_FWD, "月面を離れる", "Lunar liftoff",
     "74 時間 59 分の月面滞在を終え、上昇段が飛び立った。月面車に置いたテレビカメラが離陸を追って映した",
     "After 74 h 59 min on the Moon the ascent stage lifted off, tracked by the television camera left on the rover"},
    {"185:28:56", 1, B_MOON, "月を回る軌道へ", "Into lunar orbit", "上昇段が月を回る軌道に入った", "The ascent stage reached lunar orbit"},
    {"186:15:58", 1, B_MOON, "司令船へ向かう", "Terminal phase initiation", "司令船へまっすぐ近づいていく噴射", "The burn that set the ascent stage on its final approach"},
    {"187:37:15", 0, B_MOON, "ドッキング", "Docking", "チャレンジャーの上昇段がアメリカとドッキングした", "Challenger's ascent stage docked with America"},
    {"192:58:14", 1, B_MOON, "上昇段を月面に落とす", "Ascent stage deorbit",
     "切り離した上昇段を、置いてきた地震計で調べるため月面に落とした", "The jettisoned ascent stage was crashed into the Moon for the seismometers left behind"},
    {"234:02:09", 0, B_EARTH, "地球へ向かう噴射", "Transearth injection",
     "月の裏側で主エンジンを 2 分 24 秒噴射し、地球へ向かう軌道に", "Behind the Moon the main engine fired for 2 min 24 s onto a course for Earth"},
    {"254:54:40", 0, B_EARTH, "深宇宙での船外活動", "Deep-space spacewalk",
     "エバンスが宇宙船の外に出て、科学機器室からフィルムを回収した", "Evans went outside to retrieve film from the scientific instrument bay"},
    {"298:38:01", 0, B_EARTH, "軌道の修正", "Midcourse correction", "9 秒の小さな修正", "A 9-second correction"},
    {"301:23:49", 0, B_EARTH, "機械船を切り離す", "Service module jettisoned",
     "司令船だけで大気に入るため、機械船を切り離した", "The service module was jettisoned so the command module could enter the atmosphere alone"},
    {"301:38:38", 0, AP_LOOK_FWD, "大気圏に突入", "Atmospheric entry",
     "高度 約 122 km、秒速 11.0 km で大気に突入。最大で 6.5 G がかかった", "Entering the atmosphere at about 122 km altitude and 11.0 km/s, peaking at 6.5 g"},
    {"301:46:22", 0, AP_LOOK_FWD, "パラシュートを開く", "Parachutes",
     "減速用のドローグ・パラシュートを開き、続けて 3 つの主パラシュートを開いた", "The drogue parachutes opened, followed by the three main parachutes"},
    {"301:51:59", 0, AP_LOOK_FWD, "着水", "Splashdown",
     "サモアの南東の太平洋（南緯 17.88°・西経 166.11°）に着水し、空母タイコンデロガが回収した。打ち上げから 12 日 13 時間 52 分、\nアポロ計画の最後の月への飛行",
     "Splashdown in the Pacific southeast of Samoa (17.88° S, 166.11° W); the carrier USS Ticonderoga recovered the crew.\nThe flight lasted 12 days 13 hours 52 minutes, the last Apollo voyage to the Moon"},
};
// 打ち上げ脱出用のロケットの切り離しは実際の時刻の記録がないので、予定の 0:03:19（SP-4029）
static const ApolloShape APOLLO17_SHAPES[] = {
    {"-3:00:00", 0, PM_SATURN_V}, {"0:02:42.9", 0, PM_SATURN_SII_RING}, {"0:03:12.90", 0, PM_SATURN_SII_LES}, {"0:03:19", 0, PM_SATURN_SII},
    {"0:09:20.6", 0, PM_SATURN_SIVB}, {"3:42:27.6", 0, PM_APOLLO_CSM}, {"3:57:03", 0, PM_APOLLO_CSM_LM},
    {"106:22:03", 0, PM_APOLLO_CSM_LMG}, {"107:47:56", 0, PM_APOLLO_CSM}, {"187:37:15", 0, PM_APOLLO_CSM_LMA}, {"191:18:31", 0, PM_APOLLO_CSM},
    {"301:23:49", 0, PM_APOLLO_CM}, {"301:46:22", 0, PM_CM_DROGUE}, {"301:47:13", 0, PM_CM_MAIN}, {"301:51:59", 0, PM_APOLLO_CM},
    {"107:47:56", 1, PM_APOLLO_LM}, {"185:21:37", 1, PM_LM_ASCENT}, {"187:37:15", 1, -1}, {"191:18:31", 1, PM_LM_ASCENT},
};

// 11 号: Mission Report 4.4・7.4.2・8.6.1 と Apollo Flight Journal。離れる速さは 0.7 ft/s（コリンズ「0.7 で止める」）、
// 振り向きは平均 1.75°/s（離れて 20 秒後に始める予定）、自動操縦が何度も止まって最も離れたのは少なくとも 100 ft、
// 接触は 0.1 ft/s、引き出しは 0.7 ft/s。第3段の液体酸素の放出（太陽を回る軌道へ移る）は 5:03:19
static const ApolloTde APOLLO11_TDE = {"3:17:04.6", "3:24:03.1", "4:16:59.1", "5:03:19", 0.21, 30.5, 20, 1.75, 0.03, 0.21};
// ほかの号（Mission Report と Apollo Flight Journal。記録が見つからない値は手順書の予定の値: 離れる速さ 0.5 ft/s、離れて
// 15〜20 秒後に 0.5°/s で向きを変える、接触 0.1 ft/s、引き出しはばね 0.8 ft/s と噴射 0.4 ft/s で 1.2 ft/s）
// 8 号: 月着陸船の代わりに重り（LTA-B。形の記録がないので描かない）を積み、ドッキングはせずに編隊飛行。離れる速さ 0.4 ft/s
// （EMS が 100.4）、3:40:00 に地球から外向きに 1.15 ft/s 離れる噴射。第3段は 4:44:55 から液体酸素の放出の向きへ回る
static const ApolloTde APOLLO8_TDE = {"3:20:59", nullptr, nullptr, "4:44:55", 0.12, 0, 10, 0.5, 0, 0, nullptr, 0, "3:40:00", 0.35};
// 10 号: 0.6 ft/s で離れ、1.5°/s で向きを変えた時は 150 ft 離れていた。接触 0.2 ft/s（3:17:03）、引き出し 3:56:25。
// 第3段の液体酸素の放出は 4:44:41 の「あと 3 分」から
static const ApolloTde APOLLO10_TDE = {"3:02:42.4", "3:17:03", "3:56:25", "4:44:41", 0.18, 46, 10, 1.5, 0.06, 0.37};
// 12 号: 引き出し 4:13:00、第3段は 4:18:26 から回避の噴射の向きへ回る
static const ApolloTde APOLLO12_TDE = {"3:18:04.9", "3:26:53.3", "4:13:00", "4:18:26", 0.15, 21.3, 20, 0.5, 0.03, 0.37};
// 13 号: 離れて約 1 分で 60° 回り（約 1.4°/s）、80 ft 離れた。引き出し 4:01:03、第3段は 4:08:26 から回避の向きへ
static const ApolloTde APOLLO13_TDE = {"3:06:38.9", "3:19:08.8", "4:01:03", "4:08:26", 0.15, 24.4, 15, 1.4, 0.03, 0.37};
// 14 号: 掛け金がかからず 6 回目でドッキング。接触は 3:13:55（0.2 ft/s）・3:14:58・3:17:45 頃・3:23:50・4:32:25 頃、
// 最後は 4:54:56 に 15 ft、4:55:40 に 6 ft、4:56:21 に 2 ft、4:56:35 に 1 ft（乗員の目測）。やり直しの間に下がった距離は
// 記録がないので 2〜4 ft とする。引き出し 5:47:14、第3段は 5:55:31 から向きを変える
static const ApolloTdeKey APOLLO14_DOCK[] = {
    {"3:13:55", 0}, {"3:14:25", 0.6}, {"3:14:58", 0}, {"3:16:30", 0.6}, {"3:17:45", 0}, {"3:20:30", 1.2}, {"3:23:50", 0},
    {"3:28:00", 1.2}, {"4:32:11", 1.2}, {"4:32:25", 0}, {"4:54:56", 4.6}, {"4:55:40", 1.8}, {"4:56:21", 0.6}, {"4:56:35", 0.3}};
static const ApolloTde APOLLO14_TDE = {"3:02:29.4", "4:56:56.7", "5:47:14", "5:55:31", 0.15, 21.3, 15, 0.5, 0.06, 0.37,
                                       APOLLO14_DOCK, (int)(sizeof APOLLO14_DOCK / sizeof APOLLO14_DOCK[0])};
// 15 号: 離れて 15 秒後に 2°/s で向きを変え、0.1 ft/s で接触（3:32:49）、3:33:49.5 に固くつながった。引き出しは
// ばね 0.25 m/s と噴射 0.12 m/s（4:18:01）、第3段は 4:31 に向きを変える
static const ApolloTdeKey APOLLO15_DOCK[] = {{"3:32:49", 0}};
static const ApolloTde APOLLO15_TDE = {"3:22:27.2", "3:33:49.5", "4:18:01", "4:31:00", 0.15, 0, 15, 2.0, 0.03, 0.37, APOLLO15_DOCK, 1};
// 16 号: 離れてすぐ向きを変え始め、3:08:28 には向き終わっていた（約 0.9°/s）。80 ft 離れた。3:15:23 に接触、引き出し
// 3:59:14、第3段は 4:10:23 から向きを変える
static const ApolloTde APOLLO16_TDE = {"3:04:59.0", "3:15:23", "3:59:15.1", "4:10:23", 0.15, 24.4, 5, 0.9, 0.03, 0.37};
// 17 号: 3:57:03 に 0.1 ft/s で接触、引き出し 4:45:02.3、第3段は 4:52:46 から向きを変える
static const ApolloTde APOLLO17_TDE = {"3:42:27.6", "3:57:03", "4:45:02.3", "4:52:46", 0.15, 0, 15, 0.5, 0.03, 0.37};

// 月を回る軌道での切り離しとドッキング（Apollo Flight Journal）。切り離した後は、40 秒ほど離れてから止まり（10 号「40 秒ほど
// 離れてから離れる速さを止める」、11 号は 59 秒後に止めた）、30〜40 ft で待つ（10 号）。月着陸船は 1 分ほどして脚を司令船に
// 見せる向きへ回る（10 号 56 秒後、11 号 73 秒後、12 号 61 秒後に回り始め、12 号・15 号は 2 分ほどで回り終えた）。ドッキングの前は、
// 減速を終えて司令船の先 40 ft で待ち、上昇段の天頂の側を司令船へ向けて（11 号の Technical debrief）、0.1 ft/s ほどで接触する
// （11 号のコリンズ「0.1 ft/s を少し超える」）。時刻は 17 号を除いて記録の GET（17 号の記録は打ち上げの遅れの分 2:40:00 進めた GET
// なので、その分を戻した）
// 10 号: 脚 97:45:03。ドッキングの前は 25 ft で止まり（Technical debrief）、106:20:14 に 5 ft、106:21:14 に捕まえた。
// 待ち始めと回り始めの時刻は記録がないので、0.1 ft/s で近づいたとして決めた
static const ApolloTdeKey APOLLO10_RDV[] = {{"106:11:51", 7.6}, {"106:16:51", 7.6}, {"106:20:14", 1.5}};
static const ApolloLunarDock APOLLO10_LUNAR = {"97:45:03", "98:11:57", "98:12:53", "106:22:02", "106:21:14", "106:14:21", APOLLO10_RDV, 3};
// 11 号: 脚 98:14:35、切り離した後は 100:13:13 に回り始めた。ドッキングの前は 127:41:21 に 1.5 海里、127:43:43 に 0.7 海里、
// 127:52:05 に待ち始め（40 ft）、127:51:21 から回り、127:58:42 に 15 ft（127:59:14 の「10 ft、5 ft くらい」とは合わない乗員の目測）。
// 接触はドッキングの時刻 128:03:00（Mission Report）とする
static const ApolloTdeKey APOLLO11_RDV[] = {{"127:41:21", 2780}, {"127:43:43", 1300}, {"127:52:05", 12.2}, {"127:58:42", 4.6}};
static const ApolloLunarDock APOLLO11_LUNAR = {"98:14:35", "100:12:00.0", "100:13:13", "128:03:00.0", nullptr, "127:51:21", APOLLO11_RDV, 4};
// 12 号: 脚 104:47:54、切り離した後は 107:55:03 に回り始めた。ドッキングの前は 1.7 海里から 200 ft まで乗員が距離を読み上げ
// （145:14:42〜145:25:08）、145:35:23 に捕まえた。待つ距離と近づき始める時刻は記録がないので手順書の値
static const ApolloTdeKey APOLLO12_RDV[] = {{"145:14:42", 3150}, {"145:16:42", 1852}, {"145:17:41", 1372}, {"145:18:38", 926},
                                            {"145:19:24", 610}, {"145:20:10", 457}, {"145:20:44", 366}, {"145:22:13", 274},
                                            {"145:23:44", 152}, {"145:25:08", 61}, {"145:26:30", 12.2}, {"145:28:43", 12.2}};
static const ApolloLunarDock APOLLO12_LUNAR = {"104:47:54", "107:54:02.3", "107:55:03", "145:36:20.2", "145:35:23", "145:26:13",
                                               APOLLO12_RDV, (int)(sizeof APOLLO12_RDV / sizeof APOLLO12_RDV[0])};
// 14 号: 脚 102:59:12（開いたと報告）。ドッキングの前の距離の記録は見つからないので手順書の値
static const ApolloLunarDock APOLLO14_LUNAR = {"102:59:12", "103:47:41.6", nullptr, "143:32:50.5", nullptr, nullptr};
// 15 号: 脚 98:40 頃（Flight Journal の予定の時刻）、切り離した後は 100:40:47 に回り始めた。ドッキングの前は 173:09:13 に 1 海里、
// 3,000 ft・1,500 ft と減速し、173:15:44 に 120 ft で待ち始め、173:28:03 に回り始めた。173:36:28 に固くつながった
static const ApolloTdeKey APOLLO15_RDV[] = {{"173:09:13", 1852}, {"173:11:05", 914}, {"173:12:18", 457}, {"173:15:44", 37}, {"173:30:03", 37}};
static const ApolloLunarDock APOLLO15_LUNAR = {"98:40:01", "100:39:16.2", "100:40:47", "173:36:25.5", nullptr, "173:28:03", APOLLO15_RDV, 5};
// 16 号: 脚 94:00 頃（94:00:09 に開く手順）。ドッキングの前は 3.15 海里から 200 ft まで乗員が距離を読み上げ（176:57:06〜177:11:16）、
// 177:12:18 に待ち始め（距離の記録はないので 40 ft）、177:34:02 に回り始め、177:38:58 に 5 ft、177:41:18 に捕まえた
static const ApolloTdeKey APOLLO16_RDV[] = {{"176:57:06", 5834}, {"177:00:03", 4572}, {"177:03:45", 2438}, {"177:05:25", 1524},
                                            {"177:06:19", 1219}, {"177:07:50", 610}, {"177:09:07", 244}, {"177:09:37", 165},
                                            {"177:10:04", 122}, {"177:10:39", 73}, {"177:11:16", 61}, {"177:12:18", 12.2},
                                            {"177:36:02", 12.2}, {"177:38:58", 1.5}};
static const ApolloLunarDock APOLLO16_LUNAR = {"94:00:09", "96:13:31", nullptr, "177:41:18", nullptr, "177:34:02",
                                               APOLLO16_RDV, (int)(sizeof APOLLO16_RDV / sizeof APOLLO16_RDV[0])};
// 17 号: 脚 106:22:03（記録の GET 109:02:03）。ドッキングの前の記録は Flight Journal にまだ無いので手順書の値
static const ApolloLunarDock APOLLO17_LUNAR = {"106:22:03", "107:47:56", nullptr, "187:37:15", nullptr, nullptr};

static const ApolloMission APOLLO_MISSIONS[] = {
    {"Apollo 8", "アポロ8号", "Apollo 8", 1968, 12, 21, 12, 51, 0.0, {"Apollo 8", nullptr},
     "人類が初めて月を回った", "First crewed flight around the Moon",
     APOLLO8_EVENTS, (int)(sizeof APOLLO8_EVENTS / sizeof APOLLO8_EVENTS[0]),
     APOLLO8_SHAPES, (int)(sizeof APOLLO8_SHAPES / sizeof APOLLO8_SHAPES[0]), NAN, NAN, nullptr, &APOLLO8_TDE, 8, nullptr, 19589, 300, 5.5},
    {"Apollo 10", "アポロ10号", "Apollo 10", 1969, 5, 18, 16, 49, 0.0, {"Apollo 10 Charlie Brown", "Apollo 10 Snoopy"},
     "着陸の予行演習（月面の上空 約 14 km まで）", "Landing dress rehearsal (down to about 14 km)",
     APOLLO10_EVENTS, (int)(sizeof APOLLO10_EVENTS / sizeof APOLLO10_EVENTS[0]),
     APOLLO10_SHAPES, (int)(sizeof APOLLO10_SHAPES / sizeof APOLLO10_SHAPES[0]), NAN, NAN, nullptr, &APOLLO10_TDE, 8, &APOLLO10_LUNAR, 13767, 300, 5.5},
    {"Apollo 11", "アポロ11号", "Apollo 11", 1969, 7, 16, 13, 32, 0.0, {"Apollo 11 Columbia", "Apollo 11 Eagle"},
     "人類初の月面着陸（静かの海）", "First crewed landing (Sea of Tranquility)",
     APOLLO11_EVENTS, (int)(sizeof APOLLO11_EVENTS / sizeof APOLLO11_EVENTS[0]),
     APOLLO11_SHAPES, (int)(sizeof APOLLO11_SHAPES / sizeof APOLLO11_SHAPES[0]), 0.67408, 23.47297, "124:22:00.79", &APOLLO11_TDE, 8, &APOLLO11_LUNAR, 14549, 300, 5.5},
    {"Apollo 12", "アポロ12号", "Apollo 12", 1969, 11, 14, 16, 22, 0.0, {"Apollo 12 Yankee Clipper", "Apollo 12 Intrepid"},
     "嵐の大洋、サーベイヤー3号のそばへピンポイント着陸", "Pinpoint landing beside Surveyor 3 (Ocean of Storms)",
     APOLLO12_EVENTS, (int)(sizeof APOLLO12_EVENTS / sizeof APOLLO12_EVENTS[0]),
     APOLLO12_SHAPES, (int)(sizeof APOLLO12_SHAPES / sizeof APOLLO12_SHAPES[0]), -3.01239, -23.42157, "142:03:47.78", &APOLLO12_TDE, 8, &APOLLO12_LUNAR, 13161, 300, 5.5},
    {"Apollo 13", "アポロ13号", "Apollo 13", 1970, 4, 11, 19, 13, 0.0, {"Apollo 13 Odyssey", "Apollo 13 Aquarius"},
     "酸素タンクの爆発で着陸を中止し、月の裏を回って帰還", "Landing aborted after an oxygen tank explosion; looped the Moon and returned",
     APOLLO13_EVENTS, (int)(sizeof APOLLO13_EVENTS / sizeof APOLLO13_EVENTS[0]),
     APOLLO13_SHAPES, (int)(sizeof APOLLO13_SHAPES / sizeof APOLLO13_SHAPES[0]), NAN, NAN, nullptr, &APOLLO13_TDE},
    {"Apollo 14", "アポロ14号", "Apollo 14", 1971, 1, 31, 21, 3, 2.0, {"Apollo 14 Kitty Hawk", "Apollo 14 Antares"},
     "フラ・マウロ高地に着陸", "Landing in the Fra Mauro highlands",
     APOLLO14_EVENTS, (int)(sizeof APOLLO14_EVENTS / sizeof APOLLO14_EVENTS[0]),
     APOLLO14_SHAPES, (int)(sizeof APOLLO14_SHAPES / sizeof APOLLO14_SHAPES[0]), -3.64530, -17.47136, "141:45:40", &APOLLO14_TDE, 8, &APOLLO14_LUNAR, 11660, 25, 2},
    {"Apollo 15", "アポロ15号", "Apollo 15", 1971, 7, 26, 13, 34, 0.6, {"Apollo 15 Endeavour", "Apollo 15 Falcon"},
     "ハドリー谷に着陸、初めて月面車を使用", "Landing at Hadley Rille; first lunar rover",
     APOLLO15_EVENTS, (int)(sizeof APOLLO15_EVENTS / sizeof APOLLO15_EVENTS[0]),
     APOLLO15_SHAPES, (int)(sizeof APOLLO15_SHAPES / sizeof APOLLO15_SHAPES[0]), 26.13222, 3.63386, "171:37:23.2", &APOLLO15_TDE, 4, &APOLLO15_LUNAR, 13358, 25, 2},
    {"Apollo 16", "アポロ16号", "Apollo 16", 1972, 4, 16, 17, 54, 0.0, {"Apollo 16 Casper", "Apollo 16 Orion"},
     "デカルト高地に着陸", "Landing in the Descartes highlands",
     APOLLO16_EVENTS, (int)(sizeof APOLLO16_EVENTS / sizeof APOLLO16_EVENTS[0]),
     APOLLO16_SHAPES, (int)(sizeof APOLLO16_SHAPES / sizeof APOLLO16_SHAPES[0]), -8.97301, 15.50019, "175:31:47.9", &APOLLO16_TDE, 8, &APOLLO16_LUNAR, 14199, 25, 2},
    {"Apollo 17", "アポロ17号", "Apollo 17", 1972, 12, 7, 5, 33, 0.0, {"Apollo 17 America", "Apollo 17 Challenger"},
     "タウルス・リットロウ谷に着陸、アポロ計画の最後", "Landing at Taurus-Littrow; the last Apollo mission",
     APOLLO17_EVENTS, (int)(sizeof APOLLO17_EVENTS / sizeof APOLLO17_EVENTS[0]),
     APOLLO17_SHAPES, (int)(sizeof APOLLO17_SHAPES / sizeof APOLLO17_SHAPES[0]), 20.19080, 30.77168, "185:21:37", &APOLLO17_TDE, 8, &APOLLO17_LUNAR, 13507, 25, 2},
};
static const int APOLLO_COUNT = (int)(sizeof APOLLO_MISSIONS / sizeof APOLLO_MISSIONS[0]);

// "hhh:mm:ss.s"（先頭に - で打ち上げ前）→ 秒
static double ApolloGetSec(const char* s) {
    bool neg = s[0] == '-';
    int h = 0, m = 0;
    double sec = 0;
    sscanf(s + (neg ? 1 : 0), "%d:%d:%lf", &h, &m, &sec);
    double v = h * 3600.0 + m * 60.0 + sec;
    return neg ? -v : v;
}
static double ApolloLaunch(const ApolloMission& m) {
    return (double)JdnFromDate(m.y, m.mo, m.d) - 2451545.0 - 0.5 + (m.h * 3600.0 + m.mi * 60.0 + m.sec) / 86400.0;
}
static double ApolloT(const ApolloMission& m, const char* get) { return ApolloLaunch(m) + ApolloGetSec(get) / 86400.0; }

// 天体 i がアポロのどの飛行のどの機体か（見つからなければ false）
static bool ApolloVehicleOf(const App& app, int i, int* mission, int* vehicle) {
    for (int k = 0; k < APOLLO_COUNT; k++)
        for (int v = 0; v < 2; v++)
            if (APOLLO_MISSIONS[k].vehicles[v] && std::strcmp(APOLLO_MISSIONS[k].vehicles[v], app.B[i].d.en) == 0) {
                *mission = k;
                *vehicle = v;
                return true;
            }
    return false;
}
static int ApolloBody(const App& app, int mission, int vehicle) {
    if (!APOLLO_MISSIONS[mission].vehicles[vehicle]) return -1;
    for (size_t i = 0; i < app.B.size(); i++)
        if (std::strcmp(app.B[i].d.en, APOLLO_MISSIONS[mission].vehicles[vehicle]) == 0) return (int)i;
    return -1;
}

// 時刻 t の機体の形（-1: 描かない）。月着陸船は最初に離れるまで司令船の側で描くので -1
static int ApolloModel(const ApolloMission& m, int vehicle, double t) {
    int model = vehicle == 0 ? PM_APOLLO_CSM : -1;
    double best = -1e300;
    for (int k = 0; k < m.shapeCount; k++) {
        const ApolloShape& s = m.shapes[k];
        double ts = ApolloT(m, s.get);
        if (s.vehicle == vehicle && ts <= t && ts >= best) {
            best = ts;
            model = s.model;
        }
    }
    return model;
}

// 司令船とつながっていて単独では描かない機体（マーカーも出さない。軌跡の位置は司令船から数 m ずれていて、
// マーカーだけが司令船のまわりを回って見えるため）
static bool ApolloJoined(const App& app, int i) {
    int mission, vehicle;
    return app.B[i].d.apollo >= 0 && ApolloVehicleOf(app, i, &mission, &vehicle) && ApolloModel(APOLLO_MISSIONS[mission], vehicle, app.tDays) < 0;
}

// 天体の自転で、中心からの位置 rel の点が動く速度 km/日
static V3 ApolloSpinVel(const Body& o, V3 rel) {
    double rp = o.d.rotPeriod == 0 ? o.d.moonPeriod : o.d.rotPeriod;
    return Cross(o.pole * (2 * PI / rp), rel);
}

// 機体の今の区間（天体固定の区間なら *fixedBody にその天体、慣性なら -1）
static int ApolloFixedBody(const BodyDef& d, double t) {
    if (d.apollo < 0 || d.apollo >= (int)g_apollo.size()) return -1;
    for (const ApolloSeg& s : g_apollo[d.apollo].segs)
        if (s.t.front() <= t && t <= s.t.back()) return s.fixed ? s.body : -1;
    return -1;
}

// 機体の速度（基準天体に対する。天体固定の区間では地面に対する）km/日
static V3 ApolloVel(const App& app, int i, double t, int* refOut = nullptr) {
    const BodyDef& d = app.B[i].d;
    int fixedBody = ApolloFixedBody(d, t);
    int ref = fixedBody >= 0 ? fixedBody : B_EARTH;
    if (fixedBody < 0)
        for (const ApolloSeg& s : g_apollo[d.apollo].segs)
            if (s.t.front() <= t && t <= s.t.back()) ref = s.body;
    if (refOut) *refOut = ref;
    const double h = 0.5 / 86400;
    V3 a = PosAt(app.B, i, t - h) - PosAt(app.B, ref, t - h), b = PosAt(app.B, i, t + h) - PosAt(app.B, ref, t + h);
    V3 v = (b - a) * (1.0 / (2 * h));
    if (fixedBody >= 0) v = v - ApolloSpinVel(app.B[fixedBody], (a + b) * 0.5);   // 地面に対する速度: 自転で動く分を引く
    return v;
}

// 時刻 t の機体の形が始まった時刻（形の切り替えの表から。無ければ -1e300）
static double ApolloModelSince(const ApolloMission& m, int vehicle, double t) {
    double best = -1e300;
    for (int k = 0; k < m.shapeCount; k++) {
        double ts = ApolloT(m, m.shapes[k].get);
        if (m.shapes[k].vehicle == vehicle && ts <= t && ts > best) best = ts;
    }
    return best;
}

static double ApolloSmooth(double x) {
    x = std::max(0.0, std::min(1.0, x));
    return x * x * (3 - 2 * x);
}

// 機体（司令船の側）の形が from から to に替わる最初の時刻（形の切り替えの表から。無ければ NaN）
static double ApolloShapeChange(const ApolloMission& m, int from, int to) {
    int prev = -1;
    for (int k = 0; k < m.shapeCount; k++) {
        if (m.shapes[k].vehicle != 0) continue;
        if (m.shapes[k].model == to && prev == from) return ApolloT(m, m.shapes[k].get);
        prev = m.shapes[k].model;
    }
    return NAN;
}
// 機械船を切り離した時刻（13 号は月着陸船とつながったまま切り離した。stack に true）。無ければ NaN
static double ApolloSmSepT(const ApolloMission& m, bool* stack = nullptr) {
    double t = ApolloShapeChange(m, PM_APOLLO_CSM, PM_APOLLO_CM);
    if (stack) *stack = !std::isfinite(t);
    return std::isfinite(t) ? t : ApolloShapeChange(m, PM_APOLLO_CSM_LM, PM_APOLLO_CM_LM);
}
// 13 号の月着陸船を切り離した時刻。無ければ NaN
static double ApolloLmJettT(const ApolloMission& m) { return ApolloShapeChange(m, PM_APOLLO_CM_LM, PM_APOLLO_CM); }

// 機械船を切り離す前後の司令船の向き。切り離しの向きは、耐熱シールドを前へ向けた大気に入る向きから左へ 45° 振った向き
// （司令船の先端を軌道面の北、r×v の側へ。機械船が後ろへ離れる噴射で、大気に入る道から外れていくように。8・11 号の
// Mission Report と Apollo Flight Journal、15 号の大気に入る時の手順書）。切り離しの 10 分前から 4 分かけて切り離しの向きへ回り
// （10 号の Flight Journal では 6 分 18 秒前に切り離しの向きに着いた）、切り離した後 7 分かけて振った分を戻す（10 号は切り離して
// 7 分 42 秒後に「大気に入る向きに近づいている」）
static const double SMSEP_TURN0 = 600, SMSEP_TURN = 240, SMSEP_BACK = 420, SMSEP_YAW = 45 * DEG;

// 天体固定の区間での見方にする割合: 区間の中は 1、区間の前後 sec 秒で 0 から 1 へ（地球を回る軌道に入る時や月面への
// 降下を始める時に、見る向きが一度に変わらないように）
static double ApolloFixedFade(const BodyDef& d, double t, double sec) {
    if (d.apollo < 0 || d.apollo >= (int)g_apollo.size()) return 0;
    double best = 0;
    for (const ApolloSeg& s : g_apollo[d.apollo].segs) {
        if (!s.fixed) continue;
        double out = std::max(s.t.front() - t, t - s.t.back()) * 86400;   // 区間の外へ何秒か（中は負）
        best = std::max(best, 1 - ApolloSmooth(out / sec));
    }
    return best;
}

// 天体固定の区間で、地面に対して水平に進む向き（描画座標の単位ベクトル。v は ApolloVel、up は天体から外向き）。
// 発射台・月面・パラシュートで降りる間は水平の速さがほぼ 0 で、速度の向きは位置の差の誤差で毎フレーム揺れ、記録の
// 遅い点の向きもばらつくので、水平に秒速 30 m より遅い間は、区間の記録の水平の速度を時刻の近いものほど重く（1 分で
// 半分、遠くはゆっくり減る重み）平均した向きを使う（時刻に対して連続に変わる）。無ければ (0,0,0)
static V3 ApolloGroundDir(const App& app, int i, double t, V3 v, V3 up) {
    const double slow = 0.03 * 86400, fast = 0.1 * 86400, tau = 60.0 / 86400;
    V3 vh = v - up * Dot(v, up), g;
    const BodyDef& d = app.B[i].d;
    if (Len(vh) < fast && d.apollo >= 0 && d.apollo < (int)g_apollo.size())
        for (const ApolloSeg& s : g_apollo[d.apollo].segs) {
            if (!s.fixed || t < s.t.front() || t > s.t.back()) continue;
            V3 sum;
            for (size_t k = 0; k < s.t.size(); k++) {
                V3 u = Norm(s.p[k]), h = s.v[k] - u * Dot(s.v[k], u);
                double x = (s.t[k] - t) / tau, dt = s.t[std::min(k + 1, s.t.size() - 1)] - s.t[k > 0 ? k - 1 : 0];
                sum = sum + h * (dt / (1 + x * x));
            }
            if (Len(sum) > 0) {
                g = FixedToRender(app.B[s.body], SpinAt(app.B, s.body, t), sum);
                g = g - up * Dot(g, up);
                g = Len(g) > 1e-12 ? Norm(g) : V3();
            }
            break;
        }
    if (Len(g) == 0) return Len(vh) > 1e-9 ? Norm(vh) : V3();
    if (Len(vh) < 1e-9) return g;
    double w = ApolloSmooth((Len(vh) - slow) / (fast - slow));   // 水平に秒速 30〜100 m で今の速度の向きへ移す
    V3 r = g * (1 - w) + Norm(vh) * w;
    return Len(r) > 1e-9 ? Norm(r) : Norm(vh);
}

// 機体とカメラの上にする向き（時刻 t）: 天体固定の区間はその天体から外向き。慣性の区間は近い天体（月からの距離 6.5 万 km
// より近ければ月、ほかは地球）から外向きを、その天体から 2〜4 万 km の間で黄道の北へ移す（月へ向かいながら遠ざかる間に、
// 天体から外向きの向きが見る向きのまわりを大きく回り、画面が傾いていかないように）
static V3 ApolloUpHint(const App& app, int i, double t) {
    V3 p = PosAt(app.B, i, t);
    int fixedBody = ApolloFixedBody(app.B[i].d, t);
    if (fixedBody >= 0) return Norm(p - PosAt(app.B, fixedBody, t));
    V3 rm = p - PosAt(app.B, B_MOON, t), re = p - PosAt(app.B, B_EARTH, t);
    V3 near = ApolloSmooth((80000 - Len(rm)) / 30000) > 0.5 ? rm : re;
    double a = ApolloSmooth((40000 - Len(near)) / 20000);
    return Norm(Norm(near) * a + Y_UP * (1 - a));
}

// 噴射していない時の機体の向き（時刻 t）:打ち上げは上へ（速さが出ると進む向きへ。大気の中では空気に対する向き、高く上がる
// につれて慣性の速度の向きへ）、月着陸船の降下は噴射を進む向きの逆へ（遅くなるにつれて真上へ）、月面と月を回る間は真上、
// 司令船は機械船を切り離してから 3 分かけて耐熱シールドを前へ、パラシュートでは真上、そのほかは進む向き。
// 機体の上（+Y）は、地球や月を回る間は天体から外向き、離れて飛ぶ間は黄道の北（月へ行き来する途中で基準天体が地球と
// 月の間で替わっても向きが飛ばないように）。発射台では西（真上へ向いたサターン V が傾き始めると、空の側へ続く）
static Quaternion ApolloCoastPose(const App& app, int i, int model, double t) {
    const Body& b = app.B[i];
    int ref;
    V3 v = ApolloVel(app, i, t, &ref);
    const Body& o = app.B[ref];
    V3 rel = PosAt(app.B, i, t) - PosAt(app.B, ref, t);
    V3 up = Norm(rel);
    int fixedBody = ApolloFixedBody(b.d, t);
    V3 hint = up;
    if (fixedBody < 0) {
        // 慣性の区間: 地球に対する速度と月に対する速度を、月からの距離 5〜8 万 km の間で移す（基準天体が替わる所で
        // 進む向きが飛ばないように）。上は ApolloUpHint
        const double h = 0.5 / 86400;
        auto relVel = [&](int k) { return (PosAt(app.B, i, t + h) - PosAt(app.B, k, t + h) - PosAt(app.B, i, t - h) + PosAt(app.B, k, t - h)) * (1.0 / (2 * h)); };
        double wm = ApolloSmooth((80000 - Len(PosAt(app.B, i, t) - PosAt(app.B, B_MOON, t))) / 30000);
        v = relVel(B_EARTH) * (1 - wm) + relVel(B_MOON) * wm;
        hint = ApolloUpHint(app, i, t);
    }
    double speed = Len(v) / 86400;   // km/s
    V3 vdir = speed > 1e-9 ? Norm(v) : up;
    if (fixedBody >= 0 && speed > 1e-9) {
        // 天体固定の区間: 速度の水平の向きは、ほぼ真上・真下へ動く間（パラシュートなど）は誤差で揺れるので、地面に対して
        // 水平に進む向き（ApolloGroundDir）に置き換える
        V3 g = ApolloGroundDir(app, i, t, v, up);
        double vu = Dot(vdir, up);
        if (Len(g) > 0) vdir = Norm(up * vu + g * std::sqrt(std::max(0.0, 1 - vu * vu)));
    }
    V3 fwd = vdir;
    auto blend = [](V3 a, V3 c, double w) { V3 r = a * (1 - w) + c * w; return Len(r) > 1e-9 ? Norm(r) : a; };
    // 耐熱シールドを前へ向けた司令船（上はそのままで向きを 180° 変える）。yaw: さらに先端を軌道面の北（r×v）の側へ振る角
    auto shieldFwd = [&](double w, double yaw = 0) {
        V3 hn = Len(Cross(vdir, hint)) > 1e-6 ? hint : Len(Cross(vdir, Y_UP)) > 1e-6 ? Y_UP : V3(1, 0, 0);
        Quaternion q0 = QuatFromBasis(vdir, hn);
        // 半回転を決まった軸（機体の上）のまわりに回す（両端を球面補間すると、ちょうど逆向きなので回る側が定まらない）
        V3 axis = FromF(Vector3RotateByQuaternion({0, 1, 0}, q0));
        double north = Dot(Cross(axis, vdir), Cross(rel, v)) >= 0 ? 1 : -1;   // 半回転した先端を北へ振る回し方の符号
        Quaternion qr = QuaternionFromAxisAngle(ToF(axis), (float)(PI * w - yaw * north));
        return QuaternionMultiply(qr, q0);
    };
    if (model == PM_SATURN_V || model == PM_SATURN_SII_RING || model == PM_SATURN_SII_LES || model == PM_SATURN_SII || model == PM_SATURN_SIVB) {
        if (fixedBody >= 0) {
            V3 vi = v + ApolloSpinVel(o, rel) * ApolloSmooth((Len(rel) - o.d.radius - 20) / 80);   // 高度 20〜100 km で慣性の速度へ
            fwd = blend(up, Len(vi) > 1e-9 ? Norm(vi) : up, std::min(1.0, speed / 0.05));   // 発射台では真上
        }
        // 真上を向いている間（10° まで）は上を西へ
        V3 east = Cross(o.pole, up);
        east = Len(east) > 1e-9 ? Norm(east) : V3(1, 0, 0);
        double s = ApolloSmooth(std::acos(std::max(-1.0, std::min(1.0, Dot(fwd, up)))) / (10 * DEG));
        hint = blend(east * -1.0, hint, s);
    } else if (model == PM_APOLLO_LM || model == PM_LM_STOWED || model == PM_LM_ASCENT) {
        // 月着陸船は天頂の側（-Z）を噴射の反対へ
        if (fixedBody >= 0) {
            double w = std::min(1.0, speed / 0.15);
            bool descending = model != PM_LM_ASCENT;   // 上昇段は上昇だけ（軌道に入る所で上下の速さが 0 を切っても後ろを向かない）
            fwd = descending ? blend(up, vdir * -1.0, w * 0.85) : blend(up, vdir, w * 0.8);
        } else {
            fwd = up;
        }
        // 横は進む向きへ（月面では水平に進む向き。止まっている間に速度の誤差で回らないように）
        V3 side = fixedBody >= 0 ? ApolloGroundDir(app, i, t, v, up) : vdir;
        hint = Len(Cross(fwd, side)) > 1e-6 ? side : V3(1, 0, 0);
    } else if (model == PM_APOLLO_CM || model == PM_CM_DROGUE || model == PM_CM_MAIN) {
        int mission, vehicle;
        double since = ApolloVehicleOf(app, i, &mission, &vehicle) ? (t - ApolloModelSince(APOLLO_MISSIONS[mission], vehicle, t)) * 86400 : 1e9;
        if (model != PM_APOLLO_CM || (fixedBody >= 0 && speed < 0.05)) {
            fwd = up;   // パラシュートでは先端が上
            Quaternion qs = shieldFwd(1);
            if (Len(Cross(fwd, hint)) < 1e-6) hint = Len(Cross(fwd, Y_UP)) > 1e-6 ? Y_UP : V3(1, 0, 0);
            Quaternion q = QuatFromBasis(fwd, hint);
            // ドローグが開いた時は、耐熱シールドを前へ向けた向きから 8 秒かけて立てる
            if (model == PM_CM_DROGUE && since < 8) q = QuaternionSlerp(qs, q, (float)ApolloSmooth(since / 8));
            return q;
        }
        if (fixedBody >= 0) return shieldFwd(1.0);
        // 機械船を切り離した後は、切り離しの向きから振った分を戻す。13 号は月着陸船を切り離してから 3 分で耐熱シールドを前へ
        bool stack = true;
        if (since < 1e8) ApolloSmSepT(APOLLO_MISSIONS[mission], &stack);
        return stack ? shieldFwd(ApolloSmooth(since / 180)) : shieldFwd(1.0, SMSEP_YAW * (1 - ApolloSmooth(since / SMSEP_BACK)));
    } else if (model == PM_APOLLO_CSM && fixedBody < 0) {
        // 機械船を切り離す前: 切り離しの向きへ回る
        int mission, vehicle;
        if (ApolloVehicleOf(app, i, &mission, &vehicle) && vehicle == 0) {
            double s = (t - ApolloSmSepT(APOLLO_MISSIONS[mission])) * 86400 + SMSEP_TURN0;
            if (s > 0 && s <= SMSEP_TURN0) {
                double w = ApolloSmooth(s / SMSEP_TURN);
                return shieldFwd(w, SMSEP_YAW * w);
            }
        }
    }
    if (Len(Cross(fwd, hint)) < 1e-6) hint = Len(Cross(fwd, Y_UP)) > 1e-6 ? Y_UP : V3(1, 0, 0);
    return QuatFromBasis(fwd, hint);
}

// 時刻 t の噴射の向き（推力の向き、描画座標）
static V3 ApolloThrustDir(const ApolloBurn& bn, double t) {
    const std::vector<double>& T = bn.dt;
    if (T.size() == 1 || t <= T.front()) return bn.dir.front();
    if (t >= T.back()) return bn.dir.back();
    size_t k = 1;
    while (k + 1 < T.size() && T[k] < t) k++;
    double u = (t - T[k - 1]) / std::max(1e-12, T[k] - T[k - 1]);
    V3 d = bn.dir[k - 1] * (1 - u) + bn.dir[k] * u;
    return Len(d) > 1e-9 ? Norm(d) : bn.dir[k];
}

// 時刻 t に噴射している噴射（無ければ nullptr）
static const ApolloBurn* ApolloBurnAt(const BodyDef& d, double t) {
    if (d.apollo < 0 || d.apollo >= (int)g_apollo.size()) return nullptr;
    for (const ApolloBurn& bn : g_apollo[d.apollo].burns)
        if (bn.t0 <= t && t <= bn.t1) return &bn;
    return nullptr;
}

// 月着陸船を引き出す手順（ApolloTde）の、第3段に対する司令船の動き。第3段の座標（第3段の機体の座標。原点は底、-Z が前）で、
// 司令船の原点は離れる前は -30.95 m（アダプターの上 27.2 m に機械船の底）、ドッキングした形では -31.55 m（第3段の中の
// 月着陸船の上の口 24.2 m に、向きを変えた司令船の先端）
static const double TDE_CSM_Z0 = -30.95, TDE_CSM_ZDOCK = -31.55;
static const double TDE_UNDO_DELAY = 60, TDE_UNDO_SEC = 240;   // 引き出した後、1 分たってから 4 分かけて普段の向きへ戻す

static double ApolloHermite(double t0, double p0, double m0, double t1, double p1, double m1, double t) {
    double h = t1 - t0, u = (t - t0) / h, u2 = u * u, u3 = u2 * u;
    return (2 * u3 - 3 * u2 + 1) * p0 + (u3 - 2 * u2 + u) * h * m0 + (-2 * u3 + 3 * u2) * p1 + (u3 - u2) * h * m1;
}

// 手順の時刻（離れてからの秒）。ドッキングしない 8 号のドッキングと引き出しは無限大。release は第3段から離れて、
// 普段の向きへ戻り始める時（引き出し。8 号は第3段を描くのをやめる時）
static double ApolloTdeSec(const ApolloMission& m, const char* get) {
    return get ? (ApolloT(m, get) - ApolloT(m, m.tde->sep)) * 86400 : INFINITY;
}
static double ApolloTdeRelease(const ApolloMission& m) { return ApolloTdeSec(m, m.tde->eject ? m.tde->eject : m.tde->end); }

// 離れてから s 秒後の、司令船が第3段から前へ離れた距離 m（第3段の座標で、司令船の原点が TDE_CSM_Z0 から -Z へ動いた量）。
// 離れる速さで離れ始め、向きを変え終わる頃に最も離れ、接触の速さで最初の接触へ。やり直しがあれば決まった時刻の距離を
// つなぎ（接触へは接触の速さで）、ドッキングの後は引き出すまでそのまま、引き出した後は離れる速さで遠ざかる。
// ドッキングしない 8 号は最も離れた所で編隊飛行を続ける
static double ApolloTdeDist(const ApolloMission& m, double s) {
    const ApolloTde& d = *m.tde;
    double dDock = TDE_CSM_Z0 - TDE_CSM_ZDOCK, dockS = ApolloTdeSec(m, d.dock), ejectS = ApolloTdeSec(m, d.eject);
    double contactS = d.keyCount > 0 ? ApolloTdeSec(m, d.keys[0].get) : dockS;
    double turnEnd = d.turnStart + 180 / d.turnRate;
    double tMax = d.dMax > 0 ? std::max(turnEnd, d.dMax / d.vSep * 1.2) : turnEnd;
    tMax = std::min(tMax, contactS * 0.5);
    double dMax = d.dMax > 0 ? d.dMax : d.vSep * tMax * 0.5;
    if (s <= 0) return 0;
    if (s < tMax) return ApolloHermite(0, 0, d.vSep, tMax, dMax, 0, s);
    if (!std::isfinite(contactS)) return dMax;
    if (s < contactS) return ApolloHermite(tMax, dMax, 0, contactS, dDock, -d.vContact, s);
    if (s < dockS) {
        double t0 = contactS, g0 = 0;
        for (int k = 1; k <= d.keyCount; k++) {
            double t1 = k < d.keyCount ? ApolloTdeSec(m, d.keys[k].get) : dockS, g1 = k < d.keyCount ? d.keys[k].gap : 0;
            if (s < t1) return dDock + ApolloHermite(t0, g0, 0, t1, g1, g1 == 0 ? -d.vContact : 0, s);
            t0 = t1;
            g0 = g1;
        }
    }
    if (s < ejectS) return dDock;
    return dDock + d.vEject * (s - ejectS);
}

// 時刻 t の、第3段に対する司令船の向きの回転（離れてから turnStart 秒後に、機体の上の向きへ 180° 起こす）
static double ApolloTdeTurn(const ApolloTde& d, double s) { return PI * ApolloSmooth((s - d.turnStart) / (180 / d.turnRate)); }

static Quaternion ApolloPose(const App& app, int i, int model, double t);
// 離れる時の第3段（と司令船）の向き。第3段はこの向きのまま保つ
static Quaternion ApolloTdeStackPose(const App& app, int i, const ApolloMission& m) {
    return ApolloPose(app, i, PM_SATURN_SIVB, ApolloT(m, m.tde->sep));
}

// 手順の間の司令船の向き（司令船の機体 i、形 model）。手順の外なら q のまま
static Quaternion ApolloTdePose(const App& app, int i, int model, double t, Quaternion q) {
    int mission, vehicle;
    if (model != PM_APOLLO_CSM && model != PM_APOLLO_CSM_LM) return q;
    if (!ApolloVehicleOf(app, i, &mission, &vehicle) || vehicle != 0 || !APOLLO_MISSIONS[mission].tde) return q;
    const ApolloMission& m = APOLLO_MISSIONS[mission];
    double s = (t - ApolloT(m, m.tde->sep)) * 86400, se = ApolloTdeRelease(m);
    if (s <= 0 || s >= se + TDE_UNDO_DELAY + TDE_UNDO_SEC) return q;
    Quaternion qs = ApolloTdeStackPose(app, i, m);
    auto pitched = [&](double a) { return QuaternionMultiply(qs, QuaternionFromAxisAngle({1, 0, 0}, (float)a)); };
    if (s < se) return pitched(ApolloTdeTurn(*m.tde, s));
    // 引き出した後（8 号は第3段を描くのをやめた後）: 起こした向きを戻しながら（半回転を決まった軸のまわりに）、普段の向きへ移す。
    // QuaternionSlerp は 2 つの内積が負になると片方の符号を返して近い側を回るので、戻している途中で内積の符号が変わると
    // 1 フレームで向きが跳ぶ。符号は戻し始めの普段の向きに合わせて決め、補間の間は返さない
    double w = ApolloSmooth((s - se - TDE_UNDO_DELAY) / TDE_UNDO_SEC);
    Quaternion a = pitched(PI * (1 - w));
    Quaternion q0 = ApolloCoastPose(app, i, model, ApolloT(m, m.tde->sep) + (se + TDE_UNDO_DELAY) / 86400);
    auto dot = [](Quaternion x, Quaternion y) { return (double)x.x * y.x + (double)x.y * y.y + (double)x.z * y.z + (double)x.w * y.w; };
    if (dot(q, q0) < 0) q = {-q.x, -q.y, -q.z, -q.w};                  // q を戻し始めの向きと同じ側に
    if (dot(pitched(PI), q0) < 0) a = {-a.x, -a.y, -a.z, -a.w};         // 起こした向きも同じ側に
    double c = std::max(-1.0, std::min(1.0, dot(a, q))), th = std::acos(c);
    if (th < 1e-6) return q;
    double ka = std::sin((1 - w) * th) / std::sin(th), kb = std::sin(w * th) / std::sin(th);
    return QuaternionNormalize({(float)(a.x * ka + q.x * kb), (float)(a.y * ka + q.y * kb), (float)(a.z * ka + q.z * kb), (float)(a.w * ka + q.w * kb)});
}

// 月を回る軌道での切り離しとドッキング（ApolloLunarDock）。手順書の値: 待つ距離 m（40 ft）、近づく速さ m/s（0.1 ft/s）、
// 切り離した後に待つ距離まで離れる秒、月着陸船が回る秒。RDV_PORT: ドッキングした時の月着陸船の原点（接地点）の、司令船の
// 原点からの距離 km（ship_model.h のドッキングした形と同じ）
static const double RDV_HOLD = 12.2, RDV_VC = 0.03, RDV_OUT = 60, RDV_TURN = 120, RDV_PORT = 0.01275;
static const double RDV_BLEND = 600;   // ドッキングの前、記録の軌跡から組み立てた距離へ移す秒（最初の記録の距離まで）

// ドッキングの前の 2 機の間（ドッキングした位置からの距離 m）の時刻と距離。最後は接触で 0
struct ApolloRdvKeys { int n = 0; double t[24], gap[24]; };
static ApolloRdvKeys ApolloRdvProfile(const ApolloMission& m) {
    const ApolloLunarDock& d = *m.lunar;
    ApolloRdvKeys k;
    double tc = ApolloT(m, d.contact ? d.contact : d.dock);
    auto add = [&](double t, double gap) { k.t[k.n] = t; k.gap[k.n++] = gap; };
    if (d.keyCount == 0) {   // 記録が無い号: 40 ft で 5 分待ち、0.1 ft/s で近づく
        add(tc - (RDV_HOLD / RDV_VC + 300) / 86400, RDV_HOLD);
        add(tc - RDV_HOLD / RDV_VC / 86400, RDV_HOLD);
    }
    for (int j = 0; j < d.keyCount && k.n < 23; j++) add(ApolloT(m, d.keys[j].get), d.keys[j].gap);
    add(tc, 0);
    return k;
}
static double ApolloRdvGap(const ApolloRdvKeys& k, double t) {
    if (t <= k.t[0]) return k.gap[0];
    for (int j = 1; j < k.n; j++)
        if (t <= k.t[j]) return k.gap[j - 1] + (k.gap[j] - k.gap[j - 1]) * (t - k.t[j - 1]) / (k.t[j] - k.t[j - 1]);
    return 0;
}
// 上昇段がドッキングの向きへ回り始める時刻（記録が無ければ、最後に近づき始める 150 秒前）
static double ApolloRdvTurn(const ApolloMission& m, const ApolloRdvKeys& k) {
    return m.lunar->turn ? ApolloT(m, m.lunar->turn) : k.t[k.n - 2] - 150.0 / 86400;
}
// 切り離した後、月着陸船が回り始めるまでの秒
static double ApolloUndockTurnSec(const ApolloMission& m) {
    return m.lunar->undockTurn ? (ApolloT(m, m.lunar->undockTurn) - ApolloT(m, m.lunar->undock)) * 86400 : 60;
}

// 月着陸船の向き（機体 i、形 model）: 切り離した時と接触した時は、ドッキングした形と同じく天頂の側を司令船へ向けた向き
// （司令船の向きを上下の軸のまわりに半回転）。切り離した後は回り始めてから 2 分かけて普段の向き（天頂の側を月から外へ、
// 前の窓を後ろの司令船へ）へ回り、ドッキングの前は回り始めてから 2 分かけてその逆に回る。それ以外は q のまま
static Quaternion ApolloRdvPose(const App& app, int i, int model, double t, Quaternion q) {
    int mission, vehicle;
    if (model == PM_LM_STOWED && ApolloVehicleOf(app, i, &mission, &vehicle) && vehicle == 1) {
        // 13 号: 切り離した時のつながっていた向きのまま（AGS の姿勢保持にしてから切り離した。Apollo Flight Journal）
        const ApolloMission& m = APOLLO_MISSIONS[mission];
        double tj = ApolloLmJettT(m);
        int c = ApolloBody(app, mission, 0);
        if (!std::isfinite(tj) || t < tj || c < 0) return q;
        return QuaternionMultiply(ApolloPose(app, c, PM_APOLLO_CM_LM, tj), QuaternionFromAxisAngle({0, 1, 0}, PI));
    }
    if (model != PM_APOLLO_LM && model != PM_LM_ASCENT) return q;
    if (!ApolloVehicleOf(app, i, &mission, &vehicle) || vehicle != 1 || !APOLLO_MISSIONS[mission].lunar) return q;
    const ApolloMission& m = APOLLO_MISSIONS[mission];
    auto aligned = [&]() {
        int c = ApolloBody(app, mission, 0), mc = ApolloModel(m, 0, t);
        if (c < 0 || mc < 0) return q;
        return QuaternionMultiply(ApolloPose(app, c, mc, t), QuaternionFromAxisAngle({0, 1, 0}, PI));
    };
    if (model == PM_APOLLO_LM) {
        double s = (t - ApolloT(m, m.lunar->undock)) * 86400, ts = ApolloUndockTurnSec(m);
        if (s < 0 || s > ts + RDV_TURN) return q;
        return QuaternionSlerp(aligned(), q, (float)ApolloSmooth((s - ts) / RDV_TURN));
    }
    ApolloRdvKeys k = ApolloRdvProfile(m);
    double t0 = ApolloRdvTurn(m, k);
    if (t < t0 || t > ApolloT(m, m.lunar->dock)) return q;
    return QuaternionSlerp(q, aligned(), (float)ApolloSmooth((t - t0) * 86400 / RDV_TURN));
}

// 機体の向き（描画の回転）。主エンジンの噴射の間はエンジンを推力の反対へ向ける（減速の噴射では先端が後ろを向く）。
// 噴射の 4 分前から向きを変え始め、噴射の後 4 分かけて元の向きへ戻る（宇宙船は噴射の前に噴射の向きへ回っておく）。
// 打ち上げ・月面への降下と上昇は、その動きの向き（ApolloCoastPose）の始まりと終わりへ同じようにつなぐ
static Quaternion ApolloPose(const App& app, int i, int model, double t) {
    Quaternion q = ApolloRdvPose(app, i, model, t, ApolloTdePose(app, i, model, t, ApolloCoastPose(app, i, model, t)));
    const BodyDef& d = app.B[i].d;
    if (d.apollo < 0 || d.apollo >= (int)g_apollo.size()) return q;
    const double turn = 240.0 / 86400, sec = 1.0 / 86400;
    for (const ApolloBurn& bn : g_apollo[d.apollo].burns) {
        if (bn.engine == AP_ENG_RCS || bn.engine == AP_ENG_LM_RCS) continue;   // 小さな噴射口は向きを変えずに噴く
        if (t < bn.t0 - turn || t > bn.t1 + turn) continue;
        if (bn.dir.empty() && t >= bn.t0 && t <= bn.t1) continue;              // 動力飛行の間は ApolloCoastPose のまま
        double tt = bn.t1 - bn.t0 > 2 * sec ? std::max(bn.t0 + sec, std::min(bn.t1 - sec, t)) : (bn.t0 + bn.t1) / 2;
        Quaternion qb;
        if (bn.dir.empty()) {
            qb = ApolloCoastPose(app, i, model, tt);
        } else {
            // 機体の上は噴射していない時の向きの上に合わせる
            V3 up = FromF(Vector3RotateByQuaternion({0, 1, 0}, q)), f = ApolloThrustDir(bn, tt);
            if (bn.engine == AP_ENG_LM_DPS) f = f * -1.0;   // つながった月着陸船の下降エンジンは先端（-Z）の側にある
            qb = QuatFromBasis(f, Len(Cross(f, up)) > 1e-6 ? up : V3(1, 0, 0));
        }
        double w = t < bn.t0 ? ApolloSmooth((t - (bn.t0 - turn)) / turn) : t > bn.t1 ? ApolloSmooth(1 - (t - bn.t1) / turn) : 1.0;
        q = QuaternionSlerp(q, qb, (float)w);
    }
    return q;
}
static Quaternion ApolloPose(const App& app, int i, int model) { return ApolloPose(app, i, model, app.tDays); }

// 月を回る軌道での切り離しとドッキングの間の、月着陸船を描く位置（司令船の位置 cp と先端の向き nose、記録の軌跡の月着陸船の
// 位置 lp、時刻 t から）。間でなければ false。
// 切り離した後: ドッキングの口から離れ、1 分かけて 40 ft 先で止まって待つ。記録の軌跡の 2 機が 50〜500 m 離れる間に軌跡の位置へ移す
// （10〜12 号は司令船の分離の噴射の後、14〜17 号は切り離してすぐ離れていく）。
// ドッキングの前: 最初の記録の距離の 10 分前から、軌跡の距離を記録の距離へ移し（RDV_BLEND。距離の比で移すので、近いほど
// ゆっくり近づく）、向きも 2 km から 100 m の間に軌跡の向きから司令船の先端の先へ移す
static bool ApolloRdvPos(const ApolloMission& m, V3 cp, V3 nose, V3 lp, double t, V3* out) {
    if (!m.lunar) return false;
    V3 rel = lp - cp;
    double r = Len(rel), tDock = ApolloT(m, m.lunar->dock);
    ApolloRdvKeys k = ApolloRdvProfile(m);
    double tb0 = k.t[0] - RDV_BLEND / 86400;
    if (t >= tb0 && t <= tDock) {
        double w = ApolloSmooth((t - tb0) * 86400 / RDV_BLEND);
        double rp = RDV_PORT + ApolloRdvGap(k, t) * 0.001, rd = std::pow(std::max(r, 1e-6), 1 - w) * std::pow(rp, w);
        double sw = w * ApolloSmooth((2.0 - rd) / 1.9);
        V3 los = r > 1e-9 ? rel * (1 / r) : nose, d = los * (1 - sw) + nose * sw;
        *out = cp + (Len(d) > 1e-9 ? Norm(d) : nose) * rd;
        return true;
    }
    double s = (t - ApolloT(m, m.lunar->undock)) * 86400;
    if (s < 0 || t > tDock) return false;
    double b = ApolloSmooth((r - 0.05) / 0.45);
    if (b >= 1) return false;
    V3 syn = nose * (RDV_PORT + RDV_HOLD * 0.001 * ApolloSmooth(s / RDV_OUT));
    *out = cp + syn * (1 - b) + rel * b;
    return true;
}

// 司令船のすぐそば（100 m 以内）にいる月着陸船を、ドッキングの口の位置へ寄せる。同じ道をたどる間（切り離した直後）は
// 2 機の軌跡が同じ点で、中心どうしが重なって機体がめり込み、ドッキングの直前も上昇段が司令船の中心へ入っていくため。
// 記録には数 m の位置の情報はないので、つながっていた時の位置（月着陸船の接地点が司令船の先端の先 12.75 m、
// 機械船を切り離した 13 号は司令船の先 7.5 m）から離れ始め、20〜100 m で軌跡の位置に戻す
// 13 号の月着陸船は、つながっていた位置から、切り離した時の司令船の先端の向きへ秒速 0.67 m で離れる（Mission Report の
// 切り離しの速度の変化: 司令船 -1.54 ft/s、月着陸船 -0.65 ft/s、それぞれの X 軸の向き）。記録の軌跡の 2 機が 50〜500 m 離れる
// 間に軌跡の位置へ移す
static const double LMJ_V = 0.000667, LMJ_SHOW = 1200;
static V3 ApolloLmJettPos(V3 cp, V3 nose, V3 lp, double s) {
    V3 rel = lp - cp;
    double b = ApolloSmooth((Len(rel) - 0.05) / 0.45);
    return cp + nose * ((0.0075 + LMJ_V * s) * (1 - b)) + rel * b;
}
static void ApolloDockPort(App& app) {
    for (int k = 0; k < APOLLO_COUNT; k++) {
        int c = ApolloBody(app, k, 0), l = ApolloBody(app, k, 1);
        if (c < 0 || l < 0 || !app.B[c].active || !app.B[l].active) continue;
        int mc = ApolloModel(APOLLO_MISSIONS[k], 0, app.tDays), ml = ApolloModel(APOLLO_MISSIONS[k], 1, app.tDays);
        if (ml < 0 || (mc != PM_APOLLO_CSM && mc != PM_APOLLO_CM)) continue;
        double tj = ApolloLmJettT(APOLLO_MISSIONS[k]);
        if (std::isfinite(tj) && app.tDays >= tj) {
            V3 n0 = FromF(Vector3RotateByQuaternion({0, 0, -1}, ApolloPose(app, c, PM_APOLLO_CM_LM, tj)));
            app.B[l].pos = ApolloLmJettPos(app.B[c].pos, n0, app.B[l].pos, (app.tDays - tj) * 86400);
            continue;
        }
        V3 nose = FromF(Vector3RotateByQuaternion({0, 0, -1}, ApolloPose(app, c, mc)));
        if (ApolloRdvPos(APOLLO_MISSIONS[k], app.B[c].pos, nose, app.B[l].pos, app.tDays, &app.B[l].pos)) continue;
        double w = ApolloSmooth((0.1 - Len(app.B[l].pos - app.B[c].pos)) / 0.08);
        if (w <= 0) continue;
        app.B[l].pos = app.B[l].pos + nose * ((mc == PM_APOLLO_CSM ? 0.01275 : 0.0075) * w);
    }
}

// 月着陸船を引き出す手順: 司令船の軌跡の点を、離れる前の第3段の底（サターンの形の原点）から司令船の原点へ移す量 km
// （第3段の前の向きへ）。離れた瞬間に機体が 31 m 飛ばないように、離れてからドッキングまでは離れる前の位置のままにし、
// つながっている間に滑らかに軌跡の点へ戻す（記録の軌跡は数 km の精度で、31 m のずれは誤差より小さい）。ドッキングしない
// 8 号は、向きを変え終わってから第3段を描くのをやめるまでの間に戻す
static double ApolloTdeShift(const ApolloMission& m, double t) {
    double s = (t - ApolloT(m, m.tde->sep)) * 86400, s0 = ApolloTdeSec(m, m.tde->dock), s1 = ApolloTdeSec(m, m.tde->eject);
    if (!m.tde->dock) {
        s0 = m.tde->turnStart + 180 / m.tde->turnRate;
        s1 = ApolloTdeSec(m, m.tde->end);
    }
    if (s < 0 || s > s1) return 0;
    return -TDE_CSM_Z0 * 0.001 * (s < s0 ? 1.0 : 1 - ApolloSmooth((s - s0) / (s1 - s0)));
}

// サターンの段の切り離し: 第1段（S-IC）・段間リング・第2段（S-II）を切り離した時刻（形の切り替えの表から。無ければ false）
static bool ApolloStagingTimes(const ApolloMission& m, double* tA, double* tR, double* tB) {
    *tA = *tR = *tB = NAN;
    for (int k = 0; k < m.shapeCount; k++) {
        if (m.shapes[k].vehicle != 0) continue;
        double t = ApolloT(m, m.shapes[k].get);
        if (m.shapes[k].model == PM_SATURN_SII_RING) *tA = t;
        if (m.shapes[k].model == PM_SATURN_SII_LES) *tR = t;
        if (m.shapes[k].model == PM_SATURN_SIVB) *tB = t;
    }
    return std::isfinite(*tA) && std::isfinite(*tR) && std::isfinite(*tB);
}
// 切り離した瞬間に残った機体が跳ばないように、軌跡の点（形の原点）を前へずらす量 km。第1段を切り離すとサターンの形の
// 原点が第1段のノズルの下端から段間リングの底へ 47.9 m、第2段を切り離すと第3段の底へ 36.0 m 移るので、その分を前へずらし、
// 次の切り離しまで（第2段の後は 1 時間で）滑らかに軌跡の点へ戻す（記録の軌跡は数 km の精度で、この分は誤差より小さい）
static const double STAGE_SHIFT_A = 0.0479, STAGE_SHIFT_B = 0.0360;
static double ApolloStageShift(const ApolloMission& m, double t) {
    double tA, tR, tB;
    if (!ApolloStagingTimes(m, &tA, &tR, &tB) || t < tA) return 0;
    if (t < tB) return STAGE_SHIFT_A * (1 - ApolloSmooth((t - tA) / (tB - tA)));
    return STAGE_SHIFT_B * (1 - ApolloSmooth((t - tB) * 86400 / 3600));
}
// 機械船を切り離した瞬間に司令船が跳ばないように、司令船の軌跡の点を切り離した時の先端の向きへずらす量 km。司令・機械船の形の
// 原点は機械船の真ん中（司令船の底は 3.75 m 前）、司令船だけの形の原点は耐熱シールドの 1.5 m 後ろなので、5.25 m 前へずらし、
// 切り離して 1 分後から 7 分かけて軌跡の点へ戻す（記録の軌跡は数 km の精度で、この分は誤差より小さい）
static const double SMSEP_SHIFT = 0.00525;
static double ApolloSmShift(const ApolloMission& m, double t) {
    double s = (t - ApolloSmSepT(m)) * 86400;
    if (!(s >= 0)) return 0;
    return SMSEP_SHIFT * (1 - ApolloSmooth((s - 60) / 420));
}
// 機械船を切り離した時の司令船（13 号は月着陸船とつながった司令船）の向き
static Quaternion ApolloSmSepPose(const App& app, int c, const ApolloMission& m) {
    double ts = ApolloSmSepT(m);
    return ApolloPose(app, c, ApolloModel(m, 0, ts), ts);
}

// 司令船の機体（サターンもこの機体）の位置を、段の切り離しと月着陸船を引き出す手順と機械船の切り離しの分だけずらす
static void ApolloTdeShiftBodies(App& app) {
    for (int k = 0; k < APOLLO_COUNT; k++) {
        const ApolloMission& m = APOLLO_MISSIONS[k];
        int c = ApolloBody(app, k, 0);
        if (c < 0 || !app.B[c].active) continue;
        double o = m.tde ? ApolloTdeShift(m, app.tDays) : 0;
        if (o > 0) app.B[c].pos = app.B[c].pos + FromF(Vector3RotateByQuaternion({0, 0, -1}, ApolloTdeStackPose(app, c, m))) * o;
        double os = ApolloStageShift(m, app.tDays);
        int model = ApolloModel(m, 0, app.tDays);
        if (os > 0 && model >= 0) app.B[c].pos = app.B[c].pos + FromF(Vector3RotateByQuaternion({0, 0, -1}, ApolloPose(app, c, model, app.tDays))) * os;
        double oc = ApolloSmShift(m, app.tDays);
        if (oc > 0) app.B[c].pos = app.B[c].pos + FromF(Vector3RotateByQuaternion({0, 0, -1}, ApolloSmSepPose(app, c, m))) * oc;
    }
}

// 引き出した後（8 号は離れた後）の司令船の噴射（第3段は噴射していないので、その分だけ 2 機が離れる）。速度の変化 km/日は、
// 噴射の前後の速度の差から、その間の地球の重力の分を引いたもの。記録の軌跡に入っていない噴射（kickGet）は、地球から外向きに
// kickDv の一瞬の噴射
struct ApolloTdeKick { double t0, t1; V3 dv; };
static const std::vector<ApolloTdeKick>& ApolloTdeKicks(const App& app, int mission) {
    static std::vector<ApolloTdeKick> cache[APOLLO_COUNT];
    static bool done[APOLLO_COUNT];
    if (done[mission]) return cache[mission];
    done[mission] = true;
    const ApolloMission& m = APOLLO_MISSIONS[mission];
    int c = ApolloBody(app, mission, 0);
    int tr = c >= 0 ? app.B[c].d.apollo : -1;
    if (tr < 0 || tr >= (int)g_apollo.size()) return cache[mission];
    double te = ApolloT(m, m.tde->eject ? m.tde->eject : m.tde->sep), tEnd = ApolloT(m, m.tde->end), h = 1.0 / 86400;
    if (m.tde->kickGet) {
        double tk = ApolloT(m, m.tde->kickGet);
        V3 out = Norm(PosAt(app.B, c, tk) - PosAt(app.B, B_EARTH, tk));
        cache[mission].push_back({tk, tk, out * (m.tde->kickDv * 0.001 * 86400)});
    }
    for (const ApolloBurn& bn : g_apollo[tr].burns) {
        if (bn.t0 < te || bn.t0 > tEnd) continue;
        V3 v0 = ApolloVel(app, c, bn.t0 - h), v1 = ApolloVel(app, c, bn.t1 + h);
        double tm = (bn.t0 + bn.t1) / 2;
        V3 r = PosAt(app.B, c, tm) - PosAt(app.B, B_EARTH, tm);
        double rl = Len(r);
        V3 g = r * (-398600.4418 / (rl * rl * rl) * 86400.0 * 86400.0);   // km/日²
        cache[mission].push_back({bn.t0, bn.t1, v1 - v0 - g * (bn.t1 - bn.t0 + 2 * h)});
    }
    return cache[mission];
}

// 引き出した後の第3段を描く時の、手順の第3段と捨てたアダプターの板
struct ApolloTdeView {
    V3 sivb;            // 第3段の原点（底）の位置
    Quaternion q;       // 第3段の向き
    int model;          // PM_SIVB_LM か PM_SIVB_EMPTY（月着陸船を積まない 8 号は重りを描かずに PM_SIVB_EMPTY）
    double camW;        // カメラを 2 機が入る見方へ移す割合
    V3 camTarget;       // その見方で見る点（2 機の間）
    double camSize;     // 2 機を入れる大きさ km
    double panelS;      // 離れてからの秒（板を描く間）。板を描かない時は負
};
static const double TDE_PANEL_SEC = 120;   // 捨てた板を描く間（秒速 2.5 m で 300 m 離れる）
static const double TDE_VIEW_END = 60;     // 2 機を入れる見方は、手順の終わり（第3段が回避の向きへ回り始める）の 1 分前から戻す

// 離れてからの秒 s の、司令船に対する第3段の原点のずれ（第3段の向き fwd の、機体の座標 km）
static V3 ApolloTdeOffset(const App& app, int mission, V3 fwd, double t) {
    const ApolloMission& m = APOLLO_MISSIONS[mission];
    double s = (t - ApolloT(m, m.tde->sep)) * 86400;
    V3 disp;
    for (const ApolloTdeKick& k : ApolloTdeKicks(app, mission)) {
        if (t <= k.t0) continue;
        double dur = k.t1 - k.t0;
        disp = disp + k.dv * (t < k.t1 && dur > 0 ? (t - k.t0) * (t - k.t0) / (2 * dur) : (t - k.t1) + dur / 2);
    }
    return fwd * ((TDE_CSM_Z0 - ApolloTdeDist(m, s)) * 0.001) - disp;
}

// 時刻 t の、カメラを 2 機が入る見方へ移す割合（離れる前の 20 秒で 0 から 1 へ。2 機の間が 150〜400 m の間と、
// 手順の終わりの 1 分前から 0 へ戻す）
static double ApolloTdeCamW(const App& app, int mission, double t) {
    const ApolloMission& m = APOLLO_MISSIONS[mission];
    if (!m.tde) return 0;
    int c = ApolloBody(app, mission, 0);
    if (c < 0) return 0;
    double ts = ApolloT(m, m.tde->sep), tEnd = ApolloT(m, m.tde->end), lead = 20.0 / 86400;
    if (t < ts - lead || t > tEnd) return 0;
    if (t < ts) return ApolloSmooth((t - (ts - lead)) / lead);
    V3 fwd = FromF(Vector3RotateByQuaternion({0, 0, -1}, ApolloTdeStackPose(app, c, m)));
    double gap = Len(ApolloTdeOffset(app, mission, fwd, t) - fwd * 0.012);
    return (1 - ApolloSmooth((gap - 0.15) / 0.25)) * (1 - ApolloSmooth((t - (tEnd - TDE_VIEW_END / 86400)) * 86400 / TDE_VIEW_END));
}

// 時刻 t の手順の様子（ApolloTdeShiftBodies の後の司令船の位置から）。手順の外なら false
static bool ApolloTdeAt(const App& app, int mission, ApolloTdeView* v) {
    const ApolloMission& m = APOLLO_MISSIONS[mission];
    if (!m.tde) return false;
    int c = ApolloBody(app, mission, 0);
    if (c < 0 || !app.B[c].active) return false;
    double t = app.tDays, ts = ApolloT(m, m.tde->sep);
    double tEnd = ApolloT(m, m.tde->end), lead = 20.0 / 86400;
    if (t < ts - lead || t > tEnd) return false;
    v->q = ApolloTdeStackPose(app, c, m);
    V3 fwd = FromF(Vector3RotateByQuaternion({0, 0, -1}, v->q));
    v->panelS = t >= ts && t < ts + TDE_PANEL_SEC / 86400 ? (t - ts) * 86400 : -1;
    v->camW = ApolloTdeCamW(app, mission, t);
    if (t < ts) {
        // 離れる前の 20 秒: サターン（第3段と司令船）の真ん中を、2 機を入れる見方で見始める
        v->model = -1;
        v->sivb = app.B[c].pos;
        v->camTarget = v->sivb + fwd * 0.0215;
        v->camSize = 0.044;
        return true;
    }
    double s = (t - ts) * 86400;
    v->sivb = app.B[c].pos + ApolloTdeOffset(app, mission, fwd, t);
    v->model = s < ApolloTdeSec(m, m.tde->dock) && m.tde->dock ? PM_SIVB_LM : PM_SIVB_EMPTY;
    // 2 機の間（第3段は底から 12 m の所）を、離れた距離に合わせた大きさで見る。引き出した後（8 号は離れる噴射の後）、
    // 150〜400 m 離れる間と手順の終わりの前に、司令船だけの見方へ戻す（ApolloTdeCamW）
    V3 mid = v->sivb - fwd * 0.012;
    double gap = Len(mid - app.B[c].pos);
    v->camTarget = (mid + app.B[c].pos) * 0.5;
    v->camSize = gap + 0.025;
    return true;
}

// 場面に描く、機体から離れていく部品（月着陸船を引き出す手順の第3段とアダプターの板、切り離したサターンの段）と、
// 機体と部品が入るカメラの見方
struct ApolloPiece { int pm; V3 pos; Quaternion q; };
struct ApolloPieces {
    std::vector<ApolloPiece> parts;
    Quaternion axis;    // 横から見る時の軸の向き（-Z が前）
    double camW = 0;    // カメラを機体と部品が入る見方へ移す割合
    V3 camTarget;       // その見方で見る点
    double camSize = 0; // 入れる大きさ km
    bool pair = false;  // 司令船と月着陸船の見方（どちらを追っている時も使う）
};

// 切り離したサターンの段の、地球の重力だけでの動き（切り離した瞬間の軌跡の位置と速度から、4 次のルンゲ・クッタで）。
// 第1段は 67 km の高さで秒速 2.8 km と空気が薄く、離れていく間（数十秒）の空気の抵抗は第2段の加速に比べて小さいので
// 入れない。逆噴射のロケット（段を後ろへ）は aRetro m/s² を tRetro 秒。r, v は地球の中心からの km と km/s
static V3 ApolloStagePos(const App& app, int c, double t0, double t, V3 fwd, double aRetro, double tRetro) {
    const double GM = 398600.4418, h = 0.5 / 86400;
    V3 r = PosAt(app.B, c, t0) - PosAt(app.B, B_EARTH, t0);
    V3 v = (PosAt(app.B, c, t0 + h) - PosAt(app.B, B_EARTH, t0 + h) - PosAt(app.B, c, t0 - h) + PosAt(app.B, B_EARTH, t0 - h)) * (1.0 / (2 * h * 86400));
    // 逆噴射の間とその後を分けて積分する（刻みの途中で推力が切れると、刻みの数が変わるたびに位置が跳ぶ）
    auto run = [&](double T, double retro) {
        int n = std::max(1, (int)std::ceil(T / 1.0));
        double dt = T / n;
        auto acc = [&](V3 p) {
            double rl = Len(p);
            return p * (-GM / (rl * rl * rl)) - fwd * (retro * 0.001);
        };
        for (int k = 0; k < n; k++) {
            V3 k1v = acc(r), k1r = v;
            V3 k2v = acc(r + k1r * (dt / 2)), k2r = v + k1v * (dt / 2);
            V3 k3v = acc(r + k2r * (dt / 2)), k3r = v + k2v * (dt / 2);
            V3 k4v = acc(r + k3r * dt), k4r = v + k3v * dt;
            r = r + (k1r + k2r * 2 + k3r * 2 + k4r) * (dt / 6);
            v = v + (k1v + k2v * 2 + k3v * 2 + k4v) * (dt / 6);
        }
    };
    double T = (t - t0) * 86400;
    if (T > 0) run(std::min(T, tRetro), aRetro);
    if (T > tRetro) run(T - tRetro, 0);
    return app.B[B_EARTH].pos + r;
}

// 切り離した段（11 号の Flight Evaluation Report の切り離しの時の質量と、逆噴射のロケットの推力と時間）:
// 第1段 164,381 kg、逆噴射 8 基 × 88,600 lbf × 0.666 秒（秒速 12.8 m 遅くなる。4 基の 15 号は半分）。段間リング 3,982 kg
// （逆噴射なし）。
// 第2段 47,100 kg（第3段との間の段間を含む）、逆噴射 4 基 × 34,810 lbf × 1.52 秒（秒速 20.0 m）。向きは切り離した時のまま
// （回転の記録はない）。描くのは切り離してから 5 分（リングは 2 分）
struct ApolloStageDef { int pm; double dv, tRetro, show, gap0, gap1; };   // gap0, gap1: カメラの見方を移す距離 km
static const ApolloStageDef STAGE_SIC = {PM_SIC_SPENT, 12.8, 0.666, 300, 0.1, 0.3};
static const ApolloStageDef STAGE_RING = {PM_SII_RING_SPENT, 0, 0, 120, 0.03, 0.1};
static const ApolloStageDef STAGE_SII = {PM_SII_SPENT, 20.0, 1.52, 300, 0.1, 0.3};

// 月を回る軌道での切り離しとドッキングを、2 機の横から見る見方（時刻 t の記録の軌跡と ApolloRdvPos の位置から）。切り離す 20 秒
// 前から移り始め、月着陸船が回り終えた 1 分後から戻す。ドッキングの前は 2 機が 400 m から 150 m に近づく間に移り、ドッキングの後
// 1 分で戻す（2 機が 150〜400 m 離れる間も戻す）。見方の外は 0（out があれば、見方を入れる。approach があれば、ドッキングの前の
// 2 機の真ん中どうしの距離 km を入れる。ドッキングの前でなければ負）
static double ApolloRdvView(const App& app, int mission, double t, ApolloPieces* out, double* approach = nullptr) {
    if (approach) *approach = -1;
    const ApolloMission& m = APOLLO_MISSIONS[mission];
    if (!m.lunar) return 0;
    int c = ApolloBody(app, mission, 0), l = ApolloBody(app, mission, 1);
    if (c < 0 || l < 0) return 0;
    double tu = ApolloT(m, m.lunar->undock), td = ApolloT(m, m.lunar->dock), lead = 20.0 / 86400, fade = 60.0 / 86400;
    double uEnd = tu + (ApolloUndockTurnSec(m) + RDV_TURN + 60) / 86400;
    ApolloRdvKeys k = ApolloRdvProfile(m);
    bool und = t >= tu - lead && t <= uEnd + fade, dk = t >= k.t[0] - RDV_BLEND / 86400 && t <= td + fade;
    if (!und && !dk) return 0;
    int mc = ApolloModel(m, 0, t), ml = ApolloModel(m, 1, t);
    if (mc < 0) return 0;
    Quaternion qc = ApolloPose(app, c, mc, t);
    V3 nose = FromF(Vector3RotateByQuaternion({0, 0, -1}, qc)), cp = PosAt(app.B, c, t);
    // 司令船の真ん中と月着陸船の真ん中（月着陸船は接地点から 2.7 m、上昇段は 3.9 m。つながっている間はドッキングした形の位置）
    double cen = und ? 0.0027 : 0.0039;
    V3 cC = cp + nose * 0.0018, cL = cp + nose * (RDV_PORT - cen);
    if (ml >= 0) {
        V3 lp = PosAt(app.B, l, t);
        ApolloRdvPos(m, cp, nose, lp, t, &lp);
        cL = lp + FromF(Vector3RotateByQuaternion({0, 0, -1}, ApolloPose(app, l, ml, t))) * cen;
    }
    double gap = Len(cL - cC), w = 1 - ApolloSmooth((gap - 0.15) / 0.25);
    if (approach && dk && t <= td) *approach = gap;
    if (und) w *= t < tu ? ApolloSmooth((t - (tu - lead)) / lead) : 1 - ApolloSmooth((t - uEnd) / fade);
    else if (t > td) w *= 1 - ApolloSmooth((t - td) / fade);
    if (out && w > 0) {
        out->axis = qc;
        out->camW = w;
        out->camTarget = (cC + cL) * 0.5;
        out->camSize = gap + 0.014;
        out->pair = true;
    }
    return w;
}

// 切り離した機械船の、司令船の軌跡の点からの距離 m（切り離した時の先端の向きの逆へ）と、回った角 rad（離れてから s 秒）。
// 後ろへ離れる噴射は 4 基 × 100 lbf（445 N）を質量 smLb で割った加速度（燃料が減る分、300 秒で 3% ほどは入れない）。1969 年の
// 号（8・10〜12 号）は燃料が尽きるまで（約 300 秒）、13 号からは 25 秒（14 号の Mission Report）。回す噴射は切り離して 2 秒後から、
// 5.5 秒噴いて毎分 15 回転（14 号の Mission Report の図）。2 秒の号はその割合で毎分 5.5 回転（推定）。13 号は月着陸船の噴射で、
// 司令船に対して秒速 0.58 m（Mission Report の速度の変化: 月着陸船の +X に 0.67 ft/s、切り離してから -1.90 ft/s）。
// 2 つの位置での地球の重力の差は入れない（離れていく間の数 % の差）。1969 年の号は実際には、燃料が揺れて機械船が回りだし、
// 後ろへ離れる噴射の向きが変わって、11 号では 5 分後に司令船のそばを通るのが見えた（14 号の Mission Report。この動きの記録は無い）
static const double SM_RCS_N = 4 * 444.8, SM_SPIN_RPM = 15, SM_SPIN_ROLL = 5.5, SM13_V = 0.58, SM_SHOW = 300, SM13_SHOW = 1200;
static double ApolloSmDist(const ApolloMission& m, double s) {
    if (s <= 0) return 0;
    if (m.smLb <= 0) return SM13_V * s;
    double a = SM_RCS_N / (m.smLb * 0.45359237), T = m.smBurn;
    return s < T ? 0.5 * a * s * s : 0.5 * a * T * T + a * T * (s - T);
}
static double ApolloSmSpin(const ApolloMission& m, double s) {
    double T = m.smRoll, u = s - 2;
    if (T <= 0 || u <= 0) return 0;
    double w = SM_SPIN_RPM * 2 * PI / 60 * T / SM_SPIN_ROLL;   // 回す噴射が終わった時の角速度 rad/s
    return u < T ? 0.5 * w / T * u * u : 0.5 * w * T + w * (u - T);
}

// 機械船の切り離し（13 号は月着陸船の切り離しも）を、司令船と離れていく機体の横から見る見方と、切り離した機械船。機械船は
// 切り離す 20 秒前から移り始め、0.1〜0.3 km 離れる間に司令船へ寄せ、0.6 km までに戻す（サターンの段と同じ。13 号は 20 分、
// ほかは 5 分描く）。13 号の月着陸船は ApolloDockPort の位置で、0.15〜0.4 km 離れる間に戻す（月を回る軌道での切り離しと同じ）。
// 見方の割合を返す（out があれば、見方と機械船を入れる）
static double ApolloSepView(const App& app, int mission, double t, ApolloPieces* out) {
    const ApolloMission& m = APOLLO_MISSIONS[mission];
    int c = ApolloBody(app, mission, 0);
    if (c < 0) return 0;
    const double lead = 20.0 / 86400;
    double best = 0;
    double ts = ApolloSmSepT(m), show = m.smLb > 0 ? SM_SHOW : SM13_SHOW;
    if (std::isfinite(ts) && t >= ts - lead && t <= ts + show / 86400) {
        Quaternion q0 = ApolloSmSepPose(app, c, m);
        V3 nose = FromF(Vector3RotateByQuaternion({0, 0, -1}, q0)), track = PosAt(app.B, c, t);
        double s = (t - ts) * 86400, oC = app.probeOuter[ApolloModel(m, 0, ts)], oS = app.probeOuter[PM_APOLLO_SM];
        V3 cm = track + nose * (s < 0 ? SMSEP_SHIFT : ApolloSmShift(m, t)), sm = track - nose * (ApolloSmDist(m, s) * 0.001);
        double gap = Len(sm - cm), near = 1 - ApolloSmooth((gap - 0.1) / 0.2);
        double w = s < 0 ? ApolloSmooth((t - (ts - lead)) / lead) : (1 - ApolloSmooth((gap - 0.3) / 0.3)) * (1 - ApolloSmooth((s - (show - 60)) / 60));
        if (out && s >= 0) out->parts.push_back({PM_APOLLO_SM, sm, QuaternionMultiply(q0, QuaternionFromAxisAngle({0, 0, 1}, (float)ApolloSmSpin(m, s)))});
        if (out && w > 0) {
            out->axis = q0;
            out->camW = w;
            out->camTarget = cm + (sm - cm) * (0.5 * near);
            out->camSize = oC + (gap + oS * 0.5) * near;
        }
        best = w;
    }
    double tj = ApolloLmJettT(m);
    int l = ApolloBody(app, mission, 1);
    if (std::isfinite(tj) && l >= 0 && t >= tj - lead && t <= tj + LMJ_SHOW / 86400) {
        // 司令船の真ん中と月着陸船の真ん中（接地点から天頂の側、司令船の側へ 2.7 m）
        V3 nose = FromF(Vector3RotateByQuaternion({0, 0, -1}, ApolloPose(app, c, PM_APOLLO_CM_LM, tj))), cp = PosAt(app.B, c, t);
        V3 lp = t < tj ? cp + nose * 0.0075 : ApolloLmJettPos(cp, nose, PosAt(app.B, l, t), (t - tj) * 86400);
        V3 cL = lp - nose * 0.0027;
        double gap = Len(cL - cp), w = 1 - ApolloSmooth((gap - 0.15) / 0.25);
        if (t < tj) w *= ApolloSmooth((t - (tj - lead)) / lead);
        if (out && w > best) {
            out->axis = ApolloPose(app, c, PM_APOLLO_CM_LM, tj);
            out->camW = w;
            out->camTarget = (cp + cL) * 0.5;
            out->camSize = gap + 0.014;
            out->pair = true;
        }
        best = std::max(best, w);
    }
    return best;
}

// 時刻 t の部品とカメラ（ApolloTdeShiftBodies の後の機体の位置から）。無ければ false
static bool ApolloPiecesAt(const App& app, int mission, ApolloPieces* out) {
    out->parts.clear();
    out->camW = 0;
    out->pair = false;
    const ApolloMission& m = APOLLO_MISSIONS[mission];
    int c = ApolloBody(app, mission, 0);
    if (c < 0 || !app.B[c].active) return false;
    if (ApolloRdvView(app, mission, app.tDays, out) > 0) return true;
    ApolloTdeView tv;
    if (ApolloTdeAt(app, mission, &tv)) {
        out->axis = tv.q;
        out->camW = tv.camW;
        out->camTarget = tv.camTarget;
        out->camSize = tv.camSize;
        if (tv.model < 0) return true;
        out->parts.push_back({tv.model, tv.sivb, tv.q});
        if (tv.panelS < 0) return true;
        // 板: 火薬の噴射口で下の端（ちょうつがい）のまわりに外へ倒れ始め、45° 開いたところでちょうつがいが外れて、
        // ばねで秒速 2.5 m で離れる（Apollo Flight Journal）。倒れる速さの記録はないので、3 秒で 45°（秒速 30°）とし、
        // 外れた後もその速さで回り続ける
        const double tRel = 3.0, omega = 30 * DEG, vOut = 2.5;
        double s = tv.panelS, ang = s < tRel ? 0.5 * (omega / tRel) * s * s : 45 * DEG + omega * (s - tRel);
        float r0 = 3.3f - (3.3f - 1.956f) * SLA_FIXED / SLA_LEN, zh = -(18.7f + SLA_FIXED);
        V3 cen(-0.748, 0, -(SLA_LEN - SLA_FIXED) * 0.5);   // 板の重心（ちょうつがいの中央から）m
        for (int k = 0; k < 4; k++) {
            float phi = PI / 2 * k;
            Quaternion qz = QuaternionFromAxisAngle({0, 0, 1}, phi);
            auto open = [&](double a) { return QuaternionMultiply(qz, QuaternionFromAxisAngle({0, -1, 0}, (float)a)); };
            V3 hinge(r0 * cosf(phi), r0 * sinf(phi), zh);
            V3 o = hinge;   // 板の原点（第3段の座標 m）
            if (s >= tRel) {
                Quaternion q0 = open(45 * DEG);
                V3 n = FromF(Vector3RotateByQuaternion({1, 0, 0}, q0));   // 外れた時の板の外向き
                V3 com = hinge + FromF(Vector3RotateByQuaternion(ToF(cen), q0)) + n * (vOut * (s - tRel));
                o = com - FromF(Vector3RotateByQuaternion(ToF(cen), open(ang)));
            }
            out->parts.push_back({PM_SLA_PANEL, tv.sivb + FromF(Vector3RotateByQuaternion(ToF(o * 0.001), tv.q)), QuaternionMultiply(tv.q, open(ang))});
        }
        return true;
    }
    ApolloSepView(app, mission, app.tDays, out);
    if (!out->parts.empty() || out->camW > 0) return true;
    // サターンの段の切り離し: 切り離す 8 秒前から機体と段が入る見方へ移り、段が遠ざかったら戻す
    double tA, tR, tB;
    if (!ApolloStagingTimes(m, &tA, &tR, &tB)) return false;
    double t = app.tDays, lead = 8.0 / 86400;
    int model = ApolloModel(m, 0, t);
    if (model < 0) return false;
    Quaternion qNow = ApolloPose(app, c, model, t);
    V3 fwdNow = FromF(Vector3RotateByQuaternion({0, 0, -1}, qNow));
    const struct { double t0; int before; const ApolloStageDef* def; } seps[] = {
        {tA, PM_SATURN_V, &STAGE_SIC}, {tR, PM_SATURN_SII_RING, &STAGE_RING}, {tB, PM_SATURN_SII, &STAGE_SII}};
    const double eps = 1e-6;   // 日（0.09 秒）
    for (const auto& sp : seps) {
        const ApolloStageDef& d = *sp.def;
        if (t < sp.t0 - lead || t > sp.t0 + d.show / 86400) continue;
        if (t < sp.t0) {
            // 切り離す前: 切り離した直後と同じ見方（残る機体と段の真ん中の間）へ移り始める。切り離すと形の原点が前へ移る
            // 分（ApolloStageShift の跳び）だけ、残る機体の真ん中も前へ
            int after = ApolloModel(m, 0, sp.t0 + eps);
            double jump = ApolloStageShift(m, sp.t0 + eps) - ApolloStageShift(m, sp.t0 - eps);
            V3 ac = app.B[c].pos + fwdNow * (jump + app.probeOuter[after] * 0.5), pc = app.B[c].pos + fwdNow * (app.probeOuter[d.pm] * 0.5);
            double w = ApolloSmooth((t - (sp.t0 - lead)) / lead);
            if (w > out->camW) {
                out->camW = w;
                out->axis = qNow;
                out->camTarget = (ac + pc) * 0.5;
                out->camSize = app.probeOuter[after] + Len(ac - pc) + app.probeOuter[d.pm] * 0.5;
            }
            continue;
        }
        // 残る機体の真ん中（大きさは切り離した直後の形。脱出塔を捨てても見方が跳ばないように）
        double outer = app.probeOuter[ApolloModel(m, 0, sp.t0 + eps)];
        V3 center = app.B[c].pos + fwdNow * (outer * 0.5);
        Quaternion q = ApolloPose(app, c, sp.before, sp.t0);
        V3 fwd = FromF(Vector3RotateByQuaternion({0, 0, -1}, q));
        double shift = ApolloStageShift(m, sp.t0 - eps);   // 切り離す直前に描いていた位置（段間リングは第1段の分ずれている）
        double dv = d.pm == PM_SIC_SPENT ? d.dv * m.sicRetros / 8 : d.dv;
        V3 pos = ApolloStagePos(app, c, sp.t0, t, fwd, d.tRetro > 0 ? dv / d.tRetro : 0, d.tRetro) + fwd * shift;
        out->parts.push_back({d.pm, pos, q});
        // 段が gap0 から gap1 まで離れる間に、見る点を 2 つの真ん中から機体の真ん中へ寄せ、入れる大きさも機体だけに戻す
        // （段は画面の外へ遠ざかっていく）。その後 gap1 の 2 倍まで離れる間に、普段の見方へ戻す
        V3 pc = pos + fwd * (app.probeOuter[d.pm] * 0.5);
        double gap = Len(pc - center), near = 1 - ApolloSmooth((gap - d.gap0) / (d.gap1 - d.gap0));
        double w = 1 - ApolloSmooth((gap - d.gap1) / d.gap1);
        if (w > out->camW) {
            out->camW = w;
            out->axis = q;
            out->camTarget = center + (pc - center) * (0.5 * near);
            out->camSize = outer + (gap + app.probeOuter[d.pm] * 0.5) * near;
        }
    }
    return !out->parts.empty() || out->camW > 0;
}

// 月着陸船の下降段（月面を離れた後も着陸地点に残る）の位置。無ければ false
static bool ApolloDescentStage(const App& app, int mission, V3* pos, V3* up) {
    const ApolloMission& m = APOLLO_MISSIONS[mission];
    if (!m.liftoff || !std::isfinite(m.siteLat) || app.tDays < ApolloT(m, m.liftoff)) return false;
    int lm = ApolloBody(app, mission, 1);
    if (lm < 0 || app.B[lm].d.apollo < 0) return false;
    // 着陸していた区間の位置（月固定）をそのまま使う
    double tl = ApolloT(m, m.liftoff) - 1.0 / 86400;
    for (const ApolloSeg& s : g_apollo[app.B[lm].d.apollo].segs)
        if (s.fixed && s.body == B_MOON && s.t.front() <= tl && tl <= s.t.back()) {
            const Body& moon = app.B[B_MOON];
            V3 f = HermiteAt(s.t, s.p, s.v, tl);
            *pos = moon.pos + FixedToRender(moon, moon.spin, f);
            *up = Norm(*pos - moon.pos);
            return true;
        }
    return false;
}

// イベント一覧の「アポロ計画」のページ（年代順）。ev.t は打ち上げ、ev.moon は APOLLO_MISSIONS の番号
static std::vector<AstroEvent> FindApolloFlights(const App& app) {
    std::vector<AstroEvent> out;
    for (int k = 0; k < APOLLO_COUNT; k++) {
        int b = ApolloBody(app, k, 0);
        if (b < 0 || app.B[b].d.apollo < 0) continue;
        AstroEvent ev;
        ev.kind = EV_APOLLO;
        ev.t = ApolloLaunch(APOLLO_MISSIONS[k]);
        ev.moon = k;
        ev.probe = b;
        out.push_back(ev);
    }
    std::sort(out.begin(), out.end(), [](const AstroEvent& a, const AstroEvent& b) { return a.t < b.t; });
    return out;
}
