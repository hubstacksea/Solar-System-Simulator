# Changelog

[日本語](CHANGELOG.md) | English

The release workflow copies the section of the version being released into the GitHub release notes.

## 2.9.0 (2026-10-30)

- Body info card (I key / touch "Info"): type, radius, mass, surface gravity, escape velocity, rotation and orbital periods, orbit, moons, current distances and spacecraft that flew by. Mouse inversion is now only in the settings
- Resume: the next launch starts where you quit, with "Place only (current time)", "Place and time" or "Off" in the settings
- Gamepad for every menu: the help screen (Menu) opens the map, settings, events, date entry and body info, operated with the D-pad and A / B

## 2.8.0 (2026-10-30)

- System map: probe trajectories, time controls while the map is open (pause, faster / slower, reverse) and gamepad support
- Browser version: sharp rendering on high-resolution screens (phones, Retina) following the device pixel ratio; text and HUD scale with it
- A changelog (CHANGELOG.en.md) and release notes that include the changes of each version
- Release workflow: GitHub Actions updated to the Node.js 24 versions (checkout v7, upload-artifact v7, download-artifact v8)

## 2.7.1 (2026-10-30)

- If the window cannot be created with 4x antialiasing (MSAA), the next launch starts without it (`--no-msaa` forces it)
- Exits with a message instead of crashing when OpenGL 3.3 is not available
- `--check-assets` reads all data without opening a window; the macOS release build is checked with it in CI

## 2.7.0 (2026-10-30)

- System map (F3 / touch "Map"): seen from above with logarithmic distances, centered on the Sun or a planet; click or tap to target, again to warp
- High-DPI screens: on Windows and Linux the window and HUD follow the display scale; macOS renders at Retina resolution
- macOS: data is kept once in the app's Resources folder, halving the download to about 50 MB
- Release workflow launches the Linux build and takes screenshots

## 2.6.0 (2026-10-30)

- Text size setting (Normal / Large / Extra large) that enlarges the HUD, panels, markers and touch buttons together
- Milky Way on/off in the settings
- The asteroid particles start off on the first launch

## 2.5.0 (2026-10-29)

- Browser version loads only about 8 MB before starting; maps, shapes and other large data arrive in the background
- Share links (F2 / touch "Share") with the date, place, view, target and time scale
- macOS version (universal app for Apple Silicon and Intel)

## 2.4.0 (2026-10-29)

- Releases built automatically by GitHub Actions (Windows, Linux, browser) and published to GitHub Pages
- Touch controls for phones and tablets
- A short guide on the first launch

## 2.3.0 (2026-10-29)

- ISS, Hubble and JWST
- Gravity flight mode (R): fly under the gravity of the planets and moons, with the predicted orbit, periapsis and apoapsis
- Browser version (WebGL2)

## 2.2.0 (2026-10-29)

- OSIRIS-REx and DART; real shapes of Bennu, Itokawa, Didymos, Dimorphos and Phobos
- Total and annular solar eclipses seen from the ground
- Earth's colors for each month and real clouds; Saturn's rings from the Voyager 2 profile

## 2.1.0 (2026-10-29)

- Jupiter map with the Great Red Spot at its observed longitude
- About 100,000 asteroids, Trojans and trans-Neptunian objects as points
- Settings screen (F10)

## 2.0.0 (2026-10-29)

- Real maps for 17 planets, dwarf planets and moons (Mercury, Mars with relief, Ceres, Vesta, Pluto, the Galilean moons, Saturn's moons, Triton, Charon)
- Better sunset colors

## 1.9.0 (2026-10-29)

- Atmospheric haze on Earth and Mars
- Photo mode (Z)
- Real shapes of Ryugu and Comet Churyumov-Gerasimenko

## 1.8.0 (2026-10-29)

- Daytime sky, sunrise and sunset on Earth and Mars
- Juno, Rosetta and Hayabusa2
- Guided tour (U)

## 1.7.0 (2026-10-29)

- Milky Way
- Comets Hale-Bopp and Hyakutake
- More events: transits of Mercury and Venus, conjunctions, oppositions, comets
- A model for each spacecraft

## 1.6.0 (2026-10-28)

- Voyager, Pioneer, Galileo, Cassini, New Horizons and Parker Solar Probe on their real paths, with a spacecraft events page

## 1.5.0 (2026-10-28)

- Real starry sky (Hipparcos) and constellations (C)
- Ceres, Vesta, Eris and Halley's Comet with its tails

## 1.4.0 (2026-10-28)

- Events list (K) with eclipses and shadow transits; warp to the time and place
- Set the date and time (Y)
- More accurate planet positions

## 1.3.0 (2026-10-28)

- Moons at their real positions

## 1.2.0 (2026-10-28)

- Music for each body (Holst's *The Planets* and more)

## 1.1.0 (2026-10-28)

- Real Moon surface and position, solar and lunar eclipses, ring shadows
- Quality setting (F9), bookmarks (B / N), engine and warp sounds

## 1.0.0 (2026-10-28)

- First release: the Sun, planets and major moons at true size and distance, with positions from the PC clock
- Ship with seven engines, autopilot and warp; Japanese and English
- Earth from NASA data
- Windows (7z) and Linux (.deb / .rpm) packages
