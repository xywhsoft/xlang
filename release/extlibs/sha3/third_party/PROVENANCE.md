# Local Portable SHA3 Core

`sha3.c` / `sha3.h` are the existing locally maintained portable Keccak-f[1600]
implementation in this repository. They are **not** represented as an official
upstream release, OpenSSL source, or a certified cryptographic module. No external
algorithm source was imported in this hardening pass; existing project licensing
applies. The folder name does not establish third-party provenance.

Algorithm contract: [NIST FIPS 202](https://csrc.nist.gov/pubs/fips/202/final).
SHA3 uses suffix 0x06, SHAKE 0x1f; these are not the Keccak-256 domain.

The 2026-09-27 changes retain the permutation, explicit little-endian lane I/O,
and constants. Complete input blocks are absorbed directly, keeping a buffer only
for a partial block. Fixed-output finalization shares the existing squeeze code;
the public wrapper validates all algorithms and natural SHA3 output lengths.
Snapshot callers copy the state before finalization. The private API assumes a
valid initialized state; it is not a second public checked API.

`test/generate_vectors.mjs` uses independent Node.js crypto/OpenSSL SHA3/SHAKE
implementations. `vectors.json` records Node/OpenSSL versions, generator SHA256,
input pattern, and expected bytes. `vectors.h` is mechanically generated from
the same independent oracle, not from this implementation. Stored results include
546 boundary vectors and 12 known-answer records. `--check` compares results and
generator identity without rewriting the provenance toolchain version.

No FIPS validation, external cryptographic audit, constant-time certification,
or secret erasure guarantee is claimed by passing these tests.
