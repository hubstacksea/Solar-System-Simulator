# Solar System Voyager — Copyright (c) 2026 hubstacksea
# MIT License (LICENSE を参照) — https://github.com/hubstacksea/Solar-System-Simulator
# 取得した raylib 5.5 のソースに当てる修正（FetchContent の PATCH_COMMAND から、ソースのディレクトリで実行）。
# ウィンドウを作れない時（GPU の無い Mac の仮想マシンなど）、raylib は作れなかったウィンドウのまま GetCurrentMonitor を
# 呼んで落ちる。作れなかったらすぐ失敗を返すようにして、アプリが「ウィンドウを作れません」と出して終われるようにする
set(f src/platforms/rcore_desktop_glfw.c)
file(READ ${f} src)
set(anchor "        platform.handle = glfwCreateWindow(creationWidth, creationHeight, (CORE.Window.title != 0)? CORE.Window.title : \" \", NULL, NULL);\n")
string(FIND "${src}" "${anchor}" pos)
if(pos EQUAL -1)
  message(FATAL_ERROR "patch_raylib: the expected line was not found in ${f}")
endif()
string(FIND "${src}" "SolarSystemVoyager-patch" done)
if(done EQUAL -1)
  string(REPLACE "${anchor}" "${anchor}        if (!platform.handle) { glfwTerminate(); TRACELOG(LOG_WARNING, \"GLFW: Failed to initialize Window\"); return -1; }   // SolarSystemVoyager-patch\n" src "${src}")
  file(WRITE ${f} "${src}")
endif()
