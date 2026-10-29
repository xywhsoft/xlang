param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$xl = Join-Path $Root "release\xl.exe"
$source = Join-Path $Root "test\cases\extern_var.xl"
$hostSource = Join-Path $Root "test\cases\xlir_extern_global_boundary_host.c"
$functionSource = Join-Path $Root "test\cases\xlir_extern_global_function.xl"
$functionHostSource = Join-Path $Root "test\cases\xlir_extern_global_function_host.c"
$outputDir = Join-Path $Root "build\test"
$textPath = Join-Path $outputDir "xlir_extern_global_boundary.lowered.xlir"
$jsonPath = Join-Path $outputDir "xlir_extern_global_boundary.lowered.json"
$cPath = Join-Path $outputDir "xlir_extern_global_boundary_generated.c"
$headerPath = Join-Path $outputDir "xlir_extern_global_boundary_generated.h"
$exePath = Join-Path $outputDir "xlir_extern_global_boundary_host.exe"
$functionCPath = Join-Path $outputDir "xlir_extern_global_function_generated.c"
$functionExePath = Join-Path $outputDir "xlir_extern_global_function_host.exe"

foreach ( $path in @($xl, $source, $hostSource, $functionSource, $functionHostSource) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR extern-global gate input is missing: $path"
    }
}
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

& $xl ir $source --lowered --format text --emit $textPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR extern global lowered text IR failed" }
$text = [System.IO.File]::ReadAllText($textPath, [System.Text.Encoding]::UTF8)
foreach ( $required in @(
    "externGlobals=1",
    "extern-global x0 storage=s2 declaration=n2 type=t3 access=read-write cname=host_counter",
    "storage s2 kind=extern-global type=3 decl=n2 owner=n1"
) ) {
    if ( ! $text.Contains($required) ) {
        throw "XLIR ExternGlobal text ABI is incomplete: $required"
    }
}

& $xl ir $source --lowered --format json --emit $jsonPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR extern global lowered JSON IR failed" }
$json = [System.IO.File]::ReadAllText($jsonPath, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
$imported = @($json.externGlobals)[0]
if ( $json.externGlobalCount -ne 1 -or $imported.storageSymbol -ne 2 -or
     $imported.type -ne 3 -or $imported.accessAbi -ne "read-write" -or
     $imported.cName -ne "host_counter" ) {
    throw "XLIR ExternGlobal JSON has no exact imported storage ABI"
}

& $xl cgen $source --backend xlir --emit $cPath --emit-header $headerPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR extern global Cgen failed" }
$generated = [System.IO.File]::ReadAllText($cPath, [System.Text.Encoding]::UTF8)
$header = [System.IO.File]::ReadAllText($headerPath, [System.Text.Encoding]::UTF8)
if ( ! $generated.Contains("extern int64_t host_counter;") -or
     $generated -notmatch 'v[0-9]+ = host_counter;' -or
     $generated -notmatch '\{ &\(host_counter\), &\(xlir_committed\), sizeof\(host_counter\) \}' -or
     !$generated.Contains('(xl6SlotRuntimeV1Get()->Apply)(xlir_writes') ) {
    throw "XLIR C emitter ignored verified ExternGlobal storage mapping"
}
if ( $generated.Contains("static int64_t s2") -or $header.Contains("host_counter") ) {
    throw "XLIR extern dependency was redefined privately or leaked into the module export header"
}

$command = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror $hostSource !XL6_CGEN_LINKS! -o $exePath && $exePath"
& $env:ComSpec /d /v:on /c $command
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR extern global generated-C runtime failed" }

& $xl cgen $functionSource --backend xlir --emit $functionCPath
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR cross-function extern global Cgen failed" }
$functionGenerated = [System.IO.File]::ReadAllText($functionCPath, [System.Text.Encoding]::UTF8)
if ( $functionGenerated -notmatch 'static int64_t xlir_fn_[0-9]+\(void\)' -or
     $functionGenerated -notmatch 'v[0-9]+ = host_counter;' -or
     $functionGenerated -notmatch '\{ &\(host_counter\), &\(xlir_committed\), sizeof\(host_counter\) \}' -or
     $functionGenerated -notmatch '\(xl6SlotRuntimeV1Get\(\)->Apply\)' ) {
    throw "XLIR ExternGlobal is not available through verified user-function CFG"
}
$command = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror $functionHostSource !XL6_CGEN_LINKS! -o $functionExePath && $functionExePath"
& $env:ComSpec /d /v:on /c $command
if ( $LASTEXITCODE -ne 0 ) { throw "XLIR cross-function extern global runtime failed" }

Write-Output "XLIR extern global boundary gate ok: imported scalar storage, C-name LOAD/STORE, header isolation, module-init and user-function host mutation."
exit 0
