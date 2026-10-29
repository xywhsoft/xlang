# Embedded Package

Build from the DEMO6 directory with the current compiler:

```powershell
$env:XLANG_PACKAGE_PATH = "$PWD/release/examples/embedded_package/packages"
./release/xl.exe build release/examples/embedded_package/host.xl -o build/embedded_package.exe
New-Item -ItemType Directory build/embedded_package_deploy
Copy-Item build/embedded_package.exe build/embedded_package_deploy/host.exe
Copy-Item release/examples/embedded_package/external.xl build/embedded_package_deploy
Remove-Item Env:XLANG_PACKAGE_PATH
Push-Location build/embedded_package_deploy
./host.exe
Pop-Location
```

Only `host.exe` and `external.xl` are deployed. `next()` shares the host's
counter, while `multiply()` calls the same compiled native function. Package
descriptors and declarations are generated automatically. No handwritten
function registration, source archive or operating-system export scan is used.

Expected output:

```text
external package script=42
embedded_package=ok
```

This example requires an EXE provider and the same XRT instance as its embedded
engine. Registration is explicit and local to one Context. It is not a sandbox
for hostile native descriptors or arbitrary native pointers. See
`docs/compiled-host-packages.md` for ABI, lifetime and exposure restrictions.
