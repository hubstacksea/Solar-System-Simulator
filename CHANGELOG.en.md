# Changelog

[日本語](CHANGELOG.md) | English

The release workflow copies the section of the version being released into the GitHub release notes.

## 2.57.0 (2026-11-20)

- The ground on Earth and Mars now receives light from the sky (sunlight scattered down by the atmosphere, more on surfaces facing up). The direct sunlight plus skylight with the Sun overhead is taken as white, so midday colors and brightness are unchanged, shade receives sky-colored light, and in the morning and evening the light shifts toward the sky's color as the direct sunlight weakens. Antarctic snow at midday turned from yellowish gray to nearly white, and the lee sides of dunes are no longer pitch black
- The dimming of sunlight on the ground now comes from estimated optical depths rather than the sky-color values (Earth: Rayleigh scattering plus 0.1 of aerosols, 0.15, 0.20 and 0.32 in red, green and blue, so the ground reddens a little less at dawn and dusk; Mars: dust of 0.5 in every color, scattering 0.97, 0.92 and 0.80 of the light, so the Martian ground no longer turns bluish purple from the sunlight in the evening). The sky colors are unchanged
- Retook the README images whose look changed: the Moon, the horizon from 10 km, Valles Marineris, Everest and the Apollo 15 landing

## 2.56.0 (2026-11-20)

- Near the ground, relief finer than the maps is now shaded by surface material (terrain heights and collisions are unchanged)
  - Moon: boulders 1–12 m across in proportion to the LRO Diviner rock abundance (Bandfield et al. 2011; about 18% on the rim of Tycho, typically 0.3%)
  - Mars: from the MGS TES nighttime thermal inertia (Putzig & Mellon 2007) and the map brightness, subdued small relief where there is dust, ripples (25 m wavelength) and dunes (300 m) on dark sand, and boulders on rocky ground
  - Earth: judged from the map colors, dunes (120 m and 800 m wavelength) in sand deserts, a bumpy canopy with darker hollows in forests, and wind-carved ridges (about 20 m and 5 m) on snow and ice
  - Where each material is comes from measurements (Moon, Mars) or from the colors (Earth); shapes finer than a few hundred meters are procedural, sized after real ones. The relief shows best when the Sun is low
- Added the data assets/moon_rock.png and assets/mars_ti.png and the tool that makes them, tools/make_surface_props.py (NASA PDS Geosciences Node, public domain)

## 2.55.0 (2026-11-20)

- Airless bodies now show the opposition effect (the ground brightens when the Sun is behind you because the shadows between grains are hidden; Hapke's shadow-hiding term B0 / (1 + tan(g/2) / h) with the lunar values B0 = 1 and h = 0.06 used for all bodies; unchanged at a phase angle of 45°, 1.77 times brighter at 0°)
- Earth's ice sheets, snow and sea ice and the Martian polar caps get forward scattering that brightens them when you look toward the Sun at a low angle, plus a sheen from the smooth surface (ice is judged from the white color of the map; on Mars only poleward of latitude 60°)
- The Martian polar caps no longer get small procedural craters (the cap surfaces are young and have hardly any; the large craters in the elevation data remain)

## 2.54.2 (2026-11-20)

- Fixed the Apollo command module after service module jettison, and under its drogue parachutes, being drawn as the Huygens descent module (since 2.42.0)

## 2.54.1 (2026-11-19)

- Fixed the ship sometimes being pushed up by tens of meters right after warping to a probe on the ground (such as Perseverance), which moved the probe off the center of the screen (the fine terrain where you arrive is now loaded before probes are placed back on the terrain)

## 2.54.0 (2026-11-19)

- The globe minimap now shows day and night (the Sun's altitude at each point is found from the Sun's direction and the night side is darkened; on Earth the twilight down to a solar altitude of -6° is blended)
- The globe minimap now shows where you are heading (the points below you over the next 3 hours at your current velocity, including the body's rotation, up to the edge of the visible side; in gravity mode the coasting path under that body's gravity alone; a cross where you would hit the ground; future thrust is not included; hidden while you are stopped within 100 km of the ground and turning with it)

## 2.53.0 (2026-11-19)

- Flying near the surface of a body with a map (up to an altitude of 10% of its radius) now shows a globe minimap with a dot at your position below the top-left panel (an orthographic view centered on your position with north up, slightly see-through). It appears for Earth, the Moon, Mars, Mercury, Ceres, Vesta, Pluto, Charon, Titan, the moons of Jupiter and Saturn and others (not for Jupiter and Saturn, which have no surface, nor Venus and Iapetus, which have no visible-light map). Earth's oceans are painted from the land and sea data. It is hidden in photo mode and on voyages, and placed clear of the left buttons with touch controls

## 2.52.0 (2026-11-19)

- Maps and terrain on oblate bodies were shifted in latitude. Maps and elevation data were looked up at a latitude halfway between the direction from the center and the map's latitude, shifting Earth's surface poleward by about 0.09° at mid-latitudes (about 10 km; the summit of Mt. Fuji was about 10 km north) and Jupiter's and Saturn's by about 2–3°. Maps are now looked up in each body's latitude convention (geodetic for Earth, planetocentric for the other maps, per the Saturn map's label and the description of the Jupiter map PIA07782), in the terrain, shading, altitude and collision calculations and the fine elevation windows alike
- Earth's displayed latitude (the position at the lower left, share links and bookmarks, the ground view) and the places that turn a latitude into a position (going to a city, `--at`, eclipse locations) now use geodetic latitude too (before, they used the latitude of the direction from the center, showing N35.2° at the summit of Mt. Fuji)

## 2.51.2 (2026-11-19)

