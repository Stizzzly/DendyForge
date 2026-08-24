# Klaus Dormann 6502 functional test

`6502_functional_test.bin` is the unmodified default 64 KiB test image from
[Klaus2m5/6502_65C02_functional_tests](https://github.com/Klaus2m5/6502_65C02_functional_tests),
commit `7954e2dbb49c469ea286070bf46cdd71aeb29e4b`.

SHA-256: `FA12BFC761E6F9057E4CC01A665A7B800FF01AE91F598AF1E39A1201D01953FD`

The accompanying `LICENSE.txt` is the upstream GPLv3 license. The image is
used only as an independent test fixture. Its default configuration requires a
flat, writable 64 KiB CPU memory and NMOS decimal arithmetic; it begins at
`$0400` and passes by self-jumping at `$3469`.
