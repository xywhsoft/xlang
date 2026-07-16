# tomlc17 vendor record

- Upstream: https://github.com/cktan/tomlc17
- Snapshot version: `260618`
- License: MIT, see `LICENSE`
- `src/tomlc17.c` SHA-256: `452DF8B237E5387707647B8711436067E80E18AF01978BB2308958C2CF4924A2`
- `src/tomlc17.h` SHA-256: `7A5D09625636B245315C0CEB7671B3874D24CF4A136E677D950F6D73F957BC65`

Only the C parser source and public header are retained. Upstream tests, examples,
CI files, build files and C++ adapters are intentionally excluded from the
release package. `tomlc17_xrt.c` redirects allocations to XRT without modifying
the upstream parser sources.
