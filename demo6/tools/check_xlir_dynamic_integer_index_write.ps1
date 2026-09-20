param([string]$Root = "")

$ErrorActionPreference = "Stop"
if ( $Root -eq "" ) { $Root = Split-Path -Parent $PSScriptRoot }
$Root = (Resolve-Path -LiteralPath $Root).Path
$xl = Join-Path $Root "release\xl.exe"
$outputDir = Join-Path $Root "build\test"
[System.IO.Directory]::CreateDirectory($outputDir) | Out-Null

Push-Location $Root
try {
    foreach ( $name in @("cgen_dynamic_container_reference", "xlir_dynamic_integer_index_write") ) {
        $source = Join-Path $Root "test\cases\$name.xl"
        $prefix = Join-Path $outputDir "$name.dynamic-write"
        & $xl ir $source --lowered --format json --emit "$prefix.json"
        if ( $LASTEXITCODE -ne 0 ) { throw "Dynamic index write lowering failed: $name" }
        $module = [System.IO.File]::ReadAllText("$prefix.json") | ConvertFrom-Json
        $types = @{}
        $values = @{}
        foreach ( $type in $module.types ) { $types[[int]$type.id] = $type }
        foreach ( $instruction in $module.instructions ) {
            if ( $instruction.result -ne 0 ) { $values[[int]$instruction.result] = $instruction }
        }
        $writes = @($module.instructions | Where-Object {
            $_.runtimeOpName -eq "xvalue-index-set-xvalue-key-new"
        })
        if ( $writes.Count -lt 3 -or @($module.instructions | Where-Object {
            $_.runtimeOpName -eq "xvalue-array-set-i64-new"
        }).Count -ne 0 ) { throw "Erased integer index write guessed array storage: $name" }
        $dispatchNodes = @($module.nodes | Where-Object {
            $_.indexWriteAbiName -eq "xvalue-key-dispatch-set-new"
        })
        if ( $dispatchNodes.Count -ne $writes.Count ) {
            throw "Typed dispatch identity does not map one-to-one to lowered writes: $name"
        }
        foreach ( $write in $writes ) {
            if ( $write.operandCount -ne 3 -or $types[[int]$write.type].name -ne "bool" ) {
                throw "Dynamic write lost its checked consume-item ABI: $name"
            }
            foreach ( $operand in $write.operands ) {
                if ( $types[[int]$values[[int]$operand].type].cAbi -ne 26 ) {
                    throw "Dynamic write operand is not explicitly boxed xvalue: $name"
                }
            }
            $key = $values[[int]$write.operands[1]]
            if ( $key.kindName -eq "load" -and $key.targetSymbol -ge 0xF0000000 ) {
                $branch = @($module.instructions | Where-Object {
                    $_.index -gt $write.index -and $_.kindName -eq "branch"
                } | Select-Object -First 1)
                $release = @($module.instructions | Where-Object {
                    $_.kindName -eq "release" -and $_.targetSymbol -eq $key.targetSymbol -and
                    $_.index -gt $write.index -and $_.index -lt $branch[0].index
                })
                if ( $branch.Count -ne 1 -or $release.Count -ne 1 ) {
                    throw "Boxed key must release once before either success or failure edge: $name"
                }
            }
        }
        & $xl cgen $source --emit "$prefix.c"
        if ( $LASTEXITCODE -ne 0 ) { throw "Dynamic write Cgen failed: $name" }
        & $xl cgen $source --backend xlir --emit "$prefix.repeat.c"
        if ( $LASTEXITCODE -ne 0 -or [System.IO.File]::ReadAllText("$prefix.c") -cne
             [System.IO.File]::ReadAllText("$prefix.repeat.c") ) {
            throw "Dynamic write default/explicit C is not deterministic: $name"
        }
        $command = 'call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -include "{0}.c" test\cases\xlir_generated_module_host.c -DXLIR_GENERATED_MODULE_PREINCLUDED !XL6_CGEN_LINKS! -o "{0}.strict.exe" && "{0}.strict.exe"' -f $prefix
        & cmd.exe /d /v:on /c $command
        if ( $LASTEXITCODE -ne 0 ) { throw "Dynamic write strict C/runtime failed: $name" }
        foreach ( $backend in @("xlir", "legacy") ) {
            & $xl run $source --backend $backend
            if ( $LASTEXITCODE -ne 0 ) { throw "Dynamic write $backend run failed: $name" }
            & $xl build $source --backend $backend -o "$prefix.$backend.exe"
            if ( $LASTEXITCODE -ne 0 ) { throw "Dynamic write $backend build failed: $name" }
            & "$prefix.$backend.exe"
            if ( $LASTEXITCODE -ne 0 ) { throw "Dynamic write $backend executable failed: $name" }
        }
    }
} finally { Pop-Location }
Write-Output "XLIR erased integer index write ok: explicit runtime dispatch, boxed-key lifetime, sparse list and array aliases, single evaluation, catchable bounds failure, strict C and run/build parity."
exit 0
