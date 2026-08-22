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

The active build directory for tests is `out/build/mingw-clang-debug`. Do not
rely on the legacy `build/` directory: it can contain a cache made in another
environment (`/home/...`) and is not a valid Windows build tree.

**Playing games requires the Release build.** Measured on the project machine
(2026-08-22, Ryzen 5 7530U): the Release core sustains about 2.8x realtime
(~5 M console clocks/s), while the Debug core (`-O0`) sustains only ~0.9x
realtime and cannot hold full speed — games stutter and audio starves. Run:

```powershell
.\out\build\mingw-clang-release\DendyForgeApp.exe ".\roms\game.nes"
```

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
behavior are covered by tests. The `nestest` golden-trace regression is
committed as `tests/cpu/nestest_trace_tests.cpp` and passes for PC, operand
bytes, registers, and cycle count against `tests/cpu/roms/nestest.log`.

The cycle-accurate conversion (see `CPU_CYCLE_ACCURACY_PLAN.md`) completed
all six phases on 2026-08-22. Phases 1-5 make read-type, write-type,
immediate, implied and both JMP classes
execute one bus transaction per cycle with the hardware dummy reads
(zp,X/(zp,X) read the unindexed base; abs,X/abs,Y/(zp),Y read the unfixed
address when crossing); read-modify-write, stack (PHA/PHP/PLA/PLP),
subroutine (JSR/RTS/RTI) and BRK instructions execute per cycle with the
NMOS read -> write-old -> write-new pattern and dummy stack reads;
branches execute per cycle (not taken 2 cycles, taken 3 with a dummy fetch
of the next opcode address, taken crossing a page 4 with an internal
fix-up cycle); and hardware interrupt entry and reset run their seven
cycles per cycle (two dummy fetches at the interrupted PC, three stack
bytes, vector fetch; reset performs reads instead of pushes and
decrements SP three times). Interrupt lines are polled at the penultimate
cycle of an instruction, sampled as the final cycle begins, with the I
flag masking IRQ at the poll; `IRQ(bool line)` is level-triggered so a
dropped line clears a stale latch. Exact sequences are pinned by
`tests/cpu/cpu_bus_cycle_tests.cpp`.

The conversion is **complete** (Phases 1-6, one commit each for Phases 1-5).
Phase 6 verified Debug and Release CTest, the nestest trace, `apu_mixer` 4/4,
blargg APU timing 11/11, and the user gameplay regressions for Super Mario
Bros., Pac-Man, Contra and Jackal. Release performance was recovered with
cached address-mode dispatch and built-in bus-range fast paths: the standard
20 M-cycle Mapper 0 `nestest.nes` benchmark measured 11,944,313 console
clocks/s (6.67x realtime), above the 2.8x pre-conversion baseline. See
`CPU_CYCLE_ACCURACY_PLAN.md` for the full plan, test strategy and risks.

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
* `$6000-$7FFF`: 8 KiB cartridge PRG RAM (the iNES zero-size default is one
  8 KiB bank). This is required by common NES test ROM diagnostic ports.
* `$4000-$4013`, `$4015`, and `$4017`: initial APU pulse/triangle/noise/DMC
  implementation. It produces 44.1 kHz mono PCM samples; `$4014` remains OAM
  DMA and `$4016` remains controller 1.

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
the four-/five-step frame sequence, and a bounded SDL-fed sample queue are in
place. The nonlinear 2A03 mixer is also in place. Continue with remaining
frame-reset timing details and accuracy validation. Test register writes,
timers, envelopes, DMA, and sample generation before claiming that a game has
accurate sound.

## APU: current status

`Bus` owns `APU`, which is clocked once per CPU cycle. The current component
implements pulse channels 1 and 2, triangle, noise, and DMC with
duty/waveform sequences, timer periods, length counters, pulse/noise envelope
units, triangle linear counter, DMC sample fetches/CPU stalls/IRQ, four-/five-
step frame sequencing with frame IRQ, `$4015` enables/status, and a bounded
44.1 kHz PCM sample queue. `main.cpp` sends that queue to an SDL3 audio stream.
This is intentionally an initial audible layer, not a fully cycle-accurate 2A03
APU: hardware-level validation remains outstanding. The `$4017` frame reset
uses the 3/4-cycle delay and the 4-step/5-step mode distinction.

