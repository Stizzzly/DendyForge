# Blargg NES instruction tests v5

The `official_only.nes` and `all_instrs.nes` fixtures are unmodified images
from [`christopherpow/nes-test-roms`](https://github.com/christopherpow/nes-test-roms),
directory `instr_test-v5`, commit `95d8f621ae55cee0d09b91519a8989ae0e64753b`.

| Image | SHA-256 | Coverage |
| --- | --- | --- |
| `official_only.nes` | `589B8835DEB5CBC69618DAC193A3DBD675540F7F2794E2D2A92E97BEB8ABC3CB` | Official 2A03 opcodes |
| `all_instrs.nes` | `353870C157242E3D428EF7387109DEAEE0D2E158BDB432AB9AAE4E657072C785` | Official and stable unofficial opcodes |

These are iNES Mapper 1 (MMC1) cartridges. The test harness runs them through
`Console`, reads Blargg's documented `$6000` PRG-RAM status protocol, and
fails if the result is non-zero or the ROM does not finish.
