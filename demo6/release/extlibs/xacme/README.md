# xacme

This package binds the public API of XRT's `extlibs/xacme` to typed XLang
classes. The old `acme` placeholder and its independent protocol design have
been removed. Upstream owns the ACME, DNS provider, TLS and store behavior.

The implementation snapshot in `upstream/` contains XACME only; it does not
compile a second XRT runtime. See `upstream/source.json` for provenance and
`api-map.json` for the public C-to-XLang mapping.

Implementation is being added in independently committed stages. The package
will enter the shipping manifest only after lifecycle, output-ownership and
Windows/Linux JIT/AOT acceptance succeeds.
