# CPU6502 Cycle-Accuracy Plan

Status: **complete (2026-08-22). Phase 1 complete (nestest golden-trace regression
committed and passing). Phase 2 complete (2026-08-22): read-type,
write-type, immediate, implied and both JMP classes execute per cycle with
hardware dummy reads. Phase 3 complete (2026-08-22): read-modify-write,
stack (PHA/PHP/PLA/PLP), subroutine (JSR/RTS/RTI) and BRK instructions
execute per cycle with the hardware read -> write-old -> write-new pattern
and dummy stack reads. Phase 4 complete (2026-08-22): branches execute per
cycle — not taken 2 cycles, taken 3 with the dummy fetch of the next
opcode address, taken crossing a page 4 with an internal fix-up cycle.
Phase 5 complete (2026-08-22): hardware IRQ/NMI entry and reset execute
their seven cycles per cycle (dummy fetches, stack bytes, vector fetch;
reset suppresses writes and decrements SP three times), interrupt lines
are polled at the penultimate cycle of an instruction with the I flag
masking IRQ at the poll, IRQ became level-triggered through
`IRQ(bool line)`, and the restored flag-to-line timing
(`FrameIrqLineLatencyCycles = 1`, armed only on the flag's first set
cycle, decremented before the sequencer update) brought the blargg suite
back to 11/11 — including the previously regressed `08.irq_timing`.**
Phase 6 complete (2026-08-22): the final regression suite passed (CTest,
nestest trace, `apu_mixer` 4/4 and blargg APU timing 11/11); user gameplay
regression confirmed Super Mario Bros., Pac-Man, Contra and Jackal. Release
performance was recovered by caching CPU address-mode dispatch and avoiding
cartridge virtual calls on built-in bus ranges: the standard 20 M-cycle
Mapper 0 `nestest.nes` benchmark measured 11,944,313 console clocks/s
(6.67x realtime).** This document is the full implementation plan for
converting `CPU6502` from instruction-timed to cycle-accurate execution;
`AGENTS.md` remains the authoritative status document.

## Why

The CPU currently executes an entire instruction (fetch, addressing,
operation, memory side effects) during the first cycle and then burns the
remaining cycles without bus activity. Consequences:

* No dummy reads, so PPU/APU registers are not touched on the hardware
  cycles (matters for `$2002`/`$2007` races and open-bus behavior later).
* Read-modify-write instructions write memory once instead of the NMOS
  `read -> write old value -> write new value` pattern.
* Interrupt entry performs its stack/vector accesses instantly instead of
  across 7 cycles.
* Page-cross penalties exist only as added counters, not as real cycles
  with real (wrong-address) reads.

The goal: **one `Clock()` call = exactly one CPU cycle performing at most
one bus transaction (`CpuBus::CpuRead` or `CpuBus::CpuWrite`) in hardware
order.** The core stays a standalone framework depending only on
`cpu_bus.hpp`, and the public API is preserved (see "Constraints").

## Current state (measured facts, 2026-08-22)

* `core/cpu/cpu6502.cpp:575-606` — `Clock()`: on `m_cycles == 0` it runs
  `Fetch()` + the instruction's `addressMode()` + `operate()` member
  functions atomically, then burns `m_cycles`. Extra cycles come from the
  function-pointer return values combined with bitwise `&`
  (`cpu6502.cpp:602`), so a page-cross penalty only applies to read-type
  operations.
* Addressing modes (`cpu6502.cpp:687-719, 1036-1046, 1330-1352,
  1354-1410, 1448-1472`) perform no dummy reads anywhere. `ABX/ABY/IZY`
  detect page crossings by comparing high bytes; `ZPX/ZPY/IZX` wrap in
  registers.
* RMW instructions (`ASL/LSR/ROL/ROR/INC/DEC`, unofficial
  `SLO/RLA/SRE/RRA/DCP/ISB`) do one `FetchData()` read and one final
  `Write`; no dummy write-back of the old value.
* `BranchIf` (`cpu6502.cpp:1517-1533`) adds taken/page-cross cycles by
  mutating `m_cycles` directly.
