cmake_minimum_required(VERSION 3.16)
project(SolarSystemVoyager C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

# ---- raylib (描画エンジン) ------------------------------------------------
# システムに raylib 5.5 があればそれを使い、無ければソースを取得して静的リンクする。
# オフライン環境では -DFETCHCONTENT_SOURCE_DIR_RAYLIB=<raylib-5.5 のソース> を指定。
find_package(raylib 5.5 QUIET)
if(NOT raylib_FOUND)
  include(FetchContent)
  set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
  set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
  # raylib 組込みの F12 スクリーンショットは DPI スケール時に壊れた画像になるため無効化（自前で実装）
  set(CUSTOMIZE_BUILD ON CACHE BOOL "" FORCE)
  set(SUPPORT_SCREEN_CAPTURE OFF CACHE BOOL "" FORCE)
  set(SUPPORT_GIF_RECORDING OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(raylib
    URL https://github.com/raysan5/raylib/archive/refs/tags/5.5.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
  FetchContent_MakeAvailable(raylib)
endif()

add_executable(solar_voyager src/main.cpp)
target_link_libraries(solar_voyager PRIVATE raylib)

if(MINGW)
  # DLL 不要の単体 exe にする
  target_link_options(solar_voyager PRIVATE -static -static-libgcc -static-libstdc++)
  option(SOLAR_CONSOLE "Windows でコンソールウィンドウを表示する" OFF)
  if(NOT SOLAR_CONSOLE)
    set_target_properties(solar_voyager PROPERTIES WIN32_EXECUTABLE TRUE)
  endif()
elseif(UNIX AND NOT APPLE)
  find_package(OpenGL REQUIRED)
  target_link_libraries(solar_voyager PRIVATE OpenGL::GL m)
endif()
