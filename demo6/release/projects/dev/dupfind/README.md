# dupfind - duplicate file detector

Find content-identical files under a directory using a two-level (size, then
SHA-256) filter. Verifies the `hash` / `crypto` API and recursive file scanning.

## usage

```
dupfind <dir> [-r]      find content-identical files under <dir>
dupfind <dir>           scan only the top level of <dir>
dupfind                 default to current directory
dupfind -h              show help
```

## example

```
> dupfind projects\dev -r
Scanning projects\dev for duplicates...

Group 1 (SHA: 51f5c1b5b1049162...):
    dupfind\testdata\backup\calc.xl.bak 161 B
    dupfind\testdata\calc\calc.xl       161 B

Group 2 (SHA: 018506b0186f6e28...):
    dupfind\testdata\sub\report.xl      204 B
    dupfind\testdata\sub\report_copy.xl 204 B

Found 2 duplicate groups, 4 files can be deduplicated.
Skipped 4 binary files (not valid UTF-8, hash unreliable).
```

## algorithm (two-level filter)

1. **collect** every regular file via `dir.entries(dir, recursive)` (the var
   dict array already carries `path` / `size` / `isFile`).
2. **level 1 - size:** count size collisions per file (`int64(e["size"])`).
   A file whose size is globally unique can never be a duplicate, so its hash
   is never computed.
3. **level 2 - hash:** only for files in a size-collision group, read text and
   compute `crypto.sha256(content)`; cluster by `(size, sha)`. Same-size files
   with different content are correctly split into separate clusters.
4. **emit** every cluster with >= 2 members.

## design / bug workarounds (see projects/dev/README.md)

- **B2 (array has no append):** collectors are `list<T>` seeded with one
  placeholder and grown by out-of-range index assignment (primes / duptop
  idiom). Parallel lists hold `paths`, `sizes`, `hashes`, `visited`, `gIdx`.
- **B12 (containers passed to funcs come back empty / crash):** all grouping
  logic is inlined into `main`; only scalar->scalar helpers (`formatSize`,
  `relName`, `printUsage`) are factored out.
- **B13/B14 (native-backed class & Buffer unusable across functions / in
  build):** no `Buffer`, no `File` stream object is passed around; we use
  `file.readText` (free function) + `crypto.sha256` (free function) only.
- **B15 (codec.hex* clashes with crypto.*):** only `crypto.sha256` is used.
- **B6 (no return inside catch):** read failures set a `readOk` flag.

## binary file guard (BUG-18)

`file.readText` silently truncates non-UTF-8 files at the first invalid byte
and returns a tiny prefix with no error (a 393728-byte `.exe` reads back as 3
bytes). This would cause catastrophic false positives (four distinct exes all
hash to the same SHA). The guard:

```xlang
if string.byteLen(content) == int(sizes[h]) then
    hashes[h] = crypto.sha256(content)
else
    hashes[h] = "BINARY:" + paths[h]   // path-unique sentinel, never matches
    skippedBinary += 1
end
```

A correctly read UTF-8 text file always satisfies `byteLen(content) ==
file.size`; a mismatch means truncation, so the hash is unreliable and the file
is skipped (counted in the summary). Limitation: this makes dupfind correct for
**text** files; binary duplicates are reported as "skipped", not grouped,
because xlang has no usable raw-bytes read path (`Buffer` is broken in build
per B14, `File` cannot cross functions per B13, and `crypto.sha256Stream`
would need one of those).

## build

`xl build` is currently broken for every program in this release: the codegen
emits a call to `xl6_command_argv_to_arg` in the main wrapper but the helper
that emits its static definition is dead code (BUG-19). `xl run` resolves the
symbol from `xl.dll` exports, so run works; the standalone link does not.

`dupfind_shim.c` supplies the missing definition (the exact body the dead
codegen helper would have emitted) so the build links. It is a user-side build
shim (like codec's manual hex for BUG-15/16), not a compiler-source change:

```
xl build projects\dev\dupfind\dupfind.xl projects\dev\dupfind\dupfind_shim.c -o projects\dev\dupfind\dupfind.exe
```

Bonus: the shim uses `GetCommandLineW` + `WideCharToMultiByte`, so the built
exe actually receives proper UTF-8 argv (sidestepping B5 for this tool).

## verification

`xl run` and the built `dupfind.exe` produce **byte-identical** output across:
- recursive scan of `testdata` (2 controlled duplicate groups, plus a
  same-size-different-content pair that is correctly NOT grouped);
- recursive scan of `projects\dev` (the controlled groups + 4 binary `.exe`
  files correctly skipped);
- non-recursive scan (0 groups, since all dups are in subdirectories);
- `-h` help.

`hash.u64` stability re-confirmed via `examples\stdlib_hash.xl` (`hash=ok`).
See BUGS.md NOTE-18 / NOTE-19.
