# Solar System Voyager

[日本語](README.md) | **English** | [Changelog](CHANGELOG.en.md)

A 3D space flight simulator with the Sun, planets, major moons, dwarf planets, asteroids, comets and spacecraft, and the stars within about 10 light-years of the Sun with their planets, placed at **true size and true distance**.
Body positions are computed from the PC clock (UTC); at 1x time scale they orbit and rotate in real time.

- Rendering engine: [raylib 5.5](https://www.raylib.com/) (OpenGL 3.3)
- Platforms: Linux (gcc) / Windows (MinGW-w64)

![Daytime Earth seen from above Japan](docs/screenshots/earth_day.jpg)

| | |
|---|---|
| ![Saturn and its rings](docs/screenshots/saturn.jpg) Saturn and its rings (with the rings' shadow on the planet) | ![City lights of Europe at night](docs/screenshots/earth_night.jpg) City lights of Europe at night |
| ![The Moon's near side](docs/screenshots/moon.jpg) The Moon's near side (real NASA data) | ![Low flight near Tycho](docs/screenshots/moon_surface.jpg) Low flight near Tycho |
| ![Solar eclipse of 2 August 2027](docs/screenshots/solar_eclipse.jpg) Solar eclipse of 2 August 2027 (the Moon's shadow over North Africa) | ![Total lunar eclipse of 7 September 2025](docs/screenshots/lunar_eclipse.jpg) Total lunar eclipse of 7 September 2025 |
| ![Jupiter](docs/screenshots/jupiter.jpg) Jupiter | ![Warp](docs/screenshots/warp.jpg) Warp effect |
| ![Constellation lines and names](docs/screenshots/constellations.jpg) The real starry sky with constellation lines (C key) | ![Halley's Comet on 9 February 1986](docs/screenshots/halley.jpg) Halley's Comet on 9 February 1986 (perihelion) |
| ![Voyager 1 at Jupiter](docs/screenshots/voyager1_jupiter.jpg) Voyager 1 at closest approach to Jupiter, 5 March 1979 (K key, spacecraft page) | ![Voyager 2 at Neptune](docs/screenshots/voyager2_neptune.jpg) Voyager 2 passing over Neptune's night side, 25 August 1989 |
| ![Transit of Mercury, 13 November 2032](docs/screenshots/mercury_transit.jpg) Transit of Mercury, 13 November 2032 | ![Comet Hyakutake near Earth](docs/screenshots/hyakutake.jpg) Comet Hyakutake passing 0.1 AU from Earth, 25 March 1996 |
| ![Cassini arriving at Saturn](docs/screenshots/cassini.jpg) Cassini arriving at Saturn, 1 July 2004 (Huygens probe on its side) | ![Jupiter against the Milky Way](docs/screenshots/milky_way.jpg) Jupiter against the Milky Way |
| ![Dusk over the Pacific](docs/screenshots/sunset.jpg) Dusk over the Pacific | ![Daytime sky on Mars](docs/screenshots/mars_sky.jpg) The butterscotch daytime sky of Mars |
| ![Rosetta arriving at Comet Churyumov-Gerasimenko](docs/screenshots/rosetta.jpg) Guided tour (U key): Rosetta arriving at its comet, 6 August 2014 | ![Juno entering orbit around Jupiter](docs/screenshots/juno.jpg) Juno entering orbit around Jupiter, 5 July 2016 |
| ![Hazy horizon from 10 km](docs/screenshots/haze.jpg) The hazy horizon from 10 km up | ![Saturn in photo mode](docs/screenshots/saturn_photo.jpg) Saturn in photo mode (Z key) |
| ![Comet Churyumov-Gerasimenko](docs/screenshots/comet_67p.jpg) Comet Churyumov-Gerasimenko in its real shape (August 2015, perihelion) | ![Ryugu](docs/screenshots/ryugu.jpg) The spinning-top asteroid Ryugu |
| ![Mars with Valles Marineris](docs/screenshots/mars_map.jpg) Mars from Viking maps and MOLA elevations (Valles Marineris in the middle) | ![Pluto with its heart](docs/screenshots/pluto.jpg) Pluto from New Horizons maps |
| ![Io](docs/screenshots/io.jpg) Io from Galileo / Voyager maps | ![Sunset over the sea](docs/screenshots/sunset_sea.jpg) Sunset over the sea |
| ![Asteroid belt and Trojans](docs/screenshots/asteroid_belt.jpg) The main belt (ring) and Jupiter's Trojans (green) from 15 AU above | ![Jupiter with the Great Red Spot](docs/screenshots/jupiter_map.jpg) Jupiter from Cassini maps (Great Red Spot at lower right) |
| ![Total solar eclipse from the ground](docs/screenshots/eclipse_ground.jpg) The total eclipse of 2 August 2027 from the ground (corona) | ![One minute before DART's impact](docs/screenshots/dart_impact.jpg) Didymos and Dimorphos one minute before DART's impact |
| ![Itokawa](docs/screenshots/itokawa.jpg) Itokawa in its real shape | ![Saturn's rings from Voyager data](docs/screenshots/saturn_rings.jpg) Saturn's rings with the density measured by Voyager 2 |
| ![International Space Station](docs/screenshots/iss.jpg) The International Space Station above Earth | ![Gravity mode](docs/screenshots/gravity_mode.jpg) Gravity mode (R key): the predicted orbit around Earth |
| ![Browser version](docs/screenshots/browser.jpg) The browser version (WebGL2) | ![Touch controls on a phone](docs/screenshots/touch.jpg) Touch controls on a phone in landscape (during the tour) |
| ![First-run guide](docs/screenshots/first_run.jpg) The guide shown on the first launch | ![Share link](docs/screenshots/share_link.jpg) A scene opened from a share link (above Mars; F2 copies one) |
| ![Extra large text](docs/screenshots/settings_large.jpg) The settings with text size "Extra large" | ![System map](docs/screenshots/system_map.jpg) The system map (F3; 1989 with the path of Voyager 1) |
| ![Body info](docs/screenshots/body_info.jpg) The info card (I key; Jupiter) | ![Rings of Uranus](docs/screenshots/uranus_rings.jpg) Tipped-over Uranus and its narrow rings |
| ![Europa Clipper](docs/screenshots/europa_clipper.jpg) Europa Clipper on its way to Jupiter (30 m solar array span) | ![Southern sky from Mauna Kea](docs/screenshots/sky_from_ground.jpg) The night sky to the south from the summit of Mauna Kea (search, "On Earth") |
| ![From the ground in Tokyo](docs/screenshots/ground_view.jpg) Before dawn in Tokyo: the horizon, compass points and Venus's altitude, azimuth and rise/set times (lower left) | ![Geminids night](docs/screenshots/geminids.jpg) Geminids night in Tokyo (a meteor at the upper left), with Jupiter's magnitude, phase and apparent size on the right |
| ![Frame fixed to the ship](docs/screenshots/frame_lock.jpg) Reference frame fixed to the ship (Shift+F): Earth leaving at 30 km/s (top right) | ![Sirius A](docs/screenshots/sirius.jpg) Sirius A, 8.7 light-years away (top left: the white dwarf Sirius B, 30 AU away) |
| ![Interstellar warp](docs/screenshots/interstellar_warp.jpg) Interstellar warp (the sky crowds forward and turns blue as you pass through a swirling tunnel of light) | ![Epsilon Eridani b](docs/screenshots/exoplanet.jpg) Epsilon Eridani b, a Jupiter-like planet 10.5 light-years away |
| ![Stars around the Sun](docs/screenshots/star_map.jpg) The system map's "stars around the Sun" (zoom all the way out; the stems show the height above the ecliptic) | ![On Proxima Centauri b](docs/screenshots/proxima_b_ground.jpg) On the ground of Proxima Centauri b (artist's impression): the red star hangs still in the sky |
| ![The Big Dipper now and in 100,000 years](docs/screenshots/star_epoch.jpg) The "Star epoch" setting: the Big Dipper now (left) and in 100,000 years (right), deformed by the proper motions of its stars | ![Habitable zone](docs/screenshots/habitable_zone.jpg) The system map centered on Proxima Centauri with its habitable zone (green); planet b orbits inside it |
| ![Brown dwarf](docs/screenshots/brown_dwarf.jpg) The brown dwarf Luhman 16 B with cloud bands and patchy clouds | ![Altair](docs/screenshots/altair.jpg) Altair (16.7 light-years), flattened by its fast spin |
| ![Hyperdrive](docs/screenshots/hyperdrive.jpg) The hyperdrive (8): one light-year per second toward Alpha Centauri | ![At 99% of light speed](docs/screenshots/relativistic.jpg) At 99% of the speed of light the stars and the Milky Way crowd forward and turn blue and bright (aberration, Doppler effect) |
| ![Juno](docs/screenshots/probe_view.jpg) Warping to a spacecraft: centered and large in a zoomed view (Juno at Jupiter) | ![Eddington's eclipse](docs/screenshots/historic_eclipse.jpg) Historic eclipses: the 1919 total eclipse on Principe where Eddington's team confirmed general relativity |
| ![The supernova of 1054](docs/screenshots/supernova_1054.jpg) July 1054: the supernova that left the Crab Nebula, before dawn over Kaifeng, the Song capital | ![Shoemaker-Levy 9 scars](docs/screenshots/shoemaker_levy9.jpg) 18 July 1994: scars left on Jupiter by fragments of Comet Shoemaker-Levy 9 |
| ![Jupiter as Galileo saw it](docs/screenshots/galileo_1610.jpg) 7 January 1610: Jupiter and its moons as Galileo saw them from Padua (two to the east, one to the west) | ![Andromeda Galaxy](docs/screenshots/andromeda.jpg) The Andromeda Galaxy zoomed in, with its companions M32 and M110 |
| ![Carina Nebula](docs/screenshots/carina_nebula.jpg) The Carina Nebula (names shown with the C key) | ![BepiColombo arriving at Mercury](docs/screenshots/voyage_bepicolombo.jpg) Spacecraft voyage: BepiColombo just before entering Mercury orbit in November 2026 (top right: a map of its path so far) |
| ![Asteroid Dinkinesh](docs/screenshots/voyage_dinkinesh.jpg) Spacecraft voyage: asteroid Dinkinesh, passed by Lucy at 422 km in November 2023 (zoomed in automatically) | |
| ![Sirius B](docs/screenshots/sirius_b.jpg) Guided tour: the white dwarf Sirius B, with Sirius A 20 AU away | ![A flare on Proxima Centauri](docs/screenshots/red_dwarf_flare.jpg) The spotted red dwarf Proxima Centauri and a blue-white flare |

## Getting started

On the first launch a short guide (4 pages) introduces the controls and highlights (Enter or tap for next, Esc to close). Later, open the controls with H.

## Browser version

Try it in your browser without installing anything: **https://hubstacksea.github.io/Solar-System-Simulator/**

- Runs in browsers with WebGL2 (recent Chrome / Edge / Firefox / Safari). Only the 8 MB needed to start is loaded first; the planet and moon maps, asteroid shapes, Moon maps, Earth's clouds and seasons and the asteroid belt (about 25 MB) are loaded in the background while you play. Until they arrive, surfaces are drawn procedurally and "Loading maps (N left)" is shown at the bottom left
- Music is not included (sound effects are). Japanese text uses the bundled Noto Sans JP font
- Click the view to steer with the mouse (Esc to release). Settings and bookmarks are kept in the browser
- On phones and tablets, on-screen buttons appear: drag to steer, Thrust / Brake / Stop on the right, Target / Warp / Autopilot on the left, Tour / Events / Map / Settings / Help / Share at the top, and tap the engine bar to switch engines. Quality starts at Low, and small screens are rendered at 720 pixels high and scaled down
- If it runs slowly, lower the quality with F9
- High-resolution screens (phones, Retina) are drawn at the device pixel ratio (up to 1440 pixels high). At Low quality the 3D view keeps its pixel count and only text and the HUD are drawn sharper
- **F2 (or "Share" on touch screens) copies a link to the current scene** (date, place, view, target and time scale; the address bar shows the same link). Opening the link starts from that scene. F2 in the PC version copies a link to the browser version
- **The display language can be set in the URL with `lang=ja` or `lang=en`** (for example `https://hubstacksea.github.io/Solar-System-Simulator/?lang=en`, or `&lang=en` added to a share link). Without it, the language chosen with J or in the settings is used (Japanese on the first visit). The language given in the URL is not saved
- Page views of the browser version are counted with [Cloudflare Web Analytics](https://www.cloudflare.com/web-analytics/). It uses no cookies and sends no personal information or details of what you do in the app. The PC and macOS versions send nothing

## macOS version

Download `SolarSystemVoyager-<version>-macos.zip` (macOS 11 or later, Apple Silicon and Intel) from the [releases](https://github.com/hubstacksea/Solar-System-Simulator/releases), extract it and move `SolarSystemVoyager.app` to Applications or anywhere you like.

- The app is not notarized by Apple, so the first launch shows an "unidentified developer" warning. **Right-click (Control-click) the app → Open → Open.** If macOS 15 or later does not offer that, try to launch it once, then press **System Settings → Privacy & Security → Open Anyway**
- If it still does not open, run `xattr -dr com.apple.quarantine SolarSystemVoyager.app` in Terminal and try again
- Settings and bookmarks are saved in `~/Library/Application Support/SolarSystemVoyager/solar_voyager.cfg`. Japanese text uses Hiragino Sans. Retina displays are drawn at full resolution
- It is built by GitHub Actions but has not been tested on the developer's machine. Please report problems in Issues

## Building

CMake downloads the raylib 5.5 source automatically and links it statically (network access is needed on the first build only).

### Linux

```sh
# Dependencies on Debian/Ubuntu
sudo apt install build-essential cmake libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl1-mesa-dev
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/solar_voyager
```

### Windows (MSYS2 MinGW64)

```sh
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/solar_voyager.exe
```

The result is a standalone exe that needs no DLLs. Add `-DSOLAR_CONSOLE=ON` if you want a console window.

### macOS

```sh
xcode-select --install   # command line tools (clang)
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
cmake --build build -j
open build/SolarSystemVoyager.app
```

### Automatic builds and releases (GitHub Actions)

Pushing a tag such as `v2.37.7` runs `.github/workflows/release.yml`, which builds Linux (.deb / .rpm), Windows (.7z), macOS (.zip) and the browser version, creates the GitHub release with the packages, and publishes the browser version to the `gh-pages` branch for GitHub Pages. The Linux build is also launched to take screenshots (Earth, Mars, the map and the English settings) and logs, saved as `screenshots-linux` in the run (a failed launch does not stop the release). The macOS build is only checked for reading its data, because GitHub's Mac virtual machines have no GPU and cannot open a window. The browser version is also opened in headless Chrome (`tools/web_smoke.py`, including voyage links and key, click and drag input), and its screen and console output are saved as `screenshots-web`.

```sh
git tag v2.37.7
git push origin master v2.37.7
```

### Checks before a release

```sh
python3 tools/run_checks.py build/solar_voyager   # --no-display without a screen
```

It checks the embedded data, 9 solar eclipses against NASA's catalog, the number of historic events, that the browser version's font has every character shown on screen (if not, rebuild it with `tools/make_web_font.py`), warps to every body (arriving facing it and staying still) and light-speed flights just above the oblate planets (the surface must not vanish), printing PASS / FAIL for each (about 10 minutes in all).

### Browser version (Emscripten)

```sh
emcmake cmake -B build-web -DCMAKE_BUILD_TYPE=Release   # add -DSOLAR_WEB_MUSIC=ON to include the music
cmake --build build-web -j
python3 -m http.server -d build-web 8000                 # open http://localhost:8000/
```

Put `build-web/index.html`, `index.js`, `index.wasm`, `index.data` and the `assets/` folder on any web server. The data is not embedded: what is needed to start is packed into `index.data`, and the large maps and shapes are fetched from `assets/` after startup (see `WEB_PRELOAD` / `WEB_DEFERRED` in `CMakeLists.txt`), and the shaders are rewritten for WebGL2 (GLSL ES 3.00) at startup. The Japanese font `web/NotoSansJP-subset.ttf` is made by `tools/make_web_font.py`.

### Cross-building the Windows version on Linux

```sh
cmake -B build-win -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-x86_64.cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build-win -j
```

## Packaging

After building, run `cpack` in the build directory.

| Target | Command | Output |
|---|---|---|
| Windows (7-Zip) | `cd build-win && cpack` | `SolarSystemVoyager-<version>-win64.7z` (portable: extract and run) |
| Linux (.deb / .rpm) | `cd build && cpack` | `solar-voyager_<version>_amd64.deb` / `solar-voyager-<version>-1.x86_64.rpm` |
| macOS (.zip) | `cd build && cpack` | `SolarSystemVoyager-<version>-macos.zip` (the maps and other data sit in the `.app` Resources instead of the executable, shared by the Apple Silicon and Intel code; ad-hoc signature) |

- The Linux packages install `/usr/bin/solar_voyager` and add it, with its icon, to the application menu (Games). Building the `.rpm` requires `rpmbuild`
- To build only one: `cpack -G DEB` / `cpack -G RPM`
- The version comes from `project(... VERSION ...)` in `CMakeLists.txt`; set the `.deb` Maintainer field with `-DSOLAR_PACKAGE_CONTACT="Name <email>"`
- The exe file icon and the window icon are `assets/icon.ico` / `assets/icon.png` (generated by `python3 tools/make_icon.py`; requires Pillow)

## Controls

| Key | Action |
|---|---|
| Mouse / arrow keys | Pitch and yaw |
| A / D | Yaw; Q / E: roll |
| W | Accelerate (speed is kept after release) |
| S | Decelerate / reverse |
| X | Stop |
| 1–8 / wheel | Switch engine |
| Tab / Shift+Tab | Cycle target |
| F | Point the nose at the target |
| Shift+F | Fix the reference frame to the ship / back to automatic (also in the settings) |
| G / Shift+G | Warp to target / warp to near its surface (with a ~1.7 s warp animation, ~4.6 s for an interstellar warp to another star; controls are locked during it) |
| T / Shift+T | Autopilot / autopilot with automatic engine selection |
| Right mouse button (hold) | Zoom |
| V | Chase view / cockpit view |
| O / L | Orbit lines / moon markers |
| C | Constellations (off → lines → lines and names) |
| [ / ] | Time scale (x1 real time to 1 year per second). Affects both orbits/rotation of the bodies and the ship's acceleration and movement |
| P / Backspace | Pause time / sync to current time at x1 |
| Y | Set the date and time (UTC; type the digits and press Enter; 3000 BC to AD 2999, - for BC; Julian calendar before 15 October 1582) |
| F10 | Settings (language, text size, quality, sound effects, music, constellations, Milky Way & nebulae, star epoch, orbit lines, asteroids, markers, invert mouse, flight mode, resume, fullscreen; Up/Down to select, Left/Right / Enter to change, saved at once). Text size is Normal / Large / Extra large and enlarges the HUD, markers and panels together (limited so they still fit on small screens). On Windows and Linux with display scaling above 100% (4K screens etc.), the window and text start enlarged to match |
| R | Switch the flight model (arcade / gravity). In gravity mode you fall under the bodies' gravity: W / S thrust along the nose and X stops relative to the reference body. The predicted trajectory is drawn, with the periapsis and apoapsis altitudes and the period |
| Z | Photo mode (each press: hide gauges and markers → hide the ship too → back to normal; orbit lines are toggled with O) |
| U | Guided tour: 24 scenes (spacecraft highlights, great comets, eclipses, nearby stars, historic eclipses and more) in chronological order with captions, 15 seconds each. Enter for next, Shift+Enter for previous, U again to stop |
| K | Event list (select one and press Enter to warp to that time and place). Tab / Shift+Tab switches pages: astronomical events (solar and lunar eclipses, planetary conjunctions and oppositions over the next 3 years, the next shadow transit of each Galilean moon across Jupiter, and transits of Mercury and Venus up to 150 years ahead), comets and binary stars (comet perihelia and closest approaches to Earth, asteroid Apophis's 2029 Earth encounter from space and from the ground, binary periastra) and spacecraft events (closest approaches, orbit insertions, end of mission) and historic eclipses, supernovae, returns of Halley's Comet, transits of Venus and a comet hitting Jupiter; spacecraft voyages (see "Spacecraft voyages" below) |
| I | Info card for the target (type, radius, mass, surface gravity, escape velocity, rotation and orbital periods, orbit, number of moons, distances from the Sun / Earth / you, brightness (magnitude), phase, apparent size and Moon age as seen from Earth (or from where you stand on the ground), spacecraft that flew by; data period for spacecraft, altitude and period for satellites). "Info" button on touch screens, tap the card to close. Mouse inversion is in the settings (F10) |
| J | Switch language (日本語 / English) |
| B / N | Bookmark the current spot / warp to bookmarks in turn (up to 9; position and orientation are stored relative to the rotating body) |
| M / Shift+M | Sound effects (engine and warp) / music on/off |
| F9 | Quality High → Low → Medium (use Low if it runs slowly: 3D at 60% resolution with less detail) |
| / or F4 | Find a body (type part of a name in kana or Latin letters, Up/Down to select, Enter to target, Shift+Enter to warp, Tab for the category; on touch screens use the "Search" button and tap the category tabs and the list. Spacecraft not in flight at that date are shown dimmed; choosing one moves the date to its highlight (such as its arrival) and warps there. The "On Earth" tab places you on the ground at one of 20 places on Earth such as Tokyo, London or the South Pole, on the Moon (the Apollo 11 and 17 sites, the lunar south pole, Chang'e 4 on the far side) or on Mars (Perseverance, Curiosity, Viking 1), looking up at the sky) |
| F3 | System map (seen from above, centered on the Sun or a planet, logarithmic distances). Click / tap a body to target it, again (or Enter / G) to warp, with Shift to the surface. Up/Down / wheel / + - to zoom, Space / "Center" to switch between the Sun and the target's planet, Esc / F3 to close. Zooming all the way out from the Sun turns it into a map of the stars within 18 light-years of the Sun (seen obliquely, with stems showing the height above the ecliptic; it opens this way when you are far from the Sun). Probe trajectories are drawn too (brighter where they have been). While it is open, Left/Right / << >> change the time speed, P / || pauses and R / <- runs time backwards, so you can watch the planets orbit and the probes travel (reverse ends when the map is closed) |
| F2 | Copy a share link to this scene (date, place, view, target, time scale, zoom; opening it in the browser version starts from the same scene; the PC version opens one with `--share <link>`); during a spacecraft voyage, a link to that scene |
| F11 / F12 | Fullscreen / screenshot (saved to `SolarSystemVoyager` in your Pictures folder) |
| Esc | Release / capture the mouse |
| Ctrl+Q | Quit |

### Controller (gamepad)

Common pads listed in the SDL controller database (Xbox, PlayStation, Switch Pro, etc.) are supported.
They are detected automatically when connected (hot-plugging works). Button names use Xbox labels (PS: A=×, B=○, X=□, Y=△).

| Input | Action |
|---|---|
| Left stick | Pitch and yaw (invert Y with the I key) |
| Right stick horizontal / LB, RB | Roll |
| RT / LT | Accelerate / decelerate, reverse (proportional to trigger pressure) |
| D-pad left/right | Switch engine |
| D-pad up/down | Time scale |
| A | Autopilot on/off |
| B | Stop |
| X | Next target |
| Y / left stick click | Warp to target / warp to near its surface |
| Right stick click (hold) | Zoom |
| View (Back) / Menu (Start) | Switch view / help (also a menu: A map, X settings, Y events, LB set date, RB body info, B close) |
| Settings / events / date / guide | D-pad Up/Down to select, Left/Right / A to change or confirm, B to close. X / RB switch event pages. In the date entry, Up/Down ±1 day, Left/Right ±30 days, LB / RB ±1 year, A to set |
| LB + RB (together) | System map. While open: D-pad Left/Right: target, D-pad Up/Down: zoom, A / Y: warp, X: switch center, B: close |

## Autopilot

Automatically turns, accelerates, cruises and decelerates toward the target, stopping at about 3 times the target's radius (the same position a warp puts you).

- Deceleration follows a braking curve (v = √(2 · acceleration · remaining distance)), so speed never exceeds what the current engine can stop from
- It does not overshoot the arrival point even at high time scales
- It detours around bodies in the way. When starting from a surface toward a target below the horizon, it climbs tangentially first
- **Shift+T (automatic engine selection)**: picks the slowest engine that arrives within about 60 seconds of real time (time scale included) and shifts down as it approaches
- Cancelled by manual input (steering, W/S, X) or a warp. Switching targets redirects it to the new target
- Targets keep moving along their orbits, so slow engines may never catch a planet (Earth orbits at about 30 km/s)

## Engines

| # | Engine | Acceleration | Top speed |
|---|---|---|---|
| 1 | Car | 3.0 m/s² | 100 km/h |
| 2 | Train | 0.8 m/s² | 130 km/h |
| 3 | Bullet train | 0.72 m/s² | 320 km/h |
| 4 | Mach 1 | 10 m/s² | 343 m/s (1,235 km/h) |
| 5 | Jet | 15 m/s² | 850 m/s (Mach 2.5) |
| 6 | Rocket engine | 30 m/s² | 50 km/s |
| 7 | Light speed | 3×10⁷ m/s² | 299,792.458 km/s |
| 8 | Hyperdrive (fictional, for fun) | about 4.7×10¹⁵ m/s² (top speed in about 2 s) | 1 light-year/s (about 30 million times light speed) |

The values can be changed in the `ENGINES` table in `src/app/bodies.h`. Switching to a slower engine decelerates to its top speed.

## On-screen display

- **Sky**: inside the atmospheres of Earth and Mars the sky changes with the Sun's height (blue sky, sunrise and sunset colors and twilight on Earth; a butterscotch day and a bluish sunset on Mars). Stars are hidden in daytime, and the sky darkens into black space as you climb. Seen from inside the atmosphere, distant land and sea fade into haze with the color of the sky, and the ground, clouds and sea are lit by redder light when the Sun is low (also visible from space as a reddish band along the terminator)
- **Nearby stars**: the 15 stars and brown dwarfs within about 10 light-years (Alpha Centauri A and B, Proxima Centauri, Barnard's Star, Luhman 16 A and B, WISE 0855−0714, Wolf 359, Lalande 21185, Sirius A and B, BL Ceti and UV Ceti, Ross 154, Epsilon Eridani) and 12 notable stars out to about 17 light-years (Procyon A and B, 61 Cygni A and B, Ross 128, Tau Ceti, Epsilon Indi A, Teegarden's Star, 40 Eridani A, B and C, Altair) are placed at their real positions and sizes, colored by their surface temperature. Positions follow straight-line motion from the parallaxes, proper motions and radial velocities of Gaia DR3, Hipparcos and other sources (so the stars move over the centuries), and the binaries (Alpha Centauri, Sirius, BL/UV Ceti, Luhman 16, Procyon) orbit their barycenters with published orbital elements ; 40 Eridani B and C follow the orbit of Mason et al. (2017) too ; 61 Cygni A and B follow the orbit of Shakht et al. (2017; period 664 years), checked against the positions measured by Hipparcos and Gaia. Altair is flattened by its fast spin, and gravity darkening makes its poles hot, white and bright and its equator warmer in color and darker. From afar they look like the other stars of the sky, as points as bright as their magnitude; from Alpha Centauri the Sun is a magnitude 0.5 star in Cassiopeia. These stars are removed from the background sky. Red dwarfs have large starspots reaching high latitudes, and the flare stars (Proxima Centauri, UV and BL Ceti, Wolf 359, Ross 154, 40 Eridani C) now and then brighten by up to about 3 magnitudes for a few minutes, with a blue-white patch on the surface (more often than in reality). The white dwarfs (Sirius B, Procyon B, 40 Eridani B) are smooth and glow an intense blue-white. The brown dwarfs (Luhman 16 A and B, WISE 0855−0714) have Jupiter-like cloud bands and the patchy clouds known from their brightness variations. Pick them in the "Stars" tab of the search and warp there; warps farther than about 1,900 AU get a longer, bigger interstellar warp (the sky crowds forward and turns blue, and you pass through a swirling tunnel of light, with the distance left and how many years light would take). The info card also tells how many years ago the light you see left the star. Masses, radii and some other values of the brown dwarfs are estimates and are marked as such on the info card. The spin axes are unknown, so binaries spin perpendicular to their orbits and the others toward the celestial pole
- **Planets of other stars**: the 17 confirmed planets (Proxima Centauri b and d, Barnard's Star b to e, Lalande 21185 b and c, Epsilon Eridani b, Ross 128 b, Tau Ceti g, h and f, Epsilon Indi A b, Teegarden's Star b, c and d) orbit their stars at their current positions from the orbital elements (period, eccentricity, time of periastron) of the NASA Exoplanet Archive, lit by their own star. For planets found only by radial velocity the orbital tilt is unknown, so it is taken as 60° (planets of the same star share one plane), the mass is derived from the minimum mass and the radius from the mass (Epsilon Eridani b has its tilt and true mass from astrometry, Epsilon Indi A b from JWST imaging). For planets without a time of periastron in the archive (Ross 128 b and the planets of Tau Ceti and Teegarden's Star) the position along the orbit is an estimate too. Their surfaces are unknown and painted by type and temperature; planets with periods under 30 days always keep the same face to their star. From the "Surface" search tab you can stand on Proxima Centauri b, where the red star hangs still in the sky (the surface is an artist's impression, as the panel says). The info card shows the equilibrium temperature and which values are estimates, and the "Stars" tab of the search lists them. A system map centered on a star shows the habitable zone, where water could be liquid, as a green band (from the star's luminosity, a conservative range after Kopparapu et al. 2013), with distances from the center drawn true rather than projected
- **Star epoch**: the "Star epoch" setting moves only the stars and the background sky to where they will be (or were) up to 100,000 years from now (the date of the planets stays; dates can be set for 3000 BC to AD 2999, where the planet positions can be computed). The background stars move at constant speed with the proper motions of the Hipparcos new reduction (radial velocities are not included), so constellations such as the Big Dipper fall apart; the nearby stars move too, and in about 30,000 years Alpha Centauri comes within a little over 3 light-years. The setting is shown next to the date while it is on
- **Near the speed of light**: approaching the speed of light with the light-speed engine, special-relativistic aberration crowds the stars and the Milky Way forward, and the Doppler effect turns the sky ahead blue and bright and behind red and dark. Planets, the Sun and other Solar System bodies shift forward in the same way, and a large planet right below you is distorted as a whole (the aberration is applied per vertex in the shader, and the Doppler effect turns its colors blue ahead and red and dark behind; orbit lines are hidden, as they would no longer match). The strength is mapped to grow evenly with speed, and after a sudden stop it eases away over about 1 second (not with the hyperdrive)
- **Warping to spacecraft**: warping to a spacecraft or satellite brings you to a distance matched to its size, in a zoomed view (right-click or thrust to return) with the spacecraft large in the center
- **Hyperdrive**: the 8th engine is a fictional one for fun that covers one light-year per second, so you can fly between the stars without warping and watch the sky shift continuously
- **Starry sky**: the background stars are the real stars of the Hipparcos catalogue (about 41,000, down to magnitude 8), with their real brightness and colors (color index), in front of the faint glow of the Milky Way (an all-sky image from NASA; it fades out when you zoom in). 35 bright nebulae, galaxies and star clusters, among them the Andromeda Galaxy, the Magellanic Clouds, the Orion Nebula, the Carina Nebula, the Veil Nebula and Omega Centauri, are drawn by the program from their published positions, sizes, orientations and brightness (they are not photographs; zoom in to see their shapes; the Crab Nebula appears only after the supernova of 1054 and keeps expanding; turn them off with "Milky Way & nebulae" in the settings). Press C to show the lines of the 88 constellations, and the names of the constellations and of the main stars (about 100 brighter than magnitude 2.6). The stars have distances from the parallaxes of the Hipparcos new reduction, so away from the Sun they are drawn in the direction and brightness seen from where you are (at Alpha Centauri the nearer stars shift within their constellations)
- **Speedometer** (bottom right): analog gauge whose full scale is the current engine's top speed, plus a digital readout
- **Heading indicator** (top center): near a body (within 30 times its radius), heading, pitch and roll with that body's rotation axis as north. Farther away, ecliptic longitude and latitude. The yellow ▲ marks the target's bearing
- **Body markers**: name and distance to the surface for the Sun and every planet. Off-screen bodies are shown as arrows at the screen edge. Moons are shown when near their parent. Bodies more than 1 light-year away other than stars, and asteroids, spacecraft and satellites more than 1 AU away, are not shown (the target always is)
- **Resume**: the place, view and target are saved when you quit (and every 30 seconds in the browser version) and the next launch starts there. The settings offer "Place only (current time)" (default), "Place and time" and "Off". A share link takes precedence
- **Target info** (top right): distance, estimated arrival time at current speed, light travel time, and travel time at top speed. With a time scale applied, it also shows how many seconds of real time you will wait

## Spacecraft voyages

On the "Spacecraft voyages" page of the event list (K), pick a spacecraft and press Enter to follow it from launch to its goal, from just behind the spacecraft (19 spacecraft whose trajectories start at Earth, plus the extended missions of Hayabusa2 and OSIRIS-APEX, from the Earth flyby that delivered the sample capsule to Torifune and Apophis).

- Time runs fast during quiet cruise (about 12 days per second for BepiColombo, about 15 minutes for the whole voyage) and slows to real time with a caption at each event: launch, leaving Earth's sphere of influence, gravity assists, passes by asteroids and moons, and arrival. Gravity assists show the altitude and the change in speed relative to the Sun; the arrival shows the years, gravity assists and distance flown
- During cruise, notes about the spacecraft (its design, its goals and stories from the mission) appear one by one at the bottom left, 14 seconds each
- A small map at the top right (the solar system seen from above) shows the path flown so far (bright), the path ahead (dim), the scene locations and the current position (green ring). Near a planet or asteroid it switches to a close-up map of that area (seen the same way as the full map, with the Sun at rest, so the spacecraft moves in the same direction as in the full map and the body moves across the map too). F3 shows or hides it
- Look around by dragging the mouse, with the arrow keys or by dragging on a touch screen (the spacecraft stays in the same place on screen; the view returns to the automatic direction 3 seconds after you stop)
- A timeline at the bottom shows the scenes as dots: click or tap a dot to go to that scene, or elsewhere on the bar to go to that time (hover over a dot to see its title). `[` and `]` choose the voyage speed: slow (about 10 minutes for the whole voyage), normal (about 4 minutes) or fast (about 1 minute)
- Enter goes to the next scene, Shift+Enter to the previous one, P pauses and Esc quits. The voyage is complete on arrival. Scenes that have not happened yet (planned trajectories) are marked "(planned)"
- Asteroids (Gaspra and Ida for Galileo; Dinkinesh, Donaldjohanson and five Jupiter Trojans for Lucy) are passed at the distance of the real trajectories, and the view zooms in automatically because they would otherwise be dots
- Distances to moons use the moon positions in this simulator, which can differ from reality
- **Opening by link**: add `?voyage=<spacecraft name>` to the browser version's URL to start that voyage at launch, `&scene=arrive` to start just before arrival, or `&scene=<number>` for that scene; `&leg=2` selects the extended mission. Press F2 during a voyage to copy a link to the current scene. Examples: [BepiColombo arriving at Mercury](https://hubstacksea.github.io/Solar-System-Simulator/?voyage=BepiColombo&scene=arrive), [the voyage of Voyager 1](https://hubstacksea.github.io/Solar-System-Simulator/?voyage=Voyager%201). On the PC, use `--share "?voyage=BepiColombo&scene=arrive"`

## Apollo program

On the "Apollo program" page of the event list (K), pick a flight and press Enter to follow it from right beside the spacecraft, in chronological order, from liftoff to the Moon, the landing, the return and splashdown (the lunar flights Apollo 8 and 10-17; Apollo 13 aborted its landing after an oxygen tank explosion and looped around the Moon to return).

- As with spacecraft voyages, time slows to real time with a caption at each event (staging, translunar injection, lunar orbit insertion, landing, moonwalks, liftoff, docking, entry, splashdown and more). While the command and lunar modules are apart, the view follows whichever the scene is about (the command module once the record of the lunar module ascent stage ends)
- The spacecraft's shape changes with time (the Saturn V stages, the command and service module with the lunar module, the descent stage left on the Moon, the command module under parachutes). While docked, the lunar module is drawn on the nose of the command module. Undocking and the ascent stage docking in lunar orbit are shown with both craft seen from the side: the lunar module backs away from the docking port and turns, and the ascent stage closes in, stationkeeps, lines up and makes contact (times and distances from each mission's records; the procedure's values where none were found). From separating from the S-IVB, turning around and docking with the lunar module stowed on top of it to pulling it out, the S-IVB and the adapter panels are drawn too, with the speeds and times of each mission's records. The first stage, the interstage ring and the second stage are drawn too after separation, slowed by their retro rockets and falling away under Earth's gravity. The map at the top right is centered on Earth and switches to a close-up around the Moon near it (drawn as seen from the Moon, so lunar orbits appear as plain ellipses)
- Even without choosing the event, the spacecraft appear and fly as recorded whenever the date falls within the flight (the descent stages stay on the Moon after liftoff)
- Engines show flames or plumes while burning (a bright flame from the Saturn V first stage, faint plumes in vacuum from the others). During main engine burns the engine points against the thrust, so braking burns such as lunar orbit insertion are flown backwards (the spacecraft turns over the 4 minutes before and after); otherwise the nose points in the direction of travel. Time slows to real time at the start and end of each burn of the followed craft
- The trajectories are computed with the gravity of Earth, the Moon and the Sun from the positions and velocities at the times listed in NASA's flight records (Apollo by the Numbers and each mission report), since no ephemerides of the spacecraft themselves are published. Velocities are matched at the burns so the paths pass the recorded positions, and the effect of the Moon's uneven gravity (not modeled) is spread over each leg. The launch follows the recorded ascent points; the lunar descent and ascent pass the recorded times, altitudes and speeds (Apollo 11's record serves as the guide between them); entry is solved from the equations of motion to match the recorded peak deceleration and the parachute time and place
- **Opening by link**: add `?apollo=11` to the URL to start that flight at liftoff, and `&scene=<number>` for a scene. On the PC, use `--share "?apollo=11"`

## How it works

- **Body positions**: Mercury to Neptune are computed from the planetary theory VSOP87 (Bretagnon & Francou 1988, truncated to about 5,700 terms): within 1.5″ of JPL Horizons (DE441) for Mercury to Saturn and 8″ for Uranus and Neptune in 1500–2500, and within 4.4′ over 3000 BC to AD 3000 (growing into the distant past with the error of VSOP87 itself). The orbit lines are ellipses from the current position and velocity, so they pass exactly through the planets. Pluto is computed from JPL mean orbital elements and their rates via Kepler's equation. The slowing of Earth's rotation (ΔT; the Espenak & Meeus expressions used by NASA's eclipse catalog for 500 BC to AD 2150 and Morrison & Stephenson 2004 outside; about 4.8 hours in 500 BC) and the precession of Earth's axis (IAU 1976) are included. The Moon uses the series of chapter 47 of Meeus's *Astronomical Algorithms* (a truncated ELP-2000/82) with a long-term correction fitted to the Moon of JPL Horizons (DE441), so its real position and phase are reproduced. Its orientation follows the IAU rotation model of the Moon, so seen from Earth it nods by about ±8° in longitude and ±7° in latitude (libration), showing a little beyond its edges. Solar eclipses from 763 BC to AD 2950 agree with NASA's catalog within 4 minutes and 2° (checked by `tools/run_checks.py`). Other moons are placed on circular orbits in the orbital planes from JPL's mean orbital elements, with their positions fitted to JPL Horizons for 1980–2070 (most within 1°, at most a few degrees; Mimas includes its ±43° libration from the resonance with Tethys). Shadows of Jupiter's moons fall on Jupiter at nearly the real times
- **Dwarf planets, asteroids and comets**: Ceres, Vesta, Eris, Ryugu, Bennu, Itokawa, Didymos (with its moon Dimorphos), Halley's Comet, Comet Hale-Bopp, Comet Hyakutake and Comet Churyumov-Gerasimenko. Their orbital elements and rates, in the same form as the planets', are fitted to JPL Horizons positions for 1980–2070 (direction seen from the Sun within 0.6°). Ryugu (2012–2024) and Churyumov-Gerasimenko (2012–2018) are fitted only around the time spacecraft visited them (errors 0.9° and 0.07°), because encounters with Earth and Jupiter change their orbits; they drift further away from those years. Comet Encke (period 3.3 years) is not included: Jupiter's perturbations and other effects shift its perihelion times from one passage to the next, so it cannot be fitted in this form (errors over 20°). The asteroids that spacecraft passed on their way (Gaspra, Ida, Dinkinesh, Donaldjohanson, Eurybates, Polymele, Leucus, Orus, Patroclus, Torifune and Apophis) are fitted around the time of those flybys (errors within 0.2°). Sizes and rotation periods come from the JPL Small-Body Database; Gaspra, Ida and Donaldjohanson have their real shapes, and the rest are spheres. Apophis passes 38,000 km from Earth's center on 13 April 2029, which changes its orbit, so from 2024 to 2060 it follows its JPL Horizons trajectory
- **Comet tails**: inside 4 AU from the Sun, comets grow a coma (the glow around the nucleus) and tails that get longer and brighter closer to the Sun (the great comet Hale-Bopp starts farther out and grows larger). The blue ion tail points straight away from the Sun, the yellowish dust tail curves back along the orbit. From the comet page of the K list you can warp to Halley's perihelia in 1986 and 2061, Hyakutake passing 0.1 AU from Earth on 25 March 1996, and more
- **Surfaces**: Jupiter carries Cassini's global map and is turned so that the Great Red Spot sits at its observed longitude for the date (System II; records since 1962, then a drift of 16° per year). Saturn also carries a Cassini global map (August 2011, showing the aftermath of the great northern storm); its poles and the band from the equator to 17° S, hidden by the ring shadow, are procedural clouds. Mercury, Mars, Ceres, Vesta, Pluto and the moons Io, Europa, Ganymede, Callisto, Mimas, Enceladus, Tethys, Dione, Rhea, Iapetus, Miranda, Ariel, Umbriel, Titania, Oberon, Triton and Charon carry real maps from spacecraft images (Olympus Mons and Valles Marineris, Pluto's heart and Enceladus's tiger stripes appear where they really are). Mars also gets relief shading from MOLA elevations. Mercury, Mars, Ceres, Vesta and Pluto rotate according to the IAU prime meridians, and the moons always keep the same face toward their planet. Areas that were never imaged (the northern hemispheres of the Uranian moons and parts of Pluto, Triton and Charon) and detail finer than the maps are filled in procedurally. Iapetus's map has its brightness flattened, so its bright and dark hemispheres are painted procedurally. Uranus, Neptune and Venus keep procedural patterns, because no freely redistributable global maps could be found (and Uranus and Venus show almost no features in visible light). The other bodies are generated procedurally in shaders; finer patterns and relief appear as you get closer, so surfaces stay detailed even near the ground
- **Earth**: built from real NASA data — continents, coastlines, elevation (relief shading), ocean depth (shallow-water color), sea ice and city lights at night (about 10 km resolution, with finer detail filled in procedurally). Its rotation follows Greenwich sidereal time, so the regions in daylight and the lit cities at night match the PC clock. The surface colors follow the date using all 12 months of Blue Marble (winter snow, green rainy seasons and so on), and clouds come from NASA's global cloud image (one day's distribution) with finer streaks added
- **Moon**: real NASA data (LRO color and LOLA elevation, about 3–5 km resolution). Maria, craters and ray systems are where they really are; smaller craters are filled in procedurally
- **Music**: plays a piece for the nearest body: Holst's *The Planets* for the planets (the parent planet's movement near its moons), Grieg's "Morning Mood" for the Sun, the first movement of Beethoven's "Pastoral" Symphony for Earth and Debussy's "Clair de lune" for the Moon. It gets louder as you approach and fades out at the edge of the sphere of influence (1 AU for the Sun). Tracks crossfade when they change and resume where they left off when you return. Nothing plays near Pluto (the later Pluto movement by another composer is still under copyright), nor near the dwarf planets, asteroids and comets
- **Spacecraft around Earth**: the International Space Station and the Hubble Space Telescope are computed from CelesTrak orbital elements (TLE of 28 October 2026), including the drift of the orbital plane caused by Earth's oblateness (within about 35 km of Horizons). They drift further off the farther you go from that epoch: the ISS by hundreds of km within days, losing its position along the orbit within months. The James Webb Space Telescope follows its JPL Horizons trajectory around the L2 point 1.5 million km from Earth (until October 2031). Select them with Tab and warp with G
- **Gravity mode**: toggled in the settings or with the R key. You fall under the gravity of the Sun, the planets, the large moons and every body above 10²⁰ kg; when you switch it on or arrive by warp you are placed on a circular orbit around the current body. Body positions are updated every frame and held still within the frame while the motion is integrated in small steps (if time runs too fast for those steps, the time scale is held back automatically)
- **Asteroids and other small bodies**: about 100,000 objects from the JPL Small-Body Database — the main belt (about 60,000 of the brightest), the outer belt including the Hildas, near-Earth asteroids, Jupiter Trojans, Centaurs and trans-Neptunian objects — move on their orbits (two-body motion, computed and projected in a GPU shader) and are drawn as points. From afar you can see the ring of the main belt and the two Trojan swarms 60° ahead of and behind Jupiter. They are shown with the orbit lines (O key). They are off on the first launch; turn them on in the settings (F10)
- **Spacecraft**: Voyager 1 and 2, Pioneer 10 and 11, Galileo, Cassini, New Horizons, Parker Solar Probe, Juno, Rosetta, Hayabusa2, OSIRIS-REx and DART fly their real paths from JPL Horizons, from launch to end of mission (spacecraft leaving the Solar System keep coasting after the data ends in 2050–2100). Inside a planet's sphere of influence they move relative to the planet, so flybys of Jupiter, Neptune and the rest happen at the real distance, direction and time (encounters with moons can be off by a few thousand km because of the moons' position errors). Rosetta orbits Comet Churyumov-Gerasimenko and descends to its nucleus on 30 September 2016; Juno orbits Jupiter from 5 July 2016 (predicted trajectory after September 2026, data until September 2028). While Hayabusa2 stays at Ryugu (27 June 2018 – 13 November 2019) it is placed at its usual station, the home position 20 km from Ryugu toward Earth, because in Horizons the spacecraft and Ryugu come from separate solutions that put them only a few km apart. Their paths are shown with the orbit lines (O key), brighter where they have already flown. Each spacecraft has its own simple model (Voyager, Pioneer, Galileo, Cassini, New Horizons, Parker, Juno, Rosetta, Hayabusa2) with the high-gain antenna pointed at Earth (Parker keeps its heat shield toward the Sun). Cassini carries the Huygens probe until 25 December 2004, Galileo its atmospheric probe until 13 July 1995. Their markers are toggled together with the moon markers (L key). OSIRIS-REx stays at Bennu from December 2018 to May 2021 and brings back a sample, and DART impacts Dimorphos on 26 September 2022 (the K list and the tour warp you to one minute before the impact). The original Hayabusa is not included, because its trajectory up to Itokawa and most of its return trip are not publicly available. Missions in flight follow their planned Horizons trajectories (they will differ if the plans change): BepiColombo (into Mercury orbit in November 2026 after six Mercury flybys), JUICE (until just before its Jupiter arrival in July 2031), Europa Clipper (into Jupiter orbit in April 2030, until September 2034), Lucy (toward Jupiter's Trojans, until April 2033) and Psyche (until February 2029) and Hera (ESA; arrived at Didymos in October 2026, until 4 November). Horizons times are dynamical time (TDB) and are converted to universal time
- **Historic sky events**: the 4th page of the event list has 18 of them. Eclipses (the Assyrian eclipse of 763 BC, the eclipse of Thales in 585 BC, the annular eclipse at the Battle of Mizushima in 1183, Columbus's lunar eclipse in 1504, Eddington's expedition in 1919, the longest total eclipse of the 21st century in 2009) take you to the ground at the historic site at the deepest eclipse there. Supernovae (1006, 1054 which left the Crab Nebula, Tycho's of 1572, Kepler's of 1604) shine in the sky for the recorded time at the recorded brightness (the light curve is approximate) and take you to the night sky where they were recorded. Returns of Halley's Comet (240 BC in the Records of the Grand Historian, 1066 on the Bayeux Tapestry, 1301 painted by Giotto) , transits of Venus (Horrocks 1639, Cook 1769, 1882) Comet Shoemaker-Levy 9 hitting Jupiter (1994) and the night Galileo discovered Jupiter's moons (7 January 1610, Padua; in a telescope-like 0.5° view they line up two to the east and one to the west, as he recorded) are there too. Dark scars appear on Jupiter at the impact times and places computed from JPL Horizons' orbits of the 20 fragments and fade over months (their size and fading are approximate)
- **Sunspot cycle**: the number of sunspots and their latitudes follow the 11-year activity cycle (near 30° at the start of a cycle, near 8° at the end). The cycles start at the observed minima of solar cycles 1-25, and the number of spots follows an average cycle shape (approximate); there are almost none in the Maunder Minimum (1645-1715) and fewer in the Dalton Minimum (1798-1823)
- **Halley's past returns**: before 1948 Halley's Comet follows the orbit of the nearest return from JPL Horizons (30 returns from 240 BC to 1910)
- **Binary periastra**: the "Comets and binary stars" page of the event list has the times when the binaries of Alpha Centauri, Sirius, Luhman 16 and BL/UV Ceti come closest (Alpha Centauri: 11.2 AU on 30 April 2035); choosing one warps you to a view of the two stars from the side
- **Astronomical events**: from the K-key list you can warp, at the right moment, to a solar eclipse (12,000 km above the center of the eclipse; total and annular eclipses can also be watched from the ground at the center, where the corona appears and the sky darkens enough to show stars during totality), a lunar eclipse (beside the Moon), a moon's shadow crossing Jupiter (right above the shadow) or a planet at opposition (from the Earth side, fully lit like a full moon). Times are computed with simplified formulas and are typically within about 15 minutes of the real eclipses; the center of a solar eclipse is within a few hundred km. Transits are viewed from the line joining Earth and the planet (the same geometry as seen from Earth, but with the planet larger), conjunctions from just outside Earth, both with a zoomed view (right-click or move to return to the normal view). Mutual events of Jupiter's moons are not included: the moons' position errors are larger than the moons themselves
- **Shadows**: solar eclipses (the Moon's shadow on Earth), lunar eclipses (a copper-red Moon), shadows of Jupiter's moons and the shadow of Saturn's rings, with umbra and penumbra from the Sun's real size
- **Sky from the ground**: when you are stopped near a surface (below 100 km) you turn with the body's rotation. Choosing a place in the "Surface" search tab puts you on the ground looking up at the sky toward the equator (on the Moon, at Earth when it is up; on a planet of another star, at its star); speed up time to watch the stars and planets wheel across the sky (by day the blue sky hides the stars, so set a night-time date with Y or let time run). While you are on the ground (below 2 km, on bodies over 100 km in radius) the horizon and the compass points are drawn, and a panel at the lower left shows the place and time, the target's altitude and azimuth (16 points) and its next rise, transit and set (with atmospheric refraction on Earth; over one lunar day on the Moon). Times are in the standard time of the Earth city you chose in the search (no daylight saving), otherwise UTC
- **Meteor showers**: the Quadrantids, Lyrids, Eta Aquariids, Perseids, Orionids, Leonids and Geminids, with the solar longitude of their peaks, radiants and ZHR from the International Meteor Organization. When you watch the night sky from the ground on Earth, meteors streak out of the radiants of active showers (ZHR × the sine of the radiant altitude per hour, plus 6 sporadic meteors an hour; more when time runs faster, up to 4 per real second). The K list (sky) shows the peaks; choosing one takes you to the ground in Tokyo on the best night around the peak, looking toward the radiant
- **Venus and Titan**: both are drawn covered by their thick clouds and haze. Venus's clouds circle the planet in about 4 days, much faster than its 243-day rotation (super-rotation), and Titan has its detached blue upper haze layer (about 500 km up) showing beyond the limb
- **Sizes**: equatorial radius and flattening (Jupiter and Saturn are visibly oblate), and Saturn's rings drawn at true scale from the opacity profile measured by Voyager 2's stellar occultation (every 10 km), so the D to F rings, the Cassini Division and the Encke Gap appear at their real positions and densities and get more opaque at shallow angles. The rings of Uranus (ten narrow rings, tipped over with the planet), Neptune (Galle, Le Verrier, Lassell, Arago and Adams, with the five arcs of the Adams ring) and Jupiter (halo, main ring and two gossamer rings) use the radii, widths and optical depths from NASA fact sheets. They are very dark and faint, so their opacity is exaggerated, and the narrow rings of Uranus and Neptune stay visible as lines when they become thinner than a pixel (the positions of Neptune's arcs are approximate). Ryugu (a spinning top), Comet Churyumov-Gerasimenko (two lobes joined by a neck), Bennu, Itokawa (a sea-otter shape), Didymos and Dimorphos, and Phobos have their real shapes from spacecraft observations and rotate. Dimorphos's orbital period shrinks from 11.92 to 11.37 hours at DART's impact on 26 September 2022
- **Reference frame**: the ship moves along with the body whose sphere of influence (SOI) it is in, so even at car speed you are not left behind by Earth. With Shift+F (or in the settings) the frame is fixed to the ship: you move along with no body and stay put relative to the Sun, watching Earth leave at 30 km/s or a moon sweep past (in gravity mode your actual motion is unchanged and the speed is shown relative to the Sun). Warping, moving to a place or an event, or starting the autopilot returns to automatic
- **Precision**: positions are stored as doubles and rendered relative to the camera. Each body is drawn at its own scale with its own near/far planes, so a 30 m ship and Neptune 4.5 billion km away can be rendered at the same time
- **Minimum altitude**: because of floating-point precision, you cannot descend below 1/100,000 of a body's radius (minimum 30 m) — about 64 m on Earth
- No physics (gravity etc.) is simulated. The ship flies arcade-style in the direction of its nose

## Earth and Moon data, starry sky, music

The images in `assets/` are made from NASA / NOAA data by `tools/make_earth_assets.py` and `tools/make_moon_assets.py` and embedded into the executable at build time (no files needed at run time).

- Surface color: NASA Earth Observatory, Blue Marble: Next Generation (April 2004)
- Elevation and ocean depth: NOAA NCEI, ETOPO 2022 Global Relief Model (60 arc-second, ice surface, subsampled to 5 arc-minutes)
- City lights: NASA Earth Observatory, Black Marble 2016
- Moon color and elevation: NASA's Scientific Visualization Studio, CGI Moon Kit (LROC WAC / LOLA)
- Planet and moon maps: global mosaics from the USGS Astrogeology Science Center map service ([planetarymaps.usgs.gov](https://planetarymaps.usgs.gov/)) (Jupiter from Cassini PIA07782 (NASA/JPL/Space Science Institute), MESSENGER, Viking MDIM 2.1 colorized by NASA Ames, Dawn, Galileo / Voyager, Cassini, Voyager 2, New Horizons) and Mars elevations from the NASA PDS MGS MOLA MEGDR; Saturn uses the [Cassini ISS global map](https://doi.org/10.17189/rkkb-6y30) from the NASA PDS Atmospheres Node (red, green and blue filters, 11 August 2011); the five major moons of Uranus use the USGS global maps distributed by JPL's [Solar System Simulator](https://space.jpl.nasa.gov/tmaps/uranus.html) (Voyager 2, southern hemispheres only; Courtesy NASA/JPL-Caltech, [JPL Image Use Policy](https://www.jpl.nasa.gov/jpl-image-use-policy)), fetched and converted by `tools/make_planet_maps.py` (requires Pillow and NumPy; downloads about 105 MB). The longitude of Jupiter's Great Red Spot follows the observations compiled by [Project Pluto](https://www.projectpluto.com/grs_lon.txt) (JUPOS since 1964)
- Orbital elements of the ISS and the Hubble Space Telescope: [CelesTrak](https://celestrak.org/) (fetched by `tools/fetch_tle.py`). JWST trajectory: NASA/JPL Horizons
- Orbital elements of asteroids and other small bodies: JPL Small-Body Database (fetched and converted by `tools/make_asteroid_assets.py`; requires NumPy; downloads about 20 MB)
- Spacecraft trajectories: NASA/JPL Horizons (fetched and thinned out by `tools/make_probe_assets.py`; Rosetta from ESA and Hayabusa2 from JAXA orbit determination)
- Apollo spacecraft trajectories: computed by `tools/make_apollo_assets.py` from the positions and velocities in NASA's flight records (R. W. Orloff, *Apollo by the Numbers*, NASA SP-2000-4029, and each mission report), with Moon and Sun positions from JPL Horizons
- Starry sky: stars from the Hipparcos catalogue (ESA), and the lines, names and star names of the 88 constellations, from [d3-celestial](https://github.com/ofrohn/d3-celestial) (Olaf Frohn, BSD 3-Clause), converted by `tools/make_sky_assets.py`. Star distances and motions: parallaxes and proper motions of the Hipparcos new reduction (van Leeuwen 2007), from the ESA Gaia Archive
- Nearby stars: astrometry from Gaia DR3 (ESA/Gaia/DPAC) and the Hipparcos new reduction (van Leeuwen 2007), with binary orbits and masses from published papers (Akeson et al. 2021, Bond et al. 2017, Kervella et al. 2016, Lazorenko & Sahlmann 2018) and others. The sources and values are listed in `tools/make_nearby_stars.py` (which writes `src/nearby_stars.h`). Planets: NASA Exoplanet Archive (Suárez Mascareño et al. 2025, Basant et al. 2025, Hurt et al. 2022, Thompson et al. 2025)
- Shapes of the comet and asteroid: Ryugu from the shape model of Watanabe et al. (2019) (source ISAS/JAXA, [JAXA DARTS](https://data.darts.isas.jaxa.jp/pub/hayabusa2/paper/Watanabe_2019/)); Comet Churyumov-Gerasimenko from ESA's NAVCAM shape model MTP019, low-resolution version (ESA/Rosetta/NAVCAM – CC BY-SA IGO 3.0, obtained from the NASA PDS Small Bodies Node). Bennu (OSIRIS-REx), Itokawa (Gaskell et al., Hayabusa AMICA), Didymos and Dimorphos (DART), Phobos (Gaskell) and Donaldjohanson (Lucy, DLR team) from NASA NAIF SPICE shape kernels (DSK). Deimos uses Thomas's shape model (from Viking Orbiter images; NASA PDS Small Bodies Node). Gaspra, Ida and the nucleus of Halley's Comet use Stooke's shape models (from Galileo images, and for Halley from Giotto and Vega images with pointing by Abergel; NASA PDS Small Bodies Node, public domain; the archive does not allow automated downloads, so fetch the zip in a browser and put it in the work directory). Converted by `tools/make_shape_assets.py` (coordinates rounded to 16 bits, Phobos thinned out, plus a table of surface radii; requires NumPy and spiceypy)
- Saturn's rings: NASA PDS Ring-Moon Systems Node, Voyager 2 photopolarimeter occultation of delta Scorpii (VG_2801), converted by `tools/make_ring_assets.py`
- Rings of Uranus, Neptune and Jupiter: radii, widths and optical depths from the NASA NSSDCA Uranian / Neptunian / Jovian Rings Fact Sheets (`RingSets` in `src/app/bodies.h`)
- Earth's seasons and clouds: NASA Earth Observatory, Blue Marble: Next Generation for January–December 2004 and the Blue Marble global cloud image, converted by `tools/make_earth_season_assets.py` (downloads about 150 MB)
- Planet positions: VSOP87A (Bureau des Longitudes), coefficients truncated by `tools/make_vsop87.py` from Greg Miller's [VSOP87-Multilang](https://github.com/gmiller123456/vsop87-multilang) (public domain)
- Milky Way: NASA/Goddard Space Flight Center Scientific Visualization Studio, [Deep Star Maps 2020](https://svs.gsfc.nasa.gov/4851/) (the Milky Way background without the bright stars; Gaia DR2: ESA/Gaia/DPAC), converted by `tools/make_milkyway_asset.py` (requires Pillow and NumPy; downloads about 35 MB)
- Music (Holst, *The Planets*): Mercury by the U.S. Air Force Band (2013, arranged by the band); the other six movements by the London Symphony Orchestra conducted by Holst, recorded 1922–23 (Columbia). Public domain
- Music (Sun, Earth): Grieg, "Morning Mood" by the Musopen Symphony; Beethoven, "Pastoral" Symphony, 1st movement by the Skidmore College Orchestra. Both public domain recordings provided by Musopen
- Music (Moon): Debussy, "Clair de lune", Laurens Goedhart, piano (2011). CC BY 3.0
- All recordings were taken from Wikimedia Commons. To regenerate: `python3 tools/make_music_assets.py <work dir>` (requires ffmpeg; downloads about 350 MB)

All of them may be used and redistributed freely, including commercially (see [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)). To regenerate them: `python3 tools/make_earth_assets.py <work dir>` (downloads about 50 MB) and `python3 tools/make_moon_assets.py <work dir>` (about 45 MB). Both require Pillow.

## Display language (日本語 / English)

- Press the **J key** at any time to switch. The chosen language is saved in `solar_voyager.cfg` and used on the next launch
- `solar_voyager.cfg` stores the language, quality, sound setting, constellation display and bookmarks. It lives next to the executable (if that folder is not writable, e.g. when installed from the .deb / .rpm, in `~/.config/solar_voyager/solar_voyager.cfg`; on macOS in `~/Library/Application Support/SolarSystemVoyager/`)
- To choose at startup: `solar_voyager --lang en` (or `ja`). Priority is: command-line option > saved setting > Japanese

For Japanese text, the program automatically searches for an OS Japanese font (Windows: Meiryo / Yu Gothic / MS Gothic, macOS: Hiragino Sans, Linux: Noto Sans CJK / IPA, etc.).
If none is found, the display is fixed to English. A font placed at `assets/font.ttf` takes priority.

## License

The program (source code, build files, tools and icon) is under the [MIT License](LICENSE).
The Earth and Moon image data (`assets/earth_*`, `assets/moon_*`), the sky data (`assets/stars.bin`, `assets/sky.txt`, `assets/milkyway.jpg`), Earth's seasons and clouds (`assets/earth_months.jpg`, `assets/earth_clouds.jpg`), Saturn's rings (`assets/saturn_rings.png`), the planet and moon maps (`assets/maps.bin`), the small-body orbital elements (`assets/asteroids.bin`), the comet and asteroid shapes (`assets/shapes.bin`; the Churyumov-Gerasimenko part is CC BY-SA IGO 3.0) and the music (`assets/music`) are not covered by it and follow the terms of its sources. See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md), which also lists the bundled libraries (raylib / GLFW, zlib license).
