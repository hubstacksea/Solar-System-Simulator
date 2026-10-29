# Solar System Voyager

[日本語](README.md) | **English**

A 3D space flight simulator with the Sun, planets, major moons, dwarf planets, asteroids, comets and spacecraft placed at **true size and true distance**.
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
| ![First-run guide](docs/screenshots/first_run.jpg) The guide shown on the first launch | |

## Getting started

On the first launch a short guide (4 pages) introduces the controls and highlights (Enter or tap for next, Esc to close). Later, open the controls with H.

## Browser version

Try it in your browser without installing anything: **https://hubstacksea.github.io/Solar-System-Simulator/**

- Runs in browsers with WebGL2 (recent Chrome / Edge / Firefox / Safari). Only the 8 MB needed to start is loaded first; the planet and moon maps, asteroid shapes, Moon maps, Earth's clouds and seasons and the asteroid belt (about 25 MB) are loaded in the background while you play. Until they arrive, surfaces are drawn procedurally and "Loading maps (N left)" is shown at the bottom left
- Music is not included (sound effects are). Japanese text uses the bundled Noto Sans JP font
- Click the view to steer with the mouse (Esc to release). Settings and bookmarks are kept in the browser
- On phones and tablets, on-screen buttons appear: drag to steer, Thrust / Brake / Stop on the right, Target / Warp / Autopilot on the left, Tour / Events / Settings / Help / Share at the top, and tap the engine bar to switch engines. Quality starts at Low, and small screens are rendered at 720 pixels high and scaled down
- If it runs slowly, lower the quality with F9
- **F2 (or "Share" on touch screens) copies a link to the current scene** (date, place, view, target and time scale; the address bar shows the same link). Opening the link starts from that scene. F2 in the PC version copies a link to the browser version

## macOS version

