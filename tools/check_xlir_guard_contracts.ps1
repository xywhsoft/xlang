param([string]$Root='',[string]$Report='')
$ErrorActionPreference='Stop'
if(!$Root){$Root=Split-Path -Parent $PSScriptRoot}
$Root=[IO.Path]::GetFullPath($Root)
$guard=Join-Path $Root 'tools/check_xlir_contract.ps1'
$tokens=$null;$errors=$null
$ast=[Management.Automation.Language.Parser]::ParseFile($guard,[ref]$tokens,[ref]$errors)
if($errors.Count){throw 'Invalid production guard syntax'}
$script:CheckedInputs=@{};$script:CheckedInputs[$guard]=(Get-FileHash -LiteralPath $guard).Hash
$PublicDirectory='';$skipped=0
# Initialize and check the real static guard context. Compiled catalog checks
# run in the normal gate, not recursively inside this guard's own unit test.
foreach($statement in $ast.EndBlock.Statements){
    $text=$statement.Extent.Text
    if($text.StartsWith('& "$PSScriptRoot/check_xlir_runtime_operand_catalog.ps1"')){++$skipped;continue}
    if($statement-is[Management.Automation.Language.FunctionDefinitionAst]-and$statement.Name-eq'Read-Utf8'){
        $text=$text.Replace('return [System.IO.File]::ReadAllText',
            '$script:CheckedInputs[$path]=(Get-FileHash -LiteralPath $path).Hash; return [System.IO.File]::ReadAllText')
    }
    . ([scriptblock]::Create($text))|Out-Null
}
if($skipped-ne 1){throw 'Expected one separately exercised compiled catalog gate'}
$script:Mutations=0
function Block([string]$marker){
    $found=@($ast.EndBlock.Statements|Where-Object {$_.Extent.Text.Contains($marker)})
    if($found.Count-ne 1){throw "Expected one actual production guard block: $marker"}
    [scriptblock]::Create($found[0].Extent.Text)
}
function Refused([string]$marker){
    $failed=$false
    try{& (Block $marker)}catch{if(!$_.Exception.Message.Contains($marker)){throw};$failed=$true}
    if(!$failed){throw "Guard accepted an invalid boundary: $marker"}
    ++$script:Mutations
}
function Remove-Witness([string]$variable,[string]$token,[string]$marker){
    $old=Get-Variable -Name $variable -Scope Script -ValueOnly
    if($old-isnot[string]-or!$old.Contains($token)){throw "Missing mutation witness: $variable / $token"}
    try{Set-Variable -Scope Script -Name $variable -Value $old.Replace($token,'REMOVED_GUARD_WITNESS');Refused $marker}
    finally{Set-Variable -Scope Script -Name $variable -Value $old}
}
$cases=@(
    @('dictRemoveDeclaration','.parameterDynamicBorrows={1,0}','Resident named adapters need'),
    @('codecDecodeDeclaration','T,Y,3,{S,I,I}','Resident named adapters need'),
    @('dynamicDropDeclaration','xl6DynamicFieldsHandlePolicyV1Get','Resident named adapters need'),
    @('cEmitter','xl6SetValueRemoveV1','mechanical collection-member runtime emission'),
    @('cEmitter','xl6ArrayConcatV1','mechanical collection-algebra emission'),
    @('cEmitter','xl6DynamicCollectionCallTakeV1','mechanical collection-method emission'),
    @('lower','XLIR borrowed safe-index result has no exact nullable projection ABI','safe-index exact projection lowering contract'),
    @('lower','xl6IrCanonicalCStringLiteral','lowering implementation is missing explicit semantic contract'),
    @('lower','XLIR await requires an exact resolved future result ABI','lowering implementation is missing explicit semantic contract'),
    @('lower','XL6_IR_INDEX_WRITE_XVALUE_OBJECT_STRING_SET_NEW','canonical IndexWriteAbi dispatch'),
    @('cEmitter','xl6ValueIteratorRuntimeV2Get()->Create','mechanical xvalue iterator emission'),
    @('cEmitter','xl6ValueContainsV1','mechanical xvalue membership emission'),
    @('membershipProvider','xl6SetErasedKeyV1','Resident membership must preserve'),
    @('membershipProvider','xl6ErrorScopePropagateFailure','Resident membership must preserve'),
    @('cEmitter','xl6ContinuationFrameFinishV1(frame, retired, &cancelled)','explicit async task ABI contract'),
    @('cEmitter','xl6SetValueAddTakeV1','deterministic backend contract'),
    @('lower','xl6IrLowerClassLayoutHasRuntimeIdentity(layout)','class value identity lowering contract'),
    @('cEmitter','xl6ValueDeepCloneV1','copy/clone C ABI contract'),
    @('taskSpawnClosureGate','XLIR task.spawn strict C runtime failed','task.spawn quality gate is incomplete'),
    @('lower','xl6IrChannelSendSpecResource','channel resource lowering contract'),
    @('channelNativeEmitter','spec->nativeSymbol','channel resource C boundary'),
    @('channelNativeEmitter','XL6_ChannelSlotV1','channel resource C boundary'),
    @('cEmitter','xl6MutexUnlockV1','synchronization resource C boundary'),
    @('syncProvider','owner->identity==identity && owner->kind==kind','Resident synchronization requires'),
    @('syncHeader','xl6SyncRetirementRetryV1','Resident synchronization requires'),
    @('cEmitter','xl6SequenceWaitQueueCreateTakeV1','special queue C boundary'),
    @('cEmitter','PopV1','special queue C boundary'),
    @('header','XL6_IR_RUNTIME_RESOURCE_OP_TREE_QUERY_##id','OrderedMap resource header contract'),
    @('cEmitter','xl6IrAvlKeyOrderAbi(module,ownerId)','OrderedMap mechanical C boundary'),
    @('cEmitter','xl6ClassRuntimeV3Get()->CommitTake','class lifecycle C boundary'),
    @('moduleTrace','xl6ExecutionSourceOwnershipV1(xlir_execution_source)','Module tracing must consume')
)
foreach($case in $cases){Remove-Witness $case[0] $case[1] $case[2]}
foreach($case in @(
    @('typedef struct xtemplateconfig','canonical public types'),
    @('xlir_sync_handle_ops','Resident synchronization requires'),
    @('xlir_special_queue_create','Concurrent queues require'),
    @('xlir_avl_handle_ops','OrderedMap storage and traversal algorithms'),
    @('xlir_xvalue_contains','Container membership algorithms must not return'))){
    $variable=if($case[0]-eq'typedef struct xtemplateconfig'){'templateBoundary'}else{'cEmitter'}
    $old=Get-Variable -Scope Script -Name $variable -ValueOnly
    try{Set-Variable -Scope Script -Name $variable -Value ($old+"`n"+$case[0]);Refused $case[1]}
    finally{Set-Variable -Scope Script -Name $variable -Value $old}
}