The SDL3 frontend keeps the device paused until a 50 ms PCM prebuffer has been
queued. It caps SDL's queued input at 100 ms and retains excess generated
samples locally instead of dropping them; this prevents startup underruns and
audio discontinuities during a transient queue backlog. The mixer was verified
on 2026-08-22 with Shay Green's `apu_mixer` ROM suite: `dmc.nes`, `noise.nes`,
`square.nes`, and `triangle.nes` each reported status code 0 (`passed`). The
ROMs remain local under `roms/nes-test-roms/apu_mixer`. Use
`DendyForgeApuMixerRunner` to run them headlessly; it creates a fresh console
per ROM, performs a requested reset after 100 ms, and reports the status/text
supplied in PRG RAM `$6000-$60FF`.

The mixer output passes through the standard NES-style 90 Hz and 440 Hz
high-pass filters plus a 14 kHz low-pass filter before reaching SDL. These
filters remove the DC component produced by the raw digital mixer and reduce
high-frequency aliasing; keep them in the APU rather than the SDL frontend.

The local `blargg_apu_2005.07.30` suite was re-run on 2026-08-22 after the
frame-sequencer rework and again after cycle-accurate CPU Phase 5: **all
eleven ROMs pass** (code 1) through `DendyForgeApuTimingRunner`, and the
`apu_mixer` suite still passes. The frame counter now models, in CPU
cycles relative to the application of a `$4017` write's delayed reset:
quarter/half events at 7456/14912/22370 and 29828 (four-step) or 37280
(five-step), the frame IRQ flag set on three consecutive cycles
29827-29829, a parity-dependent jitter offset (write on an odd CPU cycle
shifts all sequencer events two counter cycles later), a 1-cycle latency
from the flag's first set cycle to the IRQ line
(`FrameIrqLineLatencyCycles`, armed once and decremented before the
sequencer update), and half-frame clocks that sample halt state before the
same cycle's CPU write while a same-cycle length reload is ignored when
the counter is non-zero. `CPU6502` polls the interrupt lines at the
penultimate cycle of an instruction (sampled as the final cycle begins,
I flag masking IRQ at the poll) and services them through the seven-cycle
entry sequencer; `Console::Clock()` reports the level-triggered APU IRQ
line after the APU's end-of-cycle update. Remaining before marking channel
Roadmap items complete: the manual Contra listening regression.

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

## Operational appendix — current verified handoff (2026-08-22)

This appendix is newer than the narrative above. If a statement in the earlier
sections conflicts with this appendix, **this appendix takes precedence**. It
records the changes made during the PPU/APU work and the exact commands used to
verify them.

### Verified game-facing status

The following is a practical, user-confirmed regression list. It is not a
replacement for hardware conformance tests, but it is valuable when changing
timing-sensitive code.

| ROM/game | Current practical result | Meaning for future work |
| --- | --- | --- |
| Super Mario Bros. | Fully playable in the current build | Keep VBlank-only presentation; use as a scrolling PPU regression check. |
| Pac-Man | Fully playable | Useful simple rendering/input regression. |
| Contra | Fully playable and sound was audibly checked | Use to catch APU mixing, DMC and frontend-audio regressions. |
| Jackal | Fully playable | Its early-NMI boot VBlank handshake now completes after the cycle-accurate CPU conversion; no game-specific workaround was added. |
| Bomberman | Fully playable | Another simple-game rendering/input regression alongside Pac-Man (user-confirmed 2026-08-22). |

`README.md` must preserve these distinctions. In particular, it is valid to
describe a game as practically playable while PPU/APU items remain yellow.

### Current, non-negotiable frame presentation path

There was a user-visible regression where partial lines of Mario were shown
while the game scrolled, described as playing on a CRT. The cause was uploading
the PPU framebuffer before the frame had been completely rendered. The current
path is deliberately different:

1. `Console` advances CPU, APU and PPU in their clock relationship.
2. PPU enters VBlank on scanline 241 and raises its one-shot frame-complete
   event.
3. `main.cpp` consumes that event.
4. Only then does it call the SDL texture upload, clear, texture render and
   present sequence.

The frontend uses a real-time CPU-clock accumulator plus VSync. It must not
restore an unconditional `SDL_UpdateTexture`/`SDL_RenderPresent` in the outer
event loop and must not use a fixed arbitrary batch, such as 1,000 clocks, as
the decision to present. A batch may be an internal scheduling detail only if
the frame-complete boundary is still the sole upload boundary.

