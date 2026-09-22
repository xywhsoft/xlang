param([string]$Root = "")
$ErrorActionPreference = "Stop"
if ($Root -eq "") { $Root = Split-Path -Parent $PSScriptRoot }
$Root = (Resolve-Path -LiteralPath $Root).Path
$xl = Join-Path $Root "release\xl.exe"
$outputDir = Join-Path $Root "build\test"
[System.IO.Directory]::CreateDirectory($outputDir) | Out-Null

function Invoke-Captured([string]$File, [string]$Arguments, [int]$Expected, [AllowNull()][object]$Output, [string]$Marker = "") {
    $process = New-Object System.Diagnostics.Process
    $process.StartInfo = New-Object System.Diagnostics.ProcessStartInfo
    $process.StartInfo.FileName = $File
    $process.StartInfo.Arguments = $Arguments
    $process.StartInfo.UseShellExecute = $false
    $process.StartInfo.CreateNoWindow = $true
    $process.StartInfo.RedirectStandardOutput = $true
    $process.StartInfo.RedirectStandardError = $true
    $process.StartInfo.StandardOutputEncoding = [System.Text.Encoding]::UTF8
    $process.StartInfo.StandardErrorEncoding = [System.Text.Encoding]::UTF8
    try {
        [void]$process.Start()
        $stdout = $process.StandardOutput.ReadToEndAsync()
        $stderr = $process.StandardError.ReadToEndAsync()
        $process.WaitForExit()
        $diagnostic = $stdout.Result + $stderr.Result
        if ($process.ExitCode -ne $Expected -or
            ($Marker -ne "" -and !$diagnostic.Contains($Marker)) -or
            ($Expected -eq 0 -and $stderr.Result -ne "")) {
            throw "Collection member command failed: $File $Arguments exit=$($process.ExitCode) $diagnostic"
        }
        if ($null -ne $Output -and $stdout.Result.Replace("`r`n", "`n") -cne $Output) {
            throw "Collection member output differs: $File $Arguments expected=[$Output] actual=[$($stdout.Result)]"
        }
    } finally { $process.Dispose() }
}

