# Expat

- Upstream: `https://github.com/libexpat/libexpat`
- Version: `2.8.2` (`R_2_8_2`)
- Archive: `expat-2.8.2.tar.gz`
- SHA-256: `ef7d1994f533c9e7343d6c19f31064fc8ebbcbcaa144be3812b4f43052a05f4c`
- License: MIT, see `vendor/expat/COPYING`

The package manifest compiles only Expat's library core. Tests, command-line
tools, build-system files and documentation are not part of the package.

Local integration changes:
- `expat_random.c`: Windows entropy comes from XRT `xrtSecureRandom`, so the
  package does not depend on a particular CRT's `rand_s` export. The manifest
  declares `XRT_MODULE_RANDOM_SECURE` for this symbol. Other targets retain
  Expat's platform-specific sources.
- `vendor/expat/lib/internal.h`: MinGW ANSI stdio on Win64 uses 64-bit integer
  format modifiers for `ptrdiff_t` and `size_t`. Recheck this tiny patch on
  vendor upgrades.