If the game runs too fast after a frontend change, inspect the accumulator and
ensure there is exactly one caller of `Console::Clock()`. Rendering a frame is
not permission to advance the emulated machine by another frame.

### Exact component layout and CMake targets

The earlier repository map is structurally correct; the following additions
are important for current work:

```text
core/apu/apu.hpp, core/apu/apu.cpp       APU implementation.
core/mapper/mapper2.hpp/.cpp             UxROM/UNROM implementation.
tests/apu/apu_tests.cpp                  APU unit coverage.
tools/apu_mixer_runner.cpp               Headless Shay Green mixer-ROM runner.
tools/apu_timing_runner.cpp              Headless blargg APU timing-ROM runner.
```

`CMakeLists.txt` builds both runner executables in addition to the library,
tests and SDL application:

- `DendyForgeApuMixerRunner`;
- `DendyForgeApuTimingRunner`.

Do not add test ROM binaries to a target as C++ sources. A runner receives
one or more `.nes` file paths (not directories) as command-line arguments.

### Console clocking and DMA invariants

`Console` owns the master scheduling relationship:

```text
one Console cycle
  ├─ CPU executes one cycle unless OAM/DMC has stolen it
  ├─ APU::Clock() once
  ├─ PPU::Clock() three times
  ├─ delivery of pending PPU NMI to CPU
  └─ update CPU-cycle parity
```

The two DMA mechanisms intentionally have different ownership and timing:

- Writing `$4014` triggers OAM DMA: 256 reads from the requested CPU page and
  256 OAM writes, then a CPU stall of 513 or 514 cycles according to parity.
  PPU and APU must continue to clock during this stall.
- DMC fetches one byte through the memory-reader callback installed by `Bus`.
  The APU publishes a four-cycle CPU stall through
  `ConsumeDmcDmaStallCycle()`. It is not an OAM DMA and must not copy OAM.

When fixing frame timing, avoid changing DMA behavior unless a failing test
demonstrates an interaction. A broad scheduler rewrite risks corrupting both
working PPU and already passing APU mixer tests.

### Cartridge and Mapper 2 handoff

Cartridge PRG-RAM is now used by diagnostic ROMs at `$6000-$7FFF`; the default
for an iNES header declaring zero PRG-RAM remains one 8 KiB bank. The core
currently supports only Mapper 0 and Mapper 2. Mapper 2 contract:

```text
$8000-$BFFF  selected 16 KiB PRG bank
$C000-$FFFF  permanently mapped final 16 KiB PRG bank
write $8000-$FFFF  selects the lower bank (masked to available banks)
```

Do not special-case Jackal by filename. First inspect its iNES header and
confirm the mapper/board, then implement the missing general hardware with
small mapping tests. Possible future cartridge work includes trainer handling,
NES 2.0 metadata, battery-backed PRG-RAM persistence and further mappers; none
should be implied by the current Mapper 0/2 support.

#### Jackal root-cause diagnosis (2026-08-22)

`Jackal (USA).nes`: iNES mapper 2 (UxROM), 128 KiB PRG, 8 KiB CHR RAM,
vertical mirroring. Loading and Mapper 2 work; the failure is not cartridge
hardware. Instrumented findings (temporary probes, since reverted):

* The CPU boots, the reset code at $C23D runs, deliberately re-enters the
  reset path once from `JSR $C369` (warm-boot signature at $07F0-$07FF), and
  then hangs forever in the boot VBlank handshake at $D0E0
  (`LDA $2002 / BPL $D0E0`). Rendering stays disabled, the screen is a solid
  backdrop color, and only banks 0 and 1 are ever selected.
* Measured over 2.5 M cycles (84 frames): exactly 84 reads of `$2002`
  returned bit 7 set — one per frame, all from the NMI handler's opening
  `LDA $2002` at $C298. The main thread's polling loop never once observed
  the flag.
* Mechanism: Jackal enables NMI early. When the VBlank flag sets, the NMI
  preempts the polling loop, and the handler clears the flag about 16 cycles
  later. Because the current `CPU6502` executes instructions atomically and
  performs bus reads on the FIRST cycle of an instruction, an in-flight
  `LDA $2002` cannot observe a flag that sets mid-instruction, and the
  deterministic frame lock means the poller's phase never lands inside the
  visibility window. On hardware the read happens on the instruction's final
  cycle and interrupt polling occurs at the instruction boundary, so the
  in-flight read wins the race and `BPL` falls through after `RTI`.
