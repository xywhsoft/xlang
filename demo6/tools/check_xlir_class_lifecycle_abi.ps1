param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"
if ( $Root -eq "" ) { $Root = Split-Path -Parent $PSScriptRoot }

$xl = Join-Path $Root "release\xl.exe"
$genericHost = Join-Path $Root "test\cases\xlir_generated_module_host.c"
$destroyHost = Join-Path $Root "test\cases\cgen_class_destroy_host.c"
$outputDir = Join-Path $Root "build\test"
$cases = @(
    [pscustomobject]@{
        Name = "xlir_class_lifecycle_destroy_only"
        Source = "cgen_var_destroy_class_init.xl"
        Support = $null
    },
    [pscustomobject]@{
        Name = "xlir_class_lifecycle_control_flow"
        Source = "cgen_class_destroy.xl"
        Support = $destroyHost
    },
    [pscustomobject]@{
        Name = "xlir_class_lifecycle_inheritance"
        Source = "cgen_class_inherit_destroy.xl"
        Support = $destroyHost
    }
)

foreach ( $path in @($xl, $genericHost, $destroyHost) ) {
    if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
        throw "XLIR class lifecycle gate input is missing: $path"
    }
}
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

function Invoke-Compiler([string[]]$Arguments, [string]$Description)
{
    & $xl @Arguments
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR class lifecycle $Description failed"
    }
}

function Assert-ByteIdentical([string]$First, [string]$Second)
{
    $a = [System.IO.File]::ReadAllBytes($First)
    $b = [System.IO.File]::ReadAllBytes($Second)
    if ( $a.Length -ne $b.Length -or
         ! [System.Linq.Enumerable]::SequenceEqual($a, $b) ) {
        throw "XLIR class lifecycle generated C is not byte-for-byte deterministic"
    }
}

function Get-TypeId($Document, [string]$Name)
{
    $matches = @($Document.types | Where-Object { $_.name -eq $Name })
    if ( $matches.Count -ne 1 ) {
        throw "XLIR class lifecycle has no unique TypeId for $Name"
    }
    return [int]$matches[0].id
}

function Get-ClassLayout($Document, [int]$TypeId)
{
    $matches = @($Document.classLayouts | Where-Object {
        [int]$_.type -eq $TypeId
    })
    if ( $matches.Count -ne 1 ) {
        throw "XLIR class lifecycle has no unique layout for t$TypeId"
    }
    return $matches[0]
}

function Get-DestroySteps($Document, $Layout)
{
    $result = @()
    for ( $i = 0; $i -lt [int]$Layout.destroyStepCount; ++$i ) {
        $result += $Document.classDestroySteps[
            [int]$Layout.firstDestroyStep + $i
        ]
    }
    return @($result)
}

function Assert-LoweredLifecycle($Document)
{
    $lifecycleTargets = @($Document.callTargets | Where-Object {
        $_.lifecycleAbiName -eq "class-destroy-borrow"
    })
    if ( $lifecycleTargets.Count -eq 0 ) {
        throw "XLIR class lifecycle has no explicit finalizer-borrow CallTarget"
    }
    foreach ( $step in @($Document.classDestroySteps) ) {
        $targets = @($lifecycleTargets | Where-Object {
            [uint64]$_.symbol -eq [uint64]$step.targetSymbol -and
            [int]$_.declaration -eq [int]$step.declaration
        })
        if ( $targets.Count -ne 1 -or
             [int]$targets[0].parameterCount -ne 1 -or
             [int]$targets[0].parameterTypes[0] -ne [int]$step.receiverType -or
             [int]$targets[0].parameterAbis[0] -ne 2 -or
             $targets[0].errorAbiName -notin @("no-raise", "may-raise") -or
             [int]$targets[0].returnType -ne 0 ) {
            throw "XLIR class destroy step has no exact finalizer-borrow target ABI"
        }
    }
    foreach ( $target in $lifecycleTargets ) {
        $function = @($Document.functions | Where-Object {
            [uint64]$_.symbol -eq [uint64]$target.symbol
        })
        if ( $function.Count -ne 1 -or [int]$function[0].parameterCount -ne 1 ) {
            throw "XLIR class destroy target has no unique lowered body"
        }
        $receiver = [uint64]$Document.functionParameters[
            [int]$function[0].firstParameter
        ]
        $body = @($Document.instructions | Where-Object {
            [int]$_.index -ge [int]$function[0].firstInstruction -and
            [int]$_.index -lt ([int]$function[0].firstInstruction +
                [int]$function[0].instructionCount)
        })
        if ( @($body | Where-Object {
                ($_.kindName -eq "store" -or $_.kindName -eq "release") -and
                [uint64]$_.targetSymbol -eq $receiver
             }).Count -ne 0 ) {
            throw "XLIR class destroy receiver is not an immutable finalizer borrow"
        }
    }
    $commits = @($Document.instructions | Where-Object {
        $_.runtimeOpName -eq "xvalue-class-construction-commit"
    })
    $binds = @()
    foreach ( $commit in $commits ) {
        $layout = Get-ClassLayout $Document ([int]$commit.payloadType)
        if ( [int]$layout.destroyStepCount -gt 0 ) { $binds += $commit }
    }
    if ( $binds.Count -eq 0 ) {
        throw "XLIR class construction has no explicit destroy-plan bind operation"
    }
    foreach ( $bind in $binds ) {
        $layout = Get-ClassLayout $Document ([int]$bind.payloadType)
        if ( [int]$bind.type -ne [int]$bind.payloadType -or
             [uint64]$bind.targetSymbol -ne 0 -or
             [int]$layout.destroyStepCount -eq 0 -or [int]$layout.destroyFunction -le 0 ) {
            throw "XLIR class destroy bind has no exact payload/layout contract"
        }
    }
}

