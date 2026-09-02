# DendyForge

A modern Dendy/NES emulator written in C++20.

## Features

- Written from scratch
- Modern C++20
- SDL3 + Dear ImGui frontend
- Cross-platform
- Clean architecture
- Accurate 6502 emulation
- Mapper support
- Open source

## Building on Windows with MinGW

Install SDL3 in the same MinGW environment as the compiler:

```powershell
C:\msys64\usr\bin\pacman.exe -S mingw-w64-x86_64-sdl3 mingw-w64-x86_64-openssl
```

Then build and run the game library:

```powershell
cmake --build --preset mingw-clang-debug
.\out\build\mingw-clang-debug\DendyForgeApp.exe
```

Put personal ROM files in `roms/library/`. The library scans subfolders and
shows each `.nes` file as a playable tile. To show a real cover, place a PNG,
JPG, or JPEG with the same base filename next to the ROM, or put it under
`roms/library/covers/`. The ROM folder remains ignored by Git. Passing a ROM
path on the command line still starts it directly; press `Esc` in a game to
return to the library.

For ROMs declaring battery-backed RAM in their iNES header, DendyForge loads
and saves a `.sav` file next to the ROM. Saves are written when leaving a game
or closing the app; the write replaces the old file atomically. This enables
normal progress saving in games such as Kirby's Adventure and The Legend of
Zelda.

