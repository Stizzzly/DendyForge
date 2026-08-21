# DendyForge: working context for future sessions

This file is the authoritative handoff for work in this repository. Read it
before changing emulator code. It describes the code as it exists now, not an
aspirational design.

## Project purpose and language

DendyForge is a C++20 emulator for the Dendy/NES platform. The project is
deliberately split into reusable components. In particular, `CPU6502` must
remain a standalone CPU core with only the small `CpuBus` interface as its
hardware dependency. It must not acquire Dendy-specific registers, cartridge
logic, PPU knowledge, or controller logic. `Console` is the platform-specific
composition root and creates `CPU6502` with decimal arithmetic disabled.

Use the names already established in the code:

* `CPU6502` for the reusable CPU core.
* `CPU2A03` only when referring to the Dendy/NES configuration in prose.
* Do not introduce names for other CPU variants unless the user explicitly
  asks for them.

The user prefers incremental emulator development: one coherent change, tests,
commit, and push. Update `README.md` when a roadmap checkbox is genuinely
complete. The user has explicitly authorized committing and pushing each
logical change directly to `main`; do not create a feature branch for this
project unless they revoke that instruction.

## Repository map

```text
core/
  cpu/          reusable CPU6502 and CpuBus interface
  bus/          CPU address map, PPU register mapping, DMA, controller port
  console/      joins CPU, Bus, Cartridge; clocks CPU and PPU
  ppu/          PPU registers, memory and current software renderer
  controller/   serial controller-port implementation
  ines/         iNES header types and reader
  cartridge/    cartridge data plus mapper dispatch
  mapper/       mapper abstraction and Mapper 0
src/main.cpp    SDL3 window, event loop, texture upload and keyboard mapping
tests/          doctest tests and versioned CPU test ROM fixtures
roms/           user-local game ROMs; ignored by Git
```

`CMakeLists.txt` builds:

* `DendyForgeCpu`: only `core/cpu/cpu6502.cpp`.
* `DendyForgeCore`: CPU plus cartridge, bus, console, controller, mapper, and
  PPU.
* `DendyForgeTests`: doctest executable.
* `DendyForgeApp`: SDL3 frontend executable.

## Build and test on this machine

The supported Windows configuration is MinGW64 + Clang + Ninja via CMake
presets. SDL3 must be installed in that MinGW environment:

```powershell
C:\msys64\usr\bin\pacman.exe -S mingw-w64-x86_64-sdl3
cmake --preset mingw-clang-debug
cmake --build --preset mingw-clang-debug
ctest --test-dir out/build/mingw-clang-debug --output-on-failure
```

The active build directory is `out/build/mingw-clang-debug`. Do not rely on
the legacy `build/` directory: it can contain a cache made in another
environment (`/home/...`) and is not a valid Windows build tree.

Run a local ROM by passing its path as the first argument. The ROM is not
loaded merely by placing it in `roms/`:

```powershell
.\out\build\mingw-clang-debug\DendyForgeApp.exe ".\roms\game.nes"
```

With no argument, the app shows a generated PPU demo frame. A blank/checkered
window while testing a game commonly means the executable was launched without
the ROM path.

Tests use doctest. CPU ROM fixtures are intentionally versioned under
`tests/cpu/roms/`; arbitrary `*.nes` files and `roms/` are ignored. The
temporary nestest trace `tests/cpu/roms/nestestres.log` is ignored.

## Current architecture and data flow

```text
SDL3 main loop
  -> Console::Clock()
       -> CPU6502::Clock() once
       -> Bus::ClockPpu() three times
       -> deliver a pending PPU NMI to CPU6502::NMI()
  -> at completed PPU frame: SDL texture upload + RenderPresent

CPU6502 -> CpuBus (implemented by Bus)
Bus -> CPU RAM / PPU registers / controller / Cartridge
Cartridge -> Mapper -> PRG ROM or CHR ROM/RAM mapping
```

`Bus` owns the PPU and controller. `Console` owns `Bus`, `CPU6502`, and the
loaded `Cartridge`. `Console::LoadRom()` parses iNES, constructs the cartridge,
inserts it into the bus, and resets the CPU. The current PPU receives the
cartridge from `Bus::InsertCartridge()`.

## CPU6502 status

The CPU has all 56 official mnemonics and all 151 official opcodes. Its normal
instruction-level cycle accounting, page-cross penalties, branch timing,
interrupt handling, decimal-mode configuration, and the JMP indirect wrapping
behavior are covered by tests. The local `nestest` trace test has passed for
PC, registers, and cycle count. It is intentionally instruction-timed, not
yet a micro-operation/cycle-accurate CPU implementation.

