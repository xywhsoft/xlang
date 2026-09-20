param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"
if ( $Root -eq "" ) { $Root = Split-Path -Parent $PSScriptRoot }

$xl = Join-Path $Root "release\xl.exe"
$runtimeSource = Join-Path $Root "src\cgen\xl6_cgen_websocket_stream.c"
$outputDir = Join-Path $Root "build\test"
$cases = @(
    @{
        Name = "stdlib_websocket_upgrade_loopback"
        Attach = "xl6_xrt_ws_stream_attach_tcp"
        Result = "websocket-upgrade-loopback-ok"
    },
    @{
        Name = "stdlib_websocket_upgrade_tls_loopback"
        Attach = "xl6_xrt_ws_stream_attach_tls"
        Result = "websocket-upgrade-tls-loopback-ok"
    }
)

foreach ( $path in @($xl, $runtimeSource) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR WebSocket managed callback gate input is missing: $path"
    }
}
[System.IO.Directory]::CreateDirectory($outputDir) | Out-Null

function Invoke-Xl([string[]]$Arguments, [string]$Failure) {
    & $xl @Arguments
    if ( $LASTEXITCODE -ne 0 ) { throw $Failure }
}

Push-Location $Root
try {
    foreach ( $case in $cases ) {
        $source = Join-Path $Root ("test\cases\{0}.xl" -f $case.Name)
        $loweredPath = Join-Path $outputDir ("{0}.managed_callback.lowered.json" -f $case.Name)
        $generatedPath = Join-Path $outputDir ("{0}.managed_callback.xlir.c" -f $case.Name)
        $generatedAgainPath = Join-Path $outputDir ("{0}.managed_callback.xlir_again.c" -f $case.Name)
        $xlirExe = Join-Path $outputDir ("{0}.managed_callback.xlir.exe" -f $case.Name)
        $legacyExe = Join-Path $outputDir ("{0}.managed_callback.legacy.exe" -f $case.Name)
        if ( ! (Test-Path -LiteralPath $source -PathType Leaf) ) {
            throw "XLIR WebSocket managed callback source is missing: $source"
        }

        Invoke-Xl @("ir", $source, "--lowered", "--format", "json", "--emit", $loweredPath) `
            "XLIR WebSocket managed callback lowering failed: $($case.Name)"
        $module = [System.IO.File]::ReadAllText(
            $loweredPath, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
        $targets = @($module.callTargets | Where-Object {
            $_.cName -eq $case.Attach -and $_.reachability -ne "unreachable"
        })
        if ( $targets.Count -ne 1 ) {
            throw "WebSocket attach has no unique reachable extern CallTarget: $($case.Attach)"
        }
        $target = $targets[0]
        $expectedAttachBridge = if ( $case.Attach -eq "xl6_xrt_ws_stream_attach_tls" ) {
            "websocket-stream-tls"
        } else {
            "websocket-stream"
        }
        $parameterTypes = @($target.parameterTypes)
        $parameterAbis = @($target.parameterAbis)
        $physicalAbis = @($target.parameterPhysicalAbis)
        $constraintAbis = @($target.parameterConstraintAbis)
        if ( $target.kind -ne 2 -or $target.errorAbiName -ne "no-raise" -or
             $target.runtimeBridgeName -ne $expectedAttachBridge -or
             $target.parameterCount -ne 5 -or $parameterTypes.Count -ne 5 -or
             $parameterAbis.Count -ne 5 -or $physicalAbis.Count -ne 5 -or
             $constraintAbis.Count -ne 5 -or $parameterAbis[3] -ne 2 -or
             $physicalAbis[3] -ne "language-function-value" -or
             $constraintAbis[3] -ne "none" ) {
            throw "WebSocket attach lost its borrowed unconstrained XL6_Function ABI: $($case.Attach)"
        }

        $signatureType = [int]$parameterTypes[3]
        $signature = @($module.types | Where-Object { $_.id -eq $signatureType })
        if ( $signature.Count -ne 1 -or $signature[0].kind -ne 4 -or
             $signature[0].callableErasureAbiName -ne "fixed" -or
             @($signature[0].returns).Count -ne 0 ) {
            throw "WebSocket handler does not have one exact fixed void function TypeId: $($case.Attach)"
        }
        $parameterTypeNames = @()
        foreach ( $typeId in @($signature[0].params) ) {
            $types = @($module.types | Where-Object { $_.id -eq [int]$typeId })
            if ( $types.Count -ne 1 ) {
                throw "WebSocket handler parameter TypeId is not declared: $typeId"
            }
            $parameterTypeNames += $types[0].name
        }
        if ( ($parameterTypeNames -join ",") -ne "int,int,int,uint64,point,c_size" ) {
            throw "WebSocket handler signature lost uint64/raw-pointer/c_size physical types"
        }

        $adapters = @($module.closureTargets | Where-Object {
            $_.signatureType -eq $signatureType -and $_.reachability -ne "unreachable"
        })
        if ( $adapters.Count -ne 2 -or
             @($adapters | Where-Object {
                 $_.kind -ne 2 -or $_.captureCount -ne 0 -or
                 $_.rawCallbackAbiName -ne "none" -or $_.adaptedTargetSymbol -eq 0
             }).Count -ne 0 ) {
            throw "WebSocket handlers are not exact capture-free named XL6_Function adapters"
        }

        Invoke-Xl @("cgen", $source, "--backend", "xlir", "--emit", $generatedPath) `
            "XLIR WebSocket managed callback Cgen failed: $($case.Name)"
        Invoke-Xl @("cgen", $source, "--backend", "xlir", "--emit", $generatedAgainPath) `
            "XLIR WebSocket managed callback deterministic Cgen failed: $($case.Name)"
        if ( ! [System.Linq.Enumerable]::SequenceEqual(
                [byte[]][System.IO.File]::ReadAllBytes($generatedPath),
                [byte[]][System.IO.File]::ReadAllBytes($generatedAgainPath)
            ) ) {
            throw "XLIR WebSocket managed callback generated C is not deterministic: $($case.Name)"
        }
        $generated = [System.IO.File]::ReadAllText(
            $generatedPath, [System.Text.Encoding]::UTF8)
        foreach ( $bridgeTarget in @($module.callTargets | Where-Object {
            $_.cName -like "xl6_xrt_ws_*" -and $_.reachability -ne "unreachable"
        }) ) {
            $expectedBridge = if ( $bridgeTarget.cName -eq "xl6_xrt_ws_mask" -or
                                  $bridgeTarget.cName -like "xl6_xrt_ws_frame_*" -or
                                  $bridgeTarget.cName -like "xl6_xrt_ws_close_*" ) {
                "websocket-frame"
            } elseif ( $bridgeTarget.cName -like "xl6_xrt_ws_key_*" -or
                       $bridgeTarget.cName -like "xl6_xrt_ws_accept*" -or
                       $bridgeTarget.cName -like "xl6_xrt_ws_protocol*" ) {
                "websocket-handshake"
            } elseif ( $bridgeTarget.cName -like "xl6_xrt_ws_upgrade_*" ) {
                "websocket-upgrade"
            } elseif ( $bridgeTarget.cName -like "xl6_xrt_ws_stream_config_*" ) {
                "websocket-stream-config"
            } elseif ( $bridgeTarget.cName -eq "xl6_xrt_ws_stream_attach_tls" ) {
                "websocket-stream-tls"
            } else {
                "websocket-stream"
            }
            if ( $bridgeTarget.runtimeBridgeName -ne $expectedBridge ) {
                throw "WebSocket target lost its verified component identity: $($bridgeTarget.cName)"
            }
        }
        foreach ( $httpTarget in @($module.callTargets | Where-Object {
            $_.cName -like "xl6_xrt_http1_*" -and $_.reachability -ne "unreachable"
        }) ) {
            if ( $httpTarget.runtimeBridgeName -ne "http1" ) {
                throw "HTTP/1 dependency target lost its verified component identity: $($httpTarget.cName)"
            }
        }
        foreach ( $required in @(
            "#include <__xl_runtime/xruntime_decl.h>",
            "typedef struct XL6_Function",
            "extern bool xrtValueGetPointer(const xvalue*, void**)",
            "projected > (uint64_t)SIZE_MAX"
        ) ) {
            if ( ! $generated.Contains($required) ) {
                throw "XLIR WebSocket callback C lacks verified managed adapter ABI: $required"
            }
        }
        $attachDefinition = 'void\s*\*\s*' + [regex]::Escape($case.Attach) +
            '\s*\(\s*xvalue\s*\*[^,]*,\s*void\s*\*[^,]*,\s*void\s*\*[^,]*,\s*XL6_Function\s+[^,]*,\s*size_t\s+[^)]*\)\s*\{'
        if ( $generated -notmatch $attachDefinition ) {
            throw "XLIR WebSocket attach component definition is missing: $($case.Attach)"
        }
        if ( $generated -notmatch 'long\s+long\s+xl6_xrt_ws_stream_close\s*\(' ) {
            throw "XLIR WebSocket stream component definition is missing: xl6_xrt_ws_stream_close"
        }
        if ( $generated -match
             ([regex]::Escape($case.Attach) + '\s*\([^;]*xlir_closure\s*\*') ) {
            throw "XLIR WebSocket attach leaked its internal closure pointer ABI"
        }

        Invoke-Xl @("build", $source, "--backend", "xlir", "-o", $xlirExe) `
            "XLIR WebSocket managed callback build failed: $($case.Name)"
        $xlirOutput = @(& $xlirExe 2>&1)
        if ( $LASTEXITCODE -ne 0 -or
             -not (($xlirOutput | Out-String).Contains($case.Result)) ) {
            throw "XLIR WebSocket managed callback runtime failed: $($case.Name)"
        }

        Invoke-Xl @("build", $source, "--backend", "legacy", "-o", $legacyExe) `
            "Legacy WebSocket callback differential build failed: $($case.Name)"
        $legacyOutput = @(& $legacyExe 2>&1)
        if ( $LASTEXITCODE -ne 0 -or
             -not (($legacyOutput | Out-String).Contains($case.Result)) ) {
            throw "Legacy WebSocket callback differential runtime failed: $($case.Name)"
        }
    }

    $runtime = [System.IO.File]::ReadAllText(
        $runtimeSource, [System.Text.Encoding]::UTF8)
    foreach ( $required in @(
        "xrtValueUInt(iValue)",
        "xrtValueUInt((uint64)iSize)"
    ) ) {
        if ( ! $runtime.Contains($required) ) {
            throw "WebSocket fallback invoke_var lost unsigned callback boxing: $required"
        }
    }
    foreach ( $forbidden in @(
        "xrtValueInt((int64)iValue)",
        "xrtValueInt((int64)iSize)"
    ) ) {
        if ( $runtime.Contains($forbidden) ) {
            throw "WebSocket fallback invoke_var retained signed callback boxing: $forbidden"
        }
    }
} finally {
    Pop-Location
}

Write-Output "XLIR WebSocket managed callback ABI gate ok: borrowed unconstrained XL6_Function handlers, fixed uint64/raw-pointer/c_size adapters, deterministic C, unsigned fallback boxing, and TCP/TLS legacy differential runtime semantics."
exit 0
