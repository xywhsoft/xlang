# tomlc17 vendor record

- Upstream: https://github.com/cktan/tomlc17
- Snapshot version: `260618`
- License: MIT, see `LICENSE`
- Upstream `src/tomlc17.c` SHA-256: `452DF8B237E5387707647B8711436067E80E18AF01978BB2308958C2CF4924A2`
- Local `src/tomlc17.c` SHA-256: `746E456A9A623E466AB30EFDF44847A03AC8265754F72D60979AF156A3BA204A`
- `src/tomlc17.h` SHA-256: `7A5D09625636B245315C0CEB7671B3874D24CF4A136E677D950F6D73F957BC65`

Only the C parser source and public header are retained. Upstream tests, examples,
CI files, build files and C++ adapters are intentionally excluded from the
release package. The local parser removes six permissive syntax forms beyond
TOML 1.0: inline-table trailing commas and newlines, optional seconds in
times, Unicode bare keys, and `\xNN` / `\e` string escapes. The five forms other
than Unicode bare keys are in TOML 1.1; bare keys remain ASCII-only there too.
Quoted Unicode keys remain valid TOML 1.0.
`tomlc17_xrt.c` selects XRT as the parser's default allocator at compile time
without changing process-global options at runtime. The private adapter also
routes floating-point conversion to XRT's strict, locale-independent number
parser. TOML always uses an ASCII decimal point; the process locale is unchanged.
The private package declares `XRT_MODULE_NUMBER_FLOAT`, so AOT builds can trim
XRT without treating this C source as an undeclared full-XRT dependency.