* Games that enable NMI only after their boot-time VBlank polling (Super
  Mario Bros., Contra) do not hit this race, which is why they work.

The fix was the cycle-accurate CPU (per-cycle bus transactions on their
hardware cycles plus the penultimate-cycle interrupt poll), completed as
Phases 2-5 of `CPU_CYCLE_ACCURACY_PLAN.md` on 2026-08-22. Phase 6 gameplay
regression confirmed that Jackal is fully playable. No Jackal-specific
workaround was added.

### PPU precise handoff point

The PPU is intentionally paused at a strong, usable baseline. It includes
background fetch/shifter state, live scroll registers, scanline sprite
selection/fetch, odd-frame behaviour, palette/nametable handling, OAM DMA,
VBlank/NMI and a whole-frame framebuffer event. Do not discard that pipeline
to pursue cycle accuracy.

The next PPU work, when it resumes, should be narrow and test-backed:

1. Mid-scanline `$2000/$2005/$2006/$2007` scroll/address races and split
   scrolling.
2. Hardware sprite-overflow bug and exact sprite-zero-hit windows.
3. Dummy fetch/open-bus effects and VBlank/NMI edge cases.

For each item: reproduce with a focused test ROM or unit test, make the
smallest possible change, run all unit tests, then manually regression-test
Mario, Pac-Man and Contra. Do not update the PPU Roadmap merely because a
single screenshot looks correct.

### APU public integration contract

`APU` is owned by `Bus`; SDL remains strictly in `src/main.cpp`. The relevant
public API is:

```cpp
void Reset();
void Clock();
std::uint8_t CpuRead(std::uint16_t address);
void CpuWrite(std::uint16_t address, std::uint8_t value);
void SetDmcMemoryReader(DmcMemoryReader reader);
bool ConsumeDmcDmaStallCycle();
bool IrqPending() const;
std::vector<float> TakeSamples();
```

The implemented audio features are pulse 1/2 duty/envelope/sweep/length,
triangle timer/linear/length, noise LFSR/envelope/length and DMC address,
length, sample-buffer fetch, loop, IRQ and CPU stall. `$4015` enables/status,
`$4017` four-/five-step mode and frame reset delay, nonlinear 2A03 mixing, and
90 Hz + 440 Hz high-pass / 14 kHz low-pass filters are also present.

The frame counter's CPU-cycle timing is validated by the full blargg suite
(all eleven ROMs pass). Keep the four channel Roadmap entries yellow only
until the manual Contra listening regression confirms no metallic artifacts;
the Audio Mixer entry is green.

### SDL3 audio contract

The app sends APU-generated float mono PCM at 44,100 Hz to `SDL_AudioStream`.
It starts the device paused, queues about 50 ms before resuming, caps the SDL
input queue near 100 ms and retains extra locally generated audio pending a
future feed rather than dropping it. This buffering avoids underrun pops and
the observed metallic sound artifacts.

If audio artifacts return, check in this order:

1. CPU accumulator rate and duplicate/missing `Console::Clock()` calls.
2. Whether SDL's stream is started only after its prebuffer is ready.
3. SDL queued bytes and the local pending sample count.
4. Whether a new change bypassed the APU filters or changed the sample format.
5. APU register/timing correctness with the ROM runners.

Do not solve clicks by discarding queued samples or permanently pausing and
clearing the stream. That creates discontinuities rather than removing their
cause. Consult current SDL3 documentation (Context7 can be used if available)
before changing SDL3 audio calls, then compile against the local installed SDL3.

### APU ROM-test protocol and recorded baseline

There are two distinct suites. Both require a freshly constructed console per
ROM; they must never share APU state between files.

#### Shay Green / `apu_mixer`

Local path:

```text
roms/nes-test-roms/apu_mixer
```

The suite publishes status in cartridge PRG-RAM: `$6000 == 0` means pass and
diagnostic text begins near `$6004`. The runner handles the fresh reset and
waits up to 30 seconds of emulated time.

```powershell
cmake --build --preset mingw-clang-release --target DendyForgeApuMixerRunner
& .\out\build\mingw-clang-release\DendyForgeApuMixerRunner.exe .\roms\nes-test-roms\apu_mixer\*.nes
```