function Assert-DestroyOnlyContract($Typed, $Lowered)
{
    $resourceType = Get-TypeId $Typed "Resource"
    $layout = Get-ClassLayout $Typed $resourceType
    $steps = @(Get-DestroySteps $Typed $layout)
    if ( [int]$layout.hasExplicitCreate -ne 0 -or $steps.Count -ne 1 -or
         [int]$steps[0].receiverType -ne $resourceType ) {
        throw "XLIR destroy-only class was misclassified as an explicit constructor"
    }
    $projection = @($Lowered.instructions | Where-Object {
        [int]$_.sourceLine -eq 13 -and
        $_.runtimeOpName -eq "xvalue-project-xvalue-retain"
    })
    $release = @($Lowered.instructions | Where-Object {
        [int]$_.sourceLine -eq 13 -and $_.kindName -eq "release"
    })
    if ( $projection.Count -ne 1 -or $release.Count -ne 1 -or
         [int]$release[0].index -le [int]$projection[0].index ) {
        throw "XLIR flow-refined nominal test does not close its retained projection"
    }
}

function Assert-InheritanceContract($Typed, $Lowered, [string]$CText)
{
    $baseType = Get-TypeId $Typed "Base"
    $childType = Get-TypeId $Typed "Child"
    $autoType = Get-TypeId $Typed "AutoChild"
    $baseSteps = @(Get-DestroySteps $Typed (Get-ClassLayout $Typed $baseType))
    $childSteps = @(Get-DestroySteps $Typed (Get-ClassLayout $Typed $childType))
    $autoSteps = @(Get-DestroySteps $Typed (Get-ClassLayout $Typed $autoType))
    if ( $baseSteps.Count -ne 1 -or $childSteps.Count -ne 2 -or
         $autoSteps.Count -ne 1 -or
         [int]$childSteps[0].receiverType -ne $childType -or
         [int]$childSteps[1].receiverType -ne $baseType -or
         [int]$autoSteps[0].receiverType -ne $baseType ) {
        throw "XLIR class destroy plan is not flattened derived-to-base"
    }
    $helperPattern = "static bool xlir_class_destroy_finalizer_t$childType" +
        "\(xvalue\* value, void\* data\)\s*\{(?<body>.*?)\}"
    $helper = [regex]::Match(
        $CText, $helperPattern,
        [System.Text.RegularExpressions.RegexOptions]::Singleline
    )
    $layout = Get-ClassLayout $Lowered $childType
    $program = $Lowered.functions[[int]$layout.destroyFunction - 1]
    $calls = @($Lowered.instructions | Where-Object {
        $_.index -ge $program.firstInstruction -and
        $_.index -lt $program.firstInstruction + $program.instructionCount -and $_.kindName -eq 'call'
    })
    if ($program.kindName -ne 'class-fini' -or $program.errorAbi -ne 'may-raise' -or
        $calls.Count -ne 2 -or $calls[0].targetSymbol -ne $childSteps[0].targetSymbol -or
        $calls[1].targetSymbol -ne $childSteps[1].targetSymbol) {
        throw 'XLIR class-fini must own the complete derived-to-base CALL sequence'
    }
    if (!$helper.Success -or
        [regex]::Matches($helper.Groups['body'].Value, 'xlir_fn_\d+\(value\)').Count -ne 1 -or
        !$helper.Groups['body'].Value.Contains("xlir_fn_$($program.symbol)(value)") -or
        !$helper.Groups['body'].Value.Contains('outcome.ok')) {
        throw 'XLIR Cgen callback must only adapt one verified class-fini outcome'
    }
}