- `tools/run_checks.py` now checks the browser version's shader: the vertex shader must call the procedural relief `bumpAt` from one place only (writing it in several places, as in 2.51.0, kept the browser from finishing the shader translation, so it never started). The 2.51.0 shader counts 8 places and fails. The app behaves the same as 2.51.1

## 2.51.1 (2026-11-19)

- The browser version of 2.51.0 froze on the "Loading" screen in some environments. Computing the terrain vertices' relief from 8 lattice points at 2 levels wrote the heavy relief calculation 16 times into the shader, which apparently made it too large for the browser to finish translating (Chrome and Edge on Windows translate WebGL to Direct3D; it did not reproduce in headless Chrome on this PC). The calculation now appears once and runs in a loop. It looks the same as 2.51.0

## 2.51.0 (2026-11-18)

- The procedural relief (craters and slopes) added to the terrain shape in 2.49.0 no longer ripples at its edges in flight (it stood out, for example, on the crater to the right as the Apollo 16 ascent stage leaves the Moon). The terrain grid's vertices slide over the ground with the camera, so computing the relief directly at the vertices caught and missed relief close to the vertex spacing, making edges jagged along the triangles and changing their shape from frame to frame. The relief is now computed at 3D lattice points fixed to the ground (spaced at a power of two times 20 m, no closer than the vertex spacing, blended smoothly with the next level) and interpolated, the same idea as the downscaled elevation images. The relief that takes shape is about as fine as before. Altitude and collisions interpolate the same way at the finest level (20 m). Earth's slopes work the same way. Near the surface of the Moon and Mars, a frame takes about 1.5 to 2 ms longer in the native version on this PC (Earth is about the same)

## 2.50.0 (2026-11-18)

- The planet and moon maps were rebuilt to reduce the square mosaic seen up close from orbit (it stood out on the walls of Valles Marineris on Mars). The USGS map service picks the nearest pixels when asked for the target size, which makes the maps jagged, so `tools/make_planet_maps.py` now fetches them at 4 times the size and averages only the imaged pixels down. Color maps are stored as JPEG without chroma subsampling (color edges no longer come in 2-pixel steps). The maps are as detailed as before (about 5.2 km per pixel on Mars), and `assets/maps.bin` went from about 23.3 MB to about 23.0 MB. On Mars, the bluish-white pixels in the source data now show as well
- When a map pixel covers more than a screen pixel, the map color is now read with cubic (Catmull-Rom) interpolation (bilinear interpolation bends the brightness slope at pixel edges and looks square)

## 2.49.0 (2026-11-18)

