param([string]$Root = '', [string]$Report = '')
$ErrorActionPreference = 'Stop'
if (!$Root) { $Root = Split-Path -Parent $PSScriptRoot }
$Root = (Resolve-Path -LiteralPath $Root).Path
$toolsDirectory = Join-Path $Root 'tools'
$runner = Join-Path $toolsDirectory 'run_xlir_parameterized_guards.ps1'
$tokens = $null; $parseErrors = $null
$ast = [Management.Automation.Language.Parser]::ParseFile($runner, [ref]$tokens, [ref]$parseErrors)
if ($parseErrors.Count) { throw 'Invalid parameterized runner source' }

# Evaluate only the actual production metadata/functions, never its builds or
# worker invocation. Distinct placeholder directories expose wrong-role aliases.
foreach ($name in @('Get-RequiredParameters', 'New-ScenarioGate')) {
    $nodes = @($ast.FindAll({param($node)
        $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq $name
    }, $true))
    if ($nodes.Count -ne 1) { throw "Ambiguous production function: $name" }
    . ([scriptblock]::Create($nodes[0].Extent.Text))
}
foreach ($name in @('$reviewedGuards', '$contractDrivers')) {
    $nodes = @($ast.FindAll({param($node)
        $node -is [Management.Automation.Language.AssignmentStatementAst] -and $node.Left.Extent.Text -eq $name
    }, $true) | Sort-Object { $_.Extent.StartOffset })
    if (!$nodes.Count) { throw "Missing production declaration: $name" }
    . ([scriptblock]::Create($nodes[0].Extent.Text))
}
$publicDirectory = Join-Path $Root 'build/test/argument-contract-public'
$runtimeDirectory = Join-Path $Root 'build/test/argument-contract-runtime'
$contractDirectory = Join-Path $Root 'build/test/argument-contract-contracts'
$phaseDirectory = Join-Path $Root 'build/test/argument-contract-phase'
$aggregateDirectory = Join-Path $phaseDirectory 'aggregate-dependencies'
$sourceDriver = Join-Path $contractDirectory 'xlir_code_owner_binding_contract.exe'
$NativeRoot = Join-Path $Root 'argument-contract-native'
$Tcc = 'argument-contract-tcc.exe'
$inputs = @($PSCommandPath, $runner) + @($reviewedGuards | ForEach-Object { Join-Path $toolsDirectory $_ })
$inputRecords = @($inputs | Sort-Object -Unique | ForEach-Object {
    [ordered]@{Path=$_;Sha256=(Get-FileHash -LiteralPath $_).Hash}
})
$gates = @(); $checks = 0; $failure = $null; $roleFailures = @()
try {
    if ($reviewedGuards.Count -ne 74 -or @($reviewedGuards | Sort-Object -Unique).Count -ne 74) {
        throw 'Reviewed worker inventory drifted'
    }
    foreach ($name in $reviewedGuards) {
        $gate = New-ScenarioGate $name $phaseDirectory
        foreach ($required in @(Get-RequiredParameters (Join-Path $toolsDirectory $name))) {
            if (!$gate.Arguments.Contains($required)) { throw "Unbound $name / $required" }
            ++$checks
        }
        if ($gate.Arguments.Contains('PublicDriver')) {
            if ($gate.Arguments.PublicDriver -cne (Join-Path $publicDirectory 'public_source.exe')) {
                $roleFailures += "Wrong public driver role: $name"
            }
            ++$checks
        }
        if ($gate.Arguments.Contains('SourceDriver')) {
            if ($gate.Arguments.SourceDriver -cne $sourceDriver) { throw "Wrong contract source-driver role: $name" }
            ++$checks
        }
        foreach ($parameter in @('Compiler', 'ContractDriver')) {
            if ($gate.Arguments.Contains($parameter)) {
                if (!$contractDrivers.ContainsKey($name) -or
                    $gate.Arguments[$parameter] -cne (Join-Path $contractDirectory $contractDrivers[$name])) {
                    throw "Wrong contract compiler role: $name / $parameter"
                }
                ++$checks
            }
        }
        if (!$gate.Resources.Count) { throw "Missing output-resource declaration: $name" }
        $gates += $gate
    }
    $publicConsumers = @($gates | Where-Object { $_.Arguments.Contains('PublicDriver') } | ForEach-Object Script)
    if ($publicConsumers.Count -ne 2 -or 'check_xlir_c_owned_boundary_fault.ps1' -notin $publicConsumers -or
        'check_xlir_c_owned_globals_fault.ps1' -notin $publicConsumers) { throw 'Unexpected public-driver consumers' }
    foreach ($input in $inputRecords) {
        if ((Get-FileHash -LiteralPath $input.Path).Hash -cne $input.Sha256) { throw "Changed argument input: $($input.Path)" }
    }
    if ($roleFailures.Count) { throw ($roleFailures -join '; ') }
} catch { $failure = $_.Exception.Message }
$result = [ordered]@{Complete=$null -eq $failure;Failure=$failure;RoleFailures=$roleFailures;Edition=$PSVersionTable.PSEdition;
    Inputs=$inputRecords;Workers=$gates.Count;Checks=$checks;Gates=$gates;
    WorkerExecutionEvaluated=$false;FullParameterizedAccepted=$false;FullGoalComplete=$false;
    Scope='Actual production argument/resource metadata only. No builds, workers, sockets, or native callbacks executed.'}
if ($Report) {
    if (Test-Path -LiteralPath $Report) { throw 'Preserve prior argument evidence' }
    [IO.File]::WriteAllText([IO.Path]::GetFullPath($Report), ($result | ConvertTo-Json -Depth 9), [Text.UTF8Encoding]::new($false))
}
if ($failure) { throw $failure }
Write-Output "Parameterized arguments: $($gates.Count) workers, $checks required/role checks; no worker execution claim"
