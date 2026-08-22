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

The local `blargg_apu_2005.07.30` suite was run on 2026-08-22 through
`DendyForgeApuTimingRunner`. It reports its final result in CPU RAM `$00F0`
after entering a stable final loop; code 1 is pass. `01.len_ctr`,
`02.len_table`, `03.irq_flag`, and `09.reset_timing` pass. `04.clock_jitter`
fails with code 3 (frame IRQ late); `05.len_timing_mode0` and
`06.len_timing_mode1` with code 5; `07.irq_flag_timing` with code 3;
`08.irq_timing` with code 2; `10.len_halt_timing` with code 3; and
`11.len_reload_timing` with code 3. Keep the APU Roadmap items in progress
until the frame counter's exact write, IRQ, and length-clock ordering is fixed.

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
| Jackal | Does not work | Do not claim Mapper 2 alone solves it; diagnose board/mapper requirements from its header and documented hardware. |

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

Do not add test ROM binaries to a target as C++ sources. A runner receives the
directory containing `.nes` files as a command-line argument.

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

This does **not** make the APU cycle-accurate. The frame counter presently has
known errors verified by blargg tests. Keep all four channel entries yellow
(`frame-timing gaps`) and keep only the Audio Mixer entry green until the
timing suite passes.

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
& .\out\build\mingw-clang-release\DendyForgeApuMixerRunner.exe .\roms\nes-test-roms\apu_mixer
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
& .\out\build\mingw-clang-release\DendyForgeApuTimingRunner.exe .\blargg_apu_2005.07.30
```

The exact baseline is:

| ROM | Result | Interpreted diagnosis |
| --- | --- | --- |
| `01.len_ctr` | PASS | Basic length counter works. |
| `02.len_table` | PASS | Length lookup table works. |
| `03.irq_flag` | PASS | Basic frame IRQ flag works. |
| `04.clock_jitter` | FAIL, code 3 | Frame IRQ is late. |
| `05.len_timing_mode0` | FAIL, code 5 | Second length clock is late. |
| `06.len_timing_mode1` | FAIL, code 5 | Second length clock is late. |
| `07.irq_flag_timing` | FAIL, code 3 | First frame-IRQ assertion is late. |
| `08.irq_timing` | FAIL, code 2 | An IRQ timing event is early. |
| `09.reset_timing` | PASS in isolation | Do not count it as cumulative success after earlier failures. |
| `10.len_halt_timing` | FAIL, code 3 | Expected length clock around CPU cycle 14915 is wrong. |
| `11.len_reload_timing` | FAIL, code 3 | Reload just after length clock is ordered incorrectly. |

The suite's own `tests.txt` declares dependencies between tests. Therefore
later isolated passes do not prove full conformance once an earlier timing test
fails.

### Next APU implementation plan

This is the highest-priority emulator work after the current handoff:

1. From NESdev documentation and blargg source, write down a deterministic
   table of quarter-frame, half-frame and frame-IRQ CPU cycles for four-step
   and five-step operation.
2. Model a `$4017` write as a separately delayed event. Its effective delay
   depends on CPU-cycle parity; do not fold it into a single averaged counter.
3. Make the cycle-boundary order explicit: frame clocks, IRQ assertion,
   IRQ-inhibit clearing and sequencer restart must happen in hardware order.
4. Re-run the complete timing runner after every narrow change. Fix
   `04.clock_jitter`, `07.irq_flag_timing` and `08.irq_timing` first; only then
   trust the diagnostics of the dependent length timing tests.
5. When all timing ROM pass, re-run `apu_mixer`, unit tests and Contra. Listen
   specifically for the formerly metallic artifacts before marking channels
   complete in the Roadmap.

Avoid a broad APU rewrite. The evidence points to frame-sequencer ordering and
`$4017` timing, not absence of the implemented channels or mixer.

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
