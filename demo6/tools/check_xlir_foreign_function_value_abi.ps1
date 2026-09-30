param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$xl = Join-Path $Root "release\xl.exe"
$source = Join-Path $Root "test\cases\xlir_foreign_function_value_abi.xl"
$hostSource = Join-Path $Root "test\cases\xlir_foreign_function_value_abi_host.c"
$jsonPath = Join-Path $Root "build\test\xlir_foreign_function_value_abi.json"
$cPath = Join-Path $Root "build\test\xlir_foreign_function_value_abi.c"
$exePath = Join-Path $Root "build\test\xlir_foreign_function_value_abi.exe"

foreach ( $path in @($xl, $source, $hostSource) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR foreign function-value ABI gate input is missing: $path"
    }
}
[System.IO.Directory]::CreateDirectory((Split-Path -Parent $jsonPath)) | Out-Null

Push-Location $Root
try {
    & $xl ir $source --lowered --format json --emit $jsonPath
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR foreign function-value lowering failed"
    }
    $module = [System.IO.File]::ReadAllText(
        $jsonPath, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
    $languageTargets = @($module.callTargets | Where-Object {
        @($_.parameterPhysicalAbis) -contains "language-function-value"
    })
    $requiredLanguageTargets = @(
        "xl6_xrt_tls_config_verify_callback",
        "xl6_test_invoke_void",
        "xl6_test_invoke_scalars",
        "xl6_test_invoke_string",
        "xl6_test_invoke_value",
        "xl6_test_invoke_captured"
    )
    foreach ( $requiredTarget in $requiredLanguageTargets ) {
        if ( @($languageTargets | Where-Object { $_.cName -eq $requiredTarget }).Count -ne 1 ) {
            throw "XLIR extern function-value matrix lacks one exact target: $requiredTarget"
        }
    }
    $tlsTarget = @($languageTargets | Where-Object {
        $_.cName -eq "xl6_xrt_tls_config_verify_callback"
    })[0]
    $languageParameter = [array]::IndexOf(
        @($tlsTarget.parameterPhysicalAbis), "language-function-value")
    if ( $languageParameter -ne 1 -or
         @($tlsTarget.parameterAbis)[$languageParameter] -ne 2 ) {
        throw "XLIR extern method callback lost its borrowed XL6_Function parameter contract"
    }
    foreach ( $target in $languageTargets ) {
        $parameterIndex = [array]::IndexOf(
            @($target.parameterPhysicalAbis), "language-function-value")
        if ( $parameterIndex -lt 0 -or
             @($target.parameterAbis)[$parameterIndex] -ne 2 -or
             @($target.parameterConstraintAbis)[$parameterIndex] -ne "none" ) {
            throw "XLIR language function value lost its borrowed unconstrained descriptor ABI: $($target.cName)"
        }
    }
    $pointerNull = @($module.intrinsicCalls | Where-Object {
        $_.kindName -eq "raw-pointer-null-convert" -and
        $_.admissionName -eq "verified"
    })
    if ( $pointerNull.Count -ne 1 ) {
        throw "XLIR point(null) lacks one verified raw-pointer-null-convert intrinsic"
    }

    & $xl cgen $source --backend xlir --emit $cPath
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR foreign function-value Cgen failed"
    }
    $generated = [System.IO.File]::ReadAllText(
        $cPath, [System.Text.Encoding]::UTF8)
    foreach ( $required in @(
        "typedef struct XL6_Function",
        "xlir_language_function_invoke_t",
        "xlir_language_function_value_t",
        "xl6_test_invoke_scalars",
        "xl6_test_invoke_string",
        "xl6_test_invoke_captured",
        "bool xl6_xrt_tls_config_verify_callback(void* handle, XL6_Function callback)"
    ) ) {
        if ( ! $generated.Contains($required) ) {
            throw "XLIR foreign function-value generated C lacks verified ABI token: $required"
        }
    }
    if ( $generated -match
         'xl6_xrt_tls_config_verify_callback\s*\([^,]+,\s*xlir_closure\s*\*' ) {
        throw "XLIR extern function value leaked its internal closure pointer ABI"
    }
    if ( $generated.Contains(
             "extern bool xl6_xrt_tls_config_verify_callback(void*, XL6_Function)") ) {
        throw "XLIR component-supplied TLS callback retained a redundant extern declaration"
    }
    if ( $generated -notmatch
         '(?s)(v\d+)\s*=\s*NULL;.{0,400}xl6_test_point_is_null\(\1\);' ) {
        throw "XLIR point(null) generated C did not materialize the verified raw null value"
    }

    $command = "call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -DXLIR_GENERATED_MODULE_PREINCLUDED -include $cPath $hostSource !XL6_CGEN_LINKS! -o $exePath && $exePath"
    & cmd.exe /d /v:on /c $command
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR foreign function-value strict-C/runtime gate failed"
    }
} finally {
    Pop-Location
}

Write-Output "XLIR borrowed XL6_Function fixed-signature matrix, captured closure adapter invocation, and point(null) intrinsic runtime semantics ok."