Recorded result: `dmc`, `noise`, `square`, and `triangle` all pass with code
0. This is the evidence for the green **Audio Mixer** Roadmap item.

#### blargg / `blargg_apu_2005.07.30`

Local user-owned path:

```text
blargg_apu_2005.07.30
```

Read `readme.txt` and `tests.txt` in the suite before modifying behaviour.
The runner detects the stable final CPU loop and reads CPU RAM `$00F0`; value
`1` is pass. Its timeout is five seconds of emulated time per ROM.

```powershell
cmake --build --preset mingw-clang-release --target DendyForgeApuTimingRunner
& .\out\build\mingw-clang-release\DendyForgeApuTimingRunner.exe .\blargg_apu_2005.07.30\*.nes
```

The current baseline (2026-08-22, re-verified after cycle-accurate CPU
Phase 5): **all eleven ROMs report code 1 (pass)**, including
`08.irq_timing`, which had regressed to code 3 after the per-cycle CPU
phases moved the effective interrupt poll point. The Phase 5 fix combined
the penultimate-cycle poll inside the CPU, level-triggered `IRQ(bool
line)`, a 1-cycle `FrameIrqLineLatencyCycles` armed only on the flag's
first set cycle (decremented before the sequencer update), and reporting
the line after the APU's end-of-cycle update in `Console::Clock()`.
Before the frame-sequencer rework, `04`-`08`, `10` and `11` failed with
the codes recorded in git history.

The suite's own `tests.txt` declares dependencies between tests. Therefore
later isolated passes do not prove full conformance once an earlier timing test
fails.

### Next APU implementation plan

The frame-sequencer timing work is complete (all blargg timing ROMs pass,
see the status section above). Remaining follow-up:

1. Manually regression-test Contra and listen for the formerly metallic
   artifacts; only then mark the channel Roadmap items in `README.md`.
2. If audio artifacts appear, check the frame IRQ path first: the CPU
   polls the interrupt lines at each instruction's penultimate cycle, and
   the frame IRQ line uses `APU::FrameIrqLineLatencyCycles` (1) cycle of
   latency after the `$4015`-visible flag's first set cycle.

### Diagnostics and runner-writing rules

`Console::ReadCpuRamForDiagnostics(address)` is intentionally limited to CPU
RAM (`$0000-$1FFF`); `ReadCartridgeRamForDiagnostics(address)` is limited to
PRG-RAM (`$6000-$7FFF`). They exist only for runner observability. Production
emulation must still go through the normal bus.

For every new test ROM runner:

1. Read the suite documentation and, if available, its source.
2. Record the status address, pass code, reset requirement and terminal-loop
   protocol in code comments and this file.
3. Use emulated-cycle/time limits, never a machine-speed wall-clock timeout.
4. Use a clean `Console` for every ROM.
5. Surface status and diagnostic text in stdout so CI and humans can compare it.

### Linux/AppImage handoff

There is a separate WSL Debian checkout at `/home/aleksandr/DendyForge`; it can
be dirty or on a different commit from the Windows checkout. Treat it as a
separate working copy and verify its `git status`, branch and commit before
building. A prior manual artifact exists at:

```text
/home/aleksandr/DendyForge/DendyForge-x86_64.AppImage
```

On NixOS, a typical compatibility invocation is:

```bash
nix-shell -p appimage-run --run 'appimage-run ./DendyForge-x86_64.AppImage'
```

Future release work should add a versioned script or CI pipeline that starts
from a clean checkout, builds Release, creates AppDir/AppImage, bundles needed
runtime libraries, smoke-tests it and records a checksum. Do not edit the WSL
copy or rebuild an artifact unless that is the requested task.

### Final pre-commit checklist

For a C++ change:

```powershell
git diff --check
cmake --build --preset mingw-clang-debug
ctest --test-dir out/build/mingw-clang-debug --output-on-failure
git status --short
```

Then run the feature-specific ROM runner(s) and manual game regression check.
For a documentation-only change, `git diff --check` and a careful diff review
are sufficient; do not claim a build was run if it was not.

Stage exact paths only, preserve all user-owned untracked ROM suites, commit
one logical change with an imperative subject, and push `main` after successful
verification. In the final handoff, state the commit hash, exact tests run,
and remaining known limitations.
