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

static const ApolloShape APOLLO11_SHAPES[] = {
    {"-3:00:00", 0, PM_SATURN_V}, {"0:02:42.30", 0, PM_SATURN_SII_LES}, {"0:03:17.90", 0, PM_SATURN_SII},
    {"0:09:09.00", 0, PM_SATURN_SIVB}, {"3:17:04.6", 0, PM_APOLLO_CSM}, {"3:24:03.1", 0, PM_APOLLO_CSM_LM},
    {"100:12:00.0", 0, PM_APOLLO_CSM}, {"128:03:00.0", 0, PM_APOLLO_CSM_LMA}, {"130:09:31.2", 0, PM_APOLLO_CSM},
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
    {"-3:00:00", 0, PM_SATURN_V}, {"0:02:34.47", 0, PM_SATURN_SII_LES}, {"0:03:08.6", 0, PM_SATURN_SII},
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
    {"-3:00:00", 0, PM_SATURN_V}, {"0:02:42.31", 0, PM_SATURN_SII_LES}, {"0:03:17.8", 0, PM_SATURN_SII},
    {"0:09:13.50", 0, PM_SATURN_SIVB}, {"3:02:42.4", 0, PM_APOLLO_CSM_LM}, {"98:11:57", 0, PM_APOLLO_CSM},
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
    {"-3:00:00", 0, PM_SATURN_V}, {"0:02:42.4", 0, PM_SATURN_SII_LES}, {"0:03:17.9", 0, PM_SATURN_SII},
    {"0:09:13.20", 0, PM_SATURN_SIVB}, {"3:18:04.9", 0, PM_APOLLO_CSM}, {"3:26:53.3", 0, PM_APOLLO_CSM_LM},
    {"107:54:02.3", 0, PM_APOLLO_CSM}, {"145:36:20.2", 0, PM_APOLLO_CSM_LMA}, {"148:04:30.9", 0, PM_APOLLO_CSM},
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
    {"-3:00:00", 0, PM_SATURN_V}, {"0:02:44.3", 0, PM_SATURN_SII_LES}, {"0:03:21", 0, PM_SATURN_SII},
    {"0:09:53.50", 0, PM_SATURN_SIVB}, {"3:06:38.9", 0, PM_APOLLO_CSM}, {"3:19:08.8", 0, PM_APOLLO_CSM_LM},
    {"138:01:48.0", 0, PM_APOLLO_CM_LM}, {"141:30:00.2", 0, PM_APOLLO_CM},
    {"142:48:41", 0, PM_CM_DROGUE}, {"142:49:31", 0, PM_CM_MAIN}, {"142:54:41", 0, PM_APOLLO_CM},
    {"141:30:00.2", 1, PM_APOLLO_LM},
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
    {"-3:00:00", 0, PM_SATURN_V}, {"0:02:44.8", 0, PM_SATURN_SII_LES}, {"0:03:20.7", 0, PM_SATURN_SII},
    {"0:09:20.00", 0, PM_SATURN_SIVB}, {"3:02:29.4", 0, PM_APOLLO_CSM}, {"4:56:56.7", 0, PM_APOLLO_CSM_LM},
    {"103:47:41.6", 0, PM_APOLLO_CSM}, {"143:32:50.5", 0, PM_APOLLO_CSM_LMA}, {"145:44:58.0", 0, PM_APOLLO_CSM},
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
    {"-3:00:00", 0, PM_SATURN_V}, {"0:02:41.2", 0, PM_SATURN_SII_LES}, {"0:03:17", 0, PM_SATURN_SII},
    {"0:09:10.1", 0, PM_SATURN_SIVB}, {"3:22:27.2", 0, PM_APOLLO_CSM}, {"3:33:49.5", 0, PM_APOLLO_CSM_LM},
    {"100:39:16.2", 0, PM_APOLLO_CSM}, {"173:36:25.5", 0, PM_APOLLO_CSM_LMA}, {"179:30:01.4", 0, PM_APOLLO_CSM},
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
    {"-3:00:00", 0, PM_SATURN_V}, {"0:02:43.5", 0, PM_SATURN_SII_LES}, {"0:03:19.8", 0, PM_SATURN_SII},
    {"0:09:20.5", 0, PM_SATURN_SIVB}, {"3:04:59.0", 0, PM_APOLLO_CSM}, {"3:59:15.1", 0, PM_APOLLO_CSM_LM},
    {"96:13:31", 0, PM_APOLLO_CSM}, {"177:41:18", 0, PM_APOLLO_CSM_LMA}, {"195:00:12", 0, PM_APOLLO_CSM},
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
    {"-3:00:00", 0, PM_SATURN_V}, {"0:02:42.9", 0, PM_SATURN_SII_LES}, {"0:03:19", 0, PM_SATURN_SII},
    {"0:09:20.6", 0, PM_SATURN_SIVB}, {"3:42:27.6", 0, PM_APOLLO_CSM}, {"4:45:02.3", 0, PM_APOLLO_CSM_LM},
    {"107:47:56", 0, PM_APOLLO_CSM}, {"187:37:15", 0, PM_APOLLO_CSM_LMA}, {"191:18:31", 0, PM_APOLLO_CSM},
    {"301:23:49", 0, PM_APOLLO_CM}, {"301:46:22", 0, PM_CM_DROGUE}, {"301:47:13", 0, PM_CM_MAIN}, {"301:51:59", 0, PM_APOLLO_CM},
    {"107:47:56", 1, PM_APOLLO_LM}, {"185:21:37", 1, PM_LM_ASCENT}, {"187:37:15", 1, -1}, {"191:18:31", 1, PM_LM_ASCENT},
};

