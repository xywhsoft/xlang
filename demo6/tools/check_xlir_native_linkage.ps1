param([Parameter(Mandatory=$true)][string]$PublicDirectory,
    [Parameter(Mandatory=$true)][string]$NativeRoot,
    [Parameter(Mandatory=$true)][string]$Directory)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'xlir_native_process.ps1')
$rootPath=Split-Path -Parent $PSScriptRoot
Push-Location $rootPath
try {
    $outputPath=[IO.Path]::GetFullPath($Directory)
    if(Test-Path -LiteralPath $outputPath){throw 'Preserve prior native linkage evidence'}
    New-Item -ItemType Directory -Path $outputPath | Out-Null
    $public=Get-Content -LiteralPath (Join-Path $PublicDirectory 'build.json') -Raw | ConvertFrom-Json
    $nativeObject=@($public.Objects | Where-Object {[IO.Path]::GetFileName($_.Path)-eq'xrt_single_header_impl.o'})
    if($nativeObject.Count-ne1){throw 'Require the actual full-product native object'}
    $inputs=@($public.Inputs)+@($public.Objects)+@(
        'tools/gen_xrt_linkage_catalog.py','test/cases/xrt_linkage_catalog_parser.py',$PSCommandPath,
        (Join-Path $NativeRoot 'config/modules.json'),(Join-Path $NativeRoot 'extlibs/xruntime/config/modules.json'),
        (Get-Command nm).Source,(Get-Command python).Source | ForEach-Object {
            $file=Get-Item -LiteralPath $_;[pscustomobject]@{Path=$file.FullName;Sha256=(Get-FileHash -LiteralPath $file.FullName).Hash}
        })
    foreach($row in $inputs){if((Get-FileHash -LiteralPath $row.Path).Hash-ne$row.Sha256){throw "Stale native linkage input: $($row.Path)"}}
    $catalogExit=Invoke-XlirNativeToLog python @('tools/gen_xrt_linkage_catalog.py','--native-root',$NativeRoot,
        '--cache','build/cache/xrt_linkage_v2','--check','--report',"$outputPath/catalog.json") "$outputPath/catalog.log"
    $parserExit=Invoke-XlirNativeToLog python @('test/cases/xrt_linkage_catalog_parser.py') "$outputPath/parser.log"
    & nm -g --defined-only $nativeObject[0].Path 1> "$outputPath/native-exports.txt" 2> "$outputPath/nm.log"
    $nmExit=$LASTEXITCODE
    $actual=@(Get-Content -LiteralPath "$outputPath/native-exports.txt" | ForEach-Object {if($_-match '\b[TRD] (xrt\w+)$'){$Matches[1]}} | Sort-Object -Unique)
    $declared=@([regex]::Matches((Get-Content -LiteralPath src/api/xl_xrt_symbols.inc -Raw),'XL_XRT_SYMBOL\((\w+)\)') | ForEach-Object {$_.Groups[1].Value})
    $missing=@($declared | Where-Object {$_-notin$actual})
    $catalog=$null;$ownershipChecks=@()
    if($catalogExit-eq0){
        $catalog=Get-Content -LiteralPath "$outputPath/catalog.json" -Raw | ConvertFrom-Json
        $expectations=@{
            xrtEncodingUnitSize='XRT_MODULE_CHARSET';xrtUtf8Substr='XRT_MODULE_UNICODE_TEXT';
            xrtChannelRecvCancel='XRT_MODULE_CHANNEL_CANCEL';xrtBufferFromHex='XRT_MODULE_BUFFER_HEX';
            xrtBufferFromBase64='XRT_MODULE_BUFFER_BASE64';xrtMallocAt='XRT_MODULE_MEMORY_DEBUG'
        }
        foreach($symbol in $expectations.Keys){
            $row=@($catalog.Definitions | Where-Object Symbol -eq $symbol)
            $ownershipChecks+=[pscustomobject]@{Symbol=$symbol;Expected=$expectations[$symbol];
                Actual=@($row[0].Modules);Complete=($row.Count-eq1-and$row[0].Modules.Count-eq1-and$row[0].Modules[0]-eq$expectations[$symbol])}
        }
        $core=@($catalog.Definitions | Where-Object Symbol -eq 'xrtMalloc')
        $ownershipChecks+=[pscustomobject]@{Symbol='xrtMalloc';Expected='unconditional';Actual=@($core[0].Modules);Complete=($core.Count-eq1-and!$core[0].Modules.Count)}
    }
    $unchanged=!@($inputs | Where-Object {(Get-FileHash -LiteralPath $_.Path).Hash-ne$_.Sha256}).Count
    $complete=$unchanged-and$catalogExit-eq0-and$parserExit-eq0-and$nmExit-eq0-and!$missing.Count-and
        $catalog.Symbols-eq$declared.Count-and$ownershipChecks.Count-eq7-and!@($ownershipChecks | Where-Object {!$_.Complete}).Count
    $report=[pscustomobject]@{Complete=$complete;Inputs=$inputs;InputsUnchanged=$unchanged;
        CatalogExit=$catalogExit;ParserExit=$parserExit;NmExit=$nmExit;Declared=$declared.Count;
        ActualPublic=@($actual | Where-Object {$_-in$declared}).Count;Missing=$missing;Ownership=$ownershipChecks;
        Catalog="$outputPath/catalog.json";NativeObject=$nativeObject[0];
        Scope='Full shipped product public definitions against the current-source native object; per-module preprocessing and exact source provenance on Windows GCC. RuntimeOp/bridge requirements and cross-platform/release acceptance remain separate.'}
    [IO.File]::WriteAllText("$outputPath/results.json",($report | ConvertTo-Json -Depth 8),[Text.UTF8Encoding]::new($false))
    if(!$complete){throw 'Native linkage failures retained'}
    Write-Output "Native linkage: $($report.ActualPublic) actual public exports; 7 provider boundary checks"
} finally {Pop-Location}