* `EnterInterrupt` (`cpu6502.cpp:1535-1558`) pushes PC/P and reads the
  vector synchronously; `Clock()` services a latched pending interrupt at
  the instruction boundary with `m_cycles = 7`
  (`cpu6502.cpp:579-587`). This latch model is what passes blargg
  `08.irq_timing` / `09.reset_timing` today — re-validate after changing.
* `Reset()` (`cpu6502.cpp:559-573`) is synchronous, `m_cycles = 8`.
* Opcode coverage: 232 of 256 (163 explicit entries + 27 unofficial NOPs +
  42 unofficial RMW). The remaining 24 (JAM column, ANC/ALR/ARR/XAA/SBX/
  LAS/LAX-imm/SHA/TAS/SHX/SHY) decode to a finite 2-cycle no-op via
  `{"???", XXX, IMP, 2}`. Keep this status quo; nestest only needs the
  implemented set.
* `CpuBus` (`core/cpu/cpu_bus.hpp:8-15`) is exactly two virtual methods —
  enough for cycle accuracy; the CPU must call them from its private
  `Read`/`Write` wrappers only.
* **The nestest golden-trace test is not in the repository.** A local
  `nestest_trace_tests.cpp` (using long-removed `DumpLogLine()` /
  `SetTotalCycles()` API) was deleted before ever being committed; only a
  stale `.obj` and the generated `tests/cpu/roms/nestestres.log` (8991
  lines, matching golden length) remain. `README.md` claims nestest
  passes; that claim currently has no committed regression test backing
  it. The golden file `tests/cpu/roms/nestest.log` is tracked and
  contains per-instruction `PC, bytes, A, X, Y, P, SP, PPU, CYC`.
* Existing CPU tests: `tests/cpu/cpu_tests.cpp` (ROM-driven end-state
  checks via `CpuMachine`), `tests/cpu/cpu_modularity_tests.cpp`
  (cycle-count assertions per instruction, including mid-instruction
  `Cycles()` checks), shared harness `tests/cpu/cpu_test_support.hpp`
  (`CompleteInstruction`, `CompleteReset`, `ExecuteUntilSelfJump`).
* System-level gates sensitive to CPU timing: the blargg APU timing suite
  (11/11 pass, requires the current interrupt-latch timing) and the
  Shay Green mixer suite (4/4). Gameplay regression: Super Mario Bros.,
  Pac-Man, Contra.
* Performance baseline: Release core ≈ 2.8x realtime (~5 M console
  clocks/s on the Ryzen 5 7530U machine). The rewritten core must not be
  slower. Debug builds are already sub-realtime and are not a target.

## Constraints (must hold after every phase)

1. `cpu6502.hpp/.cpp` may include only `cpu_bus.hpp` and the standard
   library. No `Bus`/`PPU`/`APU`/`Console`/SDL/iNES/mapper knowledge.
2. Public API unchanged in shape: `Clock`, `Reset`, `IRQ`, `NMI`,
   getters, `Cycles()` semantics "remaining cycles of the current
   instruction, including the one in progress". Tests rely on the
   drain-loop pattern `while (cpu.Cycles() > 0) cpu.Clock();`.
3. Total cycles per instruction stay identical to the current table
   (nestest-validated). Only their *internal distribution* changes.
4. Decimal-mode configuration behavior unchanged (`Configuration::
   decimalModeEnabled`; Console passes false).
5. One logical change per commit, tests green, push to `main`.

## Phases

### Phase 1 — Reinstate the nestest golden-trace regression

Before touching the core, get the safety net back.

* New `tests/cpu/nestest_trace_tests.cpp`, registered in `CMakeLists.txt`
  next to the other test files. Drive the existing `CpuMachine` harness
  (`cpu_test_support.hpp`): load `nestest.nes`, `CompleteReset`, set
  `PC = 0xC000`, run one instruction at a time up to 8991 log lines.
* Format each line **in the test** from public getters — do not re-add
  `DumpLogLine()`/`SetTotalCycles()` to the CPU:
  * `PC`, opcode + operand bytes read from the bus,
  * `A/X/Y/P/SP` from getters,
  * `CYC` = the harness's own count of `Clock()` calls.
