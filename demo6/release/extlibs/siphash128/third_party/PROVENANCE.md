# SipHash Implementation and Test Provenance

The default xlang standard library delegates 64-bit SipHash-2-4 to XRT.
This extension compiles no 64-bit SipHash code. The local `siphash.c` /
`siphash.h` implement only the 128-bit domain-separated
variant, with explicit little-endian input/output, direct complete-block reads,
partial-tail buffering and nonterminal snapshots. They are maintained locally,
not represented as an unmodified upstream release. The package does not ship
another 64-bit algorithm or compile the reference oracle into runtime artifacts.

The independent test oracle is [veorq/SipHash](https://github.com/veorq/SipHash/tree/32d067603b93b47828700880649198e0bfbbcffa),
pinned at commit `32d067603b93b47828700880649198e0bfbbcffa`.
Files are retained unmodified in `test/reference/` under the package root.
Upstream license notices,
MIT and CC0 license files are retained. SHA-256:

| File | SHA-256 |
| --- | --- |
| siphash.c | 94B6C1F3A24072A8A5AFC4FEA7FB9E95C5FC2E99657B6B712D991029FFDAA88C |
| siphash.h | 30D4269093E5886CB9E7EA826899EF02DADDAC682E73C335C53DD748B9E20BFB |
| vectors.h | 212C44114A63C6D84710B8627F3BC5CE155698ACCF2EA7BBFF1C4B69C9F48D31 |
| LICENSE_MIT | F2E682ACDA10FEAD70F7A72918F5B229DB02AAEA32769A62C1378654E942BC98 |
| LICENSE_CC0 | 5537D4D10B76B81B6E8DFD8B644480A4B1EFA332FBB0CDB61126C5BE781EF7B4 |

The retained `test_siphash.c` is a historical combined-package test; the
active `test_siphash128_core.c` checks this engine against published 128-bit
vectors, and `test/cases/extlib_siphash128_core.xl` checks the xlang adapter.
The historical test first checked the reference against the published 64-bit
and 128-bit vectors, then compared production functions over a larger
key/alignment/length matrix, short-input split points and long messages.
HalfSipHash vectors in the upstream file are not used and do not imply a
HalfSipHash public API.

`test/reference_examples.c`, linked only with the upstream `siphash.c`, produces
the fixed paired-example digest outputs. Both platform gates regenerate these
outputs in memory and compare them to the checked-in expected text. The
non-digest lines describe separately asserted example control flow, not
properties proven by the hash oracle. Upstream retrieval is not required to
build, run or test the installed package.