$script:Overrides=@{}
function Read-Utf8([string]$relative){
    $relative=$relative.Replace('\','/')
    if($script:Overrides.ContainsKey($relative)){return $script:Overrides[$relative]}
    $path=Join-Path $Root $relative
    $script:CheckedInputs[$path]=(Get-FileHash -LiteralPath $path).Hash
    [IO.File]::ReadAllText($path)
}
# The canonical bytes alias is read inline by the real guard, not from the
# combined Cgen text. Mutate that actual read, without touching its file.
$aliasPath='src/cgen/xl6_ir_cgen_emit_xvalue_core.c';$aliasSource=Read-Utf8 $aliasPath
if(!$aliasSource.Contains('typedef xbytesview xlir_bytes_view;')){throw 'Missing public bytes alias witness'}
try{
    $script:Overrides[$aliasPath]=$aliasSource.Replace('typedef xbytesview xlir_bytes_view;','REMOVED_PUBLIC_BYTES_ALIAS')
    Refused 'canonical public types'
}finally{$script:Overrides.Clear()}
$start=-1;$end=-1
for($n=0;$n-lt$ast.EndBlock.Statements.Count;++$n){
    $text=$ast.EndBlock.Statements[$n].Extent.Text
    if($text.StartsWith('$cEmitterPaths=')){$start=$n}
    if($text.StartsWith('$cEmitter=$cEmitterPieces -join')){$end=$n}
}
if($start-lt 0-or$end-le$start){throw 'Actual recursive Cgen closure is missing'}
function Discover-Cgen {
    for($n=$start;$n-le$end;++$n){. ([scriptblock]::Create($ast.EndBlock.Statements[$n].Extent.Text))}
    $script:cEmitter=$cEmitter;$script:cEmitterSeen=$cEmitterSeen
}
$leaf='src/cgen/xl6_ir_cgen_concurrent_queue_emit.c';$original=Read-Utf8 $leaf
try{
    $script:Overrides[$leaf]=$original+"`nXL6_AstNode * forbidden_frontend_pointer;"
    Discover-Cgen
    if(!$cEmitterSeen.ContainsKey($leaf)){throw 'Nested Cgen fragment escaped scanning'}
    Refused 'XLIR C emitter must not consume AST/HIR/semantic analysis or call legacy Cgen'
    $script:Overrides[$leaf]=$original+"`n"+'#include "src/cgen/xl6_ir_cgen_concurrent_queue_emit.c"'
    $count=$cEmitterSeen.Count;Discover-Cgen
    if($cEmitterSeen.Count-ne$count){throw 'Self include must not change unique Cgen closure'}
    if(@($cEmitterSeen.Keys|Where-Object {!$_.StartsWith('src/cgen/')} ).Count){throw 'Runtime implementations entered Cgen boundary'}
    ++$script:Mutations
}finally{$script:Overrides.Clear();Discover-Cgen}
& (Block 'XLIR C emitter must not consume AST/HIR/semantic analysis or call legacy Cgen')
$unchanged=!@($script:CheckedInputs.Keys|Where-Object {(Get-FileHash -LiteralPath $_).Hash-ne$script:CheckedInputs[$_]}).Count
if(!$unchanged){throw 'Guard unit inputs changed'}
$result=[pscustomobject]@{Complete=$true;StaticContextPassed=$true;RejectedMutations=$script:Mutations-1;
    CycleSafe=$true;CgenFiles=$cEmitterSeen.Count;CompiledCatalogExecuted=$false;Inputs=$script:CheckedInputs;
    InputsUnchanged=$unchanged;ReaderSha256=(Get-FileHash -LiteralPath $PSCommandPath).Hash;
    Scope='Actual static guard blocks and in-memory/path-reading mutations; independent normal compiled gate and language/product acceptance required'}
if($Report){[IO.File]::WriteAllText($Report,($result|ConvertTo-Json -Depth 6)+"`n",[Text.UTF8Encoding]::new($false))}
Write-Output "Core guard contracts: $($result.RejectedMutations) refusals, cycle-safe nested Cgen closure of $($result.CgenFiles) files; production files unchanged"
