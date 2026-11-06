# Changelog

[日本語](CHANGELOG.md) | English

The release workflow copies the section of the version being released into the GitHub release notes.

## 2.37.1 (2026-11-07)

- Markers: asteroids, spacecraft and satellites more than 1 AU away show no name or marker (the target still does), so distant small bodies no longer crowd the screen

## 2.37.0 (2026-11-07)

- Apollo program: engine burns are now drawn. The Saturn V first stage shows a bright flame that lengthens and spreads as the air thins (ignition 8.9 seconds before liftoff); the second and third stages and the engines of the command and service module and the lunar module show faint plumes in vacuum; small course corrections fire from the attitude control thrusters. The times and directions come from the burns of the trajectory computation (115 in all)
- Apollo program: during main engine burns the engine now points against the thrust, so braking burns such as lunar orbit insertion are flown backwards as they really were. The spacecraft turns during the 4 minutes before a burn and back to its direction of travel during the 4 minutes after; before entry the command module turns its heat shield forward over 3 minutes after the service module is jettisoned. Before, the spacecraft flipped around in an instant, at entry and elsewhere
- Apollo program: right after liftoff the rocket leaned more than 30° west; it now rises straight up and then pitches over toward the east. During short burns the spacecraft used to jump (far from Earth, rounding the recorded latitude and longitude to 0.01° amounts to tens of km; 520 km in 3.5 seconds on Apollo 13); the cutoff position is now computed from the ignition state
- Apollo program: fixed the camera suddenly turning by 90-180°. While looking straight down at or up from the Moon or Earth it keeps its current roll, and it moves smoothly when entering orbit, when the spacecraft's shape changes, and when switching to the view from the ground just before landing. It also keeps the Moon in view while circling it fast
- Markers: bodies more than 1 light-year away show no name or marker, except stars and the target
- Orbit lines are no longer drawn for satellites and other bodies before they exist (the ISS orbit showed in 1970)

## 2.36.0 (2026-11-06)

- Apollo program: a new "Apollo program" page in the event list (K). Follow the lunar flights Apollo 8 and 10-17 (Apollo 13 aborted its landing after an oxygen tank explosion and returned) from right beside the spacecraft, in chronological order: liftoff, arrival at the Moon, landing, moonwalks, liftoff, docking, the return and splashdown. The spacecraft's shape changes with time (Saturn V stages, the command and service module, the lunar module, the command module under parachutes), and the descent stages stay on the Moon after liftoff. Whenever the date falls within a flight, the spacecraft appear and fly as recorded even without choosing the event
- The trajectories are computed with the gravity of Earth, the Moon and the Sun from the positions and velocities in NASA's flight records (Apollo by the Numbers and each mission report) (`tools/make_apollo_assets.py`, `assets/apollo.bin`). Entry is solved from the equations of motion to match the recorded peak deceleration and the parachute time and place
- `?apollo=11` in the URL opens that flight, and `&scene=<number>` a scene

## 2.35.1 (2026-11-06)