static const ApolloMission APOLLO_MISSIONS[] = {
    {"Apollo 8", "アポロ8号", "Apollo 8", 1968, 12, 21, 12, 51, 0.0, {"Apollo 8", nullptr},
     "人類が初めて月を回った", "First crewed flight around the Moon",
     APOLLO8_EVENTS, (int)(sizeof APOLLO8_EVENTS / sizeof APOLLO8_EVENTS[0]),
     APOLLO8_SHAPES, (int)(sizeof APOLLO8_SHAPES / sizeof APOLLO8_SHAPES[0]), NAN, NAN, nullptr},
    {"Apollo 10", "アポロ10号", "Apollo 10", 1969, 5, 18, 16, 49, 0.0, {"Apollo 10 Charlie Brown", "Apollo 10 Snoopy"},
     "着陸の予行演習（月面の上空 約 14 km まで）", "Landing dress rehearsal (down to about 14 km)",
     APOLLO10_EVENTS, (int)(sizeof APOLLO10_EVENTS / sizeof APOLLO10_EVENTS[0]),
     APOLLO10_SHAPES, (int)(sizeof APOLLO10_SHAPES / sizeof APOLLO10_SHAPES[0]), NAN, NAN, nullptr},
    {"Apollo 11", "アポロ11号", "Apollo 11", 1969, 7, 16, 13, 32, 0.0, {"Apollo 11 Columbia", "Apollo 11 Eagle"},
     "人類初の月面着陸（静かの海）", "First crewed landing (Sea of Tranquility)",
     APOLLO11_EVENTS, (int)(sizeof APOLLO11_EVENTS / sizeof APOLLO11_EVENTS[0]),
     APOLLO11_SHAPES, (int)(sizeof APOLLO11_SHAPES / sizeof APOLLO11_SHAPES[0]), 0.67408, 23.47297, "124:22:00.79"},
    {"Apollo 12", "アポロ12号", "Apollo 12", 1969, 11, 14, 16, 22, 0.0, {"Apollo 12 Yankee Clipper", "Apollo 12 Intrepid"},
     "嵐の大洋、サーベイヤー3号のそばへピンポイント着陸", "Pinpoint landing beside Surveyor 3 (Ocean of Storms)",
     APOLLO12_EVENTS, (int)(sizeof APOLLO12_EVENTS / sizeof APOLLO12_EVENTS[0]),
     APOLLO12_SHAPES, (int)(sizeof APOLLO12_SHAPES / sizeof APOLLO12_SHAPES[0]), -3.01239, -23.42157, "142:03:47.78"},
    {"Apollo 13", "アポロ13号", "Apollo 13", 1970, 4, 11, 19, 13, 0.0, {"Apollo 13 Odyssey", "Apollo 13 Aquarius"},
     "酸素タンクの爆発で着陸を中止し、月の裏を回って帰還", "Landing aborted after an oxygen tank explosion; looped the Moon and returned",
     APOLLO13_EVENTS, (int)(sizeof APOLLO13_EVENTS / sizeof APOLLO13_EVENTS[0]),
     APOLLO13_SHAPES, (int)(sizeof APOLLO13_SHAPES / sizeof APOLLO13_SHAPES[0]), NAN, NAN, nullptr},
    {"Apollo 14", "アポロ14号", "Apollo 14", 1971, 1, 31, 21, 3, 2.0, {"Apollo 14 Kitty Hawk", "Apollo 14 Antares"},
     "フラ・マウロ高地に着陸", "Landing in the Fra Mauro highlands",
     APOLLO14_EVENTS, (int)(sizeof APOLLO14_EVENTS / sizeof APOLLO14_EVENTS[0]),
     APOLLO14_SHAPES, (int)(sizeof APOLLO14_SHAPES / sizeof APOLLO14_SHAPES[0]), -3.64530, -17.47136, "141:45:40"},
    {"Apollo 15", "アポロ15号", "Apollo 15", 1971, 7, 26, 13, 34, 0.6, {"Apollo 15 Endeavour", "Apollo 15 Falcon"},
     "ハドリー谷に着陸、初めて月面車を使用", "Landing at Hadley Rille; first lunar rover",
     APOLLO15_EVENTS, (int)(sizeof APOLLO15_EVENTS / sizeof APOLLO15_EVENTS[0]),
     APOLLO15_SHAPES, (int)(sizeof APOLLO15_SHAPES / sizeof APOLLO15_SHAPES[0]), 26.13222, 3.63386, "171:37:23.2"},
    {"Apollo 16", "アポロ16号", "Apollo 16", 1972, 4, 16, 17, 54, 0.0, {"Apollo 16 Casper", "Apollo 16 Orion"},
     "デカルト高地に着陸", "Landing in the Descartes highlands",
     APOLLO16_EVENTS, (int)(sizeof APOLLO16_EVENTS / sizeof APOLLO16_EVENTS[0]),
     APOLLO16_SHAPES, (int)(sizeof APOLLO16_SHAPES / sizeof APOLLO16_SHAPES[0]), -8.97301, 15.50019, "175:31:47.9"},
    {"Apollo 17", "アポロ17号", "Apollo 17", 1972, 12, 7, 5, 33, 0.0, {"Apollo 17 America", "Apollo 17 Challenger"},
     "タウルス・リットロウ谷に着陸、アポロ計画の最後", "Landing at Taurus-Littrow; the last Apollo mission",
     APOLLO17_EVENTS, (int)(sizeof APOLLO17_EVENTS / sizeof APOLLO17_EVENTS[0]),
     APOLLO17_SHAPES, (int)(sizeof APOLLO17_SHAPES / sizeof APOLLO17_SHAPES[0]), 20.19080, 30.77168, "185:21:37"},
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

// 時刻 t の機体の形（-1: 描かない）
static int ApolloModel(const ApolloMission& m, int vehicle, double t) {
    int model = vehicle == 0 ? PM_APOLLO_CSM : PM_APOLLO_LM;
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
    if (fixedBody >= 0) {   // 地面に対する速度: 自転で動く分を引く
        const Body& o = app.B[fixedBody];
        double rp = o.d.rotPeriod == 0 ? o.d.moonPeriod : o.d.rotPeriod;
        V3 w = o.pole * (2 * PI / rp);
        v = v - Cross(w, app.B[i].pos - o.pos);
    }
    return v;
}

// 機体の向き（描画の回転）: 打ち上げは上へ（速さが出ると地面に対する進む向きへ）、月着陸船の降下は噴射を進む向きの逆へ
// （遅くなるにつれて真上へ）、月面では真上、突入は耐熱シールドを前へ、パラシュートでは真上、そのほかは進む向き
static Quaternion ApolloPose(const App& app, int i, int model) {
    const Body& b = app.B[i];
    double t = app.tDays;
    int ref;
    V3 v = ApolloVel(app, i, t, &ref);
    const Body& o = app.B[ref];
    V3 up = Norm(b.pos - o.pos);
    double speed = Len(v) / 86400;   // km/s
    V3 vdir = speed > 1e-9 ? Norm(v) : up;
    int fixedBody = ApolloFixedBody(b.d, t);
    V3 fwd = vdir, hint = up;
    auto blend = [](V3 a, V3 c, double w) { V3 r = a * (1 - w) + c * w; return Len(r) > 1e-9 ? Norm(r) : a; };
    if (model == PM_SATURN_V || model == PM_SATURN_SII_LES || model == PM_SATURN_SII || model == PM_SATURN_SIVB) {
        if (fixedBody >= 0) fwd = blend(up, vdir, std::min(1.0, speed / 0.05));   // 発射台では真上
        V3 east = Cross(o.pole, up);
        hint = Len(east) > 1e-9 ? Norm(east) : V3(1, 0, 0);
    } else if (model == PM_APOLLO_LM || model == PM_LM_ASCENT) {
        // 月着陸船は天頂の側（-Z）を噴射の反対へ
        if (fixedBody >= 0) {
            double w = std::min(1.0, speed / 0.15);
            bool descending = Dot(v, up) < 0 || model == PM_APOLLO_LM;
            fwd = descending ? blend(up, vdir * -1.0, w * 0.85) : blend(up, vdir, w * 0.8);
        } else {
            fwd = up;
        }
        hint = Len(Cross(fwd, vdir)) > 1e-6 ? vdir : V3(1, 0, 0);
    } else if (model == PM_APOLLO_CM || model == PM_CM_DROGUE || model == PM_CM_MAIN) {
        if (model != PM_APOLLO_CM || (fixedBody >= 0 && speed < 0.05)) fwd = up;   // パラシュートでは先端が上
        else if (fixedBody >= 0) fwd = vdir * -1.0;                              // 大気の中: 耐熱シールドを前へ
    }
    if (Len(Cross(fwd, hint)) < 1e-6) hint = Len(Cross(fwd, Y_UP)) > 1e-6 ? Y_UP : V3(1, 0, 0);
    return QuatFromBasis(fwd, hint);
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
