# DendyForge

A modern Dendy/NES emulator written in C++20.

## Features

- Written from scratch
- Modern C++20
- SDL3 frontend
- Cross-platform
- Clean architecture
- Accurate 6502 emulation
- Mapper support
- Open source

## Building on Windows with MinGW

Install SDL3 in the same MinGW environment as the compiler:

```powershell
C:\msys64\usr\bin\pacman.exe -S mingw-w64-x86_64-sdl3
```

Then build and run the PPU frame-buffer window:

```powershell
cmake --build --preset mingw-clang-debug
.\out\build\mingw-clang-debug\DendyForgeApp.exe
```

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

- ⬜ Pass Klaus Dormann 6502 Functional Test
- ✅ Pass nestest.nes (reference trace: PC, registers, and cycles)
- ⬜ Pass Blargg CPU Tests
- ⬜ Verify cycle accuracy against reference documentation

### Undocumented Instructions

- ⬜ Implement unofficial (illegal) opcodes
- ⬜ Match original CPU6502 hardware behavior
- ⬜ Validate unofficial opcode behavior

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
* 🟡 Scanline rendering timing
* 🟡 VRAM
* ✅ Pattern Tables
* ✅ Name Tables
* ✅ Palette RAM
* 🟡 Background Rendering
* ✅ OAM DMA ($4014)
* 🟡 Sprite Rendering
* 🟡 Scrolling
* ✅ VBlank and NMI signal
* 🟡 Sprite Zero Hit

### Controller

* ✅ Controller Port
* ✅ Input Latching

Controller 1 keyboard mapping: `W/A/S/D` — D-pad, `Backspace` — Select,
`Enter` — Start, `K` — A, `L` — B.

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
* 🟡 Game Loop

### Debugger

* ⬜ CPU Registers
* ⬜ Memory Viewer
* ⬜ Disassembler
* ⬜ Breakpoints
* ⬜ Step Execution
* ⬜ PPU Viewer
* ⬜ Pattern Table Viewer
* ⬜ Nametable Viewer

---

## Phase 5 — Mapper Support

* ✅ Mapper 2 (UxROM)
* ⬜ Mapper 3 (CNROM)
* ✅ Mapper 1 (MMC1)
* ⬜ Mapper 4 (MMC3)

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
* ⬜ Duck Hunt (without Zapper)
* ⬜ Mega Man
* ⬜ Kirby's Adventure

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