* Compare against `tests/cpu/roms/nestest.log` field-by-field (PC, bytes,
  A, X, Y, P, SP, CYC; ignore the disassembly and PPU columns). On
  mismatch report first differing line index and both lines.
* **Gate:** the *current* instruction-timed core must pass this test
  before Phase 2 begins. If it does not, stop and fix the existing core
  first — the rewrite must start from a proven-equivalent baseline.

### Phase 2 — Per-cycle sequencer skeleton with correct reads

Restructure `Clock()` into a state machine driven by an internal
per-instruction cycle counter:

* Cycle 1 is always the opcode fetch (`Read(m_pc++)`).
* Subsequent cycles execute the addressing-mode micro-steps, each cycle
  performing its real bus read, including dummy reads:

  | Mode (read-type op) | Cycles and transactions |
  | --- | --- |
  | Implied/Acc | fetch; read `PC` (no increment) |
  | Immediate | fetch; read `PC++` |
  | ZP | fetch; read ZP address; read data |
  | ZP,X / ZP,Y | fetch; read base; **dummy read `(base+X)&FF`**; read data |
  | ABS | fetch; read lo; read hi; read data |
  | ABS,X / ABS,Y | fetch; lo; hi; read `(base+idx)` with unfixed high byte; if page crossed, read fixed address (5 cycles), else that read was the data (4 cycles) |
  | (ZP,X) | fetch; read pointer; **dummy read `(ptr+X)&FF`**; read lo; read hi; read data |
  | (ZP),Y | fetch; read lo; read hi; read `(base+Y)` unfixed; if crossed, read fixed |

* Write-type ops (STA/STX/STY/SAX): the indexed dummy read stays, the
  final cycle is the write; unindexed ABS stores do their fixed 4-cycle
  form (fetch, lo, hi, write).
* `operate()` functions keep their arithmetic but execute on the final
  cycle; `FetchData()` becomes "the value read by the mode's last read
  cycle" instead of issuing a fresh read.
* Implement as instruction-class dispatch (Read / Write / RMW / Implied /
  Control) rather than per-opcode code; the `Instruction` table gains a
  class tag, the addressMode/operate pointers can remain for the ALU.
* Keep the page-cross "wrong-address read" exact: on NMOS the dummy read
  of `LDA abs,X` crossing a page goes to `base+X` without the carry.

**Tests:** new transaction-recording bus stub, e.g.

```cpp
class RecordingCpuBus final : public dendyforge::CpuBus {
public:
    std::vector<std::string> log; // "R addr", "W addr=value" per cycle
    std::array<std::uint8_t, 65536> memory{};
    // CpuRead: log then return memory[address]
    // CpuWrite: log then memory[address] = data
};
```

Assert exact sequences (address order and count) for: `LDA abs,X` with
and without page cross, `STA abs,X`, `LDA (zp),Y` crossing, implied
(`INC A`-style dummy read at PC), `LDA zp,X` dummy read.

**Gate:** all existing tests + nestest trace pass unchanged totals.
*Implemented note:* the zero-page-indexed and (zp,X) dummy reads go to the
**unindexed** base/pointer address (hardware behavior; the table above
said the indexed address — the code and `tests/cpu/cpu_bus_cycle_tests.cpp`
assert the unindexed form).

### Phase 3 — RMW and stack instructions cycle-exact

* RMW family (official + unofficial): `read data; write OLD value; write
  NEW value` — three data cycles after addressing (e.g. `INC zp` =
  fetch, addr, read, write-old, write-new = 5 cycles; `INC abs,X` = 7).
* PHA/PHP: fetch, dummy read at PC, write stack (3).
* PLA/PLP: fetch, dummy read at PC, dummy read stack, pop (4).
* JSR: fetch, read lo, **internal cycle (dummy stack read)**, push PCH,
  push PCL, read hi (6) — note hi is read *after* the pushes.
* RTS: fetch, dummy read PC, dummy read stack, pop PCL, pop PCH,
  internal (PC++) (6).
* RTI: fetch, dummy read PC, dummy read stack, pop P, pop PCL, pop PCH
  (6).
