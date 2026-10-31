// Solar System Voyager — Copyright (c) 2026 hubstacksea
// MIT License (LICENSE を参照) — https://github.com/hubstacksea/Solar-System-Simulator
// 表示文字列（日本語 / English）。
// main.cpp の一部で、main.cpp が決まった順に読み込む（単体ではコンパイルしない）

// ---------------------------------------------------------------- 表示文字列
#define UI_STRINGS(X)                                                                        \
    X(ENGINE, "エンジン", "Engine")                                                          \
    X(ACCEL, "加速", "Accel")                                                                \
    X(MAXSPD, "最高", "Max")                                                                 \
    X(SPEED, "速度", "Speed")                                                                \
    X(FRAME, "基準系", "Frame")                                                              \
    X(NEAREST, "最寄り", "Nearest")                                                          \
    X(ALT, "高度", "Alt")                                                                    \
    X(DATE, "日時", "Date")                                                                  \
    X(TSCALE, "時間倍率", "Time")                                                            \
    X(REALTIME, "実時間", "real-time")                                                       \
    X(PAUSED, "停止中", "PAUSED")                                                            \
    X(TARGET, "ターゲット", "Target")                                                        \
    X(DIST, "距離", "Dist")                                                                  \
    X(ETA, "到達予想", "ETA")                                                                \
    X(ETA_MAX, "最高速なら", "at max")                                                       \
    X(LIGHT, "光で", "light")                                                                \
    X(DAY, "日", "d")                                                                        \
    X(YEAR, "年", "y")                                                                       \
    X(HELP_HINT, "[H] 操作説明   [Esc] マウス解放/捕捉", "[H] Help   [Esc] Release/capture mouse") \
    X(VIEW_CHASE, "追尾視点", "Chase view")                                                  \
    X(VIEW_COCKPIT, "コクピット視点", "Cockpit view")                                        \
    X(WARP, "ワープ", "Warp")                                                                \
    X(LAND, "地表付近へワープ", "Surface warp")                                              \
    X(ORBITS_ON, "軌道表示 ON", "Orbits ON")                                                 \
    X(ORBITS_OFF, "軌道表示 OFF", "Orbits OFF")                                              \
    X(LABELS_ON, "衛星マーカー ON", "Moon markers ON")                                       \
    X(LABELS_OFF, "衛星マーカー OFF", "Moon markers OFF")                                    \
    X(SKY_OFF, "星座 OFF", "Constellations OFF")                                            \
    X(SKY_LINES, "星座線", "Constellation lines")                                           \
    X(SKY_NAMES, "星座線と名前", "Constellation lines & names")                             \
    X(INVERT_ON, "マウス上下反転 ON", "Invert mouse ON")                                     \
    X(INVERT_OFF, "マウス上下反転 OFF", "Invert mouse OFF")                                  \
    X(TIME_RESET, "現在時刻・等倍に同期", "Synced to current time")                          \
    X(HEADING, "方位", "Heading")                                                            \
    X(ECL_LON, "黄経", "Ecl.Lon")                                                            \
    X(ECL_LAT, "黄緯", "Ecl.Lat")                                                            \
    X(PITCH, "ピッチ", "Pitch")                                                              \
    X(ROLL, "ロール", "Roll")                                                                \
    X(N, "北", "N")                                                                          \
    X(E, "東", "E")                                                                          \
    X(S, "南", "S")                                                                          \
    X(W, "西", "W")                                                                          \
    X(REVERSE, "後退", "REV")                                                                \
    X(OVERSPEED, "減速中", "SLOWING")                                                        \
    X(HELP_TITLE, "操作説明", "Controls")                                                    \
    X(HELP1, "マウス / 矢印キー : 機首の上下・左右", "Mouse / Arrows : pitch & yaw")         \
    X(HELP2, "A / D : ヨー      Q / E : ロール", "A / D : yaw      Q / E : roll")            \
    X(HELP3, "W : 加速      S : 減速・後退      X : 停止", "W : accelerate   S : brake/reverse   X : stop") \
    X(HELP4, "1〜8 / マウスホイール : エンジン切替", "1-8 / Mouse wheel : select engine")    \
    X(HELP5, "Tab / Shift+Tab : ターゲット切替", "Tab / Shift+Tab : cycle target")           \
    X(HELP6, "F : 機首を向ける    T : 自動操縦    Shift+T : 自動操縦(エンジン自動選択)", "F : face target    T : autopilot    Shift+T : autopilot (auto engine)") \
    X(HELP7, "G : ターゲットへワープ    Shift+G : 地表付近へワープ    Shift+F : 基準系を自機に固定", "G : warp to target    Shift+G : warp to surface    Shift+F : fix the frame to the ship") \
    X(HELP8, "右クリック(押している間) : ズーム    B : 現在地をブックマーク    N : ブックマークへワープ", "Right mouse (hold) : zoom    B : bookmark this spot    N : warp to next bookmark") \
    X(HELP9, "V : 視点切替    O : 軌道表示    L : 衛星マーカー    C : 星座    Y : 日時指定    K : イベント一覧    U : 自動ツアー", "V : view    O : orbits    L : moon markers    C : constellations    Y : set date    K : events    U : guided tour") \
    X(HELP10, "[ / ] : 時間倍率    P : 時間停止    Backspace : 現在時刻に同期    Z : 写真モード    R : 重力モード", "[ / ] : time scale    P : pause    Backspace : sync to now    Z : photo mode    R : gravity mode") \
    X(HELP11, "I : 天体の情報    M : 効果音    Shift+M : 音楽    F11 : フルスクリーン    F12 : スクリーンショット", "I : body info    M : sound    Shift+M : music    F11 : fullscreen    F12 : screenshot") \
    X(HELP12, "Esc : マウス解放/捕捉    J : 言語    F2 : 共有リンク    F3 : 全体図    / , F4 : 検索    F9 : 画質    F10 : 設定    Ctrl+Q : 終了", "Esc : release/capture mouse    J : language    F2 : share link    F3 : map    / , F4 : search    F9 : quality    F10 : settings    Ctrl+Q : quit") \
    X(HELP13, "[パッド] 左スティック : 機首    右スティック左右 / LB・RB : ロール    RT / LT : 加速 / 減速", "[Pad] L-stick : pitch/yaw    R-stick X / LB・RB : roll    RT / LT : throttle / brake") \
    X(HELP14, "[パッド] 十字左右 : エンジン    十字上下 : 時間倍率    A : 自動操縦    B : 停止", "[Pad] D-pad L/R : engine    D-pad U/D : time scale    A : autopilot    B : stop") \
    X(HELP15, "[パッド] X : ターゲット    Y : ワープ    L押込 : 地表ワープ    R押込 : ズーム    View : 視点    Menu : 説明・メニュー    LB+RB : 全体図", "[Pad] X : target    Y : warp    L3 : surface warp    R3 : zoom    View : camera    Menu : help/menu    LB+RB : map") \
    X(REAL_TIME_ETA, "実時間で", "real") \
    X(AP, "自動操縦", "AUTOPILOT") \
    X(AP_ON, "自動操縦 ON", "Autopilot ON") \
    X(AP_ON_AUTO, "自動操縦 ON (エンジン自動選択)", "Autopilot ON (auto engine)") \
    X(AP_OFF, "自動操縦 OFF", "Autopilot OFF") \
    X(AP_ARRIVED, "到着", "Arrived") \
    X(AP_AUTO_ENGINE, "エンジン自動", "auto engine") \
    X(AP_TURN, "旋回中", "turning") \
    X(AP_ACCEL, "加速中", "accelerating") \
    X(AP_CRUISE, "巡航中", "cruising") \
    X(AP_DECEL, "減速中", "decelerating") \
    X(AP_AVOID, "回避中", "avoiding") \
    X(AP_REMAIN, "残り", "remaining") \
    X(LANG_SET, "言語: 日本語", "Language: English") \
    X(BM_SAVED, "ブックマーク %d を保存: %s", "Bookmark %d saved: %s") \
    X(BM_GO, "ブックマーク %d へワープ: %s", "Warp to bookmark %d: %s") \
    X(BM_NONE, "ブックマークがありません（B キーで保存）", "No bookmarks (press B to save)") \
    X(MUSIC_ON, "音楽: オン", "Music: on") \
    X(MUSIC_OFF, "音楽: オフ", "Music: off") \
    X(MUSIC_0, "♪ ホルスト《惑星》より「水星、翼のある使者」 米空軍軍楽隊 (2013)", "Now playing: Holst, The Planets: Mercury, the Winged Messenger - U.S. Air Force Band (2013)") \
    X(MUSIC_1, "♪ ホルスト《惑星》より「金星、平和をもたらす者」 ホルスト指揮 ロンドン響 (1923)", "Now playing: Holst, The Planets: Venus, the Bringer of Peace - LSO, cond. Holst (1923)") \
    X(MUSIC_2, "♪ ホルスト《惑星》より「火星、戦争をもたらす者」 ホルスト指揮 ロンドン響 (1922)", "Now playing: Holst, The Planets: Mars, the Bringer of War - LSO, cond. Holst (1922)") \
    X(MUSIC_3, "♪ ホルスト《惑星》より「木星、快楽をもたらす者」 ホルスト指揮 ロンドン響 (1923)", "Now playing: Holst, The Planets: Jupiter, the Bringer of Jollity - LSO, cond. Holst (1923)") \
    X(MUSIC_4, "♪ ホルスト《惑星》より「土星、老いをもたらす者」 ホルスト指揮 ロンドン響 (1923)", "Now playing: Holst, The Planets: Saturn, the Bringer of Old Age - LSO, cond. Holst (1923)") \
    X(MUSIC_5, "♪ ホルスト《惑星》より「天王星、魔術師」 ホルスト指揮 ロンドン響 (1923)", "Now playing: Holst, The Planets: Uranus, the Magician - LSO, cond. Holst (1923)") \
    X(MUSIC_6, "♪ ホルスト《惑星》より「海王星、神秘主義者」 ホルスト指揮 ロンドン響 (1923)", "Now playing: Holst, The Planets: Neptune, the Mystic - LSO, cond. Holst (1923)") \
    X(MUSIC_7, "♪ グリーグ《ペール・ギュント》より「朝」 Musopen Symphony (2012)", "Now playing: Grieg, Peer Gynt: Morning Mood - Musopen Symphony (2012)") \
    X(MUSIC_8, "♪ ベートーヴェン 交響曲第6番「田園」第1楽章 Skidmore College Orchestra (Musopen)", "Now playing: Beethoven, Symphony No. 6 \"Pastoral\", I - Skidmore College Orchestra (Musopen)") \
    X(MUSIC_9, "♪ ドビュッシー「月の光」 ピアノ: Laurens Goedhart (2011, CC BY 3.0)", "Now playing: Debussy, Clair de lune - Laurens Goedhart, piano (2011, CC BY 3.0)") \
    X(EV_TITLE, "天文イベント", "Astronomical events") \
    X(EV_KEYS, "↑↓ : 選択    Enter : その日時・場所へワープ    Tab : 天文 / 彗星・連星 / 探査機    Esc / K : 閉じる", "Up/Down : select    Enter : warp to that time and place    Tab : sky / comets and binaries / spacecraft    Esc / K : close") \
    X(EV_PROBE_TITLE, "探査機のできごと", "Spacecraft events") \
    X(EV_COMET_TITLE, "彗星・連星", "Comets and binary stars") \
    X(EV_COMET_NOTE, "1980〜2070 年の彗星の近日点と地球への最接近、連星の近星点。尾の長さは見た目の演出です", "Comet perihelia and closest approaches to Earth, and binary-star periastra, 1980-2070. Tail lengths are illustrative") \
    X(EV_PERIHELION, "%sが近日点を通過", "%s at perihelion") \
    X(EV_COMET_EARTH, "%sが地球に最接近", "%s: closest approach to Earth") \
    X(EV_FROM_SUN, "太陽から %s", "%s from the Sun") \
    X(EV_PERIASTRON, "%sの連星が最接近（近星点）", "%s binary at periastron") \
    X(EV_SEPARATION, "間隔 %s", "%s apart") \
    X(EV_VIEW, "%sから見た%s", "%s seen from %s")  /* 英語は引数の順を入れ替えて渡す */ \
    X(EV_FROM_EARTH, "地球から %s", "%s from Earth") \
    X(EV_TRANSIT, "%sの太陽面通過", "Transit of %s") \
    X(EV_TRANSIT_D, "太陽の中心から半径の %.2f 倍", "%.2f solar radii from the center") \
    X(EV_CONJ, "%sと%sが接近", "Conjunction: %s and %s") \
    X(EV_CONJ_D, "角距離 %.2f°", "separation %.2f°") \
    X(EV_OPPOSITION, "%sが衝", "Opposition of %s") \
    X(EV_PROBE_NOTE, "軌跡は JPL Horizons のデータ。惑星への接近は実際の距離と向きで再現します", "Trajectories from JPL Horizons. Planetary encounters are reproduced at their real distance and direction") \
    X(EV_FLYBY, "%sが%sに最接近", "%s: closest approach to %s") \
    X(EV_ARRIVE, "%sが%sに到着（周回開始）", "%s arrives at %s (orbit insertion)") \
    X(EV_END, "%sの運用終了（%sへ突入）", "%s: end of mission (enters %s)") \
    X(EV_ARRIVE_SMALL, "%sが%sに到着", "%s arrives at %s") \
    X(EV_END_LAND, "%sの運用終了（%sに降下）", "%s: end of mission (descends to %s)") \
    X(EV_IMPACT, "%sが%sに衝突", "%s impacts %s") \
    X(EV_GROUND, "%s（地上から）", "%s (from the ground)") \
    X(EV_IMPACT_D, "公転周期 11.92 → 11.37 時間", "orbital period 11.92 → 11.37 h") \
    X(EV_ALT, "高度 %s", "altitude %s") \
    X(EV_NOTE, "簡易計算のため、時刻は数分〜数十分、日食の位置は数百 km ずれることがあります。太陽面通過は 150 年先まで", "Simplified calculation: times may be off by minutes, solar eclipse locations by a few hundred km. Transits up to 150 years ahead") \
    X(EV_SEARCHING, "計算中…", "Calculating...") \
    X(EV_NONE, "見つかりませんでした", "No events found") \
    X(EV_SOLAR_TOTAL, "皆既日食", "Total solar eclipse") \
    X(EV_SOLAR_ANNULAR, "金環日食", "Annular solar eclipse") \
    X(EV_SOLAR_PARTIAL, "部分日食", "Partial solar eclipse") \
    X(EV_LUNAR_TOTAL, "皆既月食", "Total lunar eclipse") \
    X(EV_LUNAR_PARTIAL, "部分月食", "Partial lunar eclipse") \
    X(EV_SHADOW, "木星面を%sの影が通過", "Shadow of %s crosses Jupiter") \
    X(EV_CENTER, "食の中心 %s付近", "centered near %s") \
    X(EV_GO, "ワープ: %s  %s", "Warp: %s  %s") \
    X(TOUR, "ツアー %d / %d", "Tour %d / %d") \
    X(TOUR_KEYS, "Enter : 次へ    Shift+Enter : 前へ    U : ツアー終了", "Enter : next    Shift+Enter : previous    U : end tour") \
    X(TOUR_END, "ツアー終了", "Tour ended") \
    X(TOUR_NONE, "ツアーの場面が見つかりません", "No tour stops found") \
    X(PHOTO_0, "写真モード: 解除", "Photo mode: off") \
    X(SET_TITLE, "設定", "Settings") \
    X(SET_KEYS, "↑↓ : 選択    ←→ / Enter : 変更    Esc / F10 : 閉じる（変更はすぐ保存されます）", "Up/Down : select    Left/Right / Enter : change    Esc / F10 : close (changes are saved at once)") \
    X(SET_LANG, "言語", "Language") \
    X(SET_QUALITY, "画質", "Quality") \
    X(SET_SOUND, "効果音", "Sound effects") \
    X(SET_MUSIC, "音楽", "Music") \
    X(SET_SKY, "星座", "Constellations") \
    X(SET_ORBITS, "軌道表示", "Orbit lines") \
    X(SET_ASTEROIDS, "小惑星帯・トロヤ群・外縁天体の粒", "Asteroids, Trojans and TNOs") \
    X(SET_MARKERS, "衛星・探査機のマーカー", "Moon and spacecraft markers") \
    X(SET_INVERT, "マウス上下反転", "Invert mouse") \
    X(SET_FULLSCREEN, "フルスクリーン", "Fullscreen") \
    X(SET_TEXTSIZE, "文字の大きさ（計器・マーカーも）", "Text size (HUD and markers too)") \
    X(SET_MILKYWAY, "天の川", "Milky Way") \
    X(SET_SKYEPOCH, "星空の年代（恒星だけ動かす）", "Star epoch (moves the stars only)") \
    X(SKY_EPOCH_SHORT, "星空", "Stars") \
    X(SET_EPOCH_NOW, "今の日時どおり", "As the date") \
    X(SET_EPOCH_LATER, "%s 年後", "%s years later") \
    X(SET_EPOCH_EARLIER, "%s 年前", "%s years earlier") \
    X(SET_TS0, "標準", "Normal") \
    X(SET_TS1, "大", "Large") \
    X(SET_TS2, "特大", "Extra large") \
    X(SET_ON, "オン", "On") \
    X(LOADING, "地図などを読み込み中（残り %d）", "Loading maps (%d left)") \
    X(TB_THRUST, "加速", "Thrust") \
    X(TB_BRAKE, "減速", "Brake") \
    X(TB_STOP, "停止", "Stop") \
    X(TB_TARGET, "ターゲット", "Target") \
    X(TB_WARP, "ワープ", "Warp") \
    X(TB_AUTO, "自動操縦", "Autopilot") \
    X(TB_TOUR, "ツアー", "Tour") \
    X(TB_EVENTS, "イベント", "Events") \
    X(TB_SETTINGS, "設定", "Settings") \
    X(TB_HELP, "説明", "Help") \
    X(TB_SHARE, "共有", "Share") \
    X(HELP_PAD, "[パッド] この画面で A : 全体図    X : 設定    Y : イベント一覧    LB : 日時指定    RB : 天体の情報    十字上 : 検索    B : 閉じる", "[Pad] On this screen A : map    X : settings    Y : events    LB : set date    RB : body info    D-pad Up : search    B : close") \
    X(PAD_DATE, "[パッド] 十字上下 : ±1 日    十字左右 : ±30 日    LB / RB : ±1 年    A : 決定    B : 閉じる", "[Pad] D-pad Up/Down : ±1 day    Left/Right : ±30 days    LB / RB : ±1 year    A : set    B : close") \
    X(INFO_STAR, "恒星", "Star") \
    X(INFO_PLANET, "惑星", "Planet") \
    X(INFO_DWARF, "準惑星", "Dwarf planet") \
    X(INFO_MOON_OF, "%s の衛星", "Moon of %s") \
    X(INFO_ASTEROID, "小惑星", "Asteroid") \
    X(INFO_COMET, "彗星", "Comet") \
    X(INFO_PROBE, "探査機・宇宙望遠鏡", "Spacecraft") \
    X(INFO_SAT, "地球を回る人工衛星", "Earth-orbiting satellite") \
    X(INFO_RADIUS, "赤道半径", "Equatorial radius") \
    X(INFO_MASS, "質量", "Mass") \
    X(INFO_GRAVITY, "表面の重力", "Surface gravity") \
    X(INFO_ESCAPE, "脱出速度", "Escape velocity") \
    X(INFO_DAY, "自転周期", "Rotation period") \
    X(INFO_SYNC, "公転と同じ（いつも同じ面を向ける）", "same as its orbit (tidally locked)") \
    X(INFO_RETRO, "逆回り", "retrograde") \
    X(INFO_YEAR, "公転周期", "Orbital period") \
    X(INFO_SMA, "軌道の大きさ（長半径）", "Semi-major axis") \
    X(INFO_ECC, "離心率 / 軌道傾斜角", "Eccentricity / inclination") \
    X(INFO_MOONS, "衛星（このシミュレータ内）", "Moons (in this simulator)") \
    X(INFO_FROM_SUN, "太陽からの距離", "Distance from the Sun") \
    X(GND_IMAGINED, "地表は想像図（実際の様子は分かっていない）", "The surface is an artist's impression (its real appearance is unknown)") \
    X(INFO_LIGHT_LEFT, "見ている光", "Light you see") \
    X(INFO_LIGHT_LEFT_V, "%.2f 年前に出た光（%d 年ごろ）", "left %.2f years ago (around %d)") \
    X(INFO_FROM_EARTH, "地球からの距離", "Distance from Earth") \
    X(INFO_FROM_SHIP, "自機からの距離", "Distance from you") \
    X(INFO_VISITS, "近くを飛んだ探査機", "Spacecraft that flew by") \
    X(INFO_DATA, "軌跡のデータ", "Trajectory data") \
    X(INFO_ALT, "高度 / 周期", "Altitude / period") \
    X(INFO_EARTHX, "地球の %s 倍", "%s x Earth") \
    X(INFO_CLOSE, "I / タップ: 閉じる", "I / tap: close") \
    X(INFO_WHITE_DWARF, "白色矮星", "White dwarf") \
    X(INFO_BROWN_DWARF, "褐色矮星", "Brown dwarf") \
    X(INFO_SUNX, "太陽の %s 倍", "%s x Sun") \
    X(INFO_JUPX, "木星の %s 倍", "%s x Jupiter") \
    X(INFO_TEMP, "表面温度", "Surface temperature") \
    X(INFO_PARTNER, "連星の相手", "Binary companion") \
    X(INFO_PARTNER_V, "%s（周期 %.1f 年、平均 %.1f AU）", "%s (period %.1f yr, mean %.1f AU)") \
    X(INFO_ESTIMATED, "推定の値", "Estimated") \
    X(INFO_ESTIMATED_V, "質量・半径・自転周期などは推定", "mass, radius, rotation period, etc.") \
    X(LY, "光年", "ly") \
    X(INFO_PLANET_OF, "%s の惑星", "Planet of %s") \
    X(INFO_EXOPLANET, "系外惑星", "Exoplanet") \
    X(INFO_TEQ, "平衡温度（推定）", "Equilibrium temp. (est.)") \
    X(INFO_EST_ORBIT, "軌道の傾き・質量・半径・表面", "orbit tilt, mass, radius, surface") \
    X(INFO_EST_PHASE, "公転の位置・軌道の傾き・質量・半径・表面", "orbital position, orbit tilt, mass, radius, surface") \
    X(INFO_EST_SURFACE, "半径・自転・表面", "radius, rotation, surface") \
    X(WARP_STAR, "恒星間ワープ", "Interstellar warp") \
    X(LY_PER_S, "光年/秒", "ly/s") \
    X(WARP_REMAIN, "残り", "Remaining") \
    X(WARP_LIGHT_TIME, "光の速さなら", "at light speed") \
    X(TB_INFO, "情報", "Info") \
    X(TB_SEARCH, "検索", "Search") \
    X(MS_QUA, "しぶんぎ座流星群", "Quadrantids") \
    X(MS_LYR, "4 月こと座流星群", "Lyrids") \
    X(MS_ETA, "みずがめ座η流星群", "Eta Aquariids") \
    X(MS_PER, "ペルセウス座流星群", "Perseids") \
    X(MS_ORI, "オリオン座流星群", "Orionids") \
    X(MS_LEO, "しし座流星群", "Leonids") \
    X(MS_GEM, "ふたご座流星群", "Geminids") \
    X(EV_METEOR, "%sが極大", "%s peak") \
    X(EV_METEOR_D, "1 時間に最大 約 %d 個（ZHR）。東京の夜空で見る", "up to about %d per hour (ZHR); watch from Tokyo") \
    X(INFO_MAG_EARTH, "地球から見た明るさ", "Brightness from Earth") \
    X(INFO_MAG_HERE, "ここから見た明るさ", "Brightness from here") \
    X(INFO_MAG_UNIT, "%.1f 等", "mag %.1f") \
    X(INFO_PHASE, "満ち欠け（光って見える割合）", "Phase (illuminated)") \
    X(INFO_ASIZE, "見かけの大きさ", "Apparent size") \
    X(INFO_AGE, "月齢", "Moon age") \
    X(SHAPE0, "ほとんど見えない", "nearly dark") \
    X(SHAPE1, "細い三日月形", "thin crescent") \
    X(SHAPE2, "半月形", "half") \
    X(SHAPE3, "ふくらんだ形", "gibbous") \
    X(SHAPE4, "まんまる", "full") \
    X(MOON0, "新月", "new moon") \
    X(MOON1, "三日月", "waxing crescent") \
    X(MOON2, "上弦の月", "first quarter") \
    X(MOON3, "満ちていく月", "waxing gibbous") \
    X(MOON4, "満月", "full moon") \
    X(MOON5, "欠けていく月", "waning gibbous") \
    X(MOON6, "下弦の月", "last quarter") \
    X(MOON7, "明け方の細い月", "waning crescent") \
    X(DIR0, "北", "N") \
    X(DIR1, "北北東", "NNE") \
    X(DIR2, "北東", "NE") \
    X(DIR3, "東北東", "ENE") \
    X(DIR4, "東", "E") \
    X(DIR5, "東南東", "ESE") \
    X(DIR6, "南東", "SE") \
    X(DIR7, "南南東", "SSE") \
    X(DIR8, "南", "S") \
    X(DIR9, "南南西", "SSW") \
    X(DIR10, "南西", "SW") \
    X(DIR11, "西南西", "WSW") \
    X(DIR12, "西", "W") \
    X(DIR13, "西北西", "WNW") \
    X(DIR14, "北西", "NW") \
    X(DIR15, "北北西", "NNW") \
    X(GND_ALT, "高度", "alt") \
    X(GND_AZ, "方位", "az") \
    X(GND_RISE, "出", "rise") \
    X(GND_TRANSIT_S, "南中", "transit") \
    X(GND_TRANSIT_N, "北中", "transit") \
    X(GND_SET, "入り", "set") \
    X(GND_ALWAYS_UP, "地平線の下に沈まない", "Never sets") \
    X(GND_ALWAYS_DOWN, "地平線の上に昇らない", "Never rises") \
    X(SEARCH_TITLE, "天体の検索", "Find a body") \
    X(SEARCH_HINT, "名前の一部を入力（ひらがな・英字）", "Type part of a name") \
    X(SEARCH_ALL, "すべて", "All") \
    X(SEARCH_PLANETS, "惑星", "Planets") \
    X(SEARCH_MOONS, "衛星", "Moons") \
    X(SEARCH_SMALL, "小天体", "Small bodies") \
    X(SEARCH_PROBES, "探査機", "Spacecraft") \
    X(SEARCH_STARS, "恒星系", "Stars") \
    X(SEARCH_CITIES, "地上", "Surface") \
    X(SEARCH_GOTO, "%s の名場面へ移る", "go to %s") \
    X(SEARCH_JUMPED, "日時を %s に移して %s へ", "Moved to %s for %s") \
    X(CITY_AT, "%s の地上（%s %.1f°  %s %.1f°）から空を見る", "Sky from %s (%s %.1f°  %s %.1f°)") \
    X(SEARCH_MOON, "の衛星", "moon") \
    X(SEARCH_NONE, "見つかりません", "No match") \
    X(SEARCH_KEYS, "↑↓: 選ぶ   Enter: ターゲット   Shift+Enter: ワープ   Tab: 分類   Esc: 閉じる", "Up/Down: select   Enter: target   Shift+Enter: warp   Tab: category   Esc: close") \
    X(SEARCH_KEYS_TOUCH, "タップ: ターゲット（もう一度でワープ）   タブで分類   枠の外で閉じる", "Tap: target (again: warp)   Tabs: category   Tap outside: close") \
    X(SEARCH_KEYS_PAD, "[パッド] 十字上下: 選ぶ   十字左右: 分類   A: ターゲット   Y: ワープ   B: 閉じる", "[Pad] D-pad Up/Down: select   Left/Right: category   A: target   Y: warp   B: close") \
    X(SET_RESUME, "起動したら前回の続きから", "Resume the last session") \
    X(SET_RES0, "オフ（地球の近くから）", "Off (start near Earth)") \
    X(SET_RES1, "場所だけ（日時は今）", "Place only (current time)") \
    X(SET_RES2, "場所と日時", "Place and time") \
    X(SET_FRAME, "基準系", "Reference frame") \
    X(SET_FRAME_AUTO, "自動（近くの天体と一緒に動く）", "Auto (move with the nearest body)") \
    X(FRAME_SHIP, "自機に固定", "Fixed to ship") \
    X(FRAME_LOCK_ON, "基準系を自機に固定（天体と一緒に動かず、天体の方が動いていく）", "Frame fixed to the ship (bodies move past you instead of carrying you along)") \
    X(FRAME_LOCK_OFF, "基準系: 自動（近くの天体と一緒に動く）", "Frame: automatic (you move along with the nearest body)") \
    X(RESUMED, "前回の続きから: %s", "Resumed: %s") \
    X(TB_MAP, "全体図", "Map") \
    X(MAP_TITLE, "全体図", "System map") \
    X(MAP_SUN, "太陽中心", "centered on the Sun") \
    X(MAP_NOTE, "真上（黄道の北）から見た図。距離は中心ほど広げた対数目盛り。半径 %s", "Seen from above (ecliptic north), logarithmic distances, radius %s") \
    X(MAP_HZ, "ハビタブルゾーン", "Habitable zone") \
    X(MAP_STARS, "太陽のまわりの恒星", "stars around the Sun") \
    X(MAP_STARS_NOTE, "黄道の北から斜めに見た図。縦の線は黄道面からの高さ。半径 %s（拡大で太陽中心の図へ）", "Seen obliquely from ecliptic north; the stems show the height above the ecliptic. Radius %s (zoom in for the Solar System)") \
    X(MAP_KEYS, "クリック/タップ: ターゲット（もう一度でワープ）   Enter/G: ワープ   Shift: 地表へ   Tab: 次のターゲット   ↑↓/ホイール: 拡大縮小（縮小し切ると近くの恒星）   Space: 中心の切替   ←→: 時間の速さ   P: 停止   R: 逆再生   Esc/F3: 閉じる", "Click/tap: target (again: warp)   Enter/G: warp   Shift: to surface   Tab: next target   Up/Down/wheel: zoom (all the way out: nearby stars)   Space: switch center   Left/Right: time speed   P: pause   R: reverse   Esc/F3: close") \
    X(MAP_PAD, "[パッド] 十字左右: ターゲット   十字上下: 拡大縮小   A/Y: ワープ   X: 中心の切替   B / LB+RB: 閉じる", "[Pad] D-pad L/R: target   D-pad U/D: zoom   A/Y: warp   X: switch center   B / LB+RB: close") \
    X(MAP_REV, "逆再生", "reverse") \
    X(MAP_SHIP, "自機", "You") \
    X(MAP_CLOSE, "閉じる", "Close") \
    X(MAP_CENTER, "中心の切替", "Center") \
    X(SHARED, "この場面のリンクをコピーしました（日時・場所・向き・時間倍率）", "Link to this scene copied (date, place, view, time scale)") \
    X(SHARED_BAR, "この場面のリンクをアドレス欄に入れました（コピーして共有できます）", "Link to this scene is in the address bar (copy it to share)") \
    X(SHARE_OPENED, "共有リンクの場面: %s", "Shared scene: %s") \
    X(INTRO_KEYS, "Enter / → : 次へ    ← : 前へ    Esc : 閉じる（次からは表示しません。H キーの操作説明はいつでも見られます）", "Enter / Right : next    Left : back    Esc : close (not shown again; H shows the controls at any time)") \
    X(INTRO_TAP, "タップで次へ    右上の × で閉じる（次からは表示しません）", "Tap for next    × at the top right closes (not shown again)") \
    X(INTRO_T1, "Solar System Voyager へようこそ", "Welcome to Solar System Voyager") \
    X(INTRO_A1, "太陽・惑星・衛星・小天体・探査機を実寸・実距離で配置した宇宙を、自由に飛び回れます。", "Fly freely through the Solar System with the Sun, planets, moons, small bodies and spacecraft at true size and distance.") \
    X(INTRO_B1, "天体の位置は PC の時刻で計算され、等倍ではいまこの瞬間の配置です。", "Positions are computed from your clock: at x1 you see the Solar System as it is right now.") \
    X(INTRO_T2, "機体を動かす", "Flying") \
    X(INTRO_A2, "マウス・矢印キーで機首の向き、W で加速・S で減速、1〜8 でエンジン（自動車〜光速、超光速）を切り替えます。", "Mouse or arrow keys steer, W accelerates and S brakes, 1-8 switch engines (from a car up to the speed of light, and a hyperdrive).") \
    X(INTRO_A2T, "画面をドラッグして機首の向き、右の「加速」「減速」で速度、下のエンジン欄をタップして切り替えます。", "Drag the view to steer, use Thrust and Brake on the right, and tap the engine bar at the bottom to switch engines.") \
    X(INTRO_B2, "宇宙はとても広いので、遠くへは光速でも何時間もかかります。そんな時はワープを使いましょう。", "Space is vast: even at light speed distant worlds take hours. That is what warp is for.") \
    X(INTRO_T3, "目的地へワープ", "Warping to a destination") \
    X(INTRO_A3, "Tab でターゲットを選び、G でワープ（Shift+G は地表のすぐ上へ）、T で自動操縦します。", "Choose a target with Tab, press G to warp (Shift+G to just above the surface), or T for the autopilot.") \
    X(INTRO_A3T, "「ターゲット」で行き先を選び、「ワープ」で一瞬で移動、「自動操縦」で飛んで行きます。", "Pick a destination with Target, then Warp to jump there or Autopilot to fly there.") \
    X(INTRO_B3, "Y キーで日時を指定すると、過去や未来の天体の配置へ移れます。", "Set any date with Y to see the Solar System in the past or the future.") \
    X(INTRO_T4, "見どころを巡る", "Highlights") \
    X(INTRO_A4, "U で自動ツアー（探査機の名場面・大彗星・日食などを説明付きで順に）、K でイベント一覧（日食・探査機のできごと）。", "U starts a guided tour (spacecraft highlights, great comets, eclipses and more), K opens the event list.") \
    X(INTRO_A4T, "「ツアー」で名場面を説明付きで順に巡り、「イベント」で日食や探査機のできごとへワープできます。", "Tour visits the highlights with captions; Events warps you to eclipses and spacecraft encounters.") \
    X(INTRO_B4, "F10（「設定」）で画質・言語・重力モードなど、H（「説明」）で操作の一覧を見られます。", "F10 (Settings) changes quality, language, gravity mode and more; H (Help) lists all the controls.") \
    X(SET_PHYSICS, "飛行モード", "Flight model") \
    X(SET_ARCADE, "アーケード", "Arcade") \
    X(SET_GRAVITY, "重力", "Gravity") \
    X(PHYS_ON, "重力モード: 天体の重力を受けて飛びます（W / S で機首の向きに加減速、X で相対停止）", "Gravity mode: you now fall under the bodies' gravity (W / S thrust along the nose, X stops relative motion)") \
    X(PHYS_OFF, "アーケードモード: 機首の向きへ進みます", "Arcade mode: you fly where the nose points") \
    X(PHYS_ORBIT, "近点 高度 %s / 遠点 高度 %s / 周期 %s（%s のまわり）", "periapsis %s / apoapsis %s / period %s (around %s)") \
    X(PHYS_ESCAPE, "%s の重力から脱出する軌道", "escape trajectory from %s") \
    X(PHYS_SLOW, "重力の計算が追いつかないので、時間の進みを抑えています", "Time is slowed so the gravity calculation can keep up") \
    X(SET_OFF, "オフ", "Off") \
    X(SET_Q0, "低（解像度 60%）", "Low (60% resolution)") \
    X(SET_Q1, "中", "Medium") \
    X(SET_Q2, "高", "High") \
    X(SET_SKY0, "表示しない", "Off") \
    X(SET_SKY1, "星座線", "Lines") \
    X(SET_SKY2, "星座線と名前", "Lines and names") \
    X(ASTEROIDS_ON, "小惑星などの粒 ON", "Asteroids ON") \
    X(ASTEROIDS_OFF, "小惑星などの粒 OFF", "Asteroids OFF") \
    X(PHOTO_1, "写真モード: 表示を隠す（Z で自機も隠す）", "Photo mode: overlays hidden (Z again to hide the ship)") \
    X(PHOTO_2, "写真モード: 表示と自機を隠す", "Photo mode: overlays and ship hidden") \
    X(DATE_TITLE, "日時を指定（UTC）", "Set date and time (UTC)") \
    X(DATE_KEYS, "数字を入力    Backspace : 1 文字消す    Enter : 決定    Esc / Y : 取消", "Type digits    Backspace : delete    Enter : apply    Esc / Y : cancel") \
    X(DATE_SET, "日時: %s", "Date: %s") \
    X(DATE_BAD, "日時が正しくありません（1800〜2200 年）", "Invalid date (years 1800-2200)") \
    X(SOUND_ON, "効果音: オン", "Sound: on") \
    X(SOUND_OFF, "効果音: オフ", "Sound: off") \
    X(SHOT_SAVED, "保存しました: ピクチャ/", "Saved: Pictures/") \
    X(SHOT_SAVED_CWD, "保存しました: ", "Saved: ") \
    X(SHOT_FAILED, "保存できませんでした: ", "Could not save: ") \
    X(QUALITY_0, "画質: 低（解像度 60%・細部を省略）", "Quality: Low (60% resolution, less detail)") \
    X(QUALITY_1, "画質: 中（細部を省略）", "Quality: Medium (less detail)") \
    X(QUALITY_2, "画質: 高", "Quality: High") \
    X(LANG_NO_FONT, "日本語フォントが見つかりません", "Japanese font not found") \
    X(PAD_ON, "コントローラ接続", "Controller connected") \
    X(PAD_OFF, "コントローラ切断", "Controller disconnected")

enum UiId {
#define X(id, jp, en) UI_##id,
    UI_STRINGS(X)
#undef X
    UI_COUNT
};
static const char* UI_JP[] = {
#define X(id, jp, en) jp,
    UI_STRINGS(X)
#undef X
};
static const char* UI_EN[] = {
#define X(id, jp, en) en,
    UI_STRINGS(X)
#undef X
};

#ifndef APP_VERSION
#define APP_VERSION "dev"
#endif
// 操作説明・設定画面の下に出す表記
static const char* CREDIT = "Solar System Voyager " APP_VERSION "   (c) 2026 hubstacksea   MIT License   "
                            "https://github.com/hubstacksea/Solar-System-Simulator";

static bool g_jp = false;       // 表示言語（true: 日本語）
static bool g_jpFont = false;   // 日本語フォントを読み込めたか
static Font g_font;
static inline const char* T(UiId id) { return g_jp ? UI_JP[id] : UI_EN[id]; }
static inline const char* TR(const char* jp, const char* en) { return g_jp ? jp : en; }