$fixtures = @(
    @{ Name = "xlir_erased_collection_members"; Output = "xlir erased collection members ok" },
    @{ Name = "xlir_erased_generic_collection_members"; Output = "xlir erased generic collection members ok" },
    @{ Name = "xlir_erased_collection_member_continuation"; Output = "xlir erased collection continuation ok" },
    @{ Name = "xlir_erased_collection_member_lifetime"; Output = "xlir erased collection lifetime ok" },
    @{ Name = "xlir_typed_collection_methods"; Output = "xlir typed collection methods ok" }
)
$familyKinds = @("array-append", "set-add", "collection-contains", "collection-remove", "array-remove-value", "typed-collection-member")
$operations = @{}
Push-Location $Root
try {
    foreach ($fixture in $fixtures) {
        $name = $fixture.Name
        $source = Join-Path $Root "test\cases\$name.xl"
        $expected = $fixture.Output + [char]10
        foreach ($stage in @("typed", "lowered")) {
            $path = Join-Path $outputDir "$name.$stage.json"
            $repeat = Join-Path $outputDir "$name.$stage.repeat.json"
            Invoke-Captured $xl ('ir "{0}" --{1} --format json --emit "{2}"' -f $source,$stage,$path) 0 $null
            Invoke-Captured $xl ('ir "{0}" --{1} --format json --emit "{2}"' -f $source,$stage,$repeat) 0 $null
            if ((Get-FileHash $path).Hash -cne (Get-FileHash $repeat).Hash) { throw "$name $stage IR is not deterministic" }
            $module = [System.IO.File]::ReadAllText($path) | ConvertFrom-Json
            $types = @{}
            foreach ($type in $module.types) { $types[$type.id] = $type }
            $members = @($module.intrinsicCalls | Where-Object { $_.kindName -in $familyKinds })
            if ($members.Count -eq 0) { throw "$name has no closed collection member identities" }
            foreach ($member in $members) {
                $receiver = $types[$member.receiverType]
                if ($member.admissionName -ne "verified" -or $member.errorAbiName -ne "no-raise" -or
                    $receiver.cAbi -ne 26 -or $receiver.ownershipAbi -ne 2 -or $receiver.nullAbi -ne 2 -or
                    $receiver.collectionAbiName -notin @("array", "list", "set", "dict") -or
                    $receiver.collectionAbiName -ne $member.collectionAbiName) {
                    throw "$name lost its exact verified static collection shape"
                }
                if ($member.kindName -eq "typed-collection-member") {
                    $operations[$member.typedCollectionOpName] = $true
                    $result = $types[$member.resultType]
                    $owned = $member.typedCollectionOpName -in @("pop", "take") -and $result.ownershipAbi -eq 2
                    if (($member.resultAbiName -eq "owned") -ne $owned) { throw "$name lost its element transfer/result ABI" }
                    if (@($receiver.params).Count -eq 0 -and $member.typedCollectionOpName -in @("pop", "take") -and
                        ($result.cAbi -ne 26 -or $result.collectionAbiName -ne "none" -or
                         $result.typeValueId -ne $module.runtimeTypeMap.fallback)) {
                        throw "$name erased element result is not the canonical owned dynamic value"
                    }
                }
            }
            if ($name -eq "xlir_erased_collection_members") {
                foreach ($shape in @("array", "list", "set", "dict")) {
                    if (@($members | Where-Object {
                        $types[$_.receiverType].kind -eq 1 -and
                        $types[$_.receiverType].name -eq $shape -and
                        @($types[$_.receiverType].params).Count -eq 0
                    }).Count -eq 0) { throw "$name erased $shape identity was replaced with a generic or dynamic type" }
                }
            }
            if ($stage -eq "lowered") {
                $values = @{}
                foreach ($instruction in $module.instructions) {
                    if ($instruction.result -ne 0) { $values[$instruction.result] = $instruction }
                }
                $calls = @($module.instructions | Where-Object { $_.runtimeOpName -eq "xvalue-typed-collection-method" })
                if ($calls.Count -eq 0) { throw "$name has no physical collection member calls" }
                foreach ($call in $calls) {
                    $receiver = $types[$values[$call.operands[0]].type]
                    if ($receiver.collectionAbiName -ne $call.collectionAbiName) { throw "$name loses receiver shape during lowering" }
                    if ($call.typedCollectionOpName -in @("insert", "index-of") -and @($receiver.params).Count -eq 0) {
                        $boxed = $types[$values[$call.operands[$call.operands.Count - 1]].type]
                        if ($boxed.cAbi -ne 26 -or $boxed.collectionAbiName -ne "none" -or
                            $boxed.typeValueId -ne $module.runtimeTypeMap.fallback) {
                            throw "$name boxed erased item was mislabeled as its collection"
                        }
                    }
                }
                if (@($module.instructions | Where-Object { $_.runtimeOpName -eq "xvalue-dynamic-collection-method" }).Count -ne 0) {
                    throw "$name regressed to open dynamic member dispatch"
                }
            }
        }
        $cPath = Join-Path $outputDir "$name.c"
        $repeat = Join-Path $outputDir "$name.repeat.c"
        Invoke-Captured $xl ('cgen "{0}" --emit "{1}"' -f $source,$cPath) 0 $null
        Invoke-Captured $xl ('cgen "{0}" --backend xlir --emit "{1}"' -f $source,$repeat) 0 $null
        if ((Get-FileHash $cPath).Hash -cne (Get-FileHash $repeat).Hash) { throw "$name C is not deterministic" }
        if ([System.IO.File]::ReadAllText($cPath) -match '<[^>]*(?:xrt|xruntime|xl_runtime)\.h>') { throw "$name imports a private implementation SDK" }
        $strictExe = Join-Path $outputDir "$name.strict.exe"
        $command = 'call test\suites\common.bat && !XL6_CGEN_CC! !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -DXLIR_GENERATED_MODULE=\"{0}\" test\cases\xlir_generated_module_host.c test\cases\xlir_json_stream_lifetime_host.c !XL6_CGEN_LINKS! -o "{1}"' -f $cPath.Replace('\', '/'),$strictExe
        & cmd.exe /d /v:on /c $command
        if ($LASTEXITCODE -ne 0) { throw "$name strict standalone C failed" }
        Invoke-Captured $strictExe "" 0 $expected
        Invoke-Captured $xl ('run "{0}" --backend xlir --link test/cases/xlir_json_stream_lifetime_host.c' -f $source) 0 $expected
        $exe = Join-Path $outputDir "$name.xlir.exe"
        Invoke-Captured $xl ('build "{0}" --backend xlir --link test/cases/xlir_json_stream_lifetime_host.c -o "{1}"' -f $source,$exe) 0 $null
        Invoke-Captured $exe "" 0 $expected
        Write-Host "$($name): exact Typed/lowered contract, deterministic strict C and canonical run/build verified"
    }
    foreach ($operation in @("pop", "take", "clear", "capacity", "reserve", "resize", "trim", "trim-to", "swap", "insert", "index-of", "reverse", "is-disjoint")) {
        if (!$operations.ContainsKey($operation)) { throw "Collection gate misses canonical operation: $operation" }
    }
} finally { Pop-Location }
Write-Host "XLIR erased collection members ok: four preserved shapes, canonical boxed element ownership, one shared source/runtime signature table, deterministic IR/C and strict C, temporary receiver reclamation, and existing typed-method regression."
