# Blargg NES reset test fixtures

The test images are unmodified files from
[`christopherpow/nes-test-roms`](https://github.com/christopherpow/nes-test-roms),
commit `95d8f621ae55cee0d09b91519a8989ae0e64753b`.

| Image | Original directory | SHA-256 | Coverage |
| --- | --- | --- | --- |
| `registers.nes` | `cpu_reset` | `A30F33FB6C9F56012FBA38DC85DDC3DCCC06BFC0B25FEF7711B63F8207279715` | Reset preserves A, X, Y and correctly modifies P and S |
| `ram_after_reset.nes` | `cpu_reset` | `F1802A5618AAAA0C4D592CAA45B0B13C54082AF93FC311BDA0C27BCEACBC7C7F` | Reset's dummy reads and suppressed stack accesses |

Both are iNES Mapper 0 cartridges. They request a user reset through
Blargg's `$6000` result protocol; the CTest harness waits 200,000 console
clocks (more than the requested 100 ms) before issuing it.
