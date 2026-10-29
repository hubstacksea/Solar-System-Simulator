cmake_minimum_required(VERSION 3.16)
project(SolarSystemVoyager VERSION 1.7.0 LANGUAGES C CXX)

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
  set(SUPPORT_FILEFORMAT_JPG ON CACHE BOOL "" FORCE)   # 地球の地表画像
  FetchContent_Declare(raylib
    URL https://github.com/raysan5/raylib/archive/refs/tags/5.5.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
  FetchContent_MakeAvailable(raylib)
endif()

add_executable(solar_voyager src/main.cpp src/embedded_assets.cpp src/platform_paths.cpp)
if(WIN32)
  # exe のファイルアイコンとバージョン情報
  enable_language(RC)
  set(ICON_PATH "${CMAKE_CURRENT_SOURCE_DIR}/assets/icon.ico")
  set(MANIFEST_PATH "${CMAKE_CURRENT_SOURCE_DIR}/packaging/solar_voyager.manifest")
  configure_file(packaging/solar_voyager.rc "${CMAKE_CURRENT_BINARY_DIR}/solar_voyager.rc" @ONLY)
  target_sources(solar_voyager PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/solar_voyager.rc")
  set_source_files_properties("${CMAKE_CURRENT_BINARY_DIR}/solar_voyager.rc" PROPERTIES OBJECT_DEPENDS "${ICON_PATH};${MANIFEST_PATH}")
endif()
# assets/ の地球データを実行ファイルに埋め込む
target_compile_definitions(solar_voyager PRIVATE ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets")
file(GLOB MUSIC_FILES "${CMAKE_CURRENT_SOURCE_DIR}/assets/music/*.ogg")
set_source_files_properties(src/embedded_assets.cpp PROPERTIES
  OBJECT_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/assets/earth_color.jpg;${CMAKE_CURRENT_SOURCE_DIR}/assets/earth_data.png;${CMAKE_CURRENT_SOURCE_DIR}/assets/moon_color.jpg;${CMAKE_CURRENT_SOURCE_DIR}/assets/moon_data.png;${CMAKE_CURRENT_SOURCE_DIR}/assets/icon.png;${CMAKE_CURRENT_SOURCE_DIR}/assets/stars.bin;${CMAKE_CURRENT_SOURCE_DIR}/assets/sky.txt;${CMAKE_CURRENT_SOURCE_DIR}/assets/probes.bin;${CMAKE_CURRENT_SOURCE_DIR}/assets/milkyway.jpg;${MUSIC_FILES}")
target_link_libraries(solar_voyager PRIVATE raylib)

if(MINGW)
  # DLL 不要の単体 exe にする
  target_link_options(solar_voyager PRIVATE -static -static-libgcc -static-libstdc++)
  target_link_libraries(solar_voyager PRIVATE shell32 ole32)   # ピクチャフォルダの取得
  option(SOLAR_CONSOLE "Windows でコンソールウィンドウを表示する" OFF)
  if(NOT SOLAR_CONSOLE)
    set_target_properties(solar_voyager PROPERTIES WIN32_EXECUTABLE TRUE)
  endif()
elseif(UNIX AND NOT APPLE)
  find_package(OpenGL REQUIRED)
  target_link_libraries(solar_voyager PRIVATE OpenGL::GL m)
endif()

# ---- インストール / パッケージ (cpack) ------------------------------------
# Windows: 7z アーカイブ（展開してそのまま使うポータブル版）
# Linux:   .deb / .rpm（/usr/bin・メニュー登録・アイコン）
# raylib 自体のインストール（ライブラリ・ヘッダ）は含めず、app コンポーネントだけを梱包する
if(WIN32)
  install(TARGETS solar_voyager RUNTIME DESTINATION . COMPONENT app)
  install(FILES README.md README.en.md LICENSE THIRD_PARTY_NOTICES.md DESTINATION . COMPONENT app)
else()
  include(GNUInstallDirs)
  install(TARGETS solar_voyager RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT app)
  install(FILES packaging/solar_voyager.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications COMPONENT app)
  install(FILES assets/icon.png DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps
          RENAME solar_voyager.png COMPONENT app)
  install(FILES README.md README.en.md LICENSE THIRD_PARTY_NOTICES.md DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/solar-voyager COMPONENT app)
endif()
set(CPACK_INSTALL_CMAKE_PROJECTS "${CMAKE_BINARY_DIR};${PROJECT_NAME};app;/")

set(CPACK_PACKAGE_NAME "solar-voyager")
set(CPACK_PACKAGE_VENDOR "Solar System Voyager")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "3D solar system flight simulator at true scale")
set(CPACK_PACKAGE_DESCRIPTION "Fly a ship through the Sun, planets and major moons placed at true size and distance, with positions computed from the PC clock.")
set(SOLAR_PACKAGE_CONTACT "Solar System Voyager developers" CACHE STRING ".deb の Maintainer 欄（名前 <メール>）")
set(CPACK_PACKAGE_CONTACT "${SOLAR_PACKAGE_CONTACT}")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "SolarSystemVoyager")
set(CPACK_STRIP_FILES ON)
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
if(WIN32)
  set(CPACK_GENERATOR "7Z")
  set(CPACK_PACKAGE_FILE_NAME "SolarSystemVoyager-${PROJECT_VERSION}-win64")
  set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY ON)
else()
  set(CPACK_GENERATOR "DEB;RPM")
  set(CPACK_PACKAGING_INSTALL_PREFIX "/usr")
  # .deb
  set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
  set(CPACK_DEBIAN_PACKAGE_SECTION "games")
  set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
  # GLFW が実行時に dlopen する X11 / GLX は自動検出されないので明示する
  set(CPACK_DEBIAN_PACKAGE_DEPENDS "libx11-6, libxrandr2, libxi6, libxcursor1, libxinerama1, libglx0")
  # .rpm
  set(CPACK_RPM_FILE_NAME RPM-DEFAULT)
  set(CPACK_RPM_PACKAGE_LICENSE "MIT")
  set(CPACK_RPM_PACKAGE_GROUP "Amusements/Games")
  set(CPACK_RPM_PACKAGE_AUTOREQ ON)
  set(CPACK_RPM_PACKAGE_REQUIRES "libX11, libXrandr, libXi, libXcursor, libXinerama, libglvnd-glx")
  set(CPACK_RPM_SPEC_MORE_DEFINE "%define _build_id_links none")
  # 既存のシステムディレクトリを rpm の所有物にしない
  set(CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST_ADDITION
      /usr/share/applications /usr/share/icons /usr/share/icons/hicolor
      /usr/share/icons/hicolor/256x256 /usr/share/icons/hicolor/256x256/apps /usr/share/doc)
endif()
include(CPack)