To fetch missing covers automatically, create a personal API key at
[TheGamesDB](https://api.thegamesdb.net/key.php), open **Settings → Cover
service**, paste it into the `TheGamesDB API key` field, click **Save
settings**, then **Download missing covers**. The key and cached artwork stay
only under `roms/library/`, which Git ignores.

## Roadmap

# DendyForge Roadmap

## Phase 1 — Foundation

### Core

* ✅ iNES Reader
* ✅ Cartridge
* ✅ Mapper Interface
* ✅ Mapper 0 (NROM)
* ✅ Connect Cartridge to Mapper
* ✅ Bus

---

## Phase 2 — CPU 6502 Emulator

**Goal:** Develop a complete, reusable, and hardware-accurate CPU6502 emulator independent of the Dendy.

### CPU Core

- ✅ CPU Registers (A, X, Y, SP, PC)
- ✅ Processor Status Register
- ✅ Bus Interface
- ✅ Standalone CPU6502 library
- ✅ Shared public Forge6502 library consumed as a pinned submodule
- ✅ Stack Operations
- ✅ Reset Sequence
- ✅ Instruction Fetch
- ✅ Instruction Decode 
- ✅ Instruction Execute
- ✅ IRQ Handling
- ✅ NMI Handling
- ✅ BRK / RTI
- ✅ Clock Cycle Emulation

### Addressing Modes

- ✅ Implement all official addressing modes
- ✅ Zero-page wrapping
- ✅ Relative addressing
- ✅ Indirect JMP hardware bug
- ✅ Page-crossing cycle penalties
- ✅ Cycle-accurate address calculation

### Official Instruction Set

- ✅ Implement all 56 official instructions (mnemonics)
- ✅ Implement all 151 official opcodes
- ✅ Correct processor flag behavior
- ✅ Correct cycle timing at instruction level
- ✅ Accurate branch timing
- ✅ Accurate interrupt timing

### Validation

- ✅ Pass Klaus Dormann 6502 Functional Test
- ✅ Pass nestest.nes (reference trace: PC, registers, and cycles)
- ✅ Pass Blargg CPU Tests
- ✅ Pass Blargg instruction timing, dummy-read, and reset ROMs
- ⬜ Verify cycle accuracy against reference documentation

### Undocumented Instructions

- ✅ Implement unofficial (illegal) opcodes
- ✅ Validate BRK/NMI vector hijack and reset bus sequences
- ⬜ Match original CPU6502 hardware behavior
- ✅ Validate unofficial opcode behavior

### Result

A standalone, reusable, and hardware-accurate CPU6502 emulator suitable for integration into future projects, including:

- Dendy
- Apple II
- Commodore 64
- Atari 2600 / 5200 / 7800
- Other CPU6502-based systems

---

## Phase 3 — NES Hardware

### PPU

* ✅ PPU register interface ($2000-$2007)
* ✅ CPU-to-PPU clock synchronization
* ✅ PPUMASK rendering controls
* ✅ Scanline rendering timing (blargg vbl_nmi_timing 7/7)
* ✅ VRAM (blargg vram_access, ppu_read_buffer)
* ✅ Pattern Tables
* ✅ Name Tables
* ✅ Palette RAM (blargg palette_ram, power_up_palette)
* ✅ Background Rendering (per-dot fetch/shifter pipeline)
* ✅ OAM DMA ($4014; real 512-cycle transfer)
* ✅ Sprite Rendering (blargg sprite_overflow 5/5, oam_read, oam_stress)
* ✅ Scrolling
* ✅ VBlank and NMI signal (NMI line sampled per CPU cycle; vbl_nmi_timing 7/7)
* ✅ Sprite Zero Hit (blargg sprite_hit_tests 11/11)
* ✅ PPU Open Bus (blargg ppu_open_bus)
* ✅ AccuracyCoin hardware accuracy suite (141/141)

### Controller

* ✅ Controller Port
* ✅ Input Latching
* ✅ Zapper (mouse aim, left button trigger, port 2 `$4017`; crosshair shown with `--zapper`)

Controller 1 defaults to `W/A/S/D` — D-pad, `Backspace` — Select, `Enter` —
Start, `K` — A, `L` — B. Change any mapping in **Settings → Controls**: click
the corresponding part of the drawn Dendy controller, press a keyboard key,
then click **Save settings**.

The library keeps each cover's original proportions, including horizontal box
art. Its interface uses the bundled Jura font (SIL Open Font License 1.1;
license in `assets/fonts/Jura-OFL.txt`).

### APU

* ✅ Pulse Channels (duty/envelope/sweep/length; blargg APU suite)
* ✅ Triangle Channel (timer/linear counter/length; blargg APU suite)
* ✅ Noise Channel (LFSR/envelope/length; blargg APU suite)
* ✅ DMC Channel (sample fetch/loop/IRQ/CPU stall; blargg APU suite)
* ✅ Audio Mixer (nonlinear 2A03 mix; `apu_mixer` ROM suite)

---

## Phase 4 — Emulator Integration

### Frontend

* ✅ SDL3 Window
* ✅ Frame Buffer Renderer
* ✅ Audio Output
* ✅ Keyboard Input
* ✅ Dear ImGui game library (searchable ROM tiles, automatic TheGamesDB cover cache,
  settings, and visual configurable controller mapping)
* 🟡 Game Loop

### Debugger

* ✅ CPU Registers (A/X/Y/SP/PC, opcode, cycles, flags)
* ✅ Memory Viewer (side-effect-free CPU-bus observation)
* ✅ Disassembler (all 256 opcode descriptions)
* ✅ Breakpoints (CPU instruction addresses)
* ✅ Step Execution (whole Console instruction step, including DMA/interrupt timing)
* ⬜ PPU Viewer
* ⬜ Pattern Table Viewer
* ⬜ Nametable Viewer

While a game is running, press `F1` to open the debugger. It pauses on the
next instruction boundary; `F5` resumes and `F10` executes one instruction.
The memory panel deliberately shows controller/APU I/O reads as `--` instead
of invoking their side effects.

---

## Phase 5 — Mapper Support

* ✅ Mapper 2 (UxROM)
* ✅ Mapper 3 (CNROM)
* ✅ Mapper 1 (MMC1)
* ✅ Mapper 4 (MMC3)
* ✅ Mapper 7 (AxROM)

---

## Phase 6 — Compatibility

### Games

* ✅ Battle City
* ✅ Bomberman
* ✅ Jackal
* ✅ Pac-Man
* ✅ Super Mario Bros.
* ✅ Contra
* ✅ Mega Man
* ✅ Tetris
* ✅ Ninja Gaiden
* ✅ Duck Hunt (with Zapper)
* ✅ Kirby's Adventure

### Test ROMs

* ⬜ Blargg Test Suite
* ⬜ NES Test ROM Collection

---

## Phase 7 — Platform Support

* 🟡 Linux
* 🟡 Windows

### Libretro

* ⬜ Libretro Core
* ⬜ RetroArch Integration

### Android

* ⬜ Android application

---

# DendyForge v1.0

Release requirements:

* Complete CPU6502 emulation
* Complete NES hardware emulation
* Accurate Mapper support
* SDL3 frontend
* Libretro Core
* Stable performance
* Major commercial games fully playable
* Comprehensive test suite passing


## Project Goals

- Learn how the Dendy/NES hardware works
- Build a maintainable emulator
- Keep the code simple and readable
- Run commercial and homebrew ROMs

## Status

🚧 Under development