Important CPU rules:

* Preserve modularity. `cpu6502.hpp/.cpp` may depend on `cpu_bus.hpp`, never
  on `Bus`, `PPU`, `Console`, SDL, iNES, or a mapper.
* `CPU6502::Configuration::decimalModeEnabled` exists because the standalone
  core supports decimal arithmetic. `Console` passes `false`; its D flag can
  exist but arithmetic remains binary.
* Do not change CPU code while fixing PPU rendering unless a failing,
  evidence-backed CPU issue requires it.

## Cartridge, mapper, and bus status

Mapper 0 and Mapper 2 (UxROM/UNROM) are implemented. Mapper 0 maps 16 KiB or
32 KiB PRG. Mapper 2 selects a 16 KiB PRG bank at `$8000-$BFFF` and fixes the
last PRG bank at `$C000-$FFFF`. Both use CHR ROM when supplied and otherwise
provide 8 KiB of CHR RAM. No other mapper must be assumed to work.

The current CPU map implemented by `Bus` is:

* `$0000-$1FFF`: 2 KiB internal RAM, mirrored by `address & $07FF`.
* `$2000-$3FFF`: PPU register interface, mirrored every eight bytes.
* `$4014`: immediate 256-byte OAM DMA copy with a 513/514-cycle CPU stall,
  selected by CPU-cycle parity. The PPU continues to clock during that stall.
* `$4016`: controller 1 serial port.
* cartridge reads/writes are tried first, then the built-in map.
* `$4000-$4013` and `$4015`: initial APU pulse/triangle/noise/DMC
  implementation. It produces 44.1 kHz mono PCM samples; `$4017` is not yet
  implemented as an APU register. `$4014` remains OAM DMA and `$4016` remains
  controller 1.

Controller order is A, B, Select, Start, Up, Down, Left, Right. A write to
`$4016` latches it on strobe or the high-to-low transition; reads shift one bit
and then return ones. SDL maps `W/A/S/D` to D-pad, Backspace to Select, Enter
to Start, K to A, and L to B.

## PPU: what is implemented

The PPU currently has a useful renderer, but it is not cycle-accurate.

Implemented and tested foundations:

* CPU-visible registers `$2000-$2007`, including mirrored access through Bus.
* `$2002` status read side effects: VBlank clear, NMI pending clear, write
  latch clear.
* `$2005` two-write scroll state; `$2006` two-write PPU address; `$2007`
  buffered reads, palette-read behavior, and address increment by 1 or 32.
* Pattern-table reads from cartridge CHR with 8 KiB fallback CHR RAM.
* 2 KiB name-table RAM, horizontal/vertical mirroring and `$3000-$3EFF`
  mirroring.
* 32-byte palette RAM and special palette mirroring (`$3F10/$14/$18/$1C`).
* PPUMASK background/sprite enable, left-edge clipping, grayscale and simple
  colour-emphasis transform.
* OAM registers, immediate `$4014` DMA, 8x8 and 8x16 sprites, sprite flips,
  palette selection, sprite/background priority, a rough sprite-zero-hit
  condition, and an 8-sprite-per-scanline limit with overflow flag.
* 256x240 ARGB framebuffer.
* PPU timing counters: scanline `-1` pre-render through `260`, cycle `0..340`.
  Visible pixels are written at visible scanlines, cycles `1..256`. VBlank is
  raised at scanline 241, cycle 1; an enabled NMI becomes pending there.
* `m_frameComplete` is set at that same VBlank point. `ConsumeFrameComplete()`
  returns the event once and clears it. The frontend uses this to upload and
  present only whole frames, never the framebuffer while visible scanlines are
  being drawn.

`main.cpp` intentionally clocks in batches of 1,000 console clocks to avoid an
event-loop-dependent one-clock UI. This is only a frontend scheduling choice;
presentation is still gated by `ConsumeFrameComplete()`. Do not reintroduce a
texture upload and `SDL_RenderPresent()` on every outer loop iteration.

## PPU: known gaps exposed by Super Mario Bros.

The screenshot with a mostly correct HUD, ground, and player but missing or
misplaced world elements is expected for the current implementation. It proves
that ROM loading, CPU execution, basic background/sprite rendering, palette,
and controller input are alive. It does **not** mean the game is supported.

