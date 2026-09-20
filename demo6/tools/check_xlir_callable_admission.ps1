param([string]$Root = '', [Parameter(Mandatory=$true)][string]$SourceDriver)
$ErrorActionPreference = 'Stop'
if ($Root -eq '') { $Root = Split-Path -Parent $PSScriptRoot }
Push-Location $Root
try {
    $results = @()
    foreach ($name in @('xlir_callable_coalesce_expression','xlir_xcall_zero_parameter_literal','xlir_xcall_assignment_literal')) {
        $prefix = "build/test/$name"; $log = "$prefix.log"; $stage = 'lowering'
        & $SourceDriver "test/cases/$name.xl" "$prefix.c" "$prefix.h" "$prefix.json" --script *> $log
        $code = $LASTEXITCODE
        if ($code -eq 0) {
            $stage = 'strict-C'
            & gcc -std=c11 -Wall -Wextra -Werror -Wno-unused-function -Wno-unused-variable -Wno-unused-parameter -Wno-unused-label -Wno-unused-const-variable -Wno-missing-field-initializers -I. -Ibuild/test -include "$prefix.c" test/cases/xlir_xcall_admission_host.c release/xl.dll -o "$prefix.exe" *>> $log
            $code = $LASTEXITCODE
        }
        if ($code -eq 0) { $stage = 'runtime'; & "$prefix.exe" *>> $log; $code = $LASTEXITCODE }
        $results += [pscustomobject]@{Case=$name;Stage=$stage;Exit=$code;Log=$log}
        Write-Output "$name $stage exit=$code"
    }
    [IO.File]::WriteAllText((Join-Path (Get-Location) 'build/test/xlir_callable_admission.results.json'), ($results | ConvertTo-Json -Depth 4), [Text.UTF8Encoding]::new($false))
    if (@($results | Where-Object Exit -ne 0).Count) { throw 'All callable admission failures retained; no expected-failure waiver.' }
} finally { Pop-Location }
