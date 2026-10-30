# tomlc17 vendor record

- Upstream: https://github.com/cktan/tomlc17
- Snapshot version: `260618`
- License: MIT, see `LICENSE`
- Upstream `src/tomlc17.c` SHA-256: `452DF8B237E5387707647B8711436067E80E18AF01978BB2308958C2CF4924A2`
- Local `src/tomlc17.c` SHA-256 (canonical LF): `14BB03344D68D07FDAA10FEA4D1BEB48DBC2AE95D686BEE0FCBD2A5E380FEB4A`
- `src/tomlc17.h` SHA-256 (canonical LF): `EEF9A891B93FC6235A9552DB16286CEFC684BE8FAE49018BEDF1C6AB9CA55B87`

Only the C parser source and public header are retained. Upstream tests, examples,
CI files, build files and C++ adapters are intentionally excluded from the
release package. The local parser removes six permissive syntax forms beyond
TOML 1.0: inline-table trailing commas and newlines, optional seconds in
times, Unicode bare keys, and `\xNN` / `\e` string escapes. The five forms other
than Unicode bare keys are in TOML 1.1; bare keys remain ASCII-only there too.
Quoted Unicode keys remain valid TOML 1.0.
The local date validator accepts the complete four-digit year range 0000..9999,
matching the TOML 1.0 ABNF and the package's existing datetime constructors.
Year zero uses the same Gregorian leap-year rule; this is independent of
Python datetime's narrower 1..9999 representation and is tested separately.
`tomlc17_xrt.c` selects XRT as the parser's default allocator at compile time
without changing process-global options at runtime. The private adapter also
routes floating-point conversion to XRT's strict, locale-independent number
parser. TOML always uses an ASCII decimal point; the process locale is unchanged.
The private package declares `XRT_MODULE_NUMBER_FLOAT`, so AOT builds can trim
XRT without treating this C source as an undeclared full-XRT dependency.
