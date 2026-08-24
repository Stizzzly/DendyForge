# Blargg NES timing test fixtures

The test images are unmodified files from
[`christopherpow/nes-test-roms`](https://github.com/christopherpow/nes-test-roms),
commit `95d8f621ae55cee0d09b91519a8989ae0e64753b`.

| Image | Original directory | SHA-256 | Coverage |
| --- | --- | --- | --- |
| `instr_timing.nes` | `instr_timing` | `3D1BCA14266F1E25B75A34DDD29C9DF1CE9C6D990C8663A218F72E7861660FB0` | Official and stable unofficial instruction durations, including page crossing |
| `instr_misc.nes` | `instr_misc` | `B6762E20A285216304DFD2B5E1F192459354B23A5E48B2F5F9FB7CB0DAC51243` | Address wrapping and dummy reads, including APU-visible accesses |

Both are iNES Mapper 1 (MMC1) cartridges. They use Blargg's `$6000` result
protocol, which the CTest harness reads through `Console`.
