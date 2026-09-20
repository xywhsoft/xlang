param([string]$Root = '')
$ErrorActionPreference = 'Stop'
if (!$Root) { $Root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot) }
$Root = (Resolve-Path -LiteralPath $Root).Path
. (Join-Path $Root 'tools/xlir_c_object_cache.ps1')
$fixture = Join-Path $Root ('build/tmp/xlir object cache ' + [Guid]::NewGuid().ToString('N'))
foreach ($directory in @('src', 'early', 'late', 'cache')) {
    [IO.Directory]::CreateDirectory((Join-Path $fixture $directory)) | Out-Null
}
function Fixture([string]$Path, [string]$Text) {
    [IO.File]::WriteAllText((Join-Path $fixture $Path), $Text, [Text.UTF8Encoding]::new($false))
}
function Require([bool]$Condition, [string]$Message) { if (!$Condition) { throw $Message } }
Fixture 'late/value.h' "#define VALUE 11`n"
Fixture 'src/part.c' "int offset(void) { return 1; }`n"
Fixture 'src/answer.c' "#include <value.h>`n#include `"part.c`"`n#ifndef EXTRA`n#define EXTRA 0`n#endif`nint answer(void) { return VALUE + offset() + EXTRA; }`n"
$flags = @('-std=c11', '-Wall', '-Wextra', '-Werror', '-O0', "-I$fixture/early", "-I$fixture/late")
$records = @()
function Build([string]$Name, [string[]]$Options = $flags, [switch]$NoCache, [switch]$OtherToolchain) {
    $context = New-XlirCBuildContext -Root $fixture -Directory (Join-Path $fixture $Name) -CacheDirectory (Join-Path $fixture 'cache') -NoCache:$NoCache
    if ($OtherToolchain) { $context.ToolchainSha256 = Get-XlirTextHash ($context.ToolchainSha256 + 'simulated other toolchain identity') }
    $record = Invoke-XlirCObject -Context $context -Source (Join-Path $fixture 'src/answer.c') -Object (Join-Path $context.Directory 'answer.o') -Flags $Options
    Assert-XlirCToolchainUnchanged $context
    $script:records += $record
    return $record
}
function RunAnswer($Record, [int]$Expected) {
    $directory = Split-Path -Parent $Record.Path
    $main = Join-Path $directory 'main.c'
    [IO.File]::WriteAllText($main, "int answer(void); int main(void) { return answer() == $Expected ? 0 : 1; }", [Text.UTF8Encoding]::new($false))
    $exe = Join-Path $directory 'answer.exe'
    & gcc $main $Record.Path -o $exe
    Require ($LASTEXITCODE -eq 0) 'Fixture link failed'
    & $exe
    Require ($LASTEXITCODE -eq 0) "Wrong compiled answer: expected $Expected"
}
$cold = Build 'cold'
Require (!$cold.CacheHit) 'First build must compile'
RunAnswer $cold 12
$direct = Join-Path $fixture 'direct.o'
& gcc @flags -c (Join-Path $fixture 'src/answer.c') -o $direct
Require ($LASTEXITCODE -eq 0 -and (Get-FileHash -LiteralPath $direct).Hash -eq $cold.Sha256) 'Cached preprocessing must match an ordinary unsplit GCC compilation'
$warm = Build 'warm'
Require ($warm.CacheHit -and $warm.Sha256 -eq $cold.Sha256) 'An unchanged unit must reuse the identical object'
RunAnswer $warm 12
$clean = Build 'clean' -NoCache
Require (!$clean.CacheHit -and !$clean.CacheEligible -and $clean.Sha256 -eq $warm.Sha256) 'Clean and cached compilation differ'
$header = Join-Path $fixture 'late/value.h'
$stamp = (Get-Item -LiteralPath $header).LastWriteTimeUtc
Fixture 'late/value.h' "#define VALUE 21`n"
(Get-Item -LiteralPath $header).LastWriteTimeUtc = $stamp
$changed = Build 'changed-header-same-timestamp'
Require (!$changed.CacheHit -and $changed.CacheKey -ne $cold.CacheKey) 'Same timestamp must not hide changed header content'
RunAnswer $changed 22
Fixture 'late/value.h' "#define VALUE 11`n"
$restored = Build 'restored-header'
Require ($restored.CacheHit -and $restored.CacheKey -eq $cold.CacheKey) 'Restoring actual input content should permit reuse'
Fixture 'early/value.h' "#define VALUE 31`n"
$shadowed = Build 'new-earlier-header'
Require (!$shadowed.CacheHit) 'A new earlier include must invalidate reuse without a prior dependency record'
RunAnswer $shadowed 32
Fixture 'src/part.c' "int offset(void) { return 2; }`n"
$included = Build 'included-c-change'
Require (!$included.CacheHit) 'Included C fragments must participate in the content key'
RunAnswer $included 33
$define = Build 'changed-options' ($flags + '-DEXTRA=7')
Require (!$define.CacheHit) 'Compiler options must participate in the content key'
RunAnswer $define 40
$other = Build 'other-toolchain' -OtherToolchain
Require (!$other.CacheHit -and $other.CacheKey -ne $included.CacheKey) 'Toolchain identity must participate in the content key'
$entryObject = Join-Path $fixture "cache/$($included.CacheKey)/object.o"
[IO.File]::WriteAllText($entryObject, 'intentional cache corruption', [Text.UTF8Encoding]::new($false))
$recovered = Build 'corrupt-cache'
Require (!$recovered.CacheHit -and $recovered.CacheRejected -and $recovered.Sha256 -eq $included.Sha256) 'Corrupt objects must be rejected and rebuilt'
RunAnswer $recovered 33
Require ((Get-Content -LiteralPath $entryObject -Raw) -eq 'intentional cache corruption') 'Rejected evidence must be preserved, not overwritten'
Fixture 'src/answer.c' "#include <does_not_exist.h>`n"
$failed = $false
try { $null = Build 'missing-header' } catch { $failed = $_.Exception.Message -like 'C preprocessing failed:*' }
Require $failed 'Missing includes must fail before any cache hit can be accepted'
Fixture 'src/answer.c' 'int answer(void) { this is invalid C; }'
$failed = $false
try { $null = Build 'invalid-c' } catch { $failed = $_.Exception.Message -like 'C compilation failed:*' }
Require $failed 'Compilation failure must not create a successful cache entry'
$failed = $false
try { $null = Build 'unsupported' ($flags + '-fplugin=hidden-input') } catch { $failed = $_.Exception.Message -like 'Unsupported cached C build flag:*' }
Require $failed 'Hidden plugin inputs must not be treated as ordinary cacheable C'
Fixture 'src/answer.c' 'int answer(void) { int value = 1; switch (value) { case 1: ++value; /* fall through */ case 2: ++value; break; default: break; } return value; }'
$annotated = Build 'fallthrough-comment'
RunAnswer $annotated 3
$directAnnotated = Join-Path $fixture 'direct-annotated.o'
& gcc @flags -c (Join-Path $fixture 'src/answer.c') -o $directAnnotated
Require ($LASTEXITCODE -eq 0 -and (Get-FileHash -LiteralPath $directAnnotated).Hash -eq $annotated.Sha256) 'Preprocessing must preserve the original fallthrough warning contract'
Fixture 'src/answer.c' 'int answer(void) { return 9; }'
$overrideBefore = [Environment]::GetEnvironmentVariable('COMPILER_PATH')
try {
    [Environment]::SetEnvironmentVariable('COMPILER_PATH', (Split-Path -Parent (Get-Command gcc).Source))
    $override = Build 'compiler-path-override'
    Require (!$override.CacheHit -and !$override.CacheEligible) 'Compiler path overrides must disable reuse'
    RunAnswer $override 9
} finally { [Environment]::SetEnvironmentVariable('COMPILER_PATH', $overrideBefore) }
$report = [ordered]@{ Passed = $true; Records = $records; Scope = 'Real GCC objects and executions; content/flags/header shadowing/included C/clean parity, simulated toolchain-key change, corruption, compile errors and compiler-path override.' }
[IO.File]::WriteAllText((Join-Path $fixture 'results.json'), ($report | ConvertTo-Json -Depth 6), [Text.UTF8Encoding]::new($false))
Write-Output "XLIR C object cache passed: real cold/warm/clean parity and invalidation/refusal cases. Evidence: $fixture"