function Invoke-StrictXlirRuntime($Case, [string]$Generated, [string]$Exe)
{
    $support = if ( $null -ne $Case.Support ) {
        '-include "' + $Case.Support + '"'
    } else { "" }
    $command = 'call test\suites\common.bat && ' +
        '!XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror ' +
        '-DXLIR_GENERATED_MODULE_PREINCLUDED -include "' + $Generated +
        '" ' + $support + ' "' + $genericHost +
        '" !XL6_CGEN_XLANG_LINKS! -o "' + $Exe + '" && "' + $Exe + '"'
    & cmd.exe /d /v:on /c $command
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR class lifecycle strict-C/runtime gate failed: $($Case.Source)"
    }
}

function Invoke-StandaloneXlirRuntime($Case, [string]$Source, [string]$Exe)
{
    $arguments = @("build", $Source)
    if ( $null -ne $Case.Support ) { $arguments += $Case.Support }
    $arguments += @("--backend", "xlir", "-o", $Exe)
    Invoke-Compiler $arguments "standalone artifact build"
    & $Exe
    if ( $LASTEXITCODE -ne 0 ) {
        throw "XLIR class lifecycle standalone artifact runtime failed: $($Case.Source)"
    }
}

Push-Location $Root
try {
    foreach ( $case in $cases ) {
        $source = Join-Path $Root "test\cases\$($case.Source)"
        $typedPath = Join-Path $outputDir "$($case.Name).typed.json"
        $loweredPath = Join-Path $outputDir "$($case.Name).lowered.json"
        $generated = Join-Path $outputDir "$($case.Name).c"
        $generatedSecond = Join-Path $outputDir "$($case.Name).second.c"
        $xlirExe = Join-Path $outputDir "$($case.Name).xlir.exe"
        $artifactExe = Join-Path $outputDir "$($case.Name).artifact.exe"
        if ( ! (Test-Path -LiteralPath $source -PathType Leaf) ) {
            throw "XLIR class lifecycle source is missing: $source"
        }
        Invoke-Compiler @("ir", $source, "--typed", "--format", "json",
            "--emit", $typedPath) "typed IR"
        Invoke-Compiler @("ir", $source, "--lowered", "--format", "json",
            "--emit", $loweredPath) "lowered IR"
        Invoke-Compiler @("cgen", $source, "--backend", "xlir", "--emit",
            $generated) "XLIR Cgen"
        Invoke-Compiler @("cgen", $source, "--backend", "xlir", "--emit",
            $generatedSecond) "second XLIR Cgen"
        Assert-ByteIdentical $generated $generatedSecond

        $typed = [System.IO.File]::ReadAllText(
            $typedPath, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
        $lowered = [System.IO.File]::ReadAllText(
            $loweredPath, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
        $cText = [System.IO.File]::ReadAllText(
            $generated, [System.Text.Encoding]::UTF8)
        Assert-LoweredLifecycle $lowered
        if ( $case.Source -eq "cgen_var_destroy_class_init.xl" ) {
            Assert-DestroyOnlyContract $typed $lowered
        }
        if ( $case.Source -eq "cgen_class_inherit_destroy.xl" ) {
            Assert-InheritanceContract $typed $lowered $cText
        }
        foreach ( $required in @(
            "xl6ClassRuntimeV2Get()->Create",
            "xlir_class_destroy_finalizer_t",
            "xl6ClassRuntimeV2Get()->CommitTake"
        ) ) {
            if ( ! $cText.Contains($required) ) {
                throw "XLIR class lifecycle generated C is missing mechanical helper: $required"
            }
        }
        Invoke-StrictXlirRuntime $case $generated $xlirExe
        Invoke-StandaloneXlirRuntime $case $source $artifactExe
    }
} finally {
    Pop-Location
}

Write-Output "XLIR class lifecycle ABI gate ok: exact destroy-only classification, flattened derived-to-base plans, immutable finalizer borrows, retained-projection cleanup, deterministic mechanical C, strict runtime, and standalone artifact semantics are exact."
exit 0
