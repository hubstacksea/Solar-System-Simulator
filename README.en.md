# Solar System Voyager

[日本語](README.md) | **English**

A 3D space flight simulator with the Sun, planets and major moons placed at **true size and true distance**.
Body positions are computed from the PC clock (UTC); at 1x time scale they orbit and rotate in real time.

- Rendering engine: [raylib 5.5](https://www.raylib.com/) (OpenGL 3.3)
- Platforms: Linux (gcc) / Windows (MinGW-w64)

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

### Cross-building the Windows version on Linux

```sh
cmake -B build-win -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-x86_64.cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build-win -j
```

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
| [ / ] | Time scale (x1 real time to 1 year per second). Affects both orbits/rotation of the bodies and the ship's acceleration and movement |
| P / Backspace | Pause time / sync to current time at x1 |
| I | Invert mouse Y |
| J | Switch language (日本語 / English) |
| F11 / F12 | Fullscreen / screenshot |
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

- **Speedometer** (bottom right): analog gauge whose full scale is the current engine's top speed, plus a digital readout
- **Heading indicator** (top center): near a body (within 30 times its radius), heading, pitch and roll with that body's rotation axis as north. Farther away, ecliptic longitude and latitude. The yellow ▲ marks the target's bearing
- **Body markers**: name and distance to the surface for the Sun and every planet. Off-screen bodies are shown as arrows at the screen edge. Moons are shown when near their parent
- **Target info** (top right): distance, estimated arrival time at current speed, light travel time, and travel time at top speed. With a time scale applied, it also shows how many seconds of real time you will wait

## How it works

- **Body positions**: planets are computed from JPL J2000 mean orbital elements via Kepler's equation (accurate to a few arcminutes). Moons use circular orbits with real radii, periods and inclinations, but their phases do not match their actual positions
- **Sizes**: equatorial radius and flattening (Jupiter and Saturn are visibly oblate), and Saturn's rings (C ring to A ring, Cassini Division, Encke Gap) drawn at true scale
- **Reference frame**: the ship moves along with the body whose sphere of influence (SOI) it is in, so even at car speed you are not left behind by Earth
- **Precision**: positions are stored as doubles and rendered relative to the camera. Each body is drawn at its own scale with its own near/far planes, so a 30 m ship and Neptune 4.5 billion km away can be rendered at the same time
- **Minimum altitude**: because of floating-point precision, you cannot descend below 1/100,000 of a body's radius (minimum 30 m) — about 64 m on Earth
- No physics (gravity etc.) is simulated. The ship flies arcade-style in the direction of its nose

## Display language (日本語 / English)

- Press the **J key** at any time to switch. The chosen language is saved in `solar_voyager.cfg` next to the executable and used on the next launch
- To choose at startup: `solar_voyager --lang en` (or `ja`). Priority is: command-line option > saved setting > Japanese

For Japanese text, the program automatically searches for an OS Japanese font (Windows: Meiryo / Yu Gothic / MS Gothic, Linux: Noto Sans CJK / IPA, etc.).
If none is found, the display is fixed to English. A font placed at `assets/font.ttf` takes priority.