The renderer has live `v`, `t`, and fine-X scrolling; coarse-X/Y progression;
background name-table/attribute/pattern fetch latches and 16-bit shifters; and
a scanline sprite pipeline. It is still not fully cycle-accurate:

* Mid-frame `$2000/$2005/$2006/$2007` races and split-scroll timing remain
  approximate.
* Sprite evaluation/fetch is scanline-based, but does not yet reproduce the
  hardware overflow bug, per-cycle OAM accesses, or exact sprite-zero timing.
* Background/sprite dummy fetches and bus-visible PPU timing are absent.
* The odd-frame skipped cycle and OAM DMA CPU stall are modeled, but PPU
  open-bus behavior and many VBlank/NMI timing races are still not.

These are the main reason a simple Mapper 0 game can be playable while a more
demanding scrolling game visibly breaks apart. Do not mark Super Mario Bros.
as supported until it is actually stable in normal play.

## Recommended next PPU plan

Work in small, independently testable commits. The order below is intentional:

1. Tighten the existing background and sprite pipelines: split-scroll register
   races, sprite overflow behavior, and exact sprite-zero timing.
2. Add remaining bus-visible timing: dummy fetches, PPU open bus, and VBlank/
   NMI edge cases.
3. Validate with small PPU test ROMs and real-game checkpoints; only then
   update README roadmap checks.

## PPU handoff point

PPU work is intentionally paused here while APU development begins. The last
completed PPU layers are background fetch latches/shifters, scanline sprite
selection/fetch, odd-frame skipping, VBlank frame presentation, and OAM DMA
CPU stalls. Resume from the three-item PPU plan above; do not reopen the
background renderer without a focused regression test and a Mario checkpoint.

## Recommended APU plan

`APU` is a standalone component owned by `Bus`, while SDL audio remains in the
frontend. Pulse channels, including envelope/sweep units, triangle, noise, DMC,
and a bounded SDL-fed sample queue are in place. Continue with the accurate
frame counter and NES nonlinear mixer. Test register writes, timers, envelopes,
DMA, and sample generation before claiming that a game has
accurate sound.

## APU: current status

`Bus` owns `APU`, which is clocked once per CPU cycle. The current component
implements pulse channels 1 and 2, triangle, noise, and DMC with
duty/waveform sequences, timer periods, length counters, pulse/noise envelope
units, triangle linear counter, DMC sample fetches/CPU stalls/IRQ, `$4015`
enables/status, and a bounded 44.1 kHz PCM sample queue. `main.cpp` sends that
queue to an SDL3 audio stream. This is intentionally an initial audible layer,
not an accurate 2A03 APU: exact frame-counter modes, nonlinear mixing, and
frame IRQs remain absent.

When choosing between a broad rewrite and a small change, preserve the existing
public PPU interface where possible and land the smallest test-backed layer.
It is acceptable to add private structures for rendering state. Avoid adding
Dendy-specific behavior to `CPU6502` to work around PPU issues.

## Test guidance

* Keep unit tests beside the component: `tests/ppu/ppu_tests.cpp`,
  `tests/controller/controller_tests.cpp`, `tests/bus/bus_tests.cpp`, etc.
* New PPU behavior needs deterministic framebuffer/register assertions. A game
  screenshot is excellent integration evidence but not a replacement for a
  test.
* Run the full CTest command after C++ changes. For documentation-only changes,
  a build is not required.
* Preserve the `nestest` fixtures and the ignored generated trace. The CPU
  regression coverage is valuable even during PPU work.

## Git and workspace hygiene

* Check `git status --short` before editing and before staging. The worktree
  may contain user changes; never overwrite or stage unrelated files.
* Use `apply_patch` for source/documentation edits. Stage exact file paths;
  never use `git add .` or `git add -A`.
* Do not use `git reset --hard` or destructive checkout. If a commit must be
  undone on shared `main`, use `git revert` so history remains intact.
* Commit each logical layer and push it to `origin main` immediately after its
  tests pass. Use clear imperative commit subjects.
* `out/`, `build/`, local ROMs, and generated logs are not source changes.
  Do not commit them.

## README discipline

`README.md` is both the roadmap and the project-facing status. It currently
marks SDL3 window, framebuffer renderer, keyboard input, controller port,
input latching, and Battle City as complete. Game Loop, most PPU areas, APU,
additional mappers, debugger, Libretro, and Android remain unfinished. Update
checkboxes only after the corresponding capability is implemented and verified.