Download `SolarSystemVoyager-<version>-macos.zip` (macOS 11 or later, Apple Silicon and Intel) from the [releases](https://github.com/hubstacksea/Solar-System-Simulator/releases), extract it and move `SolarSystemVoyager.app` to Applications or anywhere you like.

- The app is not notarized by Apple, so the first launch shows an "unidentified developer" warning. **Right-click (Control-click) the app → Open → Open.** If macOS 15 or later does not offer that, try to launch it once, then press **System Settings → Privacy & Security → Open Anyway**
- If it still does not open, run `xattr -dr com.apple.quarantine SolarSystemVoyager.app` in Terminal and try again
- Settings and bookmarks are saved in `~/Library/Application Support/SolarSystemVoyager/solar_voyager.cfg`. Japanese text uses Hiragino Sans
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

Pushing a tag such as `v2.4.0` runs `.github/workflows/release.yml`, which builds Linux (.deb / .rpm), Windows (.7z), macOS (.zip) and the browser version, creates the GitHub release with the packages, and publishes the browser version to the `gh-pages` branch for GitHub Pages.

```sh
git tag v2.4.0
git push origin master v2.4.0
```

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
| macOS (.zip) | `cd build && cpack` | `SolarSystemVoyager-<version>-macos.zip` (`.app` with an ad-hoc signature) |

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
| 1–7 / wheel | Switch engine |
| Tab / Shift+Tab | Cycle target |
| F | Point the nose at the target |
| G / Shift+G | Warp to target / warp to near its surface (with a ~1.7 s warp animation; controls are locked during it) |
| T / Shift+T | Autopilot / autopilot with automatic engine selection |
| Right mouse button (hold) | Zoom |
| V | Chase view / cockpit view |
| O / L | Orbit lines / moon markers |
| C | Constellations (off → lines → lines and names) |
| [ / ] | Time scale (x1 real time to 1 year per second). Affects both orbits/rotation of the bodies and the ship's acceleration and movement |
| P / Backspace | Pause time / sync to current time at x1 |
| Y | Set the date and time (UTC; type the digits and press Enter) |
| F10 | Settings (language, quality, sound effects, music, constellations, orbit lines, asteroids, markers, invert mouse, fullscreen; Up/Down to select, Left/Right / Enter to change, saved at once) |
| R | Switch the flight model (arcade / gravity). In gravity mode you fall under the bodies' gravity: W / S thrust along the nose and X stops relative to the reference body. The predicted trajectory is drawn, with the periapsis and apoapsis altitudes and the period |
| Z | Photo mode (each press: hide gauges and markers → hide the ship too → back to normal; orbit lines are toggled with O) |
| U | Guided tour: 15 scenes (spacecraft highlights, great comets, an eclipse and more) in chronological order with captions, 15 seconds each. Enter for next, Shift+Enter for previous, U again to stop |
| K | Event list (select one and press Enter to warp to that time and place). Tab / Shift+Tab switches pages: astronomical events (solar and lunar eclipses, planetary conjunctions and oppositions over the next 3 years, the next shadow transit of each Galilean moon across Jupiter, and transits of Mercury and Venus up to 150 years ahead), comets (perihelia and closest approaches to Earth) and spacecraft events (closest approaches, orbit insertions, end of mission) |
| I | Invert mouse Y |
| J | Switch language (日本語 / English) |
| B / N | Bookmark the current spot / warp to bookmarks in turn (up to 9; position and orientation are stored relative to the rotating body) |
| M / Shift+M | Sound effects (engine and warp) / music on/off |
| F9 | Quality High → Low → Medium (use Low if it runs slowly: 3D at 60% resolution with less detail) |
| F2 | Copy a share link to this scene (date, place, view, target, time scale; opening it in the browser version starts from the same scene; the PC version opens one with `--share <link>`) |
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
| View (Back) / Menu (Start) | Switch view / help |

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

The values can be changed in the `ENGINES` table in `src/main.cpp`. Switching to a slower engine decelerates to its top speed.

## On-screen display

- **Sky**: inside the atmospheres of Earth and Mars the sky changes with the Sun's height (blue sky, sunrise and sunset colors and twilight on Earth; a butterscotch day and a bluish sunset on Mars). Stars are hidden in daytime, and the sky darkens into black space as you climb. Seen from inside the atmosphere, distant land and sea fade into haze with the color of the sky, and the ground, clouds and sea are lit by redder light when the Sun is low (also visible from space as a reddish band along the terminator)
- **Starry sky**: the background stars are the real stars of the Hipparcos catalogue (about 41,000, down to magnitude 8), with their real brightness and colors (color index), in front of the faint glow of the Milky Way (an all-sky image from NASA; it fades out when you zoom in). Press C to show the lines of the 88 constellations, and the names of the constellations and of the main stars (about 100 brighter than magnitude 2.6)
- **Speedometer** (bottom right): analog gauge whose full scale is the current engine's top speed, plus a digital readout
- **Heading indicator** (top center): near a body (within 30 times its radius), heading, pitch and roll with that body's rotation axis as north. Farther away, ecliptic longitude and latitude. The yellow ▲ marks the target's bearing
- **Body markers**: name and distance to the surface for the Sun and every planet. Off-screen bodies are shown as arrows at the screen edge. Moons are shown when near their parent
- **Target info** (top right): distance, estimated arrival time at current speed, light travel time, and travel time at top speed. With a time scale applied, it also shows how many seconds of real time you will wait

## How it works

- **Body positions**: planets are computed from JPL J2000 mean orbital elements and their rates via Kepler's equation (within 0.15° of JPL Horizons through 2046, the inner planets within 0.01°). The Moon uses a low-precision lunar ephemeris (about 0.3°), so its real position and phase are reproduced, and it always keeps the same face toward Earth. Other moons are placed on circular orbits in the orbital planes from JPL's mean orbital elements, with their positions fitted to JPL Horizons for 1980–2070 (most within 1°, at most a few degrees; Mimas includes its ±43° libration from the resonance with Tethys). Shadows of Jupiter's moons fall on Jupiter at nearly the real times
- **Dwarf planets, asteroids and comets**: Ceres, Vesta, Eris, Ryugu, Bennu, Itokawa, Didymos (with its moon Dimorphos), Halley's Comet, Comet Hale-Bopp, Comet Hyakutake and Comet Churyumov-Gerasimenko. Their orbital elements and rates, in the same form as the planets', are fitted to JPL Horizons positions for 1980–2070 (direction seen from the Sun within 0.6°). Ryugu (2012–2024) and Churyumov-Gerasimenko (2012–2018) are fitted only around the time spacecraft visited them (errors 0.9° and 0.07°), because encounters with Earth and Jupiter change their orbits; they drift further away from those years. Comet Encke (period 3.3 years) is not included: Jupiter's perturbations and other effects shift its perihelion times from one passage to the next, so it cannot be fitted in this form (errors over 20°)
- **Comet tails**: inside 4 AU from the Sun, comets grow a coma (the glow around the nucleus) and tails that get longer and brighter closer to the Sun (the great comet Hale-Bopp starts farther out and grows larger). The blue ion tail points straight away from the Sun, the yellowish dust tail curves back along the orbit. From the comet page of the K list you can warp to Halley's perihelia in 1986 and 2061, Hyakutake passing 0.1 AU from Earth on 25 March 1996, and more
- **Surfaces**: Jupiter carries Cassini's global map and is turned so that the Great Red Spot sits at its observed longitude for the date (System II; records since 1962, then a drift of 16° per year). Mercury, Mars, Ceres, Vesta, Pluto and the moons Io, Europa, Ganymede, Callisto, Mimas, Enceladus, Tethys, Dione, Rhea, Iapetus, Triton and Charon carry real maps from spacecraft images (Olympus Mons and Valles Marineris, Pluto's heart and Enceladus's tiger stripes appear where they really are). Mars also gets relief shading from MOLA elevations. Mercury, Mars, Ceres, Vesta and Pluto rotate according to the IAU prime meridians, and the moons always keep the same face toward their planet. Areas that were never imaged (parts of Pluto, Triton and Charon) and detail finer than the maps are filled in procedurally. Iapetus's map has its brightness flattened, so its bright and dark hemispheres are painted procedurally. Saturn, Uranus and Neptune keep procedural patterns, because no freely redistributable global maps could be found. The other bodies are generated procedurally in shaders; finer patterns and relief appear as you get closer, so surfaces stay detailed even near the ground
- **Earth**: built from real NASA data — continents, coastlines, elevation (relief shading), ocean depth (shallow-water color), sea ice and city lights at night (about 10 km resolution, with finer detail filled in procedurally). Its rotation follows Greenwich sidereal time, so the regions in daylight and the lit cities at night match the PC clock. The surface colors follow the date using all 12 months of Blue Marble (winter snow, green rainy seasons and so on), and clouds come from NASA's global cloud image (one day's distribution) with finer streaks added
- **Moon**: real NASA data (LRO color and LOLA elevation, about 3–5 km resolution). Maria, craters and ray systems are where they really are; smaller craters are filled in procedurally
- **Music**: plays a piece for the nearest body: Holst's *The Planets* for the planets (the parent planet's movement near its moons), Grieg's "Morning Mood" for the Sun, the first movement of Beethoven's "Pastoral" Symphony for Earth and Debussy's "Clair de lune" for the Moon. It gets louder as you approach and fades out at the edge of the sphere of influence (1 AU for the Sun). Tracks crossfade when they change and resume where they left off when you return. Nothing plays near Pluto (the later Pluto movement by another composer is still under copyright), nor near the dwarf planets, asteroids and comets
- **Spacecraft around Earth**: the International Space Station and the Hubble Space Telescope are computed from CelesTrak orbital elements (TLE of 28 October 2026), including the drift of the orbital plane caused by Earth's oblateness (within about 35 km of Horizons). They drift further off the farther you go from that epoch: the ISS by hundreds of km within days, losing its position along the orbit within months. The James Webb Space Telescope follows its JPL Horizons trajectory around the L2 point 1.5 million km from Earth (until October 2031). Select them with Tab and warp with G
- **Gravity mode**: toggled in the settings or with the R key. You fall under the gravity of the Sun, the planets, the large moons and every body above 10²⁰ kg; when you switch it on or arrive by warp you are placed on a circular orbit around the current body. Body positions are updated every frame and held still within the frame while the motion is integrated in small steps (if time runs too fast for those steps, the time scale is held back automatically)
- **Asteroids and other small bodies**: about 100,000 objects from the JPL Small-Body Database — the main belt (about 60,000 of the brightest), the outer belt including the Hildas, near-Earth asteroids, Jupiter Trojans, Centaurs and trans-Neptunian objects — move on their orbits (two-body motion) and are drawn as points. From afar you can see the ring of the main belt and the two Trojan swarms 60° ahead of and behind Jupiter. They are shown with the orbit lines (O key) and can be turned off in the settings
- **Spacecraft**: Voyager 1 and 2, Pioneer 10 and 11, Galileo, Cassini, New Horizons, Parker Solar Probe, Juno, Rosetta, Hayabusa2, OSIRIS-REx and DART fly their real paths from JPL Horizons, from launch to end of mission (spacecraft leaving the Solar System keep coasting after the data ends in 2050–2100). Inside a planet's sphere of influence they move relative to the planet, so flybys of Jupiter, Neptune and the rest happen at the real distance, direction and time (encounters with moons can be off by a few thousand km because of the moons' position errors). Rosetta orbits Comet Churyumov-Gerasimenko and descends to its nucleus on 30 September 2016; Juno orbits Jupiter from 5 July 2016 (predicted trajectory after September 2026, data until September 2028). While Hayabusa2 stays at Ryugu (27 June 2018 – 13 November 2019) it is placed at its usual station, the home position 20 km from Ryugu toward Earth, because in Horizons the spacecraft and Ryugu come from separate solutions that put them only a few km apart. Their paths are shown with the orbit lines (O key), brighter where they have already flown. Each spacecraft has its own simple model (Voyager, Pioneer, Galileo, Cassini, New Horizons, Parker, Juno, Rosetta, Hayabusa2) with the high-gain antenna pointed at Earth (Parker keeps its heat shield toward the Sun). Cassini carries the Huygens probe until 25 December 2004, Galileo its atmospheric probe until 13 July 1995. Their markers are toggled together with the moon markers (L key). OSIRIS-REx stays at Bennu from December 2018 to May 2021 and brings back a sample, and DART impacts Dimorphos on 26 September 2022 (the K list and the tour warp you to one minute before the impact). The original Hayabusa is not included, because its trajectory up to Itokawa and most of its return trip are not publicly available
- **Astronomical events**: from the K-key list you can warp, at the right moment, to a solar eclipse (12,000 km above the center of the eclipse; total and annular eclipses can also be watched from the ground at the center, where the corona appears and the sky darkens enough to show stars during totality), a lunar eclipse (beside the Moon), a moon's shadow crossing Jupiter (right above the shadow) or a planet at opposition (from the Earth side, fully lit like a full moon). Times are computed with simplified formulas and are typically within about 15 minutes of the real eclipses; the center of a solar eclipse is within a few hundred km. Transits are viewed from the line joining Earth and the planet (the same geometry as seen from Earth, but with the planet larger), conjunctions from just outside Earth, both with a zoomed view (right-click or move to return to the normal view). Mutual events of Jupiter's moons are not included: the moons' position errors are larger than the moons themselves
- **Shadows**: solar eclipses (the Moon's shadow on Earth), lunar eclipses (a copper-red Moon), shadows of Jupiter's moons and the shadow of Saturn's rings, with umbra and penumbra from the Sun's real size
- **Sizes**: equatorial radius and flattening (Jupiter and Saturn are visibly oblate), and Saturn's rings drawn at true scale from the opacity profile measured by Voyager 2's stellar occultation (every 10 km), so the D to F rings, the Cassini Division and the Encke Gap appear at their real positions and densities and get more opaque at shallow angles. Ryugu (a spinning top), Comet Churyumov-Gerasimenko (two lobes joined by a neck), Bennu, Itokawa (a sea-otter shape), Didymos and Dimorphos, and Phobos have their real shapes from spacecraft observations and rotate. Dimorphos's orbital period shrinks from 11.92 to 11.37 hours at DART's impact on 26 September 2022
- **Reference frame**: the ship moves along with the body whose sphere of influence (SOI) it is in, so even at car speed you are not left behind by Earth
- **Precision**: positions are stored as doubles and rendered relative to the camera. Each body is drawn at its own scale with its own near/far planes, so a 30 m ship and Neptune 4.5 billion km away can be rendered at the same time
- **Minimum altitude**: because of floating-point precision, you cannot descend below 1/100,000 of a body's radius (minimum 30 m) — about 64 m on Earth
- No physics (gravity etc.) is simulated. The ship flies arcade-style in the direction of its nose

## Earth and Moon data, starry sky, music

The images in `assets/` are made from NASA / NOAA data by `tools/make_earth_assets.py` and `tools/make_moon_assets.py` and embedded into the executable at build time (no files needed at run time).

- Surface color: NASA Earth Observatory, Blue Marble: Next Generation (April 2004)
- Elevation and ocean depth: NOAA NCEI, ETOPO 2022 Global Relief Model (60 arc-second, ice surface, subsampled to 5 arc-minutes)
- City lights: NASA Earth Observatory, Black Marble 2016
- Moon color and elevation: NASA's Scientific Visualization Studio, CGI Moon Kit (LROC WAC / LOLA)
- Planet and moon maps: global mosaics from the USGS Astrogeology Science Center map service ([planetarymaps.usgs.gov](https://planetarymaps.usgs.gov/)) (Jupiter from Cassini PIA07782 (NASA/JPL/Space Science Institute), MESSENGER, Viking MDIM 2.1 colorized by NASA Ames, Dawn, Galileo / Voyager, Cassini, Voyager 2, New Horizons) and Mars elevations from the NASA PDS MGS MOLA MEGDR, fetched and converted by `tools/make_planet_maps.py` (requires Pillow and NumPy; downloads about 25 MB). The longitude of Jupiter's Great Red Spot follows the observations compiled by [Project Pluto](https://www.projectpluto.com/grs_lon.txt) (JUPOS since 1964)
- Orbital elements of the ISS and the Hubble Space Telescope: [CelesTrak](https://celestrak.org/) (fetched by `tools/fetch_tle.py`). JWST trajectory: NASA/JPL Horizons
- Orbital elements of asteroids and other small bodies: JPL Small-Body Database (fetched and converted by `tools/make_asteroid_assets.py`; requires NumPy; downloads about 20 MB)
- Spacecraft trajectories: NASA/JPL Horizons (fetched and thinned out by `tools/make_probe_assets.py`; Rosetta from ESA and Hayabusa2 from JAXA orbit determination)
- Starry sky: stars from the Hipparcos catalogue (ESA), and the lines, names and star names of the 88 constellations, from [d3-celestial](https://github.com/ofrohn/d3-celestial) (Olaf Frohn, BSD 3-Clause), converted by `tools/make_sky_assets.py`
- Shapes of the comet and asteroid: Ryugu from the shape model of Watanabe et al. (2019) (source ISAS/JAXA, [JAXA DARTS](https://data.darts.isas.jaxa.jp/pub/hayabusa2/paper/Watanabe_2019/)); Comet Churyumov-Gerasimenko from ESA's NAVCAM shape model MTP019, low-resolution version (ESA/Rosetta/NAVCAM – CC BY-SA IGO 3.0, obtained from the NASA PDS Small Bodies Node). Bennu (OSIRIS-REx), Itokawa (Gaskell et al., Hayabusa AMICA), Didymos and Dimorphos (DART) and Phobos (Gaskell) from NASA NAIF SPICE shape kernels (DSK). Converted by `tools/make_shape_assets.py` (coordinates rounded to 16 bits, Phobos thinned out, plus a table of surface radii; requires NumPy and spiceypy)
- Saturn's rings: NASA PDS Ring-Moon Systems Node, Voyager 2 photopolarimeter occultation of delta Scorpii (VG_2801), converted by `tools/make_ring_assets.py`
- Earth's seasons and clouds: NASA Earth Observatory, Blue Marble: Next Generation for January–December 2004 and the Blue Marble global cloud image, converted by `tools/make_earth_season_assets.py` (downloads about 150 MB)
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