* BRK: fetch, dummy read at PC (PC++), push PCH, push PCL, push P|B|U,
  read vector lo, read vector hi (7).
* JMP (ind): keep the page-wrap bug; transaction order fetch, lo, hi,
  ptr-lo, ptr-hi (5).

**Tests:** transaction sequences for each group; stack contents after
JSR/BRK; nestest + unit suites still green.

### Phase 4 — Branches cycle-exact

* Not taken: fetch, read offset (2 cycles, done).
* Taken: third cycle = internal op (dummy fetch of next opcode address,
  discarded, PC still points at next instruction), then `PC = PC +
  offset`; if the target crosses a page, fourth internal cycle.
* Replace `BranchIf`'s `m_cycles` mutation with the state machine's
  natural cycles.

**Tests:** 2/3/4-cycle cases, taken-not-crossed vs crossed, transaction
log shows the dummy fetch at the pre-branch PC.

### Phase 5 — Interrupts and reset cycle-exact

* Hardware IRQ/NMI entry (7 cycles): two dummy fetches (opcode fetch and
  operand fetch of the interrupted flow, discarded), push PCH, push PCL,
  push P (B clear, U set), read vector lo, read vector hi. I flag set at
  entry. NMI uses FFFA, IRQ FFFE.
* Keep the currently-passing delivery contract unless tests prove a
  better one: `Console::Clock()` polls the line before the APU's
  end-of-cycle update; the CPU latches and services at the instruction
  boundary; the APU frame IRQ line already has its 3-cycle latency
  (`APU::FrameIrqLineLatencyCycles`). Refine the poll point inside the
  CPU only if the full blargg suite stays 11/11 — `04.clock_jitter`,
  `07/08/09` are the sensitive ones.
* Optional (only if blargg stays green): NMI-hijacking of a taken BRK /
  delayed IRQ (vector FFFA pushed with B set). Treat as stretch, not a
  gate.
* Reset: 7 cycles — dummy reads, no writes, `SP -= 3`, read FFFC/FFFD.
  Keep the public `Reset()` synchronous if callers rely on immediate
  vector load (check `Console::Reset` and tests), or convert carefully.

**Gate:** blargg APU timing 11/11, mixer 4/4, all unit tests, nestest
trace.

### Phase 6 — Final regression, performance, documentation

Completed 2026-08-22:

* Debug and Release `ctest` passed, including the committed nestest golden
  trace; the `apu_mixer` runner passed 4/4 and the blargg APU timing runner
  passed 11/11.
* The Release 20 M-console-cycle Mapper 0 `nestest.nes` benchmark measured
  11,944,313 clocks/s (6.67x realtime), above the 2.8x pre-conversion
  baseline. CPU dispatch now caches the address-mode kind and the bus skips
  the cartridge virtual call for built-in RAM/register ranges.
* User gameplay regression confirmed Super Mario Bros., Pac-Man, Contra and
  Jackal. Jackal's boot VBlank handshake now completes without a game-specific
  workaround.
* `AGENTS.md`, this Status line and `README.md` reflect the verified result.

## Explicit non-goals for this effort

* Sub-cycle PPU alignment (interleaving the 3 PPU dots around each CPU
  bus access inside `Console::Clock`) — separate follow-up work.
* DMA stealing cycles mid-instruction at exact hijack points — the
  existing `Console` stall model stays.
* Implementing the remaining 24 unofficial opcodes.
* Making the Debug build realtime-capable.

## Risks and mitigations

* **Game regressions from changed access timing** — mitigated by the
  mandatory manual Mario/Pac-Man/Contra check in Phase 6 and by keeping
  each phase a separately revertible commit.
* **`cpu_modularity_tests.cpp` mid-instruction `Cycles()` assertions** —
  the "remaining cycles" semantics is preserved by design; if an
  assertion encodes the old internal ordering rather than the contract,
  change it in the same commit with a comment stating why.
* **Interrupt timing regression** — blargg suite is the hard gate for
  Phase 5; do not merge a phase that turns any of the 11 ROMs red.
* **Debug-build slowdown** — acceptable; only Release is a gameplay
  target (recorded in `AGENTS.md`, commit `7e53b55`).
