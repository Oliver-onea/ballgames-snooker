# Reverse Engineering Report

## Summary

This project started from a single Windows executable, `项目2(1).exe`, and rebuilt the game as a cross-platform open-source port.

The result has two primary playable targets:

- `native/sdl`: C++/SDL2 native desktop version for Windows, macOS, and Linux.
- `web`: Canvas/Web version for browsers and mobile-friendly fallback.

There is also `native/pygame`, a Python/Pygame fallback that was useful as a fast local SDL prototype.

The original executable is not included in the public repository. Only the cross-platform reimplementation is open-sourced.

## Why This Was Fast

The executable was unusually friendly to reverse engineer:

- It was a MinGW/GCC Win64 binary, not packed or obfuscated.
- C++ symbols were still present.
- Function names survived, including `InitializeBalls`, `RenderTable`, `UpdateBallPositions`, `HandleCollisions`, `HandlePocketing`, `RobotHitBall`, and `WndProc`.
- Source file names survived in debug metadata: `def.h`, `init.h`, `dra.h`, `handle.h`, `main.cpp`.
- The game used plain Win32 GDI drawing instead of a custom engine, DirectX, Unity, Unreal, or encrypted assets.
- There were no external assets. Balls, table, pockets, background, cue, and HUD text were all drawn procedurally.
- Important constants were stored directly as float/double values in `.rdata`.

So this was not a blind decompilation problem. It was closer to recovering a small Win32/GDI game from a symbol-rich binary.

## Initial Binary Triage

The first step was to identify what kind of executable it was:

```bash
file '项目2(1).exe'
strings -a '项目2(1).exe'
nm -C '项目2(1).exe'
objdump -x '项目2(1).exe'
```

Key observations:

- PE32+ GUI executable for Windows x86-64.
- Imports were mostly `USER32.dll`, `GDI32.dll`, `KERNEL32.dll`, `msvcrt.dll`.
- GDI functions such as `Ellipse`, `Rectangle`, `LineTo`, `Arc`, `FillRect`, and `TextOutA` confirmed that rendering was immediate-mode Win32 drawing.
- Strings revealed the title and HUD text:
  - `Snooker`
  - `BrownBricksSnooker`
  - `Player Score: `
  - `Robot Score: `
  - `Player's Turn`
  - `Robot's Turn`
  - `Arial`

The SHA-256 of the original sample was:

```text
bc751686e384477155a1131eb8e3e57d52a9fa7be1a1925c5f68202875d1c267
```

## Symbol Recovery

The biggest accelerator was this command:

```bash
nm -C '项目2(1).exe'
```

It exposed the main gameplay functions:

```text
InitializePocket()
InitializeBalls()
PlaceWhiteBall(tagPOINT)
DrawCue(HDC__*, tagPOINT, tagPOINT)
RenderTable(HDC__*, int, int)
Render(HDC__*)
UpdateBallPositions()
HandleCollisions()
HandlePocketing()
AreBallsStopped()
RobotHitBall()
WinMain
WndProc(HWND__*, unsigned int, unsigned long long, long long)
```

It also exposed globals such as:

```text
balls
pockets
cueStart
cueEnd
isAiming
isPlacingWhiteBall
isRobotTurn
gameStarted
playerScore
robotScore
tableOffsetX
tableOffsetY
```

With those names available, the job became a focused reconstruction of each known function rather than broad binary exploration.

## Data Model Reconstruction

Disassembly showed that each ball was stored in a vector of structs. By following offsets, the struct layout was recovered:

```text
offset 0x00: position.x float
offset 0x04: position.y float
offset 0x08: velocity.x float
offset 0x0c: velocity.y float
offset 0x10: mass float
offset 0x14: radius float
offset 0x18: COLORREF color
offset 0x1c: bool pocketed
offset 0x20: int score
```

That maps cleanly to:

```cpp
struct Ball {
  Vec2 position;
  Vec2 velocity;
  float mass;
  float radius;
  int color;
  bool pocketed;
  int score;
};
```

## Constants Recovered

Important constants were visible as immediate values or `.rdata` floats:

```text
table width: 840
table height: 440
inner table: 800 x 400
ball radius: 10
pocket radius: 15
friction: 0.9919999837875366
stop threshold: 0.03400000184774399
player shot speed: 10
robot shot speed: 9
placement circle radius: 100
```

Pocket positions:

```text
(20, 20)
(420, 20)
(820, 20)
(20, 420)
(420, 420)
(820, 420)
```

Initial ball layout:

```text
white:  (220, 240), score 0
blue:   (420, 220), score 5
yellow: (220, 320), score 2
green:  (220, 120), score 3
brown:  (220, 220), score 4
pink:   (600, 220), score 6
black:  (640, 220), score 7
reds:   triangle from (660, 220), score 1 each
```

The red triangle used:

```text
x = 660 + row * 17.3205089569
y = 220 - row * 10 + col * 20
row = 0..4
col = 0..row
```

## Rendering Recovery

The renderer was simple because it used GDI calls directly.

Recovered visual elements:

- Brown brick background made from repeated rectangles.
- Green table interior.
- Brown rails.
- Six black pockets.
- White baulk line and D arc.
- Colored balls as filled circles.
- Cue line when aiming.
- HUD text with Arial 24:
  - player score
  - robot score
  - current turn

Win32 `COLORREF` uses byte order `0x00bbggrr`, so colors had to be converted carefully when porting to CSS, Pygame, and SDL:

```cpp
SDL_Color colorRef(int value) {
  return {
    static_cast<Uint8>(value & 0xff),
    static_cast<Uint8>((value >> 8) & 0xff),
    static_cast<Uint8>((value >> 16) & 0xff),
    255,
  };
}
```

## Input And Turn Logic

The Windows message procedure revealed the controls:

- `WM_MOUSEMOVE`: preview white-ball placement, or update cue endpoint while aiming.
- `WM_LBUTTONDOWN`: confirm initial white-ball placement.
- `WM_RBUTTONDOWN`: start aiming if all balls are stopped and it is the player's turn.
- `WM_LBUTTONUP`: shoot if aiming.
- `WM_TIMER`: update physics and trigger robot shot.

Desktop controls in the port preserve this:

- Move mouse to preview white-ball placement.
- Left click to confirm placement.
- Right click to start aiming.
- Move mouse to adjust cue.
- Left release to shoot.

Touch controls were added as a platform adaptation:

- Drag to aim.
- Release to shoot.

That keeps the same gameplay model while making the game usable on touch devices.

## Physics Recovery

The physics loop was straightforward:

1. Add velocity to position.
2. Apply friction.
3. Bounce against table walls.
4. Snap velocity to zero under the stop threshold.
5. Resolve ball-ball collisions.
6. Check pocketing.
7. If robot turn and all balls stopped, robot shoots.

Wall bounds:

```text
left   = 20 + radius
right  = 20 + 800 - radius
top    = 20 + radius
bottom = 20 + 400 - radius
```

Ball collisions used normal/tangent decomposition and the standard 1D elastic collision formula along the collision normal. Since all masses are `1`, this effectively swaps the normal velocity components.

## Pocket And Scoring Behavior

Pocketing used:

```text
distance(ball.position, pocket.position) < 15
```

If the white ball is pocketed, the original game resets:

- all balls
- player score
- robot score
- turn back to player
- white-ball placement mode

The scoring logic is counterintuitive but preserved:

- If `isPlayerTurn` is true, pocketed colored balls add to `robotScore`.
- If `isRobotTurn` is true, pocketed colored balls add to `playerScore`.

This works with the original turn flip timing:

- After the player shoots, `isRobotTurn` remains true while balls move, so balls pocketed by the player's shot count for the player.
- When the robot shoots, the game immediately flips back to `isPlayerTurn`, so balls pocketed by the robot shot count for the robot.

The port preserves this behavior instead of "fixing" it, because the goal was matching gameplay.

## Robot Logic

The robot logic was simple:

1. Build a list of all non-pocketed non-white balls.
2. Pick one at random.
3. Aim the white ball directly at that target.
4. Apply velocity with speed `9`.

No path planning or shot evaluation was present in the original.

## Porting Strategy

The fastest path was to first preserve behavior, then improve project shape.

### Step 1: Web Canvas Prototype

The Canvas version was useful because:

- no build system was needed
- the GDI draw calls mapped naturally to Canvas 2D calls
- it allowed quick visual and gameplay verification
- it provided mobile/browser fallback

Files:

```text
web/index.html
web/src/game.js
web/src/styles.css
```

### Step 2: Python/Pygame Local Prototype

The Pygame version made it easy to test SDL-style local window behavior quickly before the C++ toolchain was installed.

Files:

```text
native/pygame/snooker_pygame.py
native/pygame/requirements.txt
```

### Step 3: C++/SDL2 Main Version

The final native version is C++/SDL2 because it is a better long-term fit:

- close to the original Win32 C++ implementation
- real desktop binaries
- Windows/macOS/Linux support
- simple event loop
- no browser dependency

Files:

```text
native/sdl/CMakeLists.txt
native/sdl/src/main.cpp
```

## Build And Environment

The local machine did not initially have `g++`, `cmake`, `pkg-config`, or SDL2 development headers. Since `sudo` required a password, the build environment was installed inside the project with micromamba:

```text
.tools/bin/micromamba
.cpp-env
```

The Makefile now uses that environment:

```bash
make
make run
make smoke
```

If the environment is missing, `scripts/bootstrap-cpp-env.sh` recreates it.

## Verification

Local checks performed:

```bash
make smoke
SDL_VIDEODRIVER=dummy ./scripts/run-pygame.sh --smoke-test
node --check web/src/game.js
python3 -m py_compile native/pygame/snooker_pygame.py
```

The GitHub repository also includes CI for:

- Ubuntu
- macOS
- Windows

Workflow:

```text
.github/workflows/build.yml
```

## Repository Hygiene

The public repo intentionally excludes:

- original `.exe`
- local build output
- virtual environments
- micromamba environment files

The `.gitignore` covers:

```text
.cpp-env/
.micromamba/
.tools/
.venv/
build/
*.exe
项目2(1).exe
```

This keeps the repository focused on source code and documentation.

## What Made The Difference

The speed came from combining four things:

1. Fast binary triage with `file`, `strings`, `nm`, and `objdump`.
2. Surviving C++ symbols and global names.
3. A small procedural Win32/GDI game with no asset pipeline.
4. Direct behavioral porting instead of trying to recover the original source verbatim.

In short: I did not "magically decompile" the game. I identified the executable's structure, recovered the gameplay state machine and constants, then rewrote the game in cross-platform runtimes while preserving behavior.
