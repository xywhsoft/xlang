$script:XlirResidentObjectsImplementation=$PSCommandPath
. (Join-Path $PSScriptRoot 'xlir_resident_closure.ps1')

function Get-XlirFixtureResidentObjects([string]$Root,[string]$PublicDirectory,
    [string]$Directory,[string[]]$Fixtures) {
    if(!$Fixtures.Count){throw 'Resident object selection needs explicit fixtures'}
    $selectedRoot=(Resolve-Path -LiteralPath $Root).Path
    $public=Get-Content -LiteralPath (Join-Path $PublicDirectory 'build.json') -Raw|ConvertFrom-Json
    $closure=Get-XlirResidentClosure -Root $selectedRoot -Directory $Directory -PublicDirectory $PublicDirectory `
        -Fixtures $Fixtures -NativeProvider 'tools/xrt_single_header_impl.c'
    # A full source cohort supplies one object per physical translation unit.
    # Select the verified fixture plan's closed sources, rather than linking
    # every runtime object (including unrelated embedded compiler policies).
    $sources=@('tools/xrt_single_header_impl.c')+@($closure.Sources)
    $objects=@();$records=@($closure.Inputs)
    foreach($source in $sources){
        $path=[IO.Path]::GetFullPath((Join-Path $selectedRoot $source))
        $matches=@($public.Objects|Where-Object {
            [IO.Path]::GetFullPath($_.Source).Equals($path,[StringComparison]::OrdinalIgnoreCase)
        })
        if($matches.Count-ne1){throw "Missing or ambiguous resident source object: $source"}
        $record=$matches[0]
        if((Get-FileHash -LiteralPath $record.Path).Hash-ine$record.Sha256){throw "Resident object changed: $source"}
        $objects+=@($record.Path);$records+=@($record)
    }
    if(@($objects|Sort-Object -Unique).Count-ne$objects.Count){throw 'Duplicate resident object provider'}
    $records+=@([pscustomobject]@{Path=$script:XlirResidentObjectsImplementation;
        Sha256=(Get-FileHash -LiteralPath $script:XlirResidentObjectsImplementation).Hash})
    [pscustomobject]@{Objects=$objects;Sources=$sources;Inputs=$records;Closure=$closure}
}