- Relief finer than the elevation data now goes into the shape of the terrain, not just its shading (the Moon, Mercury, Mars, Venus, Ceres, Vesta, Pluto, Charon and Titan). The parts of the procedural craters and slopes of the surface shading that are finer than the data are added (the lunar maria have fewer craters and are flat, and Titan's lakes and seas are flat; they do not match the real terrain). Altitude and collisions are measured from the same shape. The shape is detailed down to the spacing of the terrain grid's vertices, so kilometer-sized craters take shape near the camera (within about 10 km)
- The Moon and Mars also use fine elevations around the camera near the surface: LOLA at 16 pixels per degree for the Moon (about 1.9 km, up from about 5.3 km; `assets/moon_detail.bin`, about 18 MB) and MOLA at 16 pixels per degree for Mars (about 3.7 km, up from about 15 km; `assets/mars_detail.bin`, about 15 MB), made by `tools/make_body_detail.py` (16-bit values stored as differences from a prediction from neighboring pixels, then compressed). The browser version loads them in the background after startup
- The window of fine elevations is now moved under the ship at the start of every frame instead of when drawing (a scene placed near the surface by a share link measures altitude and collisions from the drawn terrain from the start)
- README images: Mons Hadley on the Moon and Valles Marineris were retaken, and the Everest massif and the Alps seen from orbit were added

## 2.48.0 (2026-11-18)

- Mountains on Earth now stand out as 3D relief near the surface. Land elevations are kept at the 1 arc-minute resolution of NOAA ETOPO 2022 (about 1.85 km, up from about 10 km; `assets/earth_detail.bin`, about 17 MB, made by `tools/make_earth_detail.py`), and only an area about 3,800 km across centered under the camera is unpacked and used for the terrain and its shading. In the data, Mount Fuji rises from about 1,730 m to about 3,360 m and Everest from about 6,560 m to about 8,210 m, so their shapes can be seen from the ground and from low altitude. The shading of mountain ranges seen from space is finer too
- Slopes finer than Earth's 1 arc-minute data are added procedurally according to the steepness of the terrain (rugged on steep mountains, gentle on plains; they do not match the real terrain). The terrain shape, the shading and the altitude and collision calculations all use the same values
- In the browser version, Earth's fine elevations are loaded in the background after startup (the amount needed to start is unchanged)

## 2.47.0 (2026-11-17)

- The asset tools (Earth, Earth's seasons and clouds, the Moon, the Milky Way, and the planet and moon maps) gained `--hq` to make a high-resolution set: 8192×4096 for the Earth and Moon colors and the Mercury and Mars maps, 4096×2048 for the Moon's elevations, MOLA at 16 pixels per degree for Mars, and so on (never finer than the source data). Maps larger than the 4096 pixels the USGS map service returns at once are fetched in tiles. The CMake option `EMBED_ASSET_DIR` builds with another folder of embedded data, such as the high-resolution set. The released builds keep the current resolution (a high-resolution build is about 178 MB with about 1.2 GB of textures)
- The relief shading of the Earth and the Moon now reads the size of the elevation image from the image instead of assuming it (no visible change)

## 2.46.0 (2026-11-16)

- Fixed Perseverance and Opportunity staying at their landing sites instead of driving in 2.45.0 (their table names in the asset file were too long to be read). The data check (--check-assets) now also confirms that all lander tables were read
- Rover tracks: with orbit lines on (O key), the path the target rover drove is drawn 3 m above the ground (bright where it has already driven, dim ahead)
- The info card (I key) now shows a lander's landing time, the sols since landing for Mars landers, and the distance driven for rovers (the length through the recorded points, not counting airbag bounces). Opportunity comes to 45.24 km (NASA's figure is 45.16 km)

## 2.45.0 (2026-11-16)

- Fixed a sphere about 1.5 m across that was drawn near landers in addition to their models (since Huygens in 2.42.0)
- The lander descent paths and rover drives moved from the source code into an asset file (assets/landers.bin, about 370 KB). Nothing looks different; the browser version loads it at startup
- The README gallery gained Perseverance lowering its rover on the sky crane and Spirit coming down on its airbags

## 2.44.0 (2026-11-16)

- Four more Mars landers: Spirit (4 January 2004) and Opportunity (25 January 2004) on airbags, and Phoenix (25 May 2008) and InSight (26 November 2018) on legs and engines, come down from atmospheric entry along NAIF's reconstructed descent trajectories. The landing points match the official coordinates. The "spacecraft events" page of the event list gained their four landings
- Their shape changes with time: for Spirit and Opportunity the capsule, the parachute, the lander hanging 20 m below on its bridle, the airbags and the rover; for Phoenix and InSight the capsule, the parachute, the lander under engine power and the lander with its solar arrays open. Parachute and separation times come from where the acceleration along the trajectory changes; heat shield separation is put 15-20 seconds after the parachute (approximate)
- Estimated or left out: the simplified shapes, the parachute sizes and the airbag bounces (the airbags move from the first impact to where they stopped over 10 minutes). After stopping only the rover is drawn (not the lander platform). Spirit's and Opportunity's trajectories use an altitude reference about 2 km off from the map, so they are brought to the ground from 10 km up
- Rover drives: Curiosity, Perseverance, Spirit and Opportunity now move along NAIF's records of their positions (Curiosity about 35.5 km up to 15 November 2026, Perseverance about 41.6 km up to 28 October, Spirit up to March 2010 and Opportunity up to June 2018). The rovers face the way they last drove and stay at their last position after the records end
- The rotation periods of Mercury, Venus, Jupiter, Ceres, Vesta and Pluto are now the IAU values. The rounded values had let their orientations drift (by now Vesta 94°, Ceres 10°, Jupiter 5.7°, Pluto 2°, Venus 0.4°, Mercury 0.15°)
- The eight README images from the first version (Earth by day and night, the Moon, constellation lines, Halley's Comet, the solar and lunar eclipses and Saturn) were retaken with the current version; the lunar eclipse is now the next total one (31 December 2028)

## 2.43.0 (2026-11-16)

- Mars landers' descents: Curiosity (6 August 2012) and Perseverance (18 February 2021) are now spacecraft. From about 9 minutes before entering the atmosphere they come down over Mars along the descent trajectories reconstructed by NASA's EDL teams (NAIF SPKs), and after landing they stay put until the day they first drove (22 August 2012, 4 March 2021; later traverses are not included). The landing points match the official coordinates
- Their shape changes with time: the entry capsule (4.5 m across), the parachute, after heat shield separation, the descent stage with the rover, the rover lowered on the sky crane (about 7.1 m below) and the landed rover. Perseverance's times are from the table of Karlgaard et al. (MEDLI2 reconstruction), the lowering from the SPK notes, and Curiosity's parachute and backshell separation from where the acceleration along its trajectory changes
- The "spacecraft events" page of the event list gained "Curiosity lands on Mars" and "Perseverance lands on Mars". They show the craft up close from beside its direction of travel and descend with it, from 1 minute before entry
- Estimated or left out: the simplified shapes, the parachute's inflated diameter of 16 m and 40 m lines, and Curiosity's heat shield separation (24 seconds after the parachute). The descent stage flying off after landing and the cruise stage before entry are not drawn
- Huygens now also flies the three weeks from its release from Cassini on 25 December 2004 to entry, on the JPL Horizons trajectory (which includes the prediction as of 6 January 2005). It is 43 km from the reconstructed path at entry, so the two are joined over the last hour
- The "On Earth" search tab gained Selk crater on Titan (Dragonfly's destination; launch planned for July 2028 and arrival in 2034). Dragonfly itself is not drawn, since no trajectory data is available
- Mars's rotation period is now the IAU value (1.0259567565 days). The rounded value let its orientation drift by about 0.4° a year, about 11° by now (places on the map stay put; what changes is where it is day at a given time)
- README gallery tidied: two images of the old-style Jupiter and a near-duplicate Saturn image were removed, the info card image was retaken, and Huygens under its parachute was added

## 2.42.0 (2026-11-16)

- Titan's lakes and seas: the seas and lakes filled with liquid (methane and ethane) in the north and south polar regions are now drawn. Their extent is the "seas" and "liquid-filled lakes" of the polar geomorphologic maps of Birch et al. (2017) (Birch and Hayes, CC BY 4.0), 1.08% of Titan's surface (matching the published value of about 1.1%). The liquid is flat and dark and reflects more of the sky the more obliquely it is seen (Fresnel reflection with liquid methane's refractive index of about 1.29)
- Estimated: the orientation of the lake maps' projection (the data does not describe it, so it was fitted to the dark seas in Cassini's near-infrared map) and the liquid levels (each connected body is set to the median of the coarsely interpolated elevations)
- Huygens descent: the Huygens probe is now a spacecraft. From entering the atmosphere at 09:05:53 UTC on 14 January 2005 to landing at 11:38:11 it descends over Titan along the trajectory reconstructed by the Huygens DTWG (Kazeminejad, Atkinson et al., NASA PDS), and stays on the ground after landing. Its shape changes with time (with its front shield, under the main parachute from 09:10:23, under the stabilizer parachute from 09:25:21, and landed; times from Lebreton et al. 2005)
- The "spacecraft events" page of the event list gained "Huygens lands on Titan". It shows the descending probe up close from the side, 10 minutes before landing (the camera descends with it)
- Estimated or left out: the parachute diameters (main 8.3 m, stabilizer 3.0 m) and line lengths, and the 30 seconds before the front shield is released (not drawn separately). The three weeks from release from Cassini to entry are not drawn. The entry and descent reconstructions differ by about 4 km where they meet and are blended over 60 seconds. The trajectory's landing point (10.251° S) is about 14 km from the one in the places list (10.573° S)
- Below Venus's clouds and Titan's haze, your ship and spacecraft are no longer lit by direct sunlight but by the same diffuse sky light as the ground (they used to get the same white light as above the clouds)
- Warping to a spacecraft on the ground now puts you obliquely above it
- The map data the browser version loads later (maps.bin) stays at 23.3 MB (with the extent of Titan's lakes added)

## 2.41.0 (2026-11-15)

- Titan surface: Titan is now a rocky body too, and its familiar haze look is drawn as a haze layer 150 km above the ground (it looks the same from outside). Inside the haze (50-150 km) is fog; below it the ground shows through the hazy nitrogen air, and standing on the ground gives an orange sky
- Titan's elevations are the global topography of Corlies et al. (2017) from Cassini radar (altimetry, SAR topography and stereo; CC0, 4 pixels per degree, −1.5 to +1.1 km). About 9% of the surface was measured; the rest is interpolated. The light and dark of the ground come from Cassini's near-infrared (0.94 µm) global map (PIA19658), not visible-light brightness
- Estimated: haze top 150 km and base 50 km, the light (9% at the surface, 25% at the haze base, not counting the dimness of sunlight so far from the Sun), the light color (much less blue), Rayleigh scattering of the air (a green 1/e length of about 19 km at the surface, where the air is about 4.4 times as dense as Earth's), and scattering by haze particles (1/e about 7 km inside the haze, about 100 km below it)
- Venus and Titan now share the same rendering (a table of per-body values)
- The "On Earth" search tab gained the landing sites of ten Venus landers (Venera 7-14, Vega 1 and 2, with landing dates) and the Huygens landing site on Titan (14 January 2005). Coordinates are from Wikipedia's list of landings and its Huygens article; Venera 8-10 are only known to within about 150 km, and some articles give Vega 1 as 8.05° N, 176.89° E
- The README gallery gained the surface of Venus below its clouds, Titan below its haze, Vesta's real shape and Occator crater on Ceres
- The map data the browser version loads later (maps.bin) grew from 22.1 MB to 23.3 MB

## 2.40.0 (2026-11-15)

- Browser version: when drawing is heavy, the 3D resolution is lowered automatically. When the interval between the browser's screen updates exceeds 28 ms (about 36 fps) it drops by 10% at a time (down to 0.5x and 360 pixels of 3D height); if that does not help (the load is on the CPU) it goes back. It recovers when there is headroom, and the FPS readout shows the resolution while it is lowered. In Chrome on this PC, looking straight down at Earth went from 6.3 to 49 fps and at Mars from 10.5 to 49 fps (60-80% resolution)
- Venus surface: Venus is now a rocky body, and its familiar cloud look is drawn as a cloud layer 70 km above the ground. Below the cloud tops you come down onto terrain from the Magellan altimeter elevations (Ford, Pettengill et al.; −3.7 to +10.6 km, shrunk to about 18.6 km spacing). The roughly 8% that was not measured is an estimated height joined smoothly from the surroundings
- The view aims to be realistic: inside the cloud layer (48-70 km) is white fog; below the clouds the sky is a uniform overcast with no stars. The ground fades into orange-brown haze in the thick CO2 air within a few km, and the surface cannot be seen from high up. The ground is basalt gray (the radar images are not visible colors, so they are not used) with almost no craters
- Estimated: the light relative to sunlight above the clouds (3.5% at the surface, 15% at the cloud base), the displayed brightness (as for eyes adapted to the dark: the ratio is square-rooted and doubled; the real view is darker), the light color (much less blue at the surface), the haze 1/e lengths (at the surface red 2.2 km, green 0.95 km, blue 0.43 km, scaled from Earth's air by density and the stronger scattering of CO2), a 1/e length of about 0.7 km inside the clouds, and cloud tops at 70 km and base at 48 km
- Venus now uses the IAU prime meridian (W0 = 160.20°), so the elevations sit at the right longitudes (the cloud pattern turns too). Constellation names and similar labels are hidden below the clouds, and standing on the ground shows the compass directions
- The map data the browser version loads later (maps.bin) grew from 19.4 MB to 22.1 MB

## 2.39.0 (2026-11-15)

- Mercury, Ceres, Vesta, Pluto and Charon now also draw their elevation data as true-height (1x) 3D terrain within 8 body radii, like Earth, the Moon and Mars. Altitude and surface collisions are measured from the terrain. Vesta gets its lumpy shape with the Rheasilvia basin. The maps and elevations agree in longitude (offset 0°, checked by correlating elevation slopes with the map patterns)
- The data are USGS Astrogeology global elevation models: Mercury MESSENGER (665 m spacing, Becker et al.), Ceres Dawn (137 m, Preusker et al., DLR), Vesta Dawn (93 m, DLR), Pluto and Charon New Horizons (300 m). The simulator shrinks them to 2048 × 1024 for Mercury and Pluto and 1024 × 512 for Ceres, Vesta and Charon (about 2 to 7.5 km spacing). Relief finer than that is flat
- Pluto and Charon only have data for the hemisphere New Horizons saw up close (about 46%); the rest is an estimated height joined smoothly from the surrounding values
- Vesta and Charon have large relief (−23 to +21 km and −14 to +7 km from the ellipsoid), so a format storing heights in 1 m steps was added
- The map images of Vesta and Ceres are corrected for the latitude shift when they are wrapped onto the flattened ellipsoid (up to about 7° on Vesta). Bright areas such as Occator crater on Ceres change shape slightly
- Terrain heights grow smoothly from zero to full size between 6 and 8 radii, so the shape does not jump when the sphere is replaced
- Relief shading from map elevations assumed Mars's 1440 × 720 elevation image size; it now uses the actual size
- The map data the browser version loads later (maps.bin) grew from 12.6 MB to 19.4 MB

## 2.38.2 (2026-11-15)

- Distant relief no longer ripples as much when flying fast just above the surface. The thinning of the far terrain mesh in 2.38.1 made the ring spacing coarser than the elevation data (about 34 km at 1 km altitude on the Moon, where a LOLA pixel is about 5.3 km), so the mesh vertices, which move with the camera, kept catching and missing the peaks in between
- The ring spacing is now never wider than half an elevation pixel, and the mesh has 240 rings instead of 120. Heights are read from data smoothed to the actual vertex spacing. At 1 km altitude on the Moon, the frame-to-frame wobble of the distant ridge line per 0.5 km of travel, measured against a reference drawn with a fine mesh, went from 1.07 to 0.57 pixels (some remains). The browser version runs as fast as 2.38.1

## 2.38.1 (2026-11-15)

- The browser version is much faster near the surfaces of Earth and Mars (Chrome on this PC, looking at the horizon: Earth 8 → 60 fps, Mars 5.5 → 60 fps; looking straight down: Earth 2.8 → 6.4 fps, Mars 2.9 → 10.5 fps)
- Terrain mesh: the ring spacing is recomputed every frame and thinned so rings are never closer than 32 pixels on screen. Rings near the point below the camera whose vertices crowd together around the circle are thinned too. The mesh went from 240 × 256 to 120 × 128. While the terrain is drawn, the sphere beyond it is not drawn (it is always behind the horizon)
- Earth surface color: the sea ice, city lights, cloud wisps and land patterns are skipped where they have no effect. Mars: the procedural surface is no longer computed where the map covers it. The look is unchanged
- Looking straight down at the Moon is still slower in Chrome than without terrain (in the desktop version the difference is about 10%)

## 2.38.0 (2026-11-15)

- Surface heights on Earth, the Moon and Mars: within 8 body radii the elevation data is drawn as real 3D terrain at its true height (1x). Mountains and valleys that were only shading before now rise above the horizon. A mesh centered below the camera reaches past the horizon; beyond it the old sphere is drawn
- The altitude readout and surface collisions are measured from the terrain. The Apollo lunar modules and the descent stages left on the Moon also sit on the terrain (the recorded paths come down onto the Moon's mean radius, so below 15 km they are lifted smoothly by the terrain height; the vehicle attitude still follows the velocity of the path before the lift)
- The data is the same as before: Earth ETOPO (about 9.8 km spacing), Moon LOLA (about 5.3 km), Mars MOLA (about 14.8 km). Relief finer than that is flat, and no procedural detail is added to the shape. Earth's oceans sit at sea level. The shading exaggeration (Earth 2.5x, Moon 1.5x) is unchanged
- The README screenshots are grouped by topic, with new shots of Mons Hadley on the Moon, the inside of Valles Marineris and Apollo 15 (undocking, landing, deep-space EVA, splashdown). The old low-flight Moon shot with the outdated display was removed

## 2.37.14 (2026-11-14)

- Apollo program: the deep-space EVAs Apollo 15-17 made on the way back to Earth are shown. The hatch opens and the command module pilot moves along the service module's handrails to the scientific instrument bay, stands in the foot restraint and brings back the cassettes one by one, while another crew member stands in the hatch. The view is from the side; during the EVA time runs at most 30 times real time and slows further as the hatch opens and closes and as the pilot goes out and comes back in
- Times are from the records. Apollo 15: hatch open 242:04:25, closed 242:25:15 (Apollo Flight Journal). Apollo 16: hatch unlatched 218:47:06, back inside 219:18:40, closed 219:46:50 (Apollo Flight Journal, actual GET). Apollo 17: hatch open 255:00:39, back inside 255:47:23, closed 255:49:11 (air-to-ground transcript, original GET; Schmitt stood in the hatch, per the Mission Report)
- Estimated: the hatch position (facing the same way as the bay) and size (0.74 x 0.86 m), the hinge side and the 90 degree opening, the flat door, the handrail route and body poses, the figure (about 1.9 m tall, no backpack), and the times of stretches the records give no time for (including when Apollo 16's hatch opened)
- Not drawn: the attitude maneuver for the EVA and Apollo 16's MEED experiment attitude, the umbilical, the TV pole and camera, the cassettes themselves, the bag Apollo 16 threw out, and arm and leg motion

## 2.37.13 (2026-11-14)

- Apollo program: in the separation from the S-IVB scenes (Apollo 8 and 10-17), the camera no longer zooms out instantly at separation. Before separation the camera is close in on the middle of the stack, but after separation it aimed at a point 12 m behind the S-IVB's base instead of 12 m in front of it, so the aim point jumped 24 m and the framed size jumped from 44 m to 68 m. The view now continues unchanged through separation

## 2.37.12 (2026-11-14)

- Apollo program: the scenes now show the scientific instrument module bay door that Apollo 15, 16 and 17 jettisoned from the service module on the way to the Moon. Neither the door nor the open bay after jettison was drawn before. The door is 5 ft wide and 9.5 ft long (1.5 × 2.9 m) and weighs 170 lb (Apollo 15 Flight Journal). It is jettisoned at 74:06:47.1 on Apollo 15, 69:59:01 on Apollo 16 and 81:32:40 on Apollo 17. Explosive cord pushes it straight out from the side of the service module, at "approximately 7 ft/s" (2.13 m/s) on Apollo 15, as the ground announced beforehand, and at the planned 13.7 ft/s (4.18 m/s) from the press kits on Apollo 16 and 17. It tumbles about its long axis at 12 rpm (the Apollo 17 crew's "1 revolution per 5 seconds"). The command and service module and the door are seen from the side around the jettison, and the view returns to normal by the time they are 50-150 m apart. The door is drawn for 30 minutes after jettison. After jettison the open bay (a dark panel with instrument boxes) is drawn on the command and service module and on the service module jettisoned before the return to Earth
  - Estimates and omissions: all the speeds are pre-flight values, and no measured values were found. The bay is assumed to be opposite the high-gain antenna, and its position along the service module and the instrument shapes are estimates. Apollo 15 and 16 use Apollo 17's spin rate, and the spin axis and the push direction are estimates. The jettison attitude and the turn to it (Apollo 16 records only the times of the turn and the turn back) are not modeled, and the coast attitude is kept. Not modeled: the debris that left with the door, the command and service module's own motion from the jettison, the motion from the difference in lunar gravity (a few m over 30 minutes) and the door's path after 30 minutes

## 2.37.11 (2026-11-13)

- Apollo program: the scenes now show the subsatellite that Apollo 15 and 16 released from the service module's scientific instrument module bay in lunar orbit. It was not drawn before. The subsatellite is a hexagonal prism (31 in long, 14 in across, 78.5 lb) with three 5 ft booms and a magnetometer (NASA photo S71-39481 caption, Apollo 15 mission report). It is released at 222:39:29 on Apollo 15 and 196:02:09 on Apollo 16 (each mission report). The command and service module turns over 4 minutes to point the bay perpendicular to the orbit plane (on Apollo 16 from 195:27:28, Flight Journal), holds 5 minutes and turns back to its usual attitude over 4 minutes. The subsatellite is pushed out of the orbit plane at 1.22 m/s (4 ft/s) and drifts away by the relative motion equations for a lunar orbit (Clohessy-Wiltshire). It leaves spinning at 140 rpm, which drops to 12 rpm when the booms deploy. Both craft are seen from the side around the release, and the view returns to normal by the time they are 30-80 m apart. Time slows to real time around the turns and the release
  - Estimates and omissions: the bay is assumed to be opposite the high-gain antenna, and the point where the subsatellite emerges is estimated from the bay's dimensions. The release direction is assumed to be the ecliptic-north side of the orbit normal, and the spin axis to be the release direction. The booms are assumed to deploy after 3 seconds. The start of the turn on Apollo 15 (30 minutes before) and the lengths of the turn, the hold and the turn back are estimates. The sizes of the top antenna and the magnetometer and the shape of the stowed booms are estimates, and the colors are brightened to be visible. The subsatellite is drawn for 10 minutes after release (about 700 m) and its own orbit after that is not drawn. Not modeled: the wobble of about 10° the Apollo 15 crew saw, the bay door and the subsatellite's container

## 2.37.10 (2026-11-12)

- Apollo program: the scenes now show the ascent stage jettison in lunar orbit (Apollo 10, 11, 12, 14, 15, 16 and 17). Before, the ascent stage stayed on the command module's docking port after jettison for 5-20 minutes, neither pushed nor turning, before moving onto its own path. From 10 minutes before jettison the docked pair turns over 4 minutes to point the ascent stage straight away from the Moon (recorded for Apollo 10 and 15). After jettison the ascent stage drifts away by the relative motion equations for a lunar orbit (Clohessy-Wiltshire), and both craft are seen from the side until they are apart. The separation speed is the ground's "about 2 feet per second" (0.61 m/s) on Apollo 11, and on Apollo 10 5 ft/s (1.52 m/s), when tunnel pressure blew the lunar module hatch open (mission report). The command module holds for 5 minutes (the time to the separation burn on Apollo 12, 14 and 15) and turns back to its usual attitude over 4 minutes. The ascent stage holds its attitude and moves to its usual attitude over 10 minutes from 15 minutes after jettison. Time slows to real time around the turns and the jettison
  - Estimates and omissions: the jettison attitude on Apollo 11, 12, 14, 16 and 17 is assumed to be straight up. The start of the turn (10 minutes before), its length (4 minutes) and the length of the turn back (4 minutes) are estimates. Missions without a record use Apollo 11's separation speed. The difference from the relative motion equations is removed between 15 and 25 minutes after jettison, returning to the trajectory records. Not modeled: the Apollo 16 ascent stage failing to hold attitude and tumbling after jettison, the docking parts leaving with the ascent stage, and the command module's own motion from the jettison (about 0.1 m/s)
- Apollo program: the Apollo 12 ascent stage jettison time is corrected to 147:59:31.6 (the previous 148:04:30.9 was the command module's separation burn; mission report)

## 2.37.9 (2026-11-12)

- Apollo program: the scenes now show the service module being jettisoned before the return to Earth (Apollo 8 and 10-17). Before, only the followed craft switched to the command module and the service module vanished on the spot. About 10 minutes before separation the spacecraft turns over 4 minutes to the separation attitude, heat shield forward and yawed 45° left (each mission report and the Apollo Flight Journal), and both modules are seen from the side around separation. The service module flies from the trajectory's position and velocity at separation under Earth's gravity, pushed away by its four -X jets (100 lbf each), and starts spinning from its roll jets. On Apollo 8-12 the -X jets fire until the propellant runs out (300 s) and the roll jets fire for 5.5 s, giving 15 rpm; from Apollo 14 on the roll jets fire for 2 s and the -X jets for 25 s. Service module masses are from each mission report (derived from the mass tables for Apollo 8 and 10). After separation the command module turns back to the entry attitude over 7 minutes
  - Apollo 13 has two separations: at 138:01:48 the service module is jettisoned from the command module docked with the lunar module (0.58 m/s, from the lunar module burns in the mission report), and at 141:30 tunnel pressure pushes the lunar module away (0.667 m/s between the two). The lunar module keeps its attitude at jettison (attitude hold)
  - Estimates and omissions: the start and length of the turn and the length of the turn back are Apollo 10's recorded values, used for every mission. The spin rate on missions with the 2 s roll firing is scaled from 15 rpm (estimate). Not modeled: the propellant mass loss, the gravity difference between the two modules, the early service modules tumbling from propellant slosh and separating slowly (on Apollo 11 the crew saw it pass nearby 5 minutes later), the command module separation springs and air drag. The Apollo 13 docked pair keeps its coast attitude, and the damaged service module panel is not drawn
- Apollo program: the command module no longer jumps 5.25 m at separation (the shape's origin moves from the middle of the service module to the command module; it is shifted forward by that much and slides back to the trajectory point slowly)
- Apollo program: when the view moved into or out of the side view of two craft between nearly opposite directions (such as from looking at Earth), the picture rolled up to 10° in one frame partway. The directions are now joined smoothly on the sphere (now at most 0.71° per frame)

## 2.37.8 (2026-11-11)

- Apollo program: the scenes now show the lunar module undocking in lunar orbit and the ascent stage docking (Apollo 10, 11, 12, 14, 15, 16 and 17). Before, only the followed craft switched: the lunar module left the command module's nose abruptly, and the ascent stage ran into the command module at about 7 m/s without braking (on Apollo 11 it was 14.4 km away at 127:41 against the recorded 2.8 km). Both craft are now seen from the side around undocking and docking, and the followed craft switches during that view. Time slows to real time at undocking and contact
  - Undocking: the lunar module with its legs deployed backs away from the docking port, turns and moves off. The gear deployment and undocking times come from each mission report and the Apollo Flight Journal. The turn times are recorded for Apollo 10, 11, 12 and 15, and set 60 seconds after undocking for Apollo 14, 16 and 17 (estimate)
  - Docking: the ascent stage closes in, stationkeeps, lines up and makes contact. Apollo 10, 11, 12, 15 and 16 pass through the recorded distances at their times (for example, 2.78 km at 127:41:21 and 12.2 m at 127:52:05 on Apollo 11). No approach record was found for Apollo 14 and 17, which use the procedure's values (stationkeeping at 12.2 m, contact at 3 cm/s). The start of stationkeeping and the line-up turn for Apollo 10 and 12 are estimated from the air-to-ground transcripts, the Apollo 16 stationkeeping distance is assumed to be 40 ft, and the Apollo 15 gear deployment time is the Flight Journal's planned time. On Apollo 11 the crew's range calls disagree, so contact and docking are both placed at the recorded docking time (128:03:00)
  - The inspection turns after undocking are not drawn. After undocking, Apollo 14-17 separate as the trajectory records have them (1.5-2.9 m/s)
- Apollo program: after the legs are deployed in lunar orbit, the docked shape now shows the lunar module with its legs deployed
- Apollo program: in Apollo scenes, time no longer speeds up by more than 4 times per second (in scene 19 of Apollo 15 and 16 it jumped from 53× to 812× when stationkeeping began)

## 2.37.7 (2026-11-11)

- Apollo program: fixed the camera and spacecraft motion between the lunar module extraction and arrival at the Moon (Apollo 8 and 10-17)
  - After extraction, when the view returned from the side view of both craft to following the spacecraft, the spacecraft turned 34-50° in one frame (the orientation blend switched to turning the other way round partway). It now turns smoothly (at most 3.3° per frame)
  - The picture rolled 80-180° during the coast (the top of the screen was set apart from the spacecraft's orientation, pointing away from the reference body, Earth or the Moon). Like the spacecraft's orientation, it now moves gradually from pointing away from the nearby body to ecliptic north between 20,000 and 40,000 km from it
  - The camera jumped 100-270 m in one frame on the return. The view now returns as the two craft separate, and time slows enough that the return does not jump between frames (now at most about 8 m per frame)
  - Time now slows to real time around the start and end of each burn of the followed craft. Before, time stayed fast and the spacecraft spun abruptly while turning before a burn
- Apollo program: the camera direction no longer changes 98° at once on reaching Earth orbit (where the launch record ends); it now turns over 10 seconds on either side. The 8.5° jump at the start of the powered descent to the lunar surface is smoothed the same way
- Apollo program: fixed the camera starting abruptly at the beginning of some scenes (Apollo 12 scene 6, Apollo 14 scene 5, Apollo 16 scene 5) and the spacecraft orientation jumping in one frame (54° in Apollo 10 scene 18, 35° in Apollo 11 scene 8)

## 2.37.6 (2026-11-10)

- Apollo program: the scenes now show the first stage, the interstage ring and the second stage separating. Before, the spacecraft's shape just changed and the spent stage vanished on the spot. The spent stages fly from the trajectory's position and velocity at separation under Earth's gravity, slowed by their retro rockets, and fall away behind (first stage: 8 × 88,600 lbf for 0.666 s, 4 on Apollo 15; second stage: 4 × 34,810 lbf for 1.52 s; masses from the Apollo 11 flight evaluation report). The interstage ring is cast off 30 seconds after first stage separation (each flight evaluation report). Around each separation the camera shows both from the side and moves in on the vehicle as the stage recedes. Air drag and tumbling are not modeled. The launch escape tower still vanishes when jettisoned: no record was found of how its thrust was offset, and flown straight ahead it would hit the second stage
- Apollo program: the Saturn V now has the interstage ring between the first and second stages (5.5 m) and the conical interstage between the second and third stages (5.6 m)
- Apollo program: the vehicle no longer jumps 47.9 m (first stage) and 36.0 m (second stage) at separation (the shape's origin moved from the bottom of the lowest stage to the bottom of the next; the vehicle is shifted forward by that much and slides back to the trajectory point slowly)

## 2.37.5 (2026-11-10)

- Apollo program: the scenes now show the command module separating from the S-IVB, turning around, docking with the lunar module and pulling it out (Apollo 8 and 10-17). Before, the S-IVB vanished at separation and the lunar module suddenly appeared on the command module's nose at docking. The command module moves forward, the four adapter panels fall outward and fly off, and the lunar module with its legs folded appears on top of the S-IVB. The command module turns 180°, comes back, docks and backs away from the S-IVB with the lunar module. The separation speed, distance, turning rate, contact and ejection speeds and times come from each mission report and the Apollo Flight Journal (the checklist's planned values where none were found). Apollo 14 follows the contacts and retries before docking on the sixth attempt, when the latches finally caught. Apollo 8 carried a ballast test article (LTA-B) instead of a lunar module and did not dock; the ballast is not drawn, as no record of its shape was found. The S-IVB is drawn, with both craft seen from the side, until it starts turning or firing on its own (no record of its motion after that), and time slows to real time at docking and extraction
- Apollo program: the lunar module's legs stay folded until they were deployed in lunar orbit (the Apollo 11 mission report places the deployment in the lunar module activation before undocking). Apollo 13's lunar module, which never reached that step, keeps them folded after it is cast off
- Apollo program: Apollo 16 and 17 switched to the docked shape at the time of extraction (3:59:15 and 4:45:02); they now switch at docking (3:15:23 and 3:57:03). Apollo 10 switched to the docked shape at separation; it now switches at 3:17:03
- Apollo program: the camera no longer shakes for about a second when the view switches from the lunar module on the surface to the command module in orbit (such as the command module plane changes of Apollo 12 and 14-17). The near-ground view was being blended into the view of the craft in orbit, and it took a horizontal direction from a view looking straight down at the Moon

## 2.37.4 (2026-11-07)

- Apollo program: in lunar orbit, the lunar module marker no longer circles the command module after docking, and the lunar module no longer passes back and forth through the command module just after undocking. The points of the two spacecraft's trajectories were thinned out separately, so even while they followed the same path they were 1-13 m apart from interpolation errors. Where they follow the same path or one branches off from the other, both now keep the same points (`tools/make_apollo_assets.py`, `assets/apollo.bin`). The 28 m jump of Apollo 13's lunar module at jettison is gone too
- Apollo program: the lunar module shows no marker while it is joined to the command module. It was also drawn standing still at the start of its trajectory, up to 95 km ahead of the command module, for 2 minutes before undocking
- Apollo program: just after undocking and just before docking (within 100 m of each other), the lunar module is drawn moving away from or toward the docking port on the command module's nose. Before, the centers of the two coincided and the models overlapped
- The checking option `--voyage-trace` now also prints the distance to the other Apollo spacecraft

## 2.37.3 (2026-11-07)

- Apollo program: the close-up map around the Moon at the top right now shows the path as seen from the Moon. Before, it was drawn in a frame fixed to Earth in which the Moon moves too, so the lunar orbit drifted along with the Moon and looked like a spiral. The heading reads "Path seen from Moon". The Earth-centered overall map and the close-ups of probe flybys are unchanged

## 2.37.2 (2026-11-07)

- Apollo program: the camera no longer shakes from side to side every frame on the launch pad and under parachutes (the direction of travel, used while the spacecraft hardly moves horizontally, wobbled with position errors; it now uses the recorded horizontal velocity averaged over nearby times)
- Apollo program: the spacecraft no longer changes attitude in an instant in Earth orbit (it rolled 180° at the northernmost and southernmost points of each orbit), at separation from the third stage, where the reference body changes on the way back from the Moon, while the command module turns its heat shield forward (the side it turned toward flipped from frame to frame), when the drogue parachutes open, and when the lunar module ascent stage reaches orbit (it turned backwards)
- Apollo program: the view kept following the lunar module ascent stage after its record ended, until the next scene, and the camera flew hundreds of light years away (after Apollo 10 sent it into solar orbit, and after Apollo 12 and 17 crashed it on the Moon). It now follows the command module once the record ends
- The checking option `--voyage-trace` now also prints the Apollo spacecraft's nose elevation, its rotation since the previous frame and its distance from Earth

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
