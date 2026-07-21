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

**Goal:** Develop a complete, reusable, and hardware-accurate MOS 6502 CPU emulator independent of the Dendy.
The resulting CPU core should be reusable in future emulation projects (NES/Famicom, Apple II, Commodore 64, Atari systems, and more).

### CPU Core

- ✅ CPU Registers (A, X, Y, SP, PC)
- ⬜ Processor Status Register
- ⬜ Stack Operations
- ⬜ Bus Interface
- ⬜ Instruction Fetch / Decode / Execute
- ⬜ Reset Sequence
- ⬜ IRQ Handling
- ⬜ NMI Handling
- ⬜ BRK / RTI
- ⬜ Clock Cycle Emulation

### Addressing Modes

- ⬜ Implement all official addressing modes
- ⬜ Zero-page wrapping
- ⬜ Relative addressing
- ⬜ Indirect JMP hardware bug
- ⬜ Page-crossing cycle penalties
- ⬜ Cycle-accurate address calculation

### Official Instruction Set

- ⬜ Implement all 56 official instructions (mnemonics)
- ⬜ Implement all 151 official opcodes
- ⬜ Correct processor flag behavior
- ⬜ Correct cycle timing
- ⬜ Accurate branch timing
- ⬜ Accurate interrupt timing

### Validation

- ⬜ Pass Klaus Dormann 6502 Functional Test
- ⬜ Pass nestest.nes
- ⬜ Pass Blargg CPU Tests
- ⬜ Verify cycle accuracy against reference documentation

### Undocumented Instructions

- ⬜ Implement unofficial (illegal) opcodes
- ⬜ Match original MOS 6502 hardware behavior
- ⬜ Validate unofficial opcode behavior

### Result

A standalone, reusable, and hardware-accurate MOS 6502 emulator suitable for integration into future projects, including:

- Dendy
- Apple II
- Commodore 64
- Atari 2600 / 5200 / 7800
- Other MOS 6502-based systems

---

## Phase 3 — NES Hardware

### PPU

* ⬜ VRAM
* ⬜ Pattern Tables
* ⬜ Name Tables
* ⬜ Palette RAM
* ⬜ Background Rendering
* ⬜ Sprite Rendering
* ⬜ Scrolling
* ⬜ VBlank
* ⬜ Sprite Zero Hit

### Controller

* ⬜ Controller Port
* ⬜ Input Latching

### APU

* ⬜ Pulse Channels
* ⬜ Triangle Channel
* ⬜ Noise Channel
* ⬜ DMC Channel
* ⬜ Audio Mixer

---

## Phase 4 — Emulator Integration

### Frontend

* ⬜ SDL3 Window
* ⬜ Renderer
* ⬜ Audio Output
* ⬜ Keyboard Input
* ⬜ Game Loop

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

* ⬜ Mapper 2 (UxROM)
* ⬜ Mapper 3 (CNROM)
* ⬜ Mapper 1 (MMC1)
* ⬜ Mapper 4 (MMC3)

---

## Phase 6 — Compatibility

### Games

* ⬜ Battle City
* ⬜ Super Mario Bros.
* ⬜ Contra
* ⬜ Duck Hunt (without Zapper)
* ⬜ Mega Man
* ⬜ Kirby's Adventure

### Test ROMs

* ⬜ Blargg Test Suite
* ⬜ NES Test ROM Collection

---

## Phase 7 — Platform Support

* ⬜ Linux
* ⬜ Windows

### Libretro

* ⬜ Libretro Core
* ⬜ RetroArch Integration

---

# DendyForge v1.0

Release requirements:

* Complete MOS 6502 emulation
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
