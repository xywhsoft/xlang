param(
    [Parameter(Mandatory=$true)][string]$PublicDirectory,
    [Parameter(Mandatory=$true)][string]$ContractDirectory,
    [Parameter(Mandatory=$true)][string]$Directory,
    [string]$PublicationContractDriver = '',
    [string]$ClosureContractDriver = '',
    [switch]$ContinuationScopes,
    [string]$ClassStorageContractDriver = '',
    [switch]$ClassCopyStorage,
    [switch]$ResidentOwnershipScopes,
    [switch]$CellStorage,
    [switch]$SlotTransfers,
    [switch]$GeneratorStorage,
    [switch]$AsyncStorage,
    [switch]$TaskStorage,
    [switch]$FutureCallbackStorage,
    [switch]$OwnershipPlans,
    [switch]$ModuleCycles,
    [switch]$ClassCallableCycles,
    [switch]$FutureRetirement,
    [switch]$ModulePreparation,
    [switch]$PendingModulePreparation,
    [ValidateSet('discovery','verification')][string]$Phase = 'discovery',
    [ValidateRange(1,8)][int]$MaxParallel = 3
)
$ErrorActionPreference = 'Stop'
$rootPath = Split-Path -Parent $PSScriptRoot
Push-Location $rootPath
try {
    # All six gates have been audited for private generated artifacts, headers
    # and native/TCC temporary files. The resource declarations below apply to
    # these exact scripts, not arbitrary checks with similar names.
    $outputPath = [IO.Path]::GetFullPath($Directory)
    if (Test-Path -LiteralPath $outputPath) { throw 'Use a fresh ownership batch directory' }
    $PublicDirectory = [IO.Path]::GetRelativePath($rootPath, [IO.Path]::GetFullPath($PublicDirectory))
    $ContractDirectory = [IO.Path]::GetRelativePath($rootPath, [IO.Path]::GetFullPath($ContractDirectory))
    $Directory = [IO.Path]::GetRelativePath($rootPath, $outputPath)
    foreach ($relative in @($PublicDirectory, $ContractDirectory, $Directory)) {
        if ($relative.StartsWith('..') -or [IO.Path]::IsPathRooted($relative)) { throw 'Ownership batch paths must remain within the project' }
    }
    $generationDriver = Join-Path $ContractDirectory 'xlir_code_owner_binding_contract.exe'
    $frameDriver = Join-Path $ContractDirectory 'xlir_frame_ownership_contract.exe'
    $generatorDriver = Join-Path $ContractDirectory 'xlir_generator_ownership_contract.exe'
    $moduleDriver = Join-Path $ContractDirectory 'xlir_module_ownership_contract.exe'
    foreach ($file in @($generationDriver,$frameDriver,$generatorDriver,$moduleDriver,(Join-Path $PublicDirectory 'build.json'))) {
        if (!(Test-Path -LiteralPath $file -PathType Leaf)) { throw "Missing shared batch build input: $file" }
    }
    $gates = @(
        @{ Script='check_xlir_frame_ownership.ps1'; Resources=@('private-frame'); Arguments=@{
            PublicDirectory=$PublicDirectory; ContractDriver=$frameDriver; Directory=(Join-Path $Directory 'frame') } },
        @{ Script='check_xlir_generator_ownership.ps1'; Resources=@('private-generator'); Arguments=@{
            PublicDirectory=$PublicDirectory; ContractDriver=$generatorDriver; Directory=(Join-Path $Directory 'generator') } },
        @{ Script='check_xlir_module_ownership.ps1'; Resources=@('private-module'); Arguments=@{
            PublicDirectory=$PublicDirectory; ContractDriver=$generationDriver; IrContractDriver=$moduleDriver; Directory=(Join-Path $Directory 'module') } },
        @{ Script='check_xlir_object_escape.ps1'; Resources=@('private-objects'); Arguments=@{
            SourceDriver=$generationDriver; SourceRuntimeBuildManifest=(Join-Path $PublicDirectory 'build.json'); Directory=(Join-Path $Directory 'objects') } },
        @{ Script='check_xlir_code_finalizer.ps1'; Resources=@('private-finalizer'); Arguments=@{
            PublicDirectory=$PublicDirectory; Directory=(Join-Path $Directory 'finalizer') } },
        @{ Script='check_xlir_public_source_batch.ps1'; Resources=@('private-public'); Arguments=@{
            Directory=$PublicDirectory; OutputDirectory=(Join-Path $Directory 'public') } }
    )
    if ($FutureRetirement) {
        $gates += @{ Script='check_xlir_future_retirement.ps1'; Resources=@('private-future-retirement'); Arguments=@{
            PublicDirectory=$PublicDirectory; ContractDirectory=$ContractDirectory; Directory=(Join-Path $Directory 'future-retirement') } }
    }
    if ($ModulePreparation) {
        $gates += @{ Script='check_xlir_module_prepare.ps1'; Resources=@('private-module-preparation'); Arguments=@{
            PublicDirectory=$PublicDirectory; ContractDirectory=$ContractDirectory; Directory=(Join-Path $Directory 'module-preparation') } }
    }
    if ($PendingModulePreparation) {
        $gates += @{ Script='check_xlir_pending_module_preparation.ps1'; Resources=@('private-pending-module-preparation'); Arguments=@{
            PublicDirectory=$PublicDirectory; ContractDirectory=$ContractDirectory; Directory=(Join-Path $Directory 'pending-module-preparation') } }
    }
    # Additive gate: the original six gates and their denominators are intact.
    if ($PublicationContractDriver) {
        $publicationPath=[IO.Path]::GetFullPath($PublicationContractDriver)
        if (!(Test-Path -LiteralPath $publicationPath -PathType Leaf)) { throw 'Missing publication contract driver' }
        $gates += @{ Script='check_xlir_code_publication.ps1'; Resources=@('private-publication'); Arguments=@{
            PublicDirectory=$PublicDirectory; ContractDriver=$publicationPath; Directory=(Join-Path $Directory 'publication') } }
    }
    if ($ClosureContractDriver) {
        $closurePath=[IO.Path]::GetFullPath($ClosureContractDriver)
        if (!(Test-Path -LiteralPath $closurePath -PathType Leaf)) { throw 'Missing closure contract driver' }
        $gates += @{ Script='check_xlir_resident_closure.ps1'; Resources=@('private-closure'); Arguments=@{
            PublicDirectory=$PublicDirectory; ContractDriver=$closurePath; Directory=(Join-Path $Directory 'closure') } }
    }
    if ($ContinuationScopes) {
        $gates += @{ Script='check_xlir_continuation_scopes.ps1'; Resources=@('private-continuation-scopes'); Arguments=@{
            PublicDirectory=$PublicDirectory; Directory=(Join-Path $Directory 'continuation-scopes') } }
    }
    if ($ClassStorageContractDriver) {
        $classPath=[IO.Path]::GetFullPath($ClassStorageContractDriver)
        if (!(Test-Path -LiteralPath $classPath -PathType Leaf)) { throw 'Missing class storage contract driver' }
        $gates += @{ Script='check_xlir_class_storage.ps1'; Resources=@('private-class-storage'); Arguments=@{
            PublicDirectory=$PublicDirectory; ContractDriver=$classPath; Directory=(Join-Path $Directory 'class-storage') } }
    }
    if ($ClassCopyStorage) {
        $gates += @{ Script='check_xlir_class_copy_storage.ps1'; Resources=@('private-class-copy'); Arguments=@{
            PublicDirectory=$PublicDirectory; Directory=(Join-Path $Directory 'class-copy') } }
    }
    if ($ResidentOwnershipScopes) {
        $gates += @{ Script='check_xlir_resident_ownership_scopes.ps1'; Resources=@('private-resident-ownership-scopes'); Arguments=@{
            PublicDirectory=$PublicDirectory; Directory=(Join-Path $Directory 'resident-ownership-scopes') } }
    }
    if ($CellStorage) {
        $gates += @{ Script='check_xlir_cell_storage.ps1'; Resources=@('private-cell-storage'); Arguments=@{
            PublicDirectory=$PublicDirectory; ContractDirectory=$ContractDirectory; Directory=(Join-Path $Directory 'cell-storage') } }
    }
    if ($SlotTransfers) {
        $slotDriver=Join-Path $ContractDirectory 'xlir_slot_transfer_contract.exe'
        if (!(Test-Path -LiteralPath $slotDriver -PathType Leaf)) { throw 'Missing slot transfer contract driver' }
        $gates += @{ Script='check_xlir_slot_transfers.ps1'; Resources=@('private-slot-transfers'); Arguments=@{
            PublicDirectory=$PublicDirectory; ContractDriver=$slotDriver; Directory=(Join-Path $Directory 'slot-transfers') } }
    }
    if ($GeneratorStorage) {
        $gates += @{ Script='check_xlir_generator_storage.ps1'; Resources=@('private-generator-storage'); Arguments=@{
            PublicDirectory=$PublicDirectory; ContractDirectory=$ContractDirectory; Directory=(Join-Path $Directory 'generator-storage') } }
    }
    if ($AsyncStorage) {
        $asyncDriver=Join-Path $ContractDirectory 'xlir_async_storage_contract.exe'
        if (!(Test-Path -LiteralPath $asyncDriver -PathType Leaf)) { throw 'Missing async storage contract driver' }
        $gates += @{ Script='check_xlir_async_storage.ps1'; Resources=@('private-async-storage'); Arguments=@{
            PublicDirectory=$PublicDirectory; ContractDriver=$asyncDriver; Directory=(Join-Path $Directory 'async-storage') } }
    }
    if ($TaskStorage) {
        $gates += @{ Script='check_xlir_task_storage.ps1'; Resources=@('private-task-storage'); Arguments=@{
            PublicDirectory=$PublicDirectory; ContractDirectory=$ContractDirectory; Directory=(Join-Path $Directory 'task-storage') } }
    }
    if ($FutureCallbackStorage) {
        $gates += @{ Script='check_xlir_future_callback_storage.ps1'; Resources=@('private-future-callback-storage'); Arguments=@{
            PublicDirectory=$PublicDirectory; ContractDirectory=$ContractDirectory; Directory=(Join-Path $Directory 'future-callback-storage') } }
    }
    if ($OwnershipPlans) {
        $gates += @{ Script='check_xlir_ownership_plans.ps1'; Resources=@('private-ownership-plans'); Arguments=@{
            PublicDirectory=$PublicDirectory; Directory=(Join-Path $Directory 'ownership-plans') } }
    }
    if ($ModuleCycles) {
        $gates += @{ Script='check_xlir_module_cycles.ps1'; Resources=@('private-module-cycles'); Arguments=@{
            PublicDirectory=$PublicDirectory; ContractDriver=$generationDriver; Directory=(Join-Path $Directory 'module-cycles') } }
    }
    if ($ClassCallableCycles) {
        $gates += @{ Script='check_xlir_class_callable_cycles.ps1'; Resources=@('private-class-callable-cycles'); Arguments=@{
            PublicDirectory=$PublicDirectory; Directory=(Join-Path $Directory 'class-callable-cycles') } }
    }
    New-Item -ItemType Directory -Path $outputPath | Out-Null
    $manifestPath = Join-Path $outputPath 'manifest.json'
    $manifest = [ordered]@{ SchemaVersion=2; Name="ownership-$Phase"; Phase=$Phase; Gates=$gates }
    [IO.File]::WriteAllText($manifestPath, ($manifest | ConvertTo-Json -Depth 7), [Text.UTF8Encoding]::new($false))
    & (Join-Path $PSScriptRoot 'run_xlir_batch.ps1') -Root $rootPath -ManifestPath $manifestPath -ReportPath (Join-Path $outputPath 'results.json') -MaxParallel $MaxParallel
    exit $LASTEXITCODE
} finally { Pop-Location }
