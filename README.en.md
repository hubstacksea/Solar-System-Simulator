# Solar System Voyager

[日本語](README.md) | **English** | [Changelog](CHANGELOG.en.md)

A 3D space flight simulator with the Sun, planets, major moons, dwarf planets, asteroids, comets and spacecraft, and the stars within about 10 light-years of the Sun with their planets, placed at **true size and true distance**.
Body positions are computed from the PC clock (UTC); at 1x time scale they orbit and rotate in real time.

- Rendering engine: [raylib 5.5](https://www.raylib.com/) (OpenGL 3.3)
- Platforms: Linux (gcc) / Windows (MinGW-w64)

![Daytime Earth seen from above Japan](docs/screenshots/earth_day.jpg)

**Earth and the Moon**

<table>
<tr>
<td width="50%" valign="top"><img src="docs/screenshots/earth_night.jpg" alt="City lights of Europe at night" width="100%"><br>City lights of Europe at night</td>
<td width="50%" valign="top"><img src="docs/screenshots/sunset.jpg" alt="Dusk over the Pacific" width="100%"><br>Dusk over the Pacific</td>
</tr>
<tr>
<td width="50%" valign="top"><img src="docs/screenshots/sunset_sea.jpg" alt="Sunset over the sea" width="100%"><br>Sunset over the sea</td>
<td width="50%" valign="top"><img src="docs/screenshots/haze.jpg" alt="Hazy horizon from 10 km" width="100%"><br>The hazy horizon from 10 km up</td>
</tr>
<tr>
<td width="50%" valign="top"><img src="docs/screenshots/earth_mountains.jpg" alt="The Everest massif" width="100%"><br>The Everest massif from about 7 km up (Copernicus DEM 3 arc-second (about 90 m) elevations as true-height relief; finer slopes are procedural)</td>
<td width="50%" valign="top"><img src="docs/screenshots/earth_alps.jpg" alt="The Alps from orbit" width="100%"><br>The Alps at midday in November from 300 km up (shading of ridges and valleys)</td>
</tr>
<tr>
<td width="50%" valign="top"><img src="docs/screenshots/iss.jpg" alt="International Space Station" width="100%"><br>The International Space Station above Earth</td>
<td width="50%" valign="top"><img src="docs/screenshots/moon.jpg" alt="The Moon&#x27;s near side" width="100%"><br>The Moon's near side (real NASA data)</td>
</tr>
<tr>
<td width="50%" valign="top"><img src="docs/screenshots/moon_terrain.jpg" alt="Mons Hadley on the Moon" width="100%"><br>Low over the Moon: Mons Hadley near the Apollo 15 landing site, with Hadley Rille in front (SLDEM2015 elevations of about 60 m as true-height relief; smaller craters are procedural)</td>
<td width="50%" valign="top"><img src="docs/screenshots/lunar_eclipse.jpg" alt="Total lunar eclipse of 31 December 2028" width="100%"><br>Total lunar eclipse of 31 December 2028 (seen from beside the Moon)</td>
</tr>
<tr>
<td width="50%" valign="top"><img src="docs/screenshots/solar_eclipse.jpg" alt="Solar eclipse of 2 August 2027" width="100%"><br>Solar eclipse of 2 August 2027 (the Moon's shadow over North Africa)</td>
<td width="50%" valign="top"><img src="docs/screenshots/eclipse_ground.jpg" alt="Total solar eclipse from the ground" width="100%"><br>The total eclipse of 2 August 2027 from the ground (corona)</td>
</tr>
</table>

**The Apollo program**

<table>
<tr>
<td width="50%" valign="top"><img src="docs/screenshots/apollo15_undock.jpg" alt="Lunar module undocking" width="100%"><br>30 July 1971: the lunar module undocking from the command and service module in lunar orbit (Apollo 15)</td>
<td width="50%" valign="top"><img src="docs/screenshots/apollo15_landing.jpg" alt="Apollo 15 landing" width="100%"><br>The Apollo 15 lunar module coming down onto the lunar terrain just before landing</td>
</tr>
<tr>
<td width="50%" valign="top"><img src="docs/screenshots/apollo15_eva.jpg" alt="Apollo 15 deep-space EVA" width="100%"><br>The deep-space EVA on the way home: the command module pilot at the service module's instrument bay (Apollo 15)</td>
<td width="50%" valign="top"><img src="docs/screenshots/apollo15_splashdown.jpg" alt="Apollo 15 splashdown" width="100%"><br>7 August 1971: the Apollo 15 command module coming down on two of its three main parachutes</td>
</tr>
</table>

**Planets and moons**

<table>
<tr>
<td width="50%" valign="top"><img src="docs/screenshots/mars_map.jpg" alt="Mars with Valles Marineris" width="100%"><br>Mars from Viking maps and MOLA elevations (Valles Marineris in the middle)</td>
<td width="50%" valign="top"><img src="docs/screenshots/mars_terrain.jpg" alt="Inside Valles Marineris" width="100%"><br>Looking east inside Valles Marineris from low altitude (blended MOLA and HRSC elevations as true-height relief; the layered steps, ridges and gullies of the cliffs are procedural)</td>
</tr>
<tr>
<td width="50%" valign="top"><img src="docs/screenshots/mars_sky.jpg" alt="Daytime sky on Mars" width="100%"><br>The butterscotch daytime sky of Mars</td>
<td width="50%" valign="top"><img src="docs/screenshots/jupiter_map.jpg" alt="Jupiter with the Great Red Spot" width="100%"><br>Jupiter from Cassini maps (Great Red Spot at lower right)</td>
</tr>
<tr>
<td width="50%" valign="top"><img src="docs/screenshots/shoemaker_levy9.jpg" alt="Shoemaker-Levy 9 scars" width="100%"><br>18 July 1994: scars left on Jupiter by fragments of Comet Shoemaker-Levy 9</td>
<td width="50%" valign="top"><img src="docs/screenshots/io.jpg" alt="Io" width="100%"><br>Io from Galileo / Voyager maps</td>
</tr>
<tr>
<td width="50%" valign="top"><img src="docs/screenshots/saturn.jpg" alt="Saturn and its rings" width="100%"><br>Saturn and its rings (with the rings' shadow on the planet)</td>
<td width="50%" valign="top"><img src="docs/screenshots/saturn_rings.jpg" alt="Saturn&#x27;s rings from Voyager data" width="100%"><br>Saturn's rings with the density measured by Voyager 2</td>
</tr>
<tr>
<td width="50%" valign="top"><img src="docs/screenshots/uranus_rings.jpg" alt="Rings of Uranus" width="100%"><br>Tipped-over Uranus and its narrow rings</td>
<td width="50%" valign="top"><img src="docs/screenshots/pluto.jpg" alt="Pluto with its heart" width="100%"><br>Pluto from New Horizons maps</td>
</tr>
<tr>
<td width="50%" valign="top"><img src="docs/screenshots/venus_surface.jpg" alt="Surface of Venus" width="100%"><br>The surface of Venus below the clouds (Venera 13 landing site). The thick haze hides everything beyond a few km (brightness and haze density are estimates)</td>
<td width="50%" valign="top"><img src="docs/screenshots/titan_surface.jpg" alt="Surface of Titan" width="100%"><br>Titan below its haze, 5 km above the Huygens landing site (Cassini radar elevations; brightness and haze density are estimates)</td>
</tr>
<tr>
<td width="50%" valign="top"><img src="docs/screenshots/europa_ridges.jpg" alt="Double ridges on Europa" width="100%"><br>Double ridges crossing Europa's icy surface (about 200 m high; the layout is procedural, the sizes are estimates from observations)</td>
<td width="50%" valign="top"><img src="docs/screenshots/pluto_sputnik.jpg" alt="Sputnik Planitia" width="100%"><br>The polygons of Pluto's Sputnik Planitia, formed by convection in nitrogen ice (about 25 km across, with troughs about 100 m deep at their edges; the layout is procedural, the sizes are estimates from observations)</td>
</tr>
<tr>
<td width="50%" valign="top"><img src="docs/screenshots/triton_cantaloupe.jpg" alt="Cantaloupe terrain on Triton" width="100%"><br>Triton's cantaloupe terrain: a network of dimples about 35 km across with ridges between them (about 500 m deep; the layout is procedural, the sizes and depth are estimates from Voyager 2 images)</td>
<td width="50%"></td>
</tr>
</table>

**Comets and asteroids**

<table>
<tr>
<td width="50%" valign="top"><img src="docs/screenshots/halley.jpg" alt="Halley&#x27;s Comet on 9 February 1986" width="100%"><br>Halley's Comet on 9 February 1986 (perihelion)</td>
<td width="50%" valign="top"><img src="docs/screenshots/hyakutake.jpg" alt="Comet Hyakutake near Earth" width="100%"><br>Comet Hyakutake passing 0.1 AU from Earth, 25 March 1996</td>
</tr>
<tr>
<td width="50%" valign="top"><img src="docs/screenshots/comet_67p.jpg" alt="Comet Churyumov-Gerasimenko" width="100%"><br>Comet Churyumov-Gerasimenko in its real shape (August 2015, perihelion)</td>
<td width="50%" valign="top"><img src="docs/screenshots/ryugu.jpg" alt="Ryugu" width="100%"><br>The spinning-top asteroid Ryugu</td>
</tr>
<tr>
<td width="50%" valign="top"><img src="docs/screenshots/itokawa.jpg" alt="Itokawa" width="100%"><br>Itokawa in its real shape</td>
<td width="50%" valign="top"><img src="docs/screenshots/asteroid_belt.jpg" alt="Asteroid belt and Trojans" width="100%"><br>The main belt (ring) and Jupiter's Trojans (green) from 15 AU above</td>
</tr>
<tr>
<td width="50%" valign="top"><img src="docs/screenshots/vesta.jpg" alt="Vesta" width="100%"><br>Vesta in its real lumpy shape from Dawn elevations</td>
<td width="50%" valign="top"><img src="docs/screenshots/ceres_occator.jpg" alt="Occator crater on Ceres" width="100%"><br>The bright deposits of Occator crater on Ceres, with its rim rising on the horizon (Dawn elevations as true-height relief)</td>
</tr>
</table>

**Spacecraft**

<table>
<tr>
<td width="50%" valign="top"><img src="docs/screenshots/voyager1_jupiter.jpg" alt="Voyager 1 at Jupiter" width="100%"><br>Voyager 1 at closest approach to Jupiter, 5 March 1979 (K key, spacecraft page)</td>
<td width="50%" valign="top"><img src="docs/screenshots/voyager2_neptune.jpg" alt="Voyager 2 at Neptune" width="100%"><br>Voyager 2 passing over Neptune's night side, 25 August 1989</td>
</tr>
<tr>
<td width="50%" valign="top"><img src="docs/screenshots/cassini.jpg" alt="Cassini arriving at Saturn" width="100%"><br>Cassini arriving at Saturn, 1 July 2004 (Huygens probe on its side)</td>
<td width="50%" valign="top"><img src="docs/screenshots/juno.jpg" alt="Juno entering orbit around Jupiter" width="100%"><br>Juno entering orbit around Jupiter, 5 July 2016</td>
</tr>
<tr>
<td width="50%" valign="top"><img src="docs/screenshots/rosetta.jpg" alt="Rosetta arriving at Comet Churyumov-Gerasimenko" width="100%"><br>Guided tour (U key): Rosetta arriving at its comet, 6 August 2014</td>
<td width="50%" valign="top"><img src="docs/screenshots/dart_impact.jpg" alt="One minute before DART&#x27;s impact" width="100%"><br>Didymos and Dimorphos one minute before DART's impact</td>
</tr>
<tr>
<td width="50%" valign="top"><img src="docs/screenshots/europa_clipper.jpg" alt="Europa Clipper" width="100%"><br>Europa Clipper on its way to Jupiter (30 m solar array span)</td>
<td width="50%" valign="top"><img src="docs/screenshots/probe_view.jpg" alt="Juno" width="100%"><br>Warping to a spacecraft: centered and large in a zoomed view (Juno at Jupiter)</td>
</tr>
<tr>
<td width="50%" valign="top"><img src="docs/screenshots/voyage_bepicolombo.jpg" alt="BepiColombo arriving at Mercury" width="100%"><br>Spacecraft voyage: BepiColombo just before entering Mercury orbit in November 2026 (top right: a map of its path so far)</td>
<td width="50%" valign="top"><img src="docs/screenshots/voyage_dinkinesh.jpg" alt="Asteroid Dinkinesh" width="100%"><br>Spacecraft voyage: asteroid Dinkinesh, passed by Lucy at 422 km in November 2023 (zoomed in automatically)</td>
</tr>
<tr>
<td width="50%" valign="top"><img src="docs/screenshots/perseverance_skycrane.jpg" alt="Perseverance on the sky crane" width="100%"><br>18 February 2021: Perseverance's sky crane lowering the rover about 7 m below it (8.5 seconds before touchdown; NAIF's reconstructed descent trajectory, simplified shapes)</td>
<td width="50%" valign="top"><img src="docs/screenshots/spirit_airbags.jpg" alt="Spirit on its airbags" width="100%"><br>4 January 2004: Spirit inside its inflated airbags, hanging 20 m below the parachute and backshell (3 seconds before first impact; shapes and parachute size are estimates)</td>
</tr>
</table>

**The sky from the ground and historic events**

<table>
<tr>
<td width="50%" valign="top"><img src="docs/screenshots/constellations.jpg" alt="Constellation lines and names" width="100%"><br>The real starry sky with constellation lines (C key)</td>
<td width="50%" valign="top"><img src="docs/screenshots/sky_from_ground.jpg" alt="Southern sky from Mauna Kea" width="100%"><br>The night sky to the south from the summit of Mauna Kea (search, "On Earth")</td>
</tr>
<tr>
<td width="50%" valign="top"><img src="docs/screenshots/ground_view.jpg" alt="From the ground in Tokyo" width="100%"><br>Before dawn in Tokyo: the horizon, compass points and Venus's altitude, azimuth and rise/set times (lower left)</td>
<td width="50%" valign="top"><img src="docs/screenshots/geminids.jpg" alt="Geminids night" width="100%"><br>Geminids night in Tokyo (a meteor at the upper left), with Jupiter's magnitude, phase and apparent size on the right</td>
</tr>
<tr>
<td width="50%" valign="top"><img src="docs/screenshots/mercury_transit.jpg" alt="Transit of Mercury, 13 November 2032" width="100%"><br>Transit of Mercury, 13 November 2032</td>
<td width="50%" valign="top"><img src="docs/screenshots/historic_eclipse.jpg" alt="Eddington&#x27;s eclipse" width="100%"><br>Historic eclipses: the 1919 total eclipse on Principe where Eddington's team confirmed general relativity</td>
</tr>
<tr>
<td width="50%" valign="top"><img src="docs/screenshots/supernova_1054.jpg" alt="The supernova of 1054" width="100%"><br>July 1054: the supernova that left the Crab Nebula, before dawn over Kaifeng, the Song capital</td>
<td width="50%" valign="top"><img src="docs/screenshots/galileo_1610.jpg" alt="Jupiter as Galileo saw it" width="100%"><br>7 January 1610: Jupiter and its moons as Galileo saw them from Padua (two to the east, one to the west)</td>
</tr>
<tr>
<td width="50%" valign="top"><img src="docs/screenshots/andromeda.jpg" alt="Andromeda Galaxy" width="100%"><br>The Andromeda Galaxy zoomed in, with its companions M32 and M110</td>
<td width="50%" valign="top"><img src="docs/screenshots/carina_nebula.jpg" alt="Carina Nebula" width="100%"><br>The Carina Nebula (names shown with the C key)</td>
</tr>
</table>

**Stars and exoplanets**

<table>
<tr>
<td width="50%" valign="top"><img src="docs/screenshots/sirius.jpg" alt="Sirius A" width="100%"><br>Sirius A, 8.7 light-years away (top left: the white dwarf Sirius B, 30 AU away)</td>
<td width="50%" valign="top"><img src="docs/screenshots/sirius_b.jpg" alt="Sirius B" width="100%"><br>Guided tour: the white dwarf Sirius B, with Sirius A 20 AU away</td>
</tr>
<tr>
<td width="50%" valign="top"><img src="docs/screenshots/red_dwarf_flare.jpg" alt="A flare on Proxima Centauri" width="100%"><br>The spotted red dwarf Proxima Centauri and a blue-white flare</td>
<td width="50%" valign="top"><img src="docs/screenshots/brown_dwarf.jpg" alt="Brown dwarf" width="100%"><br>The brown dwarf Luhman 16 B with cloud bands and patchy clouds</td>
</tr>
<tr>
<td width="50%" valign="top"><img src="docs/screenshots/altair.jpg" alt="Altair" width="100%"><br>Altair (16.7 light-years), flattened by its fast spin</td>
<td width="50%" valign="top"><img src="docs/screenshots/exoplanet.jpg" alt="Epsilon Eridani b" width="100%"><br>Epsilon Eridani b, a Jupiter-like planet 10.5 light-years away</td>
</tr>
<tr>
<td width="50%" valign="top"><img src="docs/screenshots/proxima_b_ground.jpg" alt="On Proxima Centauri b" width="100%"><br>On the ground of Proxima Centauri b (artist's impression): the red star hangs still in the sky</td>
<td width="50%" valign="top"><img src="docs/screenshots/habitable_zone.jpg" alt="Habitable zone" width="100%"><br>The system map centered on Proxima Centauri with its habitable zone (green); planet b orbits inside it</td>
</tr>
<tr>
<td width="50%" valign="top"><img src="docs/screenshots/star_map.jpg" alt="Stars around the Sun" width="100%"><br>The system map's "stars around the Sun" (zoom all the way out; the stems show the height above the ecliptic)</td>
<td width="50%" valign="top"><img src="docs/screenshots/star_epoch.jpg" alt="The Big Dipper now and in 100,000 years" width="100%"><br>The "Star epoch" setting: the Big Dipper now (left) and in 100,000 years (right), deformed by the proper motions of its stars</td>
</tr>
<tr>
<td width="50%" valign="top"><img src="docs/screenshots/interstellar_warp.jpg" alt="Interstellar warp" width="100%"><br>Interstellar warp (the sky crowds forward and turns blue as you pass through a swirling tunnel of light)</td>
<td width="50%" valign="top"><img src="docs/screenshots/hyperdrive.jpg" alt="Hyperdrive" width="100%"><br>The hyperdrive (8): one light-year per second toward Alpha Centauri</td>
</tr>
<tr>
<td width="50%" valign="top"><img src="docs/screenshots/relativistic.jpg" alt="At 99% of light speed" width="100%"><br>At 99% of the speed of light the stars and the Milky Way crowd forward and turn blue and bright (aberration, Doppler effect)</td>
<td width="50%" valign="top"><img src="docs/screenshots/frame_lock.jpg" alt="Frame fixed to the ship" width="100%"><br>Reference frame fixed to the ship (Shift+F): Earth leaving at 30 km/s (top right)</td>
</tr>
</table>

**Features and screens**

<table>
<tr>
<td width="50%" valign="top"><img src="docs/screenshots/warp.jpg" alt="Warp" width="100%"><br>Warp effect</td>
<td width="50%" valign="top"><img src="docs/screenshots/huygens_descent.jpg" alt="Huygens descending to Titan" width="100%"><br>Photo mode (Z key): Huygens coming down through Titan's haze under its stabilizer parachute on 14 January 2005 (about 2.8 km up; the parachute size is an estimate)</td>
</tr>
<tr>
<td width="50%" valign="top"><img src="docs/screenshots/gravity_mode.jpg" alt="Gravity mode" width="100%"><br>Gravity mode (R key): the predicted orbit around Earth</td>
<td width="50%" valign="top"><img src="docs/screenshots/system_map.jpg" alt="System map" width="100%"><br>The system map (F3; 1989 with the path of Voyager 1)</td>
</tr>
<tr>
<td width="50%" valign="top"><img src="docs/screenshots/body_info.jpg" alt="Body info" width="100%"><br>The info card (I key; Jupiter)</td>
<td width="50%" valign="top"><img src="docs/screenshots/share_link.jpg" alt="Share link" width="100%"><br>A scene opened from a share link (above Mars; F2 copies one)</td>
</tr>
<tr>
<td width="50%" valign="top"><img src="docs/screenshots/first_run.jpg" alt="First-run guide" width="100%"><br>The guide shown on the first launch</td>
<td width="50%" valign="top"><img src="docs/screenshots/settings_large.jpg" alt="Extra large text" width="100%"><br>The settings with text size "Extra large"</td>
</tr>
<tr>
<td width="50%" valign="top"><img src="docs/screenshots/browser.jpg" alt="Browser version" width="100%"><br>The browser version (WebGL2)</td>
<td width="50%" valign="top"><img src="docs/screenshots/touch.jpg" alt="Touch controls on a phone" width="100%"><br>Touch controls on a phone in landscape (during the tour)</td>
</tr>
</table>

## Getting started

On the first launch a short guide (4 pages) introduces the controls and highlights (Enter or tap for next, Esc to close). Later, open the controls with H.

## Browser version

Try it in your browser without installing anything: **https://hubstacksea.github.io/Solar-System-Simulator/**

- Runs in browsers with WebGL2 (recent Chrome / Edge / Firefox / Safari). Only the 21 MB needed to start is loaded first (the loading bar covers both the download and the preparation after it); the planet and moon maps, asteroid shapes, Moon maps, Earth's clouds and seasons, the fine elevations of Earth, the Moon and Mars, and the asteroid belt (about 175 MB) are loaded in the background while you play. The finer elevations of the poles of the Moon and Mars (about 26 MB for the four) are loaded when you come near that body, on that pole's hemisphere. Until they arrive, surfaces are drawn procedurally and "Loading maps (N left)" is shown at the bottom left
- Music is not included (sound effects are). Japanese text uses the bundled Noto Sans JP font
- Click the view to steer with the mouse (Esc to release). Settings and bookmarks are kept in the browser
- On phones and tablets, on-screen buttons appear: drag to steer, Thrust / Brake / Stop on the right, Target / Warp / Autopilot on the left, Tour / Events / Map / Settings / Help / Share at the top, and tap the engine bar to switch engines. Quality starts at Low, and small screens are rendered at 720 pixels high and scaled down
- If it runs slowly, lower the quality with F9. Looking straight down from just above a surface is the heaviest view. When drawing is heavy, the 3D resolution is lowered automatically (down to 0.5x; the FPS readout at the lower left shows the resolution while it is lowered)
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

Pushing a tag such as `v2.71.0` runs `.github/workflows/release.yml`, which builds Linux (.deb / .rpm), Windows (.7z), macOS (.zip) and the browser version, creates the GitHub release with the packages, and publishes the browser version to the `gh-pages` branch for GitHub Pages. The Linux build is also launched to take screenshots (Earth, Mars, the map and the English settings) and logs, saved as `screenshots-linux` in the run (a failed launch does not stop the release). The macOS build is only checked for reading its data, because GitHub's Mac virtual machines have no GPU and cannot open a window. The browser version is also opened in headless Chrome (`tools/web_smoke.py`, including voyage links and key, click and drag input), and its screen and console output are saved as `screenshots-web` (it also shows how many seconds after the page opened all the background data had arrived, and how long the data loaded on approach took to arrive).

```sh
git tag v2.71.0
git push origin master v2.71.0
```

### Checks before a release

```sh
python3 tools/run_checks.py build/solar_voyager   # --no-display without a screen
```

It checks the embedded data, 9 solar eclipses against NASA's catalog, the number of historic events, that the browser version's font has every character shown on screen (if not, rebuild it with `tools/make_web_font.py`), the browser version's shader (the vertex shader must call the relief calculation from one place only; writing it in several places kept the browser from finishing the shader translation, so it never started), warps to every body (arriving facing it and staying still) light-speed flights just above the oblate planets (the surface must not vanish) and a low flight at 70° S on the Moon (the drawn terrain must match the surface height, so the ground in front does not vanish), printing PASS / FAIL for each (about 10 minutes in all).

After changing how surfaces look, retake the README images that show the ground in the same scenes (fixed dates and places) with `python3 tools/retake_gallery.py build/solar_voyager <output folder> [name ...] --apply` (requires Pillow).

To compare drawing speed, `python3 tools/bench_render.py build/solar_voyager <previous executable>` times 200 frames in fixed scenes (near the surfaces of Pluto, Europa, Triton, the Moon, Mars (two, one below the Olympus Mons scarp) and Earth; the shortest of 3 runs each).

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
| O / L | Orbit lines (the path driven by a target rover) / moon markers |
| C | Constellations (off → lines → lines and names) |
| [ / ] | Time scale (x1 real time to 1 year per second). Affects both orbits/rotation of the bodies and the ship's acceleration and movement |
| P / Backspace | Pause time / sync to current time at x1 |
| Y | Set the date and time (UTC; type the digits and press Enter; 3000 BC to AD 2999, - for BC; Julian calendar before 15 October 1582) |
| F10 | Settings (language, text size, quality, sound effects, music, constellations, Milky Way & nebulae, star epoch, orbit lines, asteroids, markers, invert mouse, flight mode, resume, fullscreen; Up/Down to select, Left/Right / Enter to change, saved at once). Text size is Normal / Large / Extra large and enlarges the HUD, markers and panels together (limited so they still fit on small screens). On Windows and Linux with display scaling above 100% (4K screens etc.), the window and text start enlarged to match |
| R | Switch the flight model (arcade / gravity). In gravity mode you fall under the bodies' gravity: W / S thrust along the nose and X stops relative to the reference body. The predicted trajectory is drawn, with the periapsis and apoapsis altitudes and the period |
| Z | Photo mode (each press: hide gauges and markers → hide the ship too → back to normal; orbit lines are toggled with O) |
| U | Guided tour: 24 scenes (spacecraft highlights, great comets, eclipses, nearby stars, historic eclipses and more) in chronological order with captions, 15 seconds each. Enter for next, Shift+Enter for previous, U again to stop |
| K | Event list (select one and press Enter to warp to that time and place). Tab / Shift+Tab switches pages: astronomical events (solar and lunar eclipses, planetary conjunctions and oppositions over the next 3 years, the next shadow transit of each Galilean moon across Jupiter, and transits of Mercury and Venus up to 150 years ahead), comets and binary stars (comet perihelia and closest approaches to Earth, asteroid Apophis's 2029 Earth encounter from space and from the ground, binary periastra) and spacecraft events (closest approaches, orbit insertions, end of mission, the Huygens landing on Titan, the Spirit, Opportunity, Phoenix, Curiosity, InSight and Perseverance landings on Mars) and historic eclipses, supernovae, returns of Halley's Comet, transits of Venus and a comet hitting Jupiter; spacecraft voyages (see "Spacecraft voyages" below) |
| I | Info card for the target (type, radius, mass, surface gravity, escape velocity, rotation and orbital periods, orbit, number of moons, distances from the Sun / Earth / you, brightness (magnitude), phase, apparent size and Moon age as seen from Earth (or from where you stand on the ground), spacecraft that flew by; data period for spacecraft, altitude and period for satellites, landing time, sols since landing and distance driven for landers). "Info" button on touch screens, tap the card to close. Mouse inversion is in the settings (F10) |
| J | Switch language (日本語 / English) |
| B / N | Bookmark the current spot / warp to bookmarks in turn (up to 9; position and orientation are stored relative to the rotating body) |
| M / Shift+M | Sound effects (engine and warp) / music on/off |
| F9 | Quality High → Low → Medium (use Low if it runs slowly: 3D at 60% resolution with less detail) |
| / or F4 | Find a body (type part of a name in kana or Latin letters, Up/Down to select, Enter to target, Shift+Enter to warp, Tab for the category; on touch screens use the "Search" button and tap the category tabs and the list. Spacecraft not in flight at that date are shown dimmed; choosing one moves the date to its highlight (such as its arrival) and warps there. The "On Earth" tab places you on the ground at one of 20 places on Earth such as Tokyo, London or the South Pole, on the Moon (the Apollo 11 and 17 sites, the lunar south pole, Chang'e 4 on the far side), on Mars (Perseverance, Curiosity, Viking 1), on Venus (the Venera 7-14 and Vega 1 and 2 landing sites) or on Titan (the Huygens landing site and Selk crater, Dragonfly's destination), looking up at the sky) |
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
- **Globe minimap** (below the top-left panel): near the surface of a body with a map (Earth, the Moon, Mars, Mercury, moons, etc.; up to an altitude of 10% of its radius), a globe of the map centered on your position with north up, with a dot marking where you are. The night side is darkened, and a yellow line shows the ground below you over the next 3 hours at your current velocity (a cross marks where you would hit the ground)
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
- The spacecraft's shape changes with time (the Saturn V stages, the command and service module with the lunar module, the descent stage left on the Moon, the command module under parachutes). While docked, the lunar module is drawn on the nose of the command module. Undocking and the ascent stage docking in lunar orbit are shown with both craft seen from the side: the lunar module backs away from the docking port and turns, and the ascent stage closes in, stationkeeps, lines up and makes contact (times and distances from each mission's records; the procedure's values where none were found). The ascent stage jettison before leaving the Moon is shown too: the ascent stage is pointed straight away from the Moon, released and drifts slowly away. The subsatellite Apollo 15 and 16 released from the service module's instrument bay is shown too, deploying its booms and spinning as it drifts away. The instrument bay door Apollo 15-17 jettisoned on the way to the Moon is shown too, tumbling away and leaving the bay open. The deep-space EVAs Apollo 15-17 made on the way back to Earth are shown from the side too: the hatch opens, the command module pilot moves along the service module to retrieve the bay's cassettes, and another crew member stands in the hatch (times from each mission's records). From separating from the S-IVB, turning around and docking with the lunar module stowed on top of it to pulling it out, the S-IVB and the adapter panels are drawn too, with the speeds and times of each mission's records. The first stage, the interstage ring and the second stage are drawn too after separation, slowed by their retro rockets and falling away under Earth's gravity. The service module jettison before the return to Earth is shown with both modules seen from the side, the service module thrusting away and spinning as it recedes (twice on Apollo 13: the service module and the lunar module). The map at the top right is centered on Earth and switches to a close-up around the Moon near it (drawn as seen from the Moon, so lunar orbits appear as plain ellipses)
- Even without choosing the event, the spacecraft appear and fly as recorded whenever the date falls within the flight (the descent stages stay on the Moon after liftoff)
- Engines show flames or plumes while burning (a bright flame from the Saturn V first stage, faint plumes in vacuum from the others). During main engine burns the engine points against the thrust, so braking burns such as lunar orbit insertion are flown backwards (the spacecraft turns over the 4 minutes before and after); otherwise the nose points in the direction of travel. Time slows to real time at the start and end of each burn of the followed craft
- The trajectories are computed with the gravity of Earth, the Moon and the Sun from the positions and velocities at the times listed in NASA's flight records (Apollo by the Numbers and each mission report), since no ephemerides of the spacecraft themselves are published. Velocities are matched at the burns so the paths pass the recorded positions, and the effect of the Moon's uneven gravity (not modeled) is spread over each leg. The launch follows the recorded ascent points; the lunar descent and ascent pass the recorded times, altitudes and speeds (Apollo 11's record serves as the guide between them); entry is solved from the equations of motion to match the recorded peak deceleration and the parachute time and place
- **Opening by link**: add `?apollo=11` to the URL to start that flight at liftoff, and `&scene=<number>` for a scene. On the PC, use `--share "?apollo=11"`

## How it works

- **Body positions**: Mercury to Neptune are computed from the planetary theory VSOP87 (Bretagnon & Francou 1988, truncated to about 5,700 terms): within 1.5″ of JPL Horizons (DE441) for Mercury to Saturn and 8″ for Uranus and Neptune in 1500–2500, and within 4.4′ over 3000 BC to AD 3000 (growing into the distant past with the error of VSOP87 itself). The orbit lines are ellipses from the current position and velocity, so they pass exactly through the planets. Pluto is computed from JPL mean orbital elements and their rates via Kepler's equation. The slowing of Earth's rotation (ΔT; the Espenak & Meeus expressions used by NASA's eclipse catalog for 500 BC to AD 2150 and Morrison & Stephenson 2004 outside; about 4.8 hours in 500 BC) and the precession of Earth's axis (IAU 1976) are included. The Moon uses the series of chapter 47 of Meeus's *Astronomical Algorithms* (a truncated ELP-2000/82) with a long-term correction fitted to the Moon of JPL Horizons (DE441), so its real position and phase are reproduced. Its orientation follows the IAU rotation model of the Moon, so seen from Earth it nods by about ±8° in longitude and ±7° in latitude (libration), showing a little beyond its edges. Solar eclipses from 763 BC to AD 2950 agree with NASA's catalog within 4 minutes and 2° (checked by `tools/run_checks.py`). Other moons are placed on circular orbits in the orbital planes from JPL's mean orbital elements, with their positions fitted to JPL Horizons for 1980–2070 (most within 1°, at most a few degrees; Mimas includes its ±43° libration from the resonance with Tethys). Shadows of Jupiter's moons fall on Jupiter at nearly the real times
- **Dwarf planets, asteroids and comets**: Ceres, Vesta, Eris, Ryugu, Bennu, Itokawa, Didymos (with its moon Dimorphos), Halley's Comet, Comet Hale-Bopp, Comet Hyakutake and Comet Churyumov-Gerasimenko. Their orbital elements and rates, in the same form as the planets', are fitted to JPL Horizons positions for 1980–2070 (direction seen from the Sun within 0.6°). Ryugu (2012–2024) and Churyumov-Gerasimenko (2012–2018) are fitted only around the time spacecraft visited them (errors 0.9° and 0.07°), because encounters with Earth and Jupiter change their orbits; they drift further away from those years. Comet Encke (period 3.3 years) is not included: Jupiter's perturbations and other effects shift its perihelion times from one passage to the next, so it cannot be fitted in this form (errors over 20°). The asteroids that spacecraft passed on their way (Gaspra, Ida, Dinkinesh, Donaldjohanson, Eurybates, Polymele, Leucus, Orus, Patroclus, Torifune and Apophis) are fitted around the time of those flybys (errors within 0.2°). Sizes and rotation periods come from the JPL Small-Body Database; Gaspra, Ida and Donaldjohanson have their real shapes, and the rest are spheres. Apophis passes 38,000 km from Earth's center on 13 April 2029, which changes its orbit, so from 2024 to 2060 it follows its JPL Horizons trajectory
- **Comet tails**: inside 4 AU from the Sun, comets grow a coma (the glow around the nucleus) and tails that get longer and brighter closer to the Sun (the great comet Hale-Bopp starts farther out and grows larger). The blue ion tail points straight away from the Sun, the yellowish dust tail curves back along the orbit. From the comet page of the K list you can warp to Halley's perihelia in 1986 and 2061, Hyakutake passing 0.1 AU from Earth on 25 March 1996, and more
- **Surfaces**: Jupiter carries Cassini's global map and is turned so that the Great Red Spot sits at its observed longitude for the date (System II; records since 1962, then a drift of 16° per year). Saturn also carries a Cassini global map (August 2011, showing the aftermath of the great northern storm); its poles and the band from the equator to 17° S, hidden by the ring shadow, are procedural clouds. Mercury, Mars, Ceres, Vesta, Pluto and the moons Io, Europa, Ganymede, Callisto, Mimas, Enceladus, Tethys, Dione, Rhea, Iapetus, Miranda, Ariel, Umbriel, Titania, Oberon, Triton and Charon carry real maps from spacecraft images (Olympus Mons and Valles Marineris, Pluto's heart and Enceladus's tiger stripes appear where they really are). Mars also gets relief shading from MOLA elevations. Near the ground, a few regions such as famous mountains (around Everest, Fuji, Mont Blanc and the Matterhorn, Kilimanjaro and Denali on Earth; the Apollo 15 and 17 landing sites, Mons Huygens and Tycho on the Moon; Olympus Mons and the Tharsis Montes on Mars) use still finer elevations (about 60–206 m). Mercury, Mars, Ceres, Vesta and Pluto rotate according to the IAU prime meridians, and the moons always keep the same face toward their planet. Areas that were never imaged (the northern hemispheres of the Uranian moons and parts of Pluto, Triton and Charon) and detail finer than the maps are filled in procedurally. Iapetus's map has its brightness flattened, so its bright and dark hemispheres are painted procedurally. Uranus, Neptune and the clouds of Venus keep procedural patterns, because no freely redistributable global maps could be found (and Uranus and Venus's clouds show almost no features in visible light). Below the cloud layer 70 km above Venus you come down onto a surface from Magellan elevations: inside the clouds is white fog, below them a uniform overcast sky, and the ground disappears into the thick haze within a few km (the brightness and haze density are estimates). The other bodies are generated procedurally in shaders; finer patterns and relief appear as you get closer, so surfaces stay detailed even near the ground. The ground on Earth and Mars is lit by sunlight weakened on its way through the atmosphere and by the light it scatters down from the sky (more on surfaces facing up; Rayleigh scattering and aerosols on Earth, and on Mars dust that dims sunlight about equally in every color and absorbs blue; the optical depths and the like are estimates). Midday colors are unchanged, shade takes the color of the sky, and mornings and evenings turn red on Earth and reddish brown on Mars. Near the ground, the weaker the light falling on the ground, the brighter the ground and the ship are shown (like the eye adapting: the light to the power -0.7, at most 2.2 times; bright parts level off so they do not blow out, and the sky and haze are left as they are; not around sunset or from space). When the Sun is lower than 20°, mountains and crater rims cast shadows, found by tracing the elevations toward the Sun (per vertex of the terrain grid near the ground and interpolated; skylight still reaches the shadows). The Mars map (Viking MDIM 2.1 colored by NASA Ames) paints the dark regions bluish gray, so only its hue is replaced with estimated reflectance colors for bright dust and dark sand by brightness. Airless bodies are lit with the opposition effect, which brightens the ground when the Sun is behind you because the shadows between grains are hidden (Hapke's shadow-hiding term, with the lunar values B0 = 1 and h = 0.06 used for all of them). Earth's ice sheets, snow and sea ice and the Martian polar caps (the white parts of the maps) get forward scattering that brightens them when you look toward the Sun at a low angle, plus a sheen, and the Martian caps get no small procedural craters. Near the ground, relief finer than the maps is shaded by surface material: on the Moon, boulders (1–12 m across) in proportion to Diviner's rock abundance; on Mars, from the TES thermal inertia, subdued small relief where there is dust and boulders on rocky ground, with ripples (25 m wavelength) and dunes (300 m, without small craters) only in the dune fields of the USGS dune database (about 0.7% of the surface); the small Martian craters are mostly shallow and worn, filled and eroded by dust and sand (flat floors, low rims, fewer the smaller they are), and the ground gets brightness variations finer than the map (dark sand gathering in hollows, dark floors of filled craters, dark boulders; uniform where dust has settled), and Martian cliffs (slopes past 7° to 19°) become bare rock with layered steps every 250 m of elevation and ridges and gullies, without craters or dust (the fractions, strengths and shapes are estimates from the look of images); on the moons and Pluto, small craters follow the age of the surface (none on Io, in the south polar terrain of Enceladus or in Sputnik Planitia, very few on Europa, Titan and Triton, worn and shallow on Callisto), and Europa's double ridges, the ridges and grooves of Ganymede's bright terrain and of Enceladus's south polar terrain, the convection cells of Sputnik Planitia and the dimples of Triton's cantaloupe terrain are drawn as shading (sizes and heights are estimates from spacecraft images); on Earth, judged from the map colors, dunes (120 m and 800 m) in sand deserts, a bumpy canopy in forests and wind-carved ridges on snow and ice (where each material is comes from measurements or estimates; shapes finer than a few hundred meters are procedural, sized after real ones)
- **Earth**: built from real NASA data — continents, coastlines, elevation (relief shading), ocean depth (shallow-water color), sea ice and city lights at night (about 10 km resolution; the land elevations in an area about 3,800 km across around the camera are about 1.85 km; finer detail is filled in procedurally). Its rotation follows Greenwich sidereal time, so the regions in daylight and the lit cities at night match the PC clock. The surface colors follow the date using all 12 months of Blue Marble (winter snow, green rainy seasons and so on), and clouds come from NASA's global cloud image (one day's distribution) with finer streaks added
- **Moon**: real NASA data (LRO color and LOLA elevation, about 3–5 km resolution; the elevations in an area about 3,900 km across around the camera are about 1.9 km, and about 80 m poleward of 85° at both poles). Maria, craters and ray systems are where they really are; smaller craters are filled in procedurally
- **Surface heights**: within 8 body radii of Earth, the Moon, Mars, Mercury, Venus, Ceres, Vesta, Pluto, Charon and Titan, the elevation data is drawn as 3D terrain at its true height (1x). Altitude and surface collisions are measured from the terrain, and the Apollo lunar modules and the descent stages left on the Moon sit on it. The data spacing is about 1.85 km for Earth (on land around the camera; about 10 km farther away), about 1.9 km for the Moon (around the camera; about 5 km farther away), about 3.7 km for Mars (around the camera; about 460 m poleward of 80°, about 15 km farther away), about 7.5 km for Mercury, about 19 km for Venus, about 3 km for Ceres, about 1.8 km for Vesta, about 3.6 km for Pluto and Charon and about 11 km for Titan, and Earth's oceans sit at sea level. Relief finer than the data is added procedurally to the shape of the terrain (it does not match the real terrain): on Earth, slopes that follow the steepness of the terrain (rugged on steep mountains, gentle on plains; in the regions such as famous mountains, only what is finer than their data); on the other bodies, the same craters and slopes as in the surface shading (the lunar maria have fewer craters and are flat, and Titan's lakes and seas are flat). Altitude and collisions are measured from the same shape. The shape is detailed down to the spacing of the terrain grid's vertices, which gets coarser away from the camera. The relief is computed at lattice points fixed to the ground and interpolated, so its shape does not ripple in flight. Mount Fuji (about 3,360 m in the data) and Everest (about 8,210 m) stand as 3D mountains. Vesta gets its lumpy shape with the Rheasilvia basin. Pluto and Charon only have elevations for the hemisphere New Horizons saw up close; the rest is an estimated height joined smoothly from the surroundings
- **Music**: plays a piece for the nearest body: Holst's *The Planets* for the planets (the parent planet's movement near its moons), Grieg's "Morning Mood" for the Sun, the first movement of Beethoven's "Pastoral" Symphony for Earth and Debussy's "Clair de lune" for the Moon. It gets louder as you approach and fades out at the edge of the sphere of influence (1 AU for the Sun). Tracks crossfade when they change and resume where they left off when you return. Nothing plays near Pluto (the later Pluto movement by another composer is still under copyright), nor near the dwarf planets, asteroids and comets
- **Spacecraft around Earth**: the International Space Station and the Hubble Space Telescope are computed from CelesTrak orbital elements (TLE of 28 October 2026), including the drift of the orbital plane caused by Earth's oblateness (within about 35 km of Horizons). They drift further off the farther you go from that epoch: the ISS by hundreds of km within days, losing its position along the orbit within months. The James Webb Space Telescope follows its JPL Horizons trajectory around the L2 point 1.5 million km from Earth (until October 2031). Select them with Tab and warp with G
- **Gravity mode**: toggled in the settings or with the R key. You fall under the gravity of the Sun, the planets, the large moons and every body above 10²⁰ kg; when you switch it on or arrive by warp you are placed on a circular orbit around the current body. Body positions are updated every frame and held still within the frame while the motion is integrated in small steps (if time runs too fast for those steps, the time scale is held back automatically)
- **Asteroids and other small bodies**: about 100,000 objects from the JPL Small-Body Database — the main belt (about 60,000 of the brightest), the outer belt including the Hildas, near-Earth asteroids, Jupiter Trojans, Centaurs and trans-Neptunian objects — move on their orbits (two-body motion, computed and projected in a GPU shader) and are drawn as points. From afar you can see the ring of the main belt and the two Trojan swarms 60° ahead of and behind Jupiter. They are shown with the orbit lines (O key). They are off on the first launch; turn them on in the settings (F10)
- **Spacecraft**: Voyager 1 and 2, Pioneer 10 and 11, Galileo, Cassini, New Horizons, Parker Solar Probe, Juno, Rosetta, Hayabusa2, OSIRIS-REx and DART fly their real paths from JPL Horizons, from launch to end of mission (spacecraft leaving the Solar System keep coasting after the data ends in 2050–2100). Inside a planet's sphere of influence they move relative to the planet, so flybys of Jupiter, Neptune and the rest happen at the real distance, direction and time (encounters with moons can be off by a few thousand km because of the moons' position errors). Rosetta orbits Comet Churyumov-Gerasimenko and descends to its nucleus on 30 September 2016; Juno orbits Jupiter from 5 July 2016 (predicted trajectory after September 2026, data until September 2028). While Hayabusa2 stays at Ryugu (27 June 2018 – 13 November 2019) it is placed at its usual station, the home position 20 km from Ryugu toward Earth, because in Horizons the spacecraft and Ryugu come from separate solutions that put them only a few km apart. Their paths are shown with the orbit lines (O key), brighter where they have already flown. Each spacecraft has its own simple model (Voyager, Pioneer, Galileo, Cassini, New Horizons, Parker, Juno, Rosetta, Hayabusa2) with the high-gain antenna pointed at Earth (Parker keeps its heat shield toward the Sun). Cassini carries the Huygens probe until 25 December 2004, Galileo its atmospheric probe until 13 July 1995. Huygens flies for three weeks after its release, enters Titan's atmosphere on 14 January 2005 and lands after a 2 h 28 min parachute descent (the DTWG reconstructed trajectory from NASA PDS; the parachute sizes are estimates). Curiosity (6 August 2012) and Perseverance (18 February 2021) come down onto Mars from about 9 minutes before entry along NASA's reconstructed descent trajectories, landing by parachute, descent stage and sky crane. Spirit and Opportunity (2004, on airbags), Phoenix (2008) and InSight (2018, on legs and engines) also come down from atmospheric entry along their reconstructed descent trajectories (the shapes are simplified; the parachute sizes and the airbag bounces are estimates). After landing, the rovers move along NAIF's records of their positions (Curiosity and Perseverance up to recent days, Spirit to 2010 and Opportunity to 2018). Their markers are toggled together with the moon markers (L key). OSIRIS-REx stays at Bennu from December 2018 to May 2021 and brings back a sample, and DART impacts Dimorphos on 26 September 2022 (the K list and the tour warp you to one minute before the impact). The original Hayabusa is not included, because its trajectory up to Itokawa and most of its return trip are not publicly available. Missions in flight follow their planned Horizons trajectories (they will differ if the plans change): BepiColombo (into Mercury orbit in November 2026 after six Mercury flybys), JUICE (until just before its Jupiter arrival in July 2031), Europa Clipper (into Jupiter orbit in April 2030, until September 2034), Lucy (toward Jupiter's Trojans, until April 2033) and Psyche (until February 2029) and Hera (ESA; arrived at Didymos in October 2026, until 4 November). Horizons times are dynamical time (TDB) and are converted to universal time
- **Historic sky events**: the 4th page of the event list has 18 of them. Eclipses (the Assyrian eclipse of 763 BC, the eclipse of Thales in 585 BC, the annular eclipse at the Battle of Mizushima in 1183, Columbus's lunar eclipse in 1504, Eddington's expedition in 1919, the longest total eclipse of the 21st century in 2009) take you to the ground at the historic site at the deepest eclipse there. Supernovae (1006, 1054 which left the Crab Nebula, Tycho's of 1572, Kepler's of 1604) shine in the sky for the recorded time at the recorded brightness (the light curve is approximate) and take you to the night sky where they were recorded. Returns of Halley's Comet (240 BC in the Records of the Grand Historian, 1066 on the Bayeux Tapestry, 1301 painted by Giotto) , transits of Venus (Horrocks 1639, Cook 1769, 1882) Comet Shoemaker-Levy 9 hitting Jupiter (1994) and the night Galileo discovered Jupiter's moons (7 January 1610, Padua; in a telescope-like 0.5° view they line up two to the east and one to the west, as he recorded) are there too. Dark scars appear on Jupiter at the impact times and places computed from JPL Horizons' orbits of the 20 fragments and fade over months (their size and fading are approximate)
- **Sunspot cycle**: the number of sunspots and their latitudes follow the 11-year activity cycle (near 30° at the start of a cycle, near 8° at the end). The cycles start at the observed minima of solar cycles 1-25, and the number of spots follows an average cycle shape (approximate); there are almost none in the Maunder Minimum (1645-1715) and fewer in the Dalton Minimum (1798-1823)
- **Halley's past returns**: before 1948 Halley's Comet follows the orbit of the nearest return from JPL Horizons (30 returns from 240 BC to 1910)
- **Binary periastra**: the "Comets and binary stars" page of the event list has the times when the binaries of Alpha Centauri, Sirius, Luhman 16 and BL/UV Ceti come closest (Alpha Centauri: 11.2 AU on 30 April 2035); choosing one warps you to a view of the two stars from the side
- **Astronomical events**: from the K-key list you can warp, at the right moment, to a solar eclipse (12,000 km above the center of the eclipse; total and annular eclipses can also be watched from the ground at the center, where the corona appears and the sky darkens enough to show stars during totality), a lunar eclipse (beside the Moon), a moon's shadow crossing Jupiter (right above the shadow) or a planet at opposition (from the Earth side, fully lit like a full moon). Times are computed with simplified formulas and are typically within about 15 minutes of the real eclipses; the center of a solar eclipse is within a few hundred km. Transits are viewed from the line joining Earth and the planet (the same geometry as seen from Earth, but with the planet larger), conjunctions from just outside Earth, both with a zoomed view (right-click or move to return to the normal view). Mutual events of Jupiter's moons are not included: the moons' position errors are larger than the moons themselves
- **Shadows**: solar eclipses (the Moon's shadow on Earth), lunar eclipses (a copper-red Moon), shadows of Jupiter's moons and the shadow of Saturn's rings, with umbra and penumbra from the Sun's real size
- **Sky from the ground**: when you are stopped near a surface (below 100 km) you turn with the body's rotation. Choosing a place in the "Surface" search tab puts you on the ground looking up at the sky toward the equator (on the Moon, at Earth when it is up; on a planet of another star, at its star); speed up time to watch the stars and planets wheel across the sky (by day the blue sky hides the stars, so set a night-time date with Y or let time run). While you are on the ground (below 2 km, on bodies over 100 km in radius) the horizon and the compass points are drawn, and a panel at the lower left shows the place, its name and the time, the target's altitude and azimuth (16 points) and its next rise, transit and set (with atmospheric refraction on Earth; over one lunar day on the Moon). On Earth the names are the city when you are within its urban area (judged by a circle of the urban area) or a summit within 5 km, the regions around you (mountain ranges, deserts, plateaus, islands and so on; the smallest and a larger one containing it) and the country or the sea (oceans, seas, bays, straits and so on), all from Natural Earth (the polygons are thinned to a few km, so near borders and coasts you may get the neighbor's name); on other bodies the name is the smallest IAU-named feature (craters, mountains, valleys, plains and so on) around you plus the next larger one that contains it (judged by a circle of the center and diameter and a latitude-longitude box, so the edges differ from the real shapes). Times are in the standard time of the Earth city you chose in the search (no daylight saving), otherwise UTC
- **Meteor showers**: the Quadrantids, Lyrids, Eta Aquariids, Perseids, Orionids, Leonids and Geminids, with the solar longitude of their peaks, radiants and ZHR from the International Meteor Organization. When you watch the night sky from the ground on Earth, meteors streak out of the radiants of active showers (ZHR × the sine of the radiant altitude per hour, plus 6 sporadic meteors an hour; more when time runs faster, up to 4 per real second). The K list (sky) shows the peaks; choosing one takes you to the ground in Tokyo on the best night around the peak, looking toward the radiant
- **Venus and Titan**: both are drawn covered by their thick clouds and haze. Venus's clouds circle the planet in about 4 days, much faster than its 243-day rotation (super-rotation), and Titan has its detached blue upper haze layer (about 500 km up) showing beyond the limb. Below the haze layer 150 km above Titan you come down onto a surface from Cassini radar elevations (Corlies et al. 2017; about 9% of the surface was measured and the rest is interpolated) and a near-infrared map: inside the haze is fog, and below it the ground shows through the hazy nitrogen air (the brightness and haze density are estimates). The polar regions hold seas and lakes of liquid methane and ethane (extent from Birch et al. 2017, 1.08% of the surface), whose flat dark surfaces reflect more of the sky the more obliquely they are seen. Below the clouds and haze your ship and spacecraft are lit by the diffuse sky light too
- **Sizes**: equatorial radius and flattening (Jupiter and Saturn are visibly oblate), and Saturn's rings drawn at true scale from the opacity profile measured by Voyager 2's stellar occultation (every 10 km), so the D to F rings, the Cassini Division and the Encke Gap appear at their real positions and densities and get more opaque at shallow angles. The rings of Uranus (ten narrow rings, tipped over with the planet), Neptune (Galle, Le Verrier, Lassell, Arago and Adams, with the five arcs of the Adams ring) and Jupiter (halo, main ring and two gossamer rings) use the radii, widths and optical depths from NASA fact sheets. They are very dark and faint, so their opacity is exaggerated, and the narrow rings of Uranus and Neptune stay visible as lines when they become thinner than a pixel (the positions of Neptune's arcs are approximate). Ryugu (a spinning top), Comet Churyumov-Gerasimenko (two lobes joined by a neck), Bennu, Itokawa (a sea-otter shape), Didymos and Dimorphos, and Phobos have their real shapes from spacecraft observations and rotate. Dimorphos's orbital period shrinks from 11.92 to 11.37 hours at DART's impact on 26 September 2022
- **Reference frame**: the ship moves along with the body whose sphere of influence (SOI) it is in, so even at car speed you are not left behind by Earth. With Shift+F (or in the settings) the frame is fixed to the ship: you move along with no body and stay put relative to the Sun, watching Earth leave at 30 km/s or a moon sweep past (in gravity mode your actual motion is unchanged and the speed is shown relative to the Sun). Warping, moving to a place or an event, or starting the autopilot returns to automatic
- **Precision**: positions are stored as doubles and rendered relative to the camera. Each body is drawn at its own scale with its own near/far planes, so a 30 m ship and Neptune 4.5 billion km away can be rendered at the same time
- **Minimum altitude**: because of floating-point precision, you cannot descend below 1/100,000 of a body's radius (minimum 30 m) — about 64 m on Earth
- No physics (gravity etc.) is simulated. The ship flies arcade-style in the direction of its nose

## Earth and Moon data, starry sky, music

The images in `assets/` are made from NASA / NOAA data by `tools/make_earth_assets.py` and `tools/make_moon_assets.py` and embedded into the executable at build time (no files needed at run time).

- Surface color: NASA Earth Observatory, Blue Marble: Next Generation (April 2004)
- Elevation and ocean depth: NOAA NCEI, ETOPO 2022 Global Relief Model (60 arc-second, ice surface; the global elevations are subsampled to 5 arc-minutes and the coastlines come from the 60 arc-seconds, which are also used as they are around the camera near the land surface; converted by `tools/make_earth_detail.py`, downloads about 930 MB)
- City lights: NASA Earth Observatory, Black Marble 2016
- Moon color and elevation: NASA's Scientific Visualization Studio, CGI Moon Kit (LROC WAC / LOLA)
- Finer elevations for a few regions such as famous mountains (`assets/earth_patches.bin`, `moon_patches.bin`, `mars_patches.bin`): for Earth, ESA's Copernicus DEM GLO-90 (3 arc-seconds, about 90 m; produced using Copernicus WorldDEM™-90 © DLR e.V. 2010-2014 and © Airbus Defence and Space GmbH 2014-2018 provided under COPERNICUS by the European Union and ESA; all rights reserved); for the Moon, SLDEM2015 (LRO LOLA and the Kaguya Terrain Camera, Barker et al. 2016; 512 pixels per degree, about 60 m); for Mars, the USGS MOLA/HRSC blended elevations (200 m, averaged down to 288 pixels per degree, about 206 m). Made by `tools/make_detail_patches.py`
- Finer elevation of the poles of the Moon and Mars (`assets/moon_south.bin`, `moon_north.bin`, `mars_south.bin`, `mars_north.bin`): for the Moon, the LRO LOLA polar elevations LDEM_85S_40M and LDEM_85N_40M from the NASA PDS Geosciences Node (poleward of 85°; polar stereographic, 40 m per pixel reduced to 80 m); for Mars, the MGS MOLA polar elevations MEGDR MEGT_S_512_1 and MEGT_N_512_1, also from the NASA PDS Geosciences Node (poleward of 80°; polar stereographic, 512 pixels per degree (about 115 m) reduced to about 460 m, the spacing at which more than 90% of the cells hold MOLA measurements; the spacecraft's orbit did not reach within 3° of the poles, so poleward of 87° is mostly interpolated). Made with `tools/make_polar_detail.py`
- Planet and moon maps: global mosaics from the USGS Astrogeology Science Center map service ([planetarymaps.usgs.gov](https://planetarymaps.usgs.gov/)) (Jupiter from Cassini PIA07782 (NASA/JPL/Space Science Institute), MESSENGER, Viking MDIM 2.1 colorized by NASA Ames, Dawn, Galileo / Voyager, Cassini, Voyager 2, New Horizons) and Mars elevations from the NASA PDS MGS MOLA MEGDR (near the ground, the USGS MOLA and HRSC blended elevations by Fergason et al. 2018, averaged down to 32 pixels per degree); the elevations of Mercury, Venus, Ceres, Vesta, Pluto and Charon are USGS Astrogeology global elevation models (Mercury MESSENGER: Becker et al., USGS / NASA / JHUAPL / Carnegie Institution; Venus Magellan altimeter: Ford, Pettengill, Liu and Quigley; Ceres Dawn: Preusker et al. 2016, DLR; Vesta Dawn: DLR; Pluto and Charon New Horizons: NASA / JHUAPL / SwRI / LPI); Titan's elevations are the [Cassini radar global topography](https://doi.org/10.7298/m4dv-gv95) by Corlies and Hayes (Corlies et al. 2017, CC0, fetched from Cornell's [data.astro.cornell.edu](https://data.astro.cornell.edu/titan_topo_corlies/)) and its map is the Cassini ISS near-infrared global map PIA19658 (NASA/JPL-Caltech/SSI, USGS Astrogeology); Saturn uses the [Cassini ISS global map](https://doi.org/10.17189/rkkb-6y30) from the NASA PDS Atmospheres Node (red, green and blue filters, 11 August 2011); the five major moons of Uranus use the USGS global maps distributed by JPL's [Solar System Simulator](https://space.jpl.nasa.gov/tmaps/uranus.html) (Voyager 2, southern hemispheres only; Courtesy NASA/JPL-Caltech, [JPL Image Use Policy](https://www.jpl.nasa.gov/jpl-image-use-policy)), fetched and converted by `tools/make_planet_maps.py` (requires Pillow and NumPy; downloads about 3.2 GB, of which about 2.5 GB is elevation data and about 0.6 GB map images, fetched at 4 times the size and averaged down because the map service's own downscaling is jagged; the Titan map and elevations are about 36 MB). The longitude of Jupiter's Great Red Spot follows the observations compiled by [Project Pluto](https://www.projectpluto.com/grs_lon.txt) (JUPOS since 1964)
- Orbital elements of the ISS and the Hubble Space Telescope: [CelesTrak](https://celestrak.org/) (fetched by `tools/fetch_tle.py`). JWST trajectory: NASA/JPL Horizons
- Orbital elements of asteroids and other small bodies: JPL Small-Body Database (fetched and converted by `tools/make_asteroid_assets.py`; requires NumPy; downloads about 20 MB)
- Spacecraft trajectories: NASA/JPL Horizons (fetched and thinned out by `tools/make_probe_assets.py`; Rosetta from ESA and Hayabusa2 from JAXA orbit determination)
- Extent of Titan's lakes and seas: the [polar geomorphologic maps](https://doi.org/10.5281/zenodo.21685404) by Birch and Hayes (Birch et al. 2017, CC BY 4.0)
- Huygens descent trajectory: made by `tools/make_lander_tracks.py` from the trajectory reconstructed by the Huygens DTWG at the NASA PDS Atmospheres Node ([HP-SSA-DTWG-6-TRAJECTORY-V1.0](https://atmos.nmsu.edu/PDS/data/hpdtwg_0001/), B. Kazeminejad, D. H. Atkinson et al.); parachute phase times from Lebreton et al. (2005, Nature 438); from release to entry, JPL Horizons
- Mars lander descent trajectories and rover drives: made by `tools/make_lander_tracks.py` from the NAIF reconstructed EDL SPKs ([msl_edl_v01](https://naif.jpl.nasa.gov/pub/naif/MSL/kernels/spk/), [m2020_edl_v01](https://naif.jpl.nasa.gov/pub/naif/MARS2020/kernels/spk/), MSL and M2020 EDL Teams, JPL; [mer1/mer2_edl_rcb_v1](https://naif.jpl.nasa.gov/pub/naif/MER/kernels/spk/), R. C. Blanchard; [phx_edl_rec_traj](https://naif.jpl.nasa.gov/pub/naif/PHOENIX/kernels/spk/), PHX GNC Team, JPL; [insight_edl_rec_v1](https://naif.jpl.nasa.gov/pub/naif/INSIGHT/kernels/spk/), InSight EDL Team) and the rover position SPKs (msl/m2020_surf_rover_loc, mer1/mer2_surf_roverrl); Perseverance's phase times from Karlgaard et al. ([MEDLI2 reconstruction](https://ntrs.nasa.gov/citations/20210024320))
- Apollo spacecraft trajectories: computed by `tools/make_apollo_assets.py` from the positions and velocities in NASA's flight records (R. W. Orloff, *Apollo by the Numbers*, NASA SP-2000-4029, and each mission report), with Moon and Sun positions from JPL Horizons
- Starry sky: stars from the Hipparcos catalogue (ESA), and the lines, names and star names of the 88 constellations, from [d3-celestial](https://github.com/ofrohn/d3-celestial) (Olaf Frohn, BSD 3-Clause), converted by `tools/make_sky_assets.py`. Star distances and motions: parallaxes and proper motions of the Hipparcos new reduction (van Leeuwen 2007), from the ESA Gaia Archive
- Nearby stars: astrometry from Gaia DR3 (ESA/Gaia/DPAC) and the Hipparcos new reduction (van Leeuwen 2007), with binary orbits and masses from published papers (Akeson et al. 2021, Bond et al. 2017, Kervella et al. 2016, Lazorenko & Sahlmann 2018) and others. The sources and values are listed in `tools/make_nearby_stars.py` (which writes `src/nearby_stars.h`). Planets: NASA Exoplanet Archive (Suárez Mascareño et al. 2025, Basant et al. 2025, Hurt et al. 2022, Thompson et al. 2025)
- Shapes of the comet and asteroid: Ryugu from the shape model of Watanabe et al. (2019) (source ISAS/JAXA, [JAXA DARTS](https://data.darts.isas.jaxa.jp/pub/hayabusa2/paper/Watanabe_2019/)); Comet Churyumov-Gerasimenko from ESA's NAVCAM shape model MTP019, low-resolution version (ESA/Rosetta/NAVCAM – CC BY-SA IGO 3.0, obtained from the NASA PDS Small Bodies Node). Bennu (OSIRIS-REx), Itokawa (Gaskell et al., Hayabusa AMICA), Didymos and Dimorphos (DART), Phobos (Gaskell) and Donaldjohanson (Lucy, DLR team) from NASA NAIF SPICE shape kernels (DSK). Deimos uses Thomas's shape model (from Viking Orbiter images; NASA PDS Small Bodies Node). Gaspra, Ida and the nucleus of Halley's Comet use Stooke's shape models (from Galileo images, and for Halley from Giotto and Vega images with pointing by Abergel; NASA PDS Small Bodies Node, public domain; the archive does not allow automated downloads, so fetch the zip in a browser and put it in the work directory). Converted by `tools/make_shape_assets.py` (coordinates rounded to 16 bits, Phobos thinned out, plus a table of surface radii; requires NumPy and spiceypy)
- Saturn's rings: NASA PDS Ring-Moon Systems Node, Voyager 2 photopolarimeter occultation of delta Scorpii (VG_2801), converted by `tools/make_ring_assets.py`
- Rings of Uranus, Neptune and Jupiter: radii, widths and optical depths from the NASA NSSDCA Uranian / Neptunian / Jovian Rings Fact Sheets (`RingSets` in `src/app/bodies.h`)
- Earth's seasons and clouds: NASA Earth Observatory, Blue Marble: Next Generation for January–December 2004 and the Blue Marble global cloud image, converted by `tools/make_earth_season_assets.py` (downloads about 150 MB)
- Surface materials: the Moon's rock abundance from LRO Diviner (Bandfield et al. 2011) and the nighttime thermal inertia of Mars from MGS TES (Putzig & Mellon 2007, Icarus 191), both from the NASA PDS Geosciences Node, and the Martian dune fields from the USGS Mars Global Digital Dune Database (Hayward et al. 2007, 2010, 2012), converted by `tools/make_surface_props.py` (requires Pillow and NumPy; downloads about 140 MB)
- Place names: Earth's cities, summits, regions, countries and seas from Natural Earth (1:10m Populated Places, Elevation Points, Physical Labels and Marine Areas, and 1:50m Admin 0 Countries) and the other bodies' names from the IAU (USGS Gazetteer of Planetary Nomenclature), converted by `tools/make_place_names.py` (downloads about 50 MB)
- Planet positions: VSOP87A (Bureau des Longitudes), coefficients truncated by `tools/make_vsop87.py` from Greg Miller's [VSOP87-Multilang](https://github.com/gmiller123456/vsop87-multilang) (public domain)
- Milky Way: NASA/Goddard Space Flight Center Scientific Visualization Studio, [Deep Star Maps 2020](https://svs.gsfc.nasa.gov/4851/) (the Milky Way background without the bright stars; Gaia DR2: ESA/Gaia/DPAC), converted by `tools/make_milkyway_asset.py` (requires Pillow and NumPy; downloads about 35 MB)
- Music (Holst, *The Planets*): Mercury by the U.S. Air Force Band (2013, arranged by the band); the other six movements by the London Symphony Orchestra conducted by Holst, recorded 1922–23 (Columbia). Public domain
- Music (Sun, Earth): Grieg, "Morning Mood" by the Musopen Symphony; Beethoven, "Pastoral" Symphony, 1st movement by the Skidmore College Orchestra. Both public domain recordings provided by Musopen
- Music (Moon): Debussy, "Clair de lune", Laurens Goedhart, piano (2011). CC BY 3.0
- All recordings were taken from Wikimedia Commons. To regenerate: `python3 tools/make_music_assets.py <work dir>` (requires ffmpeg; downloads about 350 MB)

All of them may be used and redistributed freely, including commercially (see [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)). To regenerate them: `python3 tools/make_earth_assets.py <work dir>` (Earth's colors, elevations and coastlines at 8192×4096; downloads about 300 MB, and also uses the same 930 MB as make_earth_detail.py for the coastlines; also requires NumPy and SciPy), `python3 tools/make_earth_detail.py <work dir>` (Earth's fine land elevations; also requires NumPy; about 930 MB), `python3 tools/make_body_detail.py <work dir>` (the fine elevations of the Moon and Mars; also requires NumPy; about 11.5 GB, 11.4 GB of it the Mars source), `python3 tools/make_detail_patches.py <work dir>` (the finer elevations for a few regions such as famous mountains; requires NumPy; about 80 MB for Earth and 0.6 GB for the Moon, plus the Mars source), `python3 tools/make_polar_detail.py <work dir>` (the finer elevation of the poles of the Moon and Mars; requires NumPy; about 830 MB), `python3 tools/make_surface_props.py <work dir>` (the Moon's rock abundance and the thermal inertia and dune fields of Mars; also requires NumPy; about 140 MB) and `python3 tools/make_moon_assets.py <work dir>` (about 45 MB). All of them require Pillow. The place names come from `python3 tools/make_place_names.py <work dir>` (Python only; about 50 MB). The tools for the Earth, Earth's seasons and clouds, the Moon, the Milky Way and the planet and moon maps make a high-resolution set when `--hq` is added at the end (8192×4096 for the Moon colors and the Mercury and Mars maps, and so on; Earth's colors and elevations are 8192×4096 in the normal set too). Write it to another folder and build with `cmake -B build-hq -DEMBED_ASSET_DIR=<that folder>` to embed it (an executable of about 178 MB and about 1.2 GB of textures; the released builds use the normal resolution). The source of the Moon's 8k colors (svs.gsfc.nasa.gov) does not send its intermediate certificate, so if the download fails, add the Let's Encrypt intermediate certificate.

## Display language (日本語 / English)

- Press the **J key** at any time to switch. The chosen language is saved in `solar_voyager.cfg` and used on the next launch
- `solar_voyager.cfg` stores the language, quality, sound setting, constellation display and bookmarks. It lives next to the executable (if that folder is not writable, e.g. when installed from the .deb / .rpm, in `~/.config/solar_voyager/solar_voyager.cfg`; on macOS in `~/Library/Application Support/SolarSystemVoyager/`)
- To choose at startup: `solar_voyager --lang en` (or `ja`). Priority is: command-line option > saved setting > Japanese

For Japanese text, the program automatically searches for an OS Japanese font (Windows: Meiryo / Yu Gothic / MS Gothic, macOS: Hiragino Sans, Linux: Noto Sans CJK / IPA, etc.).
If none is found, the display is fixed to English. A font placed at `assets/font.ttf` takes priority.

## License

The program (source code, build files, tools and icon) is under the [MIT License](LICENSE).
The Earth and Moon image data (`assets/earth_*`, `assets/moon_*`), the sky data (`assets/stars.bin`, `assets/sky.txt`, `assets/milkyway.jpg`), Earth's seasons and clouds (`assets/earth_months.jpg`, `assets/earth_clouds.jpg`), Saturn's rings (`assets/saturn_rings.png`), the planet and moon maps and Mars's fine elevations (`assets/maps.bin`, `assets/mars_detail.bin`, `assets/mars_detail2.bin`, `assets/mars_patches.bin`), the small-body orbital elements (`assets/asteroids.bin`), the comet and asteroid shapes (`assets/shapes.bin`; the Churyumov-Gerasimenko part is CC BY-SA IGO 3.0) and the music (`assets/music`) are not covered by it and follow the terms of its sources. See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md), which also lists the bundled libraries (raylib / GLFW, zlib license).