- Spacecraft voyages: the close-up map at the top right is now drawn the same way as the full map, with the Sun at rest. It used to show the path as seen from the body, so the spacecraft's direction differed from the full map by the body's own motion (in Galileo's Earth flyby it moved right to left in the full map but bottom to top in the close-up). The body's path is drawn as a thin line and the body moves across the map

## 2.35.0 (2026-11-06)

- Browser version: the loading screen shows a logo and the title (SOLAR SYSTEM VOYAGER). The logo uses the same design as the app icon (a ringed planet and a spacecraft) and is drawn inside the page without image files, so it appears immediately

## 2.34.1 (2026-11-05)

- Browser version: page views are now counted with Cloudflare Web Analytics (no cookies; no personal information or details of what you do in the app are sent). The PC and macOS versions send nothing

## 2.34.0 (2026-11-05)

- Spacecraft voyages: 56 more cruise notes (size, mass, builder, launch vehicle and date, Cassini's 22 dives between Saturn and its rings, Philae's two bounces and more), 4 to 8 per spacecraft
- Browser version: rebuilt the Japanese font; 28 characters used by the 2.32.0 notes and elsewhere were missing and not shown
- The pre-release checks (`tools/run_checks.py`) now verify that the browser version's font has every character shown on screen

## 2.33.0 (2026-11-05)

- The display language can be set in the URL with `lang=ja` or `lang=en` (also on share links; the browser version's loading message follows it). Without it, the saved setting is used as before, or Japanese if there is none. The language given in the URL is not saved
- A link without a scene (such as just `?lang=en`) now starts from where you left off

## 2.32.1 (2026-11-04)

- Spacecraft voyages: on narrow screens such as phone browsers, the cruise notes ran off the edge of the screen as single lines; they now wrap to the width of their panel
- Spacecraft voyages: clearer wording of Rosetta's notes on its name and its hibernation

## 2.32.0 (2026-11-04)

- Spacecraft voyages: during cruise, notes about the spacecraft (its design, its goals and stories from the mission) appear one by one at the bottom left, 14 seconds each (20 spacecraft and 2 extended missions, 2 to 5 notes each)
- Spacecraft voyages: the Rosetta voyage no longer has a scene claiming its comet passes "inside the geostationary orbit" when it passes 1.4 AU from Earth; the scene now appears only for real passes inside it (Apophis)
- Saturn now has a global map from Cassini images (NASA PDS Atmospheres Node, August 2011, showing the aftermath of the great northern storm). The poles and the band from the equator to 17° S, hidden by the ring shadow, keep procedural clouds
- The README's description of surfaces now lists the five Uranian moons that got maps in 2.31.0

## 2.31.0 (2026-11-04)

- The five major moons of Uranus (Miranda, Ariel, Umbriel, Titania and Oberon) now have USGS maps from Voyager 2 images (only the imaged southern hemispheres; the northern hemispheres keep the procedural surface)
- Deimos and the nucleus of Halley's Comet now have their real shapes (Thomas's shape model from Viking Orbiter images for Deimos, Stooke's shape model from Giotto and Vega images for Halley). The radius of Halley's nucleus is now 4.6 km, that of a sphere of the same volume

## 2.30.1 (2026-11-03)

- Spacecraft voyages: when cruising toward a moon (such as Ganymede for Europa Clipper), the camera no longer sways from side to side following the moon as it circles its planet many times under fast time. It looks at the planet while that happens and turns to the moon as time slows down

## 2.30.0 (2026-11-03)

- The ESA probe Hera: from its October 2024 launch through the March 2025 Mars flyby to its arrival at Didymos on 27 October 2026, also as a spacecraft voyage (`?voyage=Hera&scene=arrive`)
- Arrival at an asteroid is now the first time within 200 km (100 radii for larger bodies); OSIRIS-APEX now arrives at Apophis on 17 May 2029
- The release build's browser check now also tries key, click and drag input and saves screenshots (actions in `tools/web_smoke.py`)

## 2.29.0 (2026-11-03)

- Spacecraft voyages: a timeline at the bottom. Click or tap a scene dot to go to that scene, or elsewhere on the bar to go to that time (hover over a dot to see its title). The mouse is no longer captured during a voyage; look around by dragging with the left button
- Spacecraft voyages: `[` and `]` choose the voyage speed: slow (about 10 minutes for the whole voyage), normal (about 4 minutes) or fast (about 1 minute)

## 2.28.3 (2026-11-03)

- Spacecraft voyages: the camera no longer snaps between scenes or whips around just after a gravity assist; it turns smoothly (with a limit on turning speed, and a continuous shift toward the sunlit limb on night-side passes)
- Spacecraft voyages: the top-right map now fades between the whole-voyage map and the close-up map
- New checking option `--voyage-trace` (prints the per-frame change in camera direction, position and field of view during a voyage)

## 2.28.2 (2026-11-03)

- Reverted the 2.28.1 change to raylib (it only changed how the app fails in rare environments that cannot open a window, which did not justify modifying an external library)
- The release build no longer launches the macOS version for screenshots (GitHub's Mac virtual machines have no GPU, so it always failed); the data check remains

## 2.28.1 (2026-11-03)

- Where an OpenGL 3.3 window cannot be created (such as Mac virtual machines without a GPU), the macOS version crashed silently; it now prints a message and exits (a fix applied to raylib 5.5)

## 2.28.0 (2026-11-03)

- Asteroid Apophis's Earth encounter on 13 April 2029: it passes 38,000 km from Earth's center, inside the geostationary orbit. Two scenes on the comets and binaries page of the event list, from space and from the ground directly below. Around the encounter it follows its JPL Horizons trajectory
- Extended-mission voyages: Hayabusa2 (from the Earth flyby that delivered its capsule to asteroid Torifune) and OSIRIS-APEX (three Earth flybys on the way to Apophis, including Apophis's own Earth encounter). URLs such as `?voyage=Hayabusa2&leg=2`
- Added the asteroids Torifune and Apophis
- The release build now opens the browser version in headless Chrome and saves its screen and console output (`tools/web_smoke.py`)
- Finer spacecraft trajectories near asteroids (thinning tolerance reduced from 0.5 km to 50 m)

## 2.27.0 (2026-11-03)

- Spacecraft voyages: near a planet or asteroid, the map at the top right switches to a close-up centered on that body
- Look around during a spacecraft voyage by dragging the mouse, with the arrow keys or by touch (the view returns to the automatic direction 3 seconds after you stop)
- Gaspra and Ida now have their real shapes instead of ellipsoids (Stooke's shape models, from Galileo images)
- Share links now include a zoomed-in view (for example Jupiter as Galileo saw it, or asteroid flybys)
- Time now stops at the edge of the supported range (3000 BC to AD 2999) with a notice instead of running past it

## 2.26.1 (2026-11-03)

- A small map at the top right during spacecraft voyages: the solar system from above with the path flown so far and ahead, the scene locations and the current position (F3 toggles it)

## 2.26.0 (2026-11-03)

- Spacecraft voyages: a new page in the event list (K) follows a spacecraft from launch to its goal (19 spacecraft). Time runs fast during cruise and drops to real time with a caption at launch, leaving Earth's sphere of influence, gravity assists, passes by asteroids and moons, and arrival. URLs such as `?voyage=BepiColombo&scene=arrive` start at that scene, and F2 during a voyage copies a link to the current scene
- Nine asteroids passed by spacecraft on their way: Gaspra and Ida (Galileo), Dinkinesh and Donaldjohanson (Lucy; Donaldjohanson with its real shape), and the Jupiter Trojans Eurybates, Polymele, Leucus, Orus and Patroclus (Lucy, planned). They are passed at the distances of the real trajectories
- Fixed spacecraft timing: JPL Horizons dynamical time was read as universal time, so every spacecraft ran about a minute late
- DART no longer disappears about 90 seconds before its impact; it now flies until it hits Dimorphos
- Removed a false event, "Europa Clipper: closest approach to Earth" 34 minutes after launch

## 2.25.2 (2026-11-02)

- Fixed small bodies with real shape models, such as Didymos, losing their near surface and showing the background when off-center on screen (ringed planets use the same calculation)

## 2.25.1 (2026-11-02)

- Fixed share links and "Resume the last session" silently opening at the current time for scenes before 1600 or after 2400 (they now accept 3000 BC to AD 2999, the same range as date entry)
- Share links and bookmarks with broken positions or orientations (non-numbers, parallel directions and so on) are ignored, and orientations are re-orthonormalized before use

## 2.25.0 (2026-11-02)

- Planet positions now use VSOP87: Mercury to Saturn within 1.5″ of JPL Horizons in 1500–2500 (previously up to 17′), and within 4.4′ over 3000 BC to AD 3000 (previously up to 22′). Orbit lines are ellipses that pass exactly through the planets
- 35 nebulae, galaxies and star clusters (the Andromeda Galaxy, the Magellanic Clouds, the Orion Nebula, the Carina Nebula, the Veil Nebula, Omega Centauri and more). Zoom in to see their shapes; their names appear with the C key labels. The Crab Nebula appears only after 1054 and keeps expanding
- The "Milky Way" setting is now "Milky Way & nebulae" (it hides the nebulae and galaxies too)

## 2.24.0 (2026-11-02)

- Sunspots wax and wane with the 11-year activity cycle and appear at lower latitudes as a cycle goes on (including the Maunder and Dalton minima)
- The history page adds the night Galileo discovered Jupiter's moons (7 January 1610, Padua), in a telescope-like 0.5° view with two moons to the east and one to the west, as he recorded
- The check script counts surface loss only while still flying just above the surface

## 2.23.0 (2026-11-01)

- Comet Shoemaker-Levy 9 hits Jupiter (July 1994): scars appear at the times and places computed from JPL Horizons' orbits of the 20 fragments and fade over months; also on the history page
- Lunar libration: the Moon's orientation follows the IAU model, so seen from Earth it nods by about ±8° in longitude and ±7° in latitude
- Near the speed of light, the colors of planets and moons also change with the Doppler effect (blue ahead, red and dark behind)
- Historic supernova values checked against the literature and corrected (the position and visibility of SN 1006, the visibility of SN 1054, the peak date of Tycho's supernova)

## 2.22.0 (2026-11-01)

- Ancient eclipse times fixed: the Moon is corrected to JPL Horizons (DE441) and ΔT now uses the expressions of NASA's eclipse catalog; solar eclipses from 763 BC to AD 2950 agree with NASA within 4 minutes and 2°
- 16 historic sky events: supernovae (1006, 1054, 1572, 1604; they shine in the sky for that time), returns of Halley's Comet (240 BC, 1066, 1301) and transits of Venus (1639, 1769, 1882)
- Halley's past returns: before 1948 the comet follows JPL Horizons' orbit for each return
- Near the speed of light, a large planet close by is distorted and shifted forward as a whole (per-vertex aberration in the shader)
- A check script for releases, `tools/run_checks.py` (data, eclipses against NASA, warps to every body, light-speed flights just above the surface)

## 2.21.2 (2026-11-01)

- Fixed the surface of Jupiter, Saturn, Uranus, Neptune and Mars sometimes vanishing when flying just above it with the light-speed engine (since 2.21.0; the near-light-speed view also shifted bodies that look large)

## 2.21.1 (2026-11-01)

- After a sudden stop near the speed of light, the crowded stars now ease away in about 1 second instead of 2

## 2.21.0 (2026-11-01)

- The Moon uses Meeus's full series (a truncated ELP-2000/82); modern solar eclipses agree with NASA's catalog within 2 minutes
- Historic eclipses: the 4th page of the event list has the Assyrian eclipse, the eclipse of Thales, the annular eclipse at the Battle of Mizushima, Columbus's lunar eclipse, Eddington's eclipse expedition and the 2009 total eclipse, taking you to the ground at the historic site; the tour adds Thales's and Eddington's eclipses
- Near the speed of light, the Sun, planets and other Solar System bodies shift forward like the stars
- 61 Cygni A and B orbit each other (Shakht et al. 2017)

## 2.20.1 (2026-11-01)

- The crowding of the stars near the speed of light (aberration) now changes gradually with speed, and after a sudden stop it eases away over about 2 seconds instead of vanishing at once

## 2.20.0 (2026-11-01)

- Dates from 3000 BC to AD 2999: outside 1800-2050 the planets use JPL's long-term elements, and further out the slowing of Earth's rotation (ΔT), the precession of its axis and long-term lunar terms are included, so ancient solar eclipses (such as Thales's) fall on the same days as in NASA's catalog. Julian calendar before 15 October 1582; press - in the date entry for BC
- Near the speed of light, the light-speed engine shows the stars and the Milky Way crowding forward (aberration), blue and bright ahead and red and dark behind (Doppler effect)
- Warping to a spacecraft shows it large in the center of a zoomed view, from a distance matched to its size
- Gravity darkening on Altair (bright white poles, darker equator)
- 40 Eridani B and C orbit each other (Mason et al. 2017)
- Lighter starry-sky updates while moving (no more re-sorting every time)

## 2.19.2 (2026-11-01)

- Fixed warping to a spacecraft that is not flying at the current date (after changing the date with it targeted) arriving where it is not; like the search, it now moves to the spacecraft's highlight first

## 2.19.1 (2026-11-01)

- Fixed the heading and roll of the compass jumping around when looking straight down or up, as right after a warp

## 2.19.0 (2026-10-31)

- Star epoch (settings): moves only the stars and the background sky up to 100,000 years into the past or future. The background stars move with the proper motions of the Hipparcos new reduction, so the Big Dipper and other constellations fall apart; the nearby stars move to that epoch too
- 12 notable stars out to about 17 light-years (Procyon A and B, 61 Cygni A and B, Ross 128, Tau Ceti, Epsilon Indi A, Teegarden's Star, 40 Eridani A, B and C, Altair) and 8 more planets (Ross 128 b, Tau Ceti g, h and f, Epsilon Indi A b, Teegarden's Star b, c and d). Altair is flattened by its fast spin. The "stars around the Sun" map now reaches 18 light-years
- Cloud bands and patchy clouds on the brown dwarfs (Luhman 16 A and B, WISE 0855−0714)
- The habitable zone (a green band) on system maps centered on a star, with distances from the center drawn true rather than projected
- Hyperdrive (8): a fictional engine for fun that covers one light-year per second

## 2.18.0 (2026-10-31)

- The background stars in 3D: the 41,000 stars have distances from the parallaxes of the Hipparcos new reduction, and away from the Sun they are drawn in the direction and brightness seen from where you are (constellation lines and star names too). From Alpha Centauri the nearer stars shift within their constellations
- "Stars around the Sun" on the system map: zooming all the way out from the Sun shows the stars within 12 light-years, seen obliquely with stems for the height above the ecliptic. Far from the Sun the map opens this way
- During an interstellar warp the distance left and how many years light would take are shown, and the info card of stars and exoplanets tells how many years ago the light you see left them
- Binary periastra: the "Comets and binary stars" page of the event list has the times when the binaries of Alpha Centauri, Sirius, Luhman 16 and BL/UV Ceti come closest
- Three new guided tour scenes: Sirius A seen from Sirius B, Alpha Centauri seen from Proxima Centauri b, and the 2035 periastron of Alpha Centauri
- The ground of Proxima Centauri b (the "Surface" search tab, formerly "On Earth"): the red star hangs still in the sky. The panel notes that the surface is an artist's impression
- Large starspots on red dwarfs, flares on the flare stars (Proxima Centauri, UV and BL Ceti, Wolf 359, Ross 154; up to about 3 magnitudes brighter for a few minutes, more often than in reality), and a smooth surface with an intense blue-white glow for the white dwarf Sirius B
- Fixed a crash when the window cannot be created, for example without a display (it now exits with a message)

## 2.17.0 (2026-10-31)

- Nine planets of other stars (Proxima Centauri b and d, Barnard's Star b to e, Lalande 21185 b and c, Epsilon Eridani b) orbit at their current positions from the NASA Exoplanet Archive orbital elements, lit by their own star. Estimated orbital tilts, masses, radii and surfaces are marked on the info card, which also shows the equilibrium temperature. They are in the "Stars" tab of the search and on the system map centered on their star

## 2.16.0 (2026-10-31)

- The 15 stars and brown dwarfs within about 10 light-years of the Sun (Alpha Centauri A and B, Proxima Centauri, Barnard's Star, Luhman 16 A and B, WISE 0855−0714, Wolf 359, Lalande 21185, Sirius A and B, BL Ceti and UV Ceti, Ross 154, Epsilon Eridani) at their real positions, sizes and colors. They move with their proper motions from Gaia and other sources, and the binaries follow published orbits. From afar they are stars in the sky, up close they are glowing spheres; from Alpha Centauri the Sun is a magnitude 0.5 star in Cassiopeia. A "Stars" tab in the search, an info card with the type, spectral type, surface temperature and binary companion (estimated values are marked), and distances beyond 0.1 light-years in light-years
- Interstellar warp: warps farther than about 1,900 AU get a longer, bigger animation (the sky crowds forward and turns blue ahead and red behind, you pass through a swirling tunnel and rings of light, and a shock wave spreads on arrival), with its own sound
- Surfaces no longer jitter when drawn light-years away from the Sun (bodies are drawn relative to the reference body)
- Fixed the info card counting the planets and other bodies as moons of the Sun

## 2.15.3 (2026-10-31)

- Fixed the ship jittering in the chase view while turning far from the Solar System (around a light-year out)

## 2.15.2 (2026-10-31)

- The light-speed engine now sounds high and clear like glass instead of a low, motorbike-like growl
- The macOS version is now built on macOS 15 runners (GitHub retires macOS 14 in November 2026)

## 2.15.1 (2026-10-30)

- Not released (the macOS build runner was unavailable); its changes are in 2.15.2

## 2.15.0 (2026-10-30)

- Fix the reference frame to the ship (Shift+F / settings): you no longer move along with the nearest body and stay put relative to the Sun, so you can watch Earth leave at 30 km/s. Warping, moving to a place or an event, or the autopilot returns to automatic

## 2.14.1 (2026-10-30)

- Fixed the ground hiding the surface texture: below 2 km the area under the horizon was painted a flat color. Instead, Earth's city lights, which blur into large glowing patches up close, are now dimmed near the ground

## 2.14.0 (2026-10-30)

- Appearance: the info card and the ground panel show the magnitude, phase, apparent size and Moon age as seen from Earth (or from where you stand)
- Meteor showers: seven major showers stream meteors from their radiants across the night sky from the ground; their peaks are in the events list (choosing one takes you to the best night from Tokyo)

## 2.13.0 (2026-10-30)

- Observing from the ground: the ground below the horizon is filled in (hiding the body's image stretched up close), with the horizon, compass points, local time, the target's altitude and azimuth, and its next rise, transit and set (within a minute or two of the almanac for the Sun at Tokyo)
- Places on the Moon (Apollo 11 and 17, the lunar south pole, Chang'e 4) and on Mars (Perseverance, Curiosity, Viking 1); Earth cities show their standard time

## 2.12.0 (2026-10-30)

- Search also lists spacecraft no longer (or not yet) in flight; choosing one moves to its highlight and warps there
- Sky from the ground: the "On Earth" search tab takes you to 20 places such as Tokyo, London or the South Pole. When stopped near a surface you now turn with the body's rotation and are no longer dragged along by satellites passing overhead
- The 100,000 asteroid particles are computed and projected in a GPU shader instead of on the CPU

## 2.11.0 (2026-10-30)

- Missions in flight: BepiColombo (into Mercury orbit in November 2026), JUICE, Europa Clipper, Lucy and Psyche, on their planned Horizons trajectories, with models, events and two new tour scenes
- Find a body (/ or F4 / touch "Search"): narrow down by part of the name (kana too), then target or warp; category tabs
- Venus's clouds now circle the planet in about 4 days (super-rotation); Titan gets its detached blue upper haze

## 2.10.0 (2026-10-30)

- Rings of Uranus, Neptune and Jupiter from the radii, widths and optical depths in NASA fact sheets (Uranus's narrow rings turn with its tipped-over axis; Neptune's Adams ring has its arcs; opacity exaggerated because they are faint and dark)
- Source split: the parts of `src/main.cpp` now live in `src/app/` (no change in behavior)

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
