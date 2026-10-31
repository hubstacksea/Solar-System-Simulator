cmake_minimum_required(VERSION 3.16)
project(SolarSystemVoyager VERSION 2.19.0 LANGUAGES C CXX)

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
  if(EMSCRIPTEN)
    # ブラウザ版: WebGL2（シェーダは GLSL ES 3.00 に書き換えて読む）
    set(PLATFORM Web CACHE STRING "" FORCE)
    set(OPENGL_VERSION "ES 3.0" CACHE STRING "" FORCE)
  endif()
  FetchContent_Declare(raylib
    URL https://github.com/raysan5/raylib/archive/refs/tags/5.5.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
  FetchContent_MakeAvailable(raylib)
endif()

if(EMSCRIPTEN)
  # ブラウザ版: データは実行ファイルに埋め込まず、.data ファイルにまとめてページと一緒に配る
  add_executable(solar_voyager src/main.cpp src/embedded_assets_files.cpp src/platform_paths.cpp)
  option(SOLAR_WEB_MUSIC "ブラウザ版に音楽を入れる（約 17 MB 増える）" OFF)
  set(WEB_LINK -sUSE_GLFW=3 -sASYNCIFY -sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=268435456 -sSTACK_SIZE=4194304
      -sMIN_WEBGL_VERSION=2 -sMAX_WEBGL_VERSION=2 -sFULL_ES3=1
      "--preload-file=${CMAKE_CURRENT_SOURCE_DIR}/web/NotoSansJP-subset.ttf@/assets/font.ttf"
      "--shell-file=${CMAKE_CURRENT_SOURCE_DIR}/web/shell.html")
  # 起動に要るものだけを index.data に入れ（約 8 MB）、大きなデータはページの assets/ に置いて起動後に取得する
  set(WEB_PRELOAD stars.bin sky.txt probes.bin earth_color.jpg earth_data.png milkyway.jpg saturn_rings.png icon.png)
  set(WEB_DEFERRED maps.bin shapes.bin moon_data.png moon_color.jpg earth_clouds.jpg asteroids.bin earth_months.jpg)
  foreach(f ${WEB_PRELOAD})
    list(APPEND WEB_LINK "--preload-file=${CMAKE_CURRENT_SOURCE_DIR}/assets/${f}@/assets/${f}")
  endforeach()
  if(SOLAR_WEB_MUSIC)
    list(APPEND WEB_LINK "--preload-file=${CMAKE_CURRENT_SOURCE_DIR}/assets/music@/assets/music")
  endif()
  foreach(f ${WEB_DEFERRED})
    add_custom_command(TARGET solar_voyager POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different
                       "${CMAKE_CURRENT_SOURCE_DIR}/assets/${f}" "${CMAKE_CURRENT_BINARY_DIR}/assets/${f}")
  endforeach()
  target_link_options(solar_voyager PRIVATE ${WEB_LINK})
  set_target_properties(solar_voyager PROPERTIES SUFFIX ".html" OUTPUT_NAME "index")
  target_compile_definitions(solar_voyager PRIVATE APP_VERSION="${PROJECT_VERSION}")
  target_link_libraries(solar_voyager PRIVATE raylib)
else()
add_executable(solar_voyager src/main.cpp src/platform_paths.cpp)
if(APPLE)
  # macOS: データは埋め込まず .app の Resources/assets に 1 回分だけ置く（Apple Silicon 用と Intel 用で共有）
  target_sources(solar_voyager PRIVATE src/embedded_assets_files.cpp)
  file(GLOB APP_ASSETS "${CMAKE_CURRENT_SOURCE_DIR}/assets/*.jpg" "${CMAKE_CURRENT_SOURCE_DIR}/assets/*.png"
                       "${CMAKE_CURRENT_SOURCE_DIR}/assets/*.bin" "${CMAKE_CURRENT_SOURCE_DIR}/assets/*.txt")
  file(GLOB APP_MUSIC "${CMAKE_CURRENT_SOURCE_DIR}/assets/music/*.ogg")
  target_sources(solar_voyager PRIVATE ${APP_ASSETS} ${APP_MUSIC})
  set_source_files_properties(${APP_ASSETS} PROPERTIES MACOSX_PACKAGE_LOCATION Resources/assets)
  set_source_files_properties(${APP_MUSIC} PROPERTIES MACOSX_PACKAGE_LOCATION Resources/assets/music)
else()
  target_sources(solar_voyager PRIVATE src/embedded_assets.cpp)
endif()
if(WIN32)
  # exe のファイルアイコンとバージョン情報
  enable_language(RC)
  set(ICON_PATH "${CMAKE_CURRENT_SOURCE_DIR}/assets/icon.ico")
  set(MANIFEST_PATH "${CMAKE_CURRENT_SOURCE_DIR}/packaging/solar_voyager.manifest")
  configure_file(packaging/solar_voyager.rc "${CMAKE_CURRENT_BINARY_DIR}/solar_voyager.rc" @ONLY)
  target_sources(solar_voyager PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/solar_voyager.rc")
  set_source_files_properties("${CMAKE_CURRENT_BINARY_DIR}/solar_voyager.rc" PROPERTIES OBJECT_DEPENDS "${ICON_PATH};${MANIFEST_PATH}")
endif()
# assets/ のデータを実行ファイルに埋め込む（macOS 以外）
target_compile_definitions(solar_voyager PRIVATE ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets" APP_VERSION="${PROJECT_VERSION}")
file(GLOB MUSIC_FILES "${CMAKE_CURRENT_SOURCE_DIR}/assets/music/*.ogg")
set_source_files_properties(src/embedded_assets.cpp PROPERTIES
  OBJECT_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/assets/earth_color.jpg;${CMAKE_CURRENT_SOURCE_DIR}/assets/earth_data.png;${CMAKE_CURRENT_SOURCE_DIR}/assets/moon_color.jpg;${CMAKE_CURRENT_SOURCE_DIR}/assets/moon_data.png;${CMAKE_CURRENT_SOURCE_DIR}/assets/icon.png;${CMAKE_CURRENT_SOURCE_DIR}/assets/stars.bin;${CMAKE_CURRENT_SOURCE_DIR}/assets/sky.txt;${CMAKE_CURRENT_SOURCE_DIR}/assets/probes.bin;${CMAKE_CURRENT_SOURCE_DIR}/assets/milkyway.jpg;${CMAKE_CURRENT_SOURCE_DIR}/assets/shapes.bin;${CMAKE_CURRENT_SOURCE_DIR}/assets/maps.bin;${CMAKE_CURRENT_SOURCE_DIR}/assets/asteroids.bin;${CMAKE_CURRENT_SOURCE_DIR}/assets/saturn_rings.png;${CMAKE_CURRENT_SOURCE_DIR}/assets/earth_months.jpg;${CMAKE_CURRENT_SOURCE_DIR}/assets/earth_clouds.jpg;${MUSIC_FILES}")
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
elseif(APPLE)
  # macOS: SolarSystemVoyager.app（アイコンは iconutil があれば icon.png から作る。
  # Retina で細かく描けるよう、Info.plist は NSHighResolutionCapable を入れた自前のひな形）
  set_target_properties(solar_voyager PROPERTIES
    MACOSX_BUNDLE TRUE
    OUTPUT_NAME "SolarSystemVoyager"
    MACOSX_BUNDLE_BUNDLE_NAME "Solar System Voyager"
    MACOSX_BUNDLE_GUI_IDENTIFIER "io.github.hubstacksea.solarvoyager"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    MACOSX_BUNDLE_COPYRIGHT "(c) 2026 hubstacksea, MIT License"
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/packaging/Info.plist.in")
  find_program(ICONUTIL iconutil)
  find_program(SIPS sips)
  if(ICONUTIL AND SIPS)
    set(ICONSET "${CMAKE_CURRENT_BINARY_DIR}/SolarSystemVoyager.iconset")
    set(ICNS "${CMAKE_CURRENT_BINARY_DIR}/SolarSystemVoyager.icns")
    set(ICON_CMDS COMMAND ${CMAKE_COMMAND} -E make_directory "${ICONSET}")
    foreach(sz 16 32 128 256)
      math(EXPR sz2 "${sz} * 2")
      list(APPEND ICON_CMDS COMMAND ${SIPS} -z ${sz} ${sz} "${CMAKE_CURRENT_SOURCE_DIR}/assets/icon.png" --out "${ICONSET}/icon_${sz}x${sz}.png")
      if(sz2 LESS_EQUAL 256)
        list(APPEND ICON_CMDS COMMAND ${SIPS} -z ${sz2} ${sz2} "${CMAKE_CURRENT_SOURCE_DIR}/assets/icon.png" --out "${ICONSET}/icon_${sz}x${sz}@2x.png")
      endif()
    endforeach()
    add_custom_command(OUTPUT "${ICNS}" ${ICON_CMDS} COMMAND ${ICONUTIL} -c icns "${ICONSET}" -o "${ICNS}"
                       DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/assets/icon.png" VERBATIM)
    target_sources(solar_voyager PRIVATE "${ICNS}")
    set_source_files_properties("${ICNS}" PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    set_target_properties(solar_voyager PROPERTIES MACOSX_BUNDLE_ICON_FILE "SolarSystemVoyager.icns")
  endif()
endif()
endif()   # EMSCRIPTEN

# ---- インストール / パッケージ (cpack) ------------------------------------
# Windows: 7z アーカイブ（展開してそのまま使うポータブル版）
# Linux:   .deb / .rpm（/usr/bin・メニュー登録・アイコン）
# macOS:   .zip（SolarSystemVoyager.app と説明書。署名は ad-hoc）
# raylib 自体のインストール（ライブラリ・ヘッダ）は含めず、app コンポーネントだけを梱包する
if(WIN32)
  install(TARGETS solar_voyager RUNTIME DESTINATION . COMPONENT app)
  install(FILES README.md README.en.md CHANGELOG.md CHANGELOG.en.md LICENSE THIRD_PARTY_NOTICES.md DESTINATION . COMPONENT app)
elseif(APPLE)
  install(TARGETS solar_voyager BUNDLE DESTINATION . COMPONENT app)
  install(FILES README.md README.en.md CHANGELOG.md CHANGELOG.en.md LICENSE THIRD_PARTY_NOTICES.md DESTINATION . COMPONENT app)
  # 置いた後に署名し直す（Apple Silicon は署名のない実行ファイルを起動できない）
  install(CODE "execute_process(COMMAND codesign --force --deep -s - \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/SolarSystemVoyager.app\")"
          COMPONENT app)
else()
  include(GNUInstallDirs)
  install(TARGETS solar_voyager RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT app)
  install(FILES packaging/solar_voyager.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications COMPONENT app)
  install(FILES assets/icon.png DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps
          RENAME solar_voyager.png COMPONENT app)
  install(FILES README.md README.en.md CHANGELOG.md CHANGELOG.en.md LICENSE THIRD_PARTY_NOTICES.md DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/solar-voyager COMPONENT app)
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
elseif(APPLE)
  set(CPACK_GENERATOR "ZIP")
  set(CPACK_PACKAGE_FILE_NAME "SolarSystemVoyager-${PROJECT_VERSION}-macos")
  set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY ON)
  set(CPACK_STRIP_FILES OFF)   # 署名した後に書き換えない
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
  # 変更履歴は自前で 1 行だけ書く（CPack の既定の履歴には CPack 作者の名前とアドレスが入るため）
  string(TIMESTAMP RPM_DATE "%a %b %d %Y" UTC)
  file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/rpm_changelog.txt"
       "* ${RPM_DATE} hubstacksea - ${PROJECT_VERSION}-1\n- Solar System Voyager ${PROJECT_VERSION} (https://github.com/hubstacksea/Solar-System-Simulator)\n")
  set(CPACK_RPM_CHANGELOG_FILE "${CMAKE_CURRENT_BINARY_DIR}/rpm_changelog.txt")
  # 既存のシステムディレクトリを rpm の所有物にしない
  set(CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST_ADDITION
      /usr/share/applications /usr/share/icons /usr/share/icons/hicolor
      /usr/share/icons/hicolor/256x256 /usr/share/icons/hicolor/256x256/apps /usr/share/doc)
endif()
include(CPack)
