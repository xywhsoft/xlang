param(
    [string]$Root = "",
    [switch]$Artifacts
)

$ErrorActionPreference = "Stop"

if ( $Root -eq "" ) {
    $Root = Split-Path -Parent $PSScriptRoot
}

$testBat = Join-Path $Root "test.bat"
$testSuiteDir = Join-Path $Root "test\suites"
$tmpDir = Join-Path $Root "build\test"
$failures = New-Object System.Collections.Generic.List[string]
$strictUtf8 = New-Object System.Text.UTF8Encoding($false, $true)
$looseUtf8 = [System.Text.Encoding]::UTF8
$allowEmpty = New-Object System.Collections.Generic.HashSet[string]
$allowEmpty.Add("invalid_range.err.txt") | Out-Null
$allowEmpty.Add("sem_unclosed_comment.err.txt") | Out-Null
$forbiddenArtifactPatterns = @(
    ":0:0:",
    "code generation error",
    "unsupported statement kind in C generator",
    "unsupported expression kind in C generator",
    "unsupported type in C generation",
    "cannot box unsupported type into var",
    "unsupported typed index compound assignment operator"
)

function Join-CodePoints($codes)
{
    $builder = New-Object System.Text.StringBuilder
    foreach ( $code in $codes ) {
        [void]$builder.Append([char]$code)
    }
    return $builder.ToString()
}

$diagSyntaxError = Join-CodePoints @(0x8BED, 0x6CD5, 0x9519, 0x8BEF)
$diagUnknownType = Join-CodePoints @(0x672A, 0x77E5, 0x7C7B, 0x578B)
$diagAsyncGeneratorUnsupported = Join-CodePoints @(0x5F53, 0x524D, 0x4E0D, 0x652F, 0x6301, 0x0020, 0x0061, 0x0073, 0x0079, 0x006E, 0x0063, 0x0020, 0x0067, 0x0065, 0x006E, 0x0065, 0x0072, 0x0061, 0x0074, 0x006F, 0x0072)
$diagAsyncMultiReturn = Join-CodePoints @(0x0061, 0x0073, 0x0079, 0x006E, 0x0063, 0x0020, 0x51FD, 0x6570, 0x5F53, 0x524D, 0x4E0D, 0x652F, 0x6301, 0x0020, 0x006D, 0x0075, 0x006C, 0x0074, 0x0069, 0x002D, 0x0072, 0x0065, 0x0074, 0x0075, 0x0072, 0x006E, 0xFF1B, 0x53EA, 0x80FD, 0x7701, 0x7565, 0x8FD4, 0x56DE, 0x503C, 0x6216, 0x8FD4, 0x56DE, 0x4E00, 0x4E2A, 0x503C)
$diagAsyncReturnFuture = Join-CodePoints @(0x0061, 0x0073, 0x0079, 0x006E, 0x0063, 0x0020, 0x51FD, 0x6570, 0x8FD4, 0x56DE, 0x7C7B, 0x578B, 0x5E94, 0x58F0, 0x660E, 0x4E3A, 0x5B8C, 0x6210, 0x540E, 0x7684, 0x503C, 0x7C7B, 0x578B, 0xFF0C, 0x4E0D, 0x5E94, 0x76F4, 0x63A5, 0x58F0, 0x660E, 0x0020, 0x0066, 0x0075, 0x0074, 0x0075, 0x0072, 0x0065, 0x002F, 0x0074, 0x0061, 0x0073, 0x006B)
$diagAwaitNonFuture = Join-CodePoints @(0x0061, 0x0077, 0x0061, 0x0069, 0x0074, 0x0020, 0x53EA, 0x80FD, 0x7528, 0x4E8E, 0x0020, 0x0066, 0x0075, 0x0074, 0x0075, 0x0072, 0x0065, 0x003C, 0x0054, 0x003E, 0x0020, 0x6216, 0x0020, 0x0074, 0x0061, 0x0073, 0x006B, 0x003C, 0x0054, 0x003E, 0x0020, 0x8868, 0x8FBE, 0x5F0F)
$diagChannelPlainValueElement = Join-CodePoints @(0x0063, 0x0068, 0x0061, 0x006E, 0x006E, 0x0065, 0x006C, 0x003C, 0x0054, 0x003E, 0x0020, 0x5F53, 0x524D, 0x53EA, 0x652F, 0x6301, 0x0020, 0x0062, 0x006F, 0x006F, 0x006C, 0x3001, 0x6574, 0x6570, 0x3001, 0x6D6E, 0x70B9, 0x3001, 0x0073, 0x0074, 0x0072, 0x0069, 0x006E, 0x0067, 0x3001, 0x0074, 0x0069, 0x006D, 0x0065, 0x3001, 0x0070, 0x006F, 0x0069, 0x006E, 0x0074, 0x0020, 0x7B49, 0x5177, 0x5907, 0x7A33, 0x5B9A, 0x6D88, 0x606F, 0x590D, 0x5236, 0x8BED, 0x4E49, 0x7684, 0x5143, 0x7D20, 0x7C7B, 0x578B)
$diagChannelRecvTargetType = Join-CodePoints @(0x0063, 0x0068, 0x0061, 0x006E, 0x006E, 0x0065, 0x006C, 0x002E, 0x0072, 0x0065, 0x0063, 0x0076, 0x0049, 0x006E, 0x0074, 0x006F, 0x0020, 0x76EE, 0x6807, 0x53D8, 0x91CF, 0x7C7B, 0x578B, 0x5FC5, 0x987B, 0x7B49, 0x4E8E, 0x0020, 0x0063, 0x0068, 0x0061, 0x006E, 0x006E, 0x0065, 0x006C, 0x003C, 0x0054, 0x003E, 0x0020, 0x7684, 0x5143, 0x7D20, 0x7C7B, 0x578B)
$diagChannelTryRecvTargetType = Join-CodePoints @(0x0063, 0x0068, 0x0061, 0x006E, 0x006E, 0x0065, 0x006C, 0x002E, 0x0074, 0x0072, 0x0079, 0x0052, 0x0065, 0x0063, 0x0076, 0x0049, 0x006E, 0x0074, 0x006F, 0x0020, 0x76EE, 0x6807, 0x53D8, 0x91CF, 0x7C7B, 0x578B, 0x5FC5, 0x987B, 0x7B49, 0x4E8E, 0x0020, 0x0063, 0x0068, 0x0061, 0x006E, 0x006E, 0x0065, 0x006C, 0x003C, 0x0054, 0x003E, 0x0020, 0x7684, 0x5143, 0x7D20, 0x7C7B, 0x578B)
$diagChannelSendType = Join-CodePoints @(0x0063, 0x0068, 0x0061, 0x006E, 0x006E, 0x0065, 0x006C, 0x002E, 0x0073, 0x0065, 0x006E, 0x0064, 0x0020, 0x53C2, 0x6570, 0x7C7B, 0x578B, 0x5FC5, 0x987B, 0x5339, 0x914D, 0x0020, 0x0063, 0x0068, 0x0061, 0x006E, 0x006E, 0x0065, 0x006C, 0x003C, 0x0054, 0x003E, 0x0020, 0x7684, 0x5143, 0x7D20, 0x7C7B, 0x578B)
$diagChannelNamedReceiver = Join-CodePoints @(0x0063, 0x0068, 0x0061, 0x006E, 0x006E, 0x0065, 0x006C, 0x0020, 0x0072, 0x0065, 0x0063, 0x0065, 0x0069, 0x0076, 0x0065, 0x0072, 0x0020, 0x5F53, 0x524D, 0x53EA, 0x652F, 0x6301, 0x5177, 0x540D, 0x53D8, 0x91CF)
$diagGeneratorPlainValueElement = Join-CodePoints @(0x0067, 0x0065, 0x006E, 0x0065, 0x0072, 0x0061, 0x0074, 0x006F, 0x0072, 0x003C, 0x0054, 0x003E, 0x0020, 0x5F53, 0x524D, 0x53EA, 0x652F, 0x6301, 0x0020, 0x0062, 0x006F, 0x006F, 0x006C, 0x3001, 0x6574, 0x6570, 0x3001, 0x6D6E, 0x70B9, 0x3001, 0x0073, 0x0074, 0x0072, 0x0069, 0x006E, 0x0067, 0x3001, 0x0074, 0x0069, 0x006D, 0x0065, 0x3001, 0x0070, 0x006F, 0x0069, 0x006E, 0x0074, 0x0020, 0x7B49, 0x5177, 0x5907, 0x7A33, 0x5B9A, 0x590D, 0x5236, 0x8BED, 0x4E49, 0x7684, 0x5143, 0x7D20, 0x7C7B, 0x578B)
$diagGeneratorParamPlainValue = Join-CodePoints @(0x0067, 0x0065, 0x006E, 0x0065, 0x0072, 0x0061, 0x0074, 0x006F, 0x0072, 0x0020, 0x0066, 0x0072, 0x0061, 0x006D, 0x0065, 0x0020, 0x53C2, 0x6570, 0x7C7B, 0x578B, 0x7F3A, 0x5C11, 0x7A33, 0x5B9A, 0x7684, 0x590D, 0x5236, 0x548C, 0x91CA, 0x653E, 0x8BED, 0x4E49)
$diagGeneratorYieldType = Join-CodePoints @(0x0079, 0x0069, 0x0065, 0x006C, 0x0064, 0x0020, 0x8868, 0x8FBE, 0x5F0F, 0x7C7B, 0x578B, 0x548C, 0x0020, 0x0067, 0x0065, 0x006E, 0x0065, 0x0072, 0x0061, 0x0074, 0x006F, 0x0072, 0x003C, 0x0054, 0x003E, 0x0020, 0x5143, 0x7D20, 0x7C7B, 0x578B, 0x4E0D, 0x5339, 0x914D)
$diagYieldOutsideGenerator = Join-CodePoints @(0x0079, 0x0069, 0x0065, 0x006C, 0x0064, 0x0020, 0x5FC5, 0x987B, 0x4F4D, 0x4E8E, 0x8FD4, 0x56DE, 0x0020, 0x0067, 0x0065, 0x006E, 0x0065, 0x0072, 0x0061, 0x0074, 0x006F, 0x0072, 0x003C, 0x0054, 0x003E, 0x0020, 0x7684, 0x51FD, 0x6570, 0x4F53, 0x5185)
$diagGeneratorReturnValue = Join-CodePoints @(0x0067, 0x0065, 0x006E, 0x0065, 0x0072, 0x0061, 0x0074, 0x006F, 0x0072, 0x0020, 0x51FD, 0x6570, 0x4E2D, 0x7684, 0x0020, 0x0072, 0x0065, 0x0074, 0x0075, 0x0072, 0x006E, 0x0020, 0x4E0D, 0x80FD, 0x643A, 0x5E26, 0x8FD4, 0x56DE, 0x503C)
$diagNonCopyableInit = Join-CodePoints @(0x4E0D, 0x53EF, 0x590D, 0x5236, 0x7C7B, 0x578B, 0x4E0D, 0x80FD, 0x4ECE, 0x5DF2, 0x6709, 0x503C, 0x521D, 0x59CB, 0x5316)
$diagUnknownBaseClass = Join-CodePoints @(0x672A, 0x77E5, 0x7236, 0x7C7B)
$diagBaseCreateMissingInCreate = Join-CodePoints @(0x7236, 0x7C7B, 0x0020, 0x0063, 0x0072, 0x0065, 0x0061, 0x0074, 0x0065, 0x0020, 0x9700, 0x8981, 0x53C2, 0x6570, 0xFF0C, 0x5B50, 0x7C7B, 0x0020, 0x0063, 0x0072, 0x0065, 0x0061, 0x0074, 0x0065, 0x0020, 0x5FC5, 0x987B, 0x663E, 0x5F0F, 0x8C03, 0x7528, 0x0020, 0x0062, 0x0061, 0x0073, 0x0065, 0x0028, 0x002E, 0x002E, 0x002E, 0x0029)
$diagBaseCreateMissingDefault = Join-CodePoints @(0x7236, 0x7C7B, 0x0020, 0x0063, 0x0072, 0x0065, 0x0061, 0x0074, 0x0065, 0x0020, 0x9700, 0x8981, 0x53C2, 0x6570, 0xFF0C, 0x5B50, 0x7C7B, 0x5FC5, 0x987B, 0x58F0, 0x660E, 0x0020, 0x0063, 0x0072, 0x0065, 0x0061, 0x0074, 0x0065, 0x0020, 0x5E76, 0x663E, 0x5F0F, 0x8C03, 0x7528, 0x0020, 0x0062, 0x0061, 0x0073, 0x0065, 0x0028, 0x002E, 0x002E, 0x002E, 0x0029)
$diagBaseOutsideCreate = Join-CodePoints @(0x0062, 0x0061, 0x0073, 0x0065, 0x0028, 0x002E, 0x002E, 0x002E, 0x0029, 0x0020, 0x53EA, 0x80FD, 0x5728, 0x0020, 0x0063, 0x006C, 0x0061, 0x0073, 0x0073, 0x0020, 0x7684, 0x0020, 0x0063, 0x0072, 0x0065, 0x0061, 0x0074, 0x0065, 0x0020, 0x4F53, 0x5185, 0x4F7F, 0x7528)
$diagDestroyNoParams = Join-CodePoints @(0x0064, 0x0065, 0x0073, 0x0074, 0x0072, 0x006F, 0x0079, 0x0020, 0x4E0D, 0x80FD, 0x58F0, 0x660E, 0x53C2, 0x6570)
$diagNativeBadSpecial = "class extern without func can only declare create/destroy"
$diagNativeDuplicateCSymbol = "same native-backed class cannot bind the same C symbol twice"
$diagNativeBindPoint = "native-backed class handle binding argument must be point"
$diagCreateOverloadMissing = Join-CodePoints @(0x6CA1, 0x6709, 0x5339, 0x914D, 0x7684, 0x0020, 0x0063, 0x0072, 0x0065, 0x0061, 0x0074, 0x0065, 0x0020, 0x91CD, 0x8F7D)
$diagNativeOverloadConflict = "duplicate or ambiguous class member overload signature"
$diagClosureCaptureNonCopyable = Join-CodePoints @(0x8BE5, 0x53D8, 0x91CF, 0x7C7B, 0x578B, 0x4E0D, 0x80FD, 0x88AB, 0x533F, 0x540D, 0x51FD, 0x6570, 0x6309, 0x503C, 0x6355, 0x83B7)
$diagClosureCaptureReadonly = Join-CodePoints @(0x6355, 0x83B7, 0x53D8, 0x91CF, 0x5728, 0x533F, 0x540D, 0x51FD, 0x6570, 0x4F53, 0x5185, 0x53EA, 0x8BFB)
$diagNonCopyableAssign = Join-CodePoints @(0x4E0D, 0x53EF, 0x590D, 0x5236, 0x7C7B, 0x578B, 0x4E0D, 0x80FD, 0x901A, 0x8FC7, 0x8D4B, 0x503C, 0x590D, 0x5236)
$diagNonCopyableParam = Join-CodePoints @(0x4E0D, 0x53EF, 0x590D, 0x5236, 0x7C7B, 0x578B, 0x4E0D, 0x80FD, 0x4F5C, 0x4E3A, 0x6309, 0x503C, 0x53C2, 0x6570)
$diagNonCopyableReturn = Join-CodePoints @(0x4E0D, 0x53EF, 0x590D, 0x5236, 0x7C7B, 0x578B, 0x4E0D, 0x80FD, 0x4F5C, 0x4E3A, 0x8FD4, 0x56DE, 0x503C, 0x7C7B, 0x578B)
$diagNonCopyableReturnExisting = Join-CodePoints @(0x4E0D, 0x53EF, 0x590D, 0x5236, 0x7C7B, 0x578B, 0x4E0D, 0x80FD, 0x4ECE, 0x5DF2, 0x6709, 0x503C, 0x8FD4, 0x56DE)
$diagInitTypeMismatch = Join-CodePoints @(0x521D, 0x59CB, 0x5316, 0x8868, 0x8FBE, 0x5F0F, 0x7C7B, 0x578B, 0x548C, 0x58F0, 0x660E, 0x7C7B, 0x578B, 0x4E0D, 0x5339, 0x914D)
$diagReturnTypeMismatch = Join-CodePoints @(0x0072, 0x0065, 0x0074, 0x0075, 0x0072, 0x006E, 0x0020, 0x8868, 0x8FBE, 0x5F0F, 0x7C7B, 0x578B, 0x548C, 0x51FD, 0x6570, 0x8FD4, 0x56DE, 0x7C7B, 0x578B, 0x4E0D, 0x5339, 0x914D)
$diagGenericArityMismatch = Join-CodePoints @(0x6CDB, 0x578B, 0x53C2, 0x6570, 0x6570, 0x91CF, 0x4E0D, 0x5339, 0x914D)
$diagGenericParamDuplicate = Join-CodePoints @(0x91CD, 0x590D, 0x58F0, 0x660E, 0x6CDB, 0x578B, 0x53C2, 0x6570)
$diagGenericExplicitOverloadMissing = Join-CodePoints @(0x6CA1, 0x6709, 0x5339, 0x914D, 0x7684, 0x663E, 0x5F0F, 0x6CDB, 0x578B, 0x51FD, 0x6570, 0x91CD, 0x8F7D)
$diagFunctionOverloadMissing = Join-CodePoints @(0x6CA1, 0x6709, 0x5339, 0x914D, 0x7684, 0x51FD, 0x6570, 0x91CD, 0x8F7D)
$diagGenericFunctionInstanceMissing = Join-CodePoints @(0x627E, 0x4E0D, 0x5230, 0x5339, 0x914D, 0x7684, 0x663E, 0x5F0F, 0x6CDB, 0x578B, 0x51FD, 0x6570, 0x5B9E, 0x4F8B)
$diagFunctionNameCompareOperand = Join-CodePoints @(0x51FD, 0x6570, 0x540D, 0x4E0D, 0x80FD, 0x4F5C, 0x4E3A, 0x666E, 0x901A, 0x6BD4, 0x8F83, 0x64CD, 0x4F5C, 0x6570)
$diagGenericClassArgArity = Join-CodePoints @(0x6CDB, 0x578B, 0x0020, 0x0063, 0x006C, 0x0061, 0x0073, 0x0073, 0x0020, 0x5B9E, 0x53C2, 0x6570, 0x91CF, 0x4E0D, 0x5339, 0x914D)
$diagOpenGenericClassCtor = Join-CodePoints @(0x5F00, 0x653E, 0x6CDB, 0x578B, 0x0020, 0x0063, 0x006C, 0x0061, 0x0073, 0x0073, 0x0020, 0x5FC5, 0x987B, 0x663E, 0x5F0F, 0x5B9E, 0x4F8B, 0x5316, 0x540E, 0x6784, 0x9020)
$diagGenericClassTypeParamArity = Join-CodePoints @(0x6CDB, 0x578B, 0x0020, 0x0063, 0x006C, 0x0061, 0x0073, 0x0073, 0x0020, 0x7C7B, 0x578B, 0x53C2, 0x6570, 0x6570, 0x91CF, 0x4E0D, 0x5339, 0x914D)
$diagGenericClassExtendsUnsupported = Join-CodePoints @(0x6CDB, 0x578B, 0x0020, 0x0063, 0x006C, 0x0061, 0x0073, 0x0073, 0x0020, 0x5F53, 0x524D, 0x4E0D, 0x652F, 0x6301, 0x7EE7, 0x627F)
$diagOverloadAmbiguousCall = Join-CodePoints @(0x51FD, 0x6570, 0x8C03, 0x7528, 0x5B58, 0x5728, 0x91CD, 0x8F7D, 0x6B67, 0x4E49)
$diagFunctionOverloadConflict = Join-CodePoints @(0x91CD, 0x590D, 0x6216, 0x6B67, 0x4E49, 0x7684, 0x51FD, 0x6570, 0x91CD, 0x8F7D, 0x7B7E, 0x540D)
$diagConstAssign = Join-CodePoints @(0x0063, 0x006F, 0x006E, 0x0073, 0x0074, 0x0020, 0x4E0D, 0x80FD, 0x88AB, 0x91CD, 0x65B0, 0x8D4B, 0x503C)
$diagDuplicateDecl = Join-CodePoints @(0x540C, 0x4E00, 0x4F5C, 0x7528, 0x57DF, 0x5185, 0x91CD, 0x590D, 0x58F0, 0x660E)
$diagParamDefaultTypeMismatch = Join-CodePoints @(0x9ED8, 0x8BA4, 0x503C, 0x8868, 0x8FBE, 0x5F0F, 0x7C7B, 0x578B, 0x548C, 0x53C2, 0x6570, 0x7C7B, 0x578B, 0x4E0D, 0x5339, 0x914D)
$diagParamModeDefault = Join-CodePoints @(0x0062, 0x0079, 0x0072, 0x0065, 0x0066, 0x0020, 0x548C, 0x0020, 0x006F, 0x0075, 0x0074, 0x0020, 0x53C2, 0x6570, 0x4E0D, 0x80FD, 0x58F0, 0x660E, 0x9ED8, 0x8BA4, 0x503C)
$diagOutReadBeforeWrite = Join-CodePoints @(0x006F, 0x0075, 0x0074, 0x0020, 0x53C2, 0x6570, 0x5728, 0x6574, 0x4F53, 0x8D4B, 0x503C, 0x524D, 0x4E0D, 0x80FD, 0x8BFB, 0x53D6)
$diagOutMissingWrite = Join-CodePoints @(0x006F, 0x0075, 0x0074, 0x0020, 0x53C2, 0x6570, 0x5FC5, 0x987B, 0x5728, 0x51FD, 0x6570, 0x6B63, 0x5E38, 0x7ED3, 0x675F, 0x524D, 0x6574, 0x4F53, 0x8D4B, 0x503C)
$diagOutCapture = Join-CodePoints @(0x533F, 0x540D, 0x51FD, 0x6570, 0x4E0D, 0x80FD, 0x6355, 0x83B7, 0x0020, 0x0062, 0x0079, 0x0072, 0x0065, 0x0066, 0x0020, 0x6216, 0x0020, 0x006F, 0x0075, 0x0074, 0x0020, 0x53C2, 0x6570)
$diagSelfAssign = Join-CodePoints @(0x0073, 0x0065, 0x006C, 0x0066, 0x0020, 0x4E0D, 0x80FD, 0x88AB, 0x91CD, 0x65B0, 0x8D4B, 0x503C)
$diagSafeAssignNameOnly = Join-CodePoints @(0x5F53, 0x524D, 0x8D4B, 0x503C, 0x5DE6, 0x4FA7, 0x5FC5, 0x987B, 0x662F, 0x53D8, 0x91CF, 0x540D)
$diagArrayListIndexInt = Join-CodePoints @(0x0061, 0x0072, 0x0072, 0x0061, 0x0079, 0x002F, 0x006C, 0x0069, 0x0073, 0x0074, 0x0020, 0x7D22, 0x5F15, 0x5FC5, 0x987B, 0x662F, 0x0020, 0x0069, 0x006E, 0x0074)
$diagDictIndexString = Join-CodePoints @(0x0064, 0x0069, 0x0063, 0x0074, 0x0020, 0x7D22, 0x5F15, 0x952E, 0x5FC5, 0x987B, 0x662F, 0x0020, 0x0073, 0x0074, 0x0072, 0x0069, 0x006E, 0x0067)
$diagArrayLiteralPlainList = Join-CodePoints @(0x0061, 0x0072, 0x0072, 0x0061, 0x0079, 0x0020, 0x5B57, 0x9762, 0x91CF, 0x5FC5, 0x987B, 0x4F7F, 0x7528, 0x666E, 0x901A, 0x0020, 0x005B, 0x005D, 0x0020, 0x503C, 0x5217, 0x8868)
$diagSetLiteralPlainList = Join-CodePoints @(0x0073, 0x0065, 0x0074, 0x0020, 0x5B57, 0x9762, 0x91CF, 0x5FC5, 0x987B, 0x4F7F, 0x7528, 0x666E, 0x901A, 0x0020, 0x007B, 0x007D, 0x0020, 0x503C, 0x5217, 0x8868)
$diagStringConcatOperands = Join-CodePoints @(0x5B57, 0x7B26, 0x4E32, 0x62FC, 0x63A5, 0x4E24, 0x4FA7, 0x90FD, 0x5FC5, 0x987B, 0x662F, 0x0020, 0x0073, 0x0074, 0x0072, 0x0069, 0x006E, 0x0067)
$diagTimeLiteralUnclosed = Join-CodePoints @(0x672A, 0x95ED, 0x5408, 0x7684, 0x65F6, 0x95F4, 0x5B57, 0x9762, 0x91CF)
$diagTypeCompareValue = Join-CodePoints @(0x0074, 0x0079, 0x0070, 0x0065, 0x0020, 0x503C, 0x53EA, 0x80FD, 0x548C, 0x0020, 0x0074, 0x0079, 0x0070, 0x0065, 0x0020, 0x503C, 0x6216, 0x7C7B, 0x578B, 0x5B57, 0x9762, 0x91CF, 0x6BD4, 0x8F83)
$diagTypeLiteralEqualityOnly = Join-CodePoints @(0x7C7B, 0x578B, 0x5B57, 0x9762, 0x91CF, 0x53EA, 0x80FD, 0x7528, 0x4E8E, 0x0020, 0x0074, 0x0079, 0x0070, 0x0065, 0x0020, 0x76F8, 0x7B49, 0x6027, 0x6BD4, 0x8F83)
$diagTypeCallArity = Join-CodePoints @(0x0074, 0x0079, 0x0070, 0x0065, 0x0028, 0x002E, 0x002E, 0x002E, 0x0029, 0x0020, 0x5FC5, 0x987B, 0x63A5, 0x6536, 0x4E00, 0x4E2A, 0x8868, 0x8FBE, 0x5F0F, 0x53C2, 0x6570)
$diagRangeStepZero = Join-CodePoints @(0x0072, 0x0061, 0x006E, 0x0067, 0x0065, 0x0020, 0x0073, 0x0074, 0x0065, 0x0070, 0x0020, 0x4E0D, 0x80FD, 0x662F, 0x0020, 0x0030)
$diagMultiReturnSingleArgument = Join-CodePoints @(0x591A, 0x8FD4, 0x56DE, 0x503C, 0x51FD, 0x6570, 0x8C03, 0x7528, 0x4E0D, 0x80FD, 0x4F5C, 0x4E3A, 0x5355, 0x4E2A, 0x5B9E, 0x53C2)
$diagImportNoGlobalPollution = "undeclared identifier 'counterAdd'"

$requiredArtifactSubstrings = @{
    "sem_break_outside_loop.err.txt" = "break/continue " + (Join-CodePoints @(0x5FC5, 0x987B, 0x4F4D, 0x4E8E, 0x5FAA, 0x73AF, 0x4F53, 0x5185))
    "sem_continue_zero.err.txt" = "break/continue " + (Join-CodePoints @(0x5C42, 0x6570, 0x5FC5, 0x987B, 0x5927, 0x4E8E)) + " 0"
    "sem_break_too_deep.err.txt" = "break/continue " + (Join-CodePoints @(0x5C42, 0x6570, 0x8D85, 0x8FC7, 0x5F53, 0x524D, 0x5FAA, 0x73AF, 0x6DF1, 0x5EA6))
    "sem_if_condition_type.err.txt" = "if " + (Join-CodePoints @(0x6761, 0x4EF6, 0x5FC5, 0x987B, 0x662F)) + " bool"
    "sem_while_condition_type.err.txt" = "while " + (Join-CodePoints @(0x6761, 0x4EF6, 0x5FC5, 0x987B, 0x662F)) + " bool"
    "sem_conditional_condition_type.err.txt" = (Join-CodePoints @(0x6761, 0x4EF6, 0x8868, 0x8FBE, 0x5F0F, 0x6761, 0x4EF6, 0x5FC5, 0x987B, 0x662F)) + " bool"
    "sem_logical_operand_type.err.txt" = "logical left operand must be bool"
    "sem_return_outside_func.err.txt" = "return " + (Join-CodePoints @(0x5FC5, 0x987B, 0x4F4D, 0x4E8E, 0x51FD, 0x6570, 0x6216, 0x6784, 0x9020, 0x002F, 0x6790, 0x6784, 0x4F53, 0x5185))
    "sem_return_value_in_no_return.err.txt" = "return " + (Join-CodePoints @(0x8FD4, 0x56DE, 0x503C, 0x6570, 0x91CF, 0x548C, 0x51FD, 0x6570, 0x58F0, 0x660E, 0x4E0D, 0x5339, 0x914D))
    "sem_return_missing_value.err.txt" = "return " + (Join-CodePoints @(0x8FD4, 0x56DE, 0x503C, 0x6570, 0x91CF, 0x548C, 0x51FD, 0x6570, 0x58F0, 0x660E, 0x4E0D, 0x5339, 0x914D))
    "sem_return_multi_count.err.txt" = "return " + (Join-CodePoints @(0x8FD4, 0x56DE, 0x503C, 0x6570, 0x91CF, 0x548C, 0x51FD, 0x6570, 0x58F0, 0x660E, 0x4E0D, 0x5339, 0x914D))
    "sem_multi_decl_count.err.txt" = (Join-CodePoints @(0x591A, 0x8FD4, 0x56DE, 0x503C, 0x58F0, 0x660E, 0x6570, 0x91CF, 0x548C, 0x51FD, 0x6570, 0x8FD4, 0x56DE, 0x503C, 0x6570, 0x91CF, 0x4E0D, 0x5339, 0x914D))
    "sem_multi_decl_rhs.err.txt" = (Join-CodePoints @(0x591A, 0x8FD4, 0x56DE, 0x503C, 0x58F0, 0x660E, 0x53F3, 0x4FA7, 0x5FC5, 0x987B, 0x662F, 0x591A, 0x8FD4, 0x56DE, 0x503C, 0x51FD, 0x6570, 0x8C03, 0x7528))
    "sem_multi_assign_count.err.txt" = (Join-CodePoints @(0x591A, 0x8FD4, 0x56DE, 0x503C, 0x8D4B, 0x503C, 0x6570, 0x91CF, 0x548C, 0x51FD, 0x6570, 0x8FD4, 0x56DE, 0x503C, 0x6570, 0x91CF, 0x4E0D, 0x5339, 0x914D))
    "sem_multi_single_decl.err.txt" = (Join-CodePoints @(0x591A, 0x8FD4, 0x56DE, 0x503C, 0x51FD, 0x6570, 0x8C03, 0x7528, 0x5FC5, 0x987B, 0x4F7F, 0x7528, 0x591A, 0x503C, 0x63A5, 0x6536))
    "sem_multi_single_assign.err.txt" = (Join-CodePoints @(0x591A, 0x8FD4, 0x56DE, 0x503C, 0x51FD, 0x6570, 0x8C03, 0x7528, 0x5FC5, 0x987B, 0x4F7F, 0x7528, 0x591A, 0x503C, 0x63A5, 0x6536))
    "sem_multi_return_arg.err.txt" = $diagMultiReturnSingleArgument
    "sem_task_spawn_arg_multi_return.err.txt" = $diagMultiReturnSingleArgument
    "sem_multi_return_single.err.txt" = (Join-CodePoints @(0x591A, 0x8FD4, 0x56DE, 0x503C, 0x51FD, 0x6570, 0x8C03, 0x7528, 0x4E0D, 0x80FD, 0x4F5C, 0x4E3A, 0x5355, 0x4E2A, 0x0020, 0x0072, 0x0065, 0x0074, 0x0075, 0x0072, 0x006E, 0x0020, 0x8868, 0x8FBE, 0x5F0F))
    "sem_call_named_order.err.txt" = (Join-CodePoints @(0x4F4D, 0x7F6E, 0x53C2, 0x6570, 0x4E0D, 0x80FD, 0x51FA, 0x73B0, 0x5728, 0x547D, 0x540D, 0x53C2, 0x6570, 0x4E4B, 0x540E))
    "sem_call_duplicate_named.err.txt" = (Join-CodePoints @(0x91CD, 0x590D, 0x4F20, 0x5165, 0x540C, 0x4E00, 0x4E2A, 0x547D, 0x540D, 0x53C2, 0x6570))
    "sem_call_arg_type.err.txt" = "target int, value string"
    "sem_call_missing_required.err.txt" = "expected 2, actual 1"
    "sem_param_order.err.txt" = (Join-CodePoints @(0x5FC5, 0x9009, 0x53C2, 0x6570, 0x4E0D, 0x80FD, 0x51FA, 0x73B0, 0x5728, 0x53EF, 0x9009, 0x53C2, 0x6570, 0x4E4B, 0x540E))
    "sem_vararg_position.err.txt" = (Join-CodePoints @(0x53EF, 0x53D8, 0x53C2, 0x6570, 0x5FC5, 0x987B, 0x4F4D, 0x4E8E, 0x53C2, 0x6570, 0x8868, 0x6700, 0x540E))
    "sem_vararg_arg_redeclare.err.txt" = (Join-CodePoints @(0x540C, 0x4E00, 0x4F5C, 0x7528, 0x57DF, 0x5185, 0x91CD, 0x590D, 0x58F0, 0x660E))
    "sem_function_type_bad_name.err.txt" = (Join-CodePoints @(0x8BED, 0x6CD5, 0x9519, 0x8BEF))
    "sem_function_type_vararg_position.err.txt" = (Join-CodePoints @(0x0066, 0x0075, 0x006E, 0x0063, 0x0020, 0x7C7B, 0x578B, 0x53EF, 0x53D8, 0x53C2, 0x6570, 0x5FC5, 0x987B, 0x4F4D, 0x4E8E, 0x6700, 0x540E))
    "sem_function_signature_mismatch.err.txt" = "target function, value function"
    "cgen_function_signature_mismatch.err.txt" = "target function, value function"
    "sem_function_bare_named_arg.err.txt" = (Join-CodePoints @(0x0066, 0x0075, 0x006E, 0x0063, 0x0020, 0x8C03, 0x7528, 0x4E0D, 0x652F, 0x6301, 0x547D, 0x540D, 0x53C2, 0x6570))
    "sem_function_typed_named_arg.err.txt" = (Join-CodePoints @(0x0066, 0x0075, 0x006E, 0x0063, 0x0020, 0x503C, 0x8C03, 0x7528, 0x4E0D, 0x652F, 0x6301, 0x547D, 0x540D, 0x53C2, 0x6570))
    "sem_class_property_vararg.err.txt" = "property parameters cannot be vararg"
    "sem_function_bare_overload_name.err.txt" = (Join-CodePoints @(0x0066, 0x0075, 0x006E, 0x0063, 0x0020, 0x76EE, 0x6807, 0x65E0, 0x6CD5, 0x4ECE, 0x91CD, 0x8F7D, 0x51FD, 0x6570, 0x540D, 0x9009, 0x62E9, 0x552F, 0x4E00, 0x5019, 0x9009))
    "sem_function_call_named_order.err.txt" = (Join-CodePoints @(0x4F4D, 0x7F6E, 0x53C2, 0x6570, 0x4E0D, 0x80FD, 0x51FA, 0x73B0, 0x5728, 0x547D, 0x540D, 0x53C2, 0x6570, 0x4E4B, 0x540E))
    "sem_function_call_arg_type.err.txt" = (Join-CodePoints @(0x0066, 0x0075, 0x006E, 0x0063, 0x0020, 0x8C03, 0x7528, 0x5B9E, 0x53C2, 0x7C7B, 0x578B, 0x548C, 0x7B7E, 0x540D, 0x4E0D, 0x5339, 0x914D))
    "sem_member_function_field_call_arg_type.err.txt" = (Join-CodePoints @(0x0066, 0x0075, 0x006E, 0x0063, 0x0020, 0x8C03, 0x7528, 0x5B9E, 0x53C2, 0x7C7B, 0x578B, 0x548C, 0x7B7E, 0x540D, 0x4E0D, 0x5339, 0x914D))
    "sem_function_multi_return_arg_type.err.txt" = (Join-CodePoints @(0x0066, 0x0075, 0x006E, 0x0063, 0x0020, 0x8C03, 0x7528, 0x5B9E, 0x53C2, 0x7C7B, 0x578B, 0x548C, 0x7B7E, 0x540D, 0x4E0D, 0x5339, 0x914D))
    "sem_auto_param.err.txt" = $diagSyntaxError
    "sem_auto_return.err.txt" = $diagSyntaxError
    "sem_auto_field.err.txt" = $diagSyntaxError
    "sem_auto_alias.err.txt" = $diagSyntaxError
    "sem_auto_function_type.err.txt" = $diagSyntaxError
    "sem_for_in_three_targets.err.txt" = $diagSyntaxError
    "sem_for_await_three_targets.err.txt" = $diagSyntaxError
    "sem_void_type.err.txt" = $diagUnknownType
    "sem_type_builtin_name.err.txt" = (Join-CodePoints @(0x81EA, 0x5B9A, 0x4E49, 0x7C7B, 0x578B, 0x540D, 0x4E0D, 0x80FD, 0x548C, 0x5185, 0x7F6E, 0x7C7B, 0x578B, 0x540D, 0x91CD, 0x590D))
    "sem_type_duplicate_decl.err.txt" = (Join-CodePoints @(0x91CD, 0x590D, 0x58F0, 0x660E, 0x81EA, 0x5B9A, 0x4E49, 0x7C7B, 0x578B))
    "sem_unknown_type.err.txt" = $diagUnknownType
    "sem_export_duplicate.err.txt" = (Join-CodePoints @(0x91CD, 0x590D, 0x7684)) + " C export symbol"
    "sem_export_optional.err.txt" = "C ABI export " + (Join-CodePoints @(0x4E0D, 0x652F, 0x6301, 0x53EF, 0x9009, 0x53C2, 0x6570))
    "sem_export_vararg.err.txt" = "C ABI export " + (Join-CodePoints @(0x4E0D, 0x652F, 0x6301, 0x53EF, 0x53D8, 0x53C2, 0x6570))
    "sem_export_string_return.err.txt" = (Join-CodePoints @(0x8BE5, 0x8FD4, 0x56DE, 0x7C7B, 0x578B, 0x4E0D, 0x80FD, 0x76F4, 0x63A5, 0x8FDB, 0x5165, 0x7B2C, 0x4E8C, 0x9636, 0x6BB5)) + " C ABI export"
    "sem_export_generic.err.txt" = $diagSyntaxError
    "sem_extern_c_symbol_mismatch.err.txt" = "extern C symbol declared with incompatible ABI"
    "sem_extern_c_symbol_kind_mismatch.err.txt" = "extern C symbol declared with incompatible ABI"
    "sem_extern_bad_type.err.txt" = "parameter type cannot enter first-stage C ABI extern directly"
    "sem_extern_multi_return.err.txt" = "C ABI extern does not support multiple return values"
    "sem_extern_optional.err.txt" = "C ABI extern does not support optional parameters"
    "sem_extern_vararg.err.txt" = "C ABI extern does not support varargs"
    "sem_extern_generic.err.txt" = $diagSyntaxError
    "sem_throw_int.err.txt" = (Join-CodePoints @(0x0074, 0x0068, 0x0072, 0x006F, 0x0077, 0x0020, 0x8868, 0x8FBE, 0x5F0F, 0x5FC5, 0x987B, 0x662F, 0x0020, 0x0073, 0x0074, 0x0072, 0x0069, 0x006E, 0x0067))
    "sem_rethrow_outside_catch.err.txt" = (Join-CodePoints @(0x0074, 0x0068, 0x0072, 0x006F, 0x0077, 0x0020, 0x53EA, 0x80FD, 0x5728, 0x0020, 0x0063, 0x0061, 0x0074, 0x0063, 0x0068, 0x0020, 0x5757, 0x5185, 0x91CD, 0x65B0, 0x629B, 0x51FA, 0x5F53, 0x524D, 0x5F02, 0x5E38))
    "sem_assert_message_int.err.txt" = (Join-CodePoints @(0x0061, 0x0073, 0x0073, 0x0065, 0x0072, 0x0074, 0x0020, 0x6D88, 0x606F, 0x5FC5, 0x987B, 0x662F, 0x0020, 0x0073, 0x0074, 0x0072, 0x0069, 0x006E, 0x0067))
    "sem_try_without_handler.err.txt" = (Join-CodePoints @(0x0074, 0x0072, 0x0079, 0x0020, 0x5FC5, 0x987B, 0x5305, 0x542B, 0x0020, 0x0063, 0x0061, 0x0074, 0x0063, 0x0068, 0x0020, 0x6216, 0x0020, 0x0066, 0x0069, 0x006E, 0x0061, 0x006C, 0x006C, 0x0079))
    "sem_try_return_boundary.err.txt" = (Join-CodePoints @(0x0074, 0x0072, 0x0079, 0x002F, 0x0063, 0x0061, 0x0074, 0x0063, 0x0068, 0x002F, 0x0066, 0x0069, 0x006E, 0x0061, 0x006C, 0x006C, 0x0079, 0x0020, 0x5F53, 0x524D, 0x4E0D, 0x652F, 0x6301, 0x0020, 0x0072, 0x0065, 0x0074, 0x0075, 0x0072, 0x006E, 0x002F, 0x0062, 0x0072, 0x0065, 0x0061, 0x006B, 0x002F, 0x0063, 0x006F, 0x006E, 0x0074, 0x0069, 0x006E, 0x0075, 0x0065, 0x0020, 0x7A7F, 0x8FC7, 0x5F02, 0x5E38, 0x8FB9, 0x754C))
    "sem_doc_unknown_field.err.txt" = (Join-CodePoints @(0x0023, 0x0064, 0x006F, 0x0063, 0x0020, 0x672A, 0x77E5, 0x5B57, 0x6BB5))
    "sem_doc_naked_text.err.txt" = (Join-CodePoints @(0x0023, 0x0064, 0x006F, 0x0063, 0x0020, 0x5B58, 0x5728, 0x672A, 0x77E5, 0x5B57, 0x6BB5, 0x6216, 0x672A, 0x5F52, 0x5C5E, 0x5B57, 0x6BB5, 0x7684, 0x88F8, 0x6587, 0x672C))
    "sem_doc_bad_param.err.txt" = (Join-CodePoints @(0x0023, 0x0064, 0x006F, 0x0063, 0x0020, 0x0070, 0x0061, 0x0072, 0x0061, 0x006D, 0x0020, 0x5B57, 0x6BB5, 0x5FC5, 0x987B, 0x5BF9, 0x5E94, 0x76EE, 0x6807, 0x58F0, 0x660E, 0x4E2D, 0x7684, 0x53C2, 0x6570))
    "sem_doc_duplicate_param.err.txt" = (Join-CodePoints @(0x0023, 0x0064, 0x006F, 0x0063, 0x0020, 0x0070, 0x0061, 0x0072, 0x0061, 0x006D, 0x002F, 0x0072, 0x0065, 0x0074, 0x0075, 0x0072, 0x006E, 0x0020, 0x5B57, 0x6BB5, 0x4E0D, 0x80FD, 0x91CD, 0x590D, 0x63CF, 0x8FF0, 0x540C, 0x4E00, 0x4E2A, 0x540D, 0x79F0))
    "sem_async_multi_return.err.txt" = $diagAsyncMultiReturn
    "sem_async_return_channel.err.txt" = $diagNonCopyableReturn
    "sem_async_return_future.err.txt" = $diagAsyncReturnFuture
    "sem_async_return_sync.err.txt" = $diagNonCopyableReturn
    "sem_async_return_thread.err.txt" = $diagNonCopyableReturn
    "sem_async_param_future.err.txt" = $diagNonCopyableParam
    "sem_async_param_generator.err.txt" = $diagNonCopyableParam
    "sem_async_param_channel.err.txt" = $diagNonCopyableParam
    "sem_async_param_sync.err.txt" = $diagNonCopyableParam
    "sem_async_param_thread.err.txt" = $diagNonCopyableParam
    "sem_await_non_future.err.txt" = $diagAwaitNonFuture
    "sem_future_then_arg_count.err.txt" = "future method argument count mismatch"
    "sem_future_then_param_type.err.txt" = "future.then named callback must have one matching parameter and one return value"
    "sem_future_then_no_return.err.txt" = "future.then named callback must have one matching parameter and one return value"
    "sem_future_then_multi_return.err.txt" = "future.then named callback must have one matching parameter and one return value"
    "sem_future_catch_signature.err.txt" = "future.catch named callback must match func(string) as T"
    "sem_future_finally_param.err.txt" = "future.finally named callback must match func()"
    "sem_future_finally_return.err.txt" = "future.finally named callback must match func()"
    "sem_future_continuation_named_argument.err.txt" = "future method does not accept named arguments"
    "sem_await_sync_func.err.txt" = "await can only be used at top level, inside async func, or inside async generator"
    "sem_for_await_non_generator.err.txt" = "for await requires generator<T>"
    "sem_async_type_non_generator.err.txt" = "async type marker is only supported on generator<T>"
    "sem_task_unknown_member.err.txt" = "standard library namespace has no such symbol"
    "sem_task_spawn_return_channel.err.txt" = $diagNonCopyableReturn
    "sem_task_spawn_return_sync.err.txt" = $diagNonCopyableReturn
    "sem_task_spawn_return_thread.err.txt" = $diagNonCopyableReturn
    "sem_task_spawn_arg_byref.err.txt" = "task.spawn cannot capture byref or out caller storage"
    "sem_task_spawn_arg_out.err.txt" = "task.spawn cannot capture byref or out caller storage"
    "sem_task_all_return_channel.err.txt" = "move-only resource handles cannot be aggregated"
    "sem_task_all_return_generator.err.txt" = "move-only resource handles cannot be aggregated"
    "sem_task_all_typed_array_return_channel.err.txt" = "move-only resource handles cannot be aggregated"
    "sem_task_all_typed_array_return_generator.err.txt" = "move-only resource handles cannot be aggregated"
    "sem_task_race_return_sync.err.txt" = "move-only resource handles cannot be aggregated"
    "sem_task_race_return_thread.err.txt" = "move-only resource handles cannot be aggregated"
    "sem_task_race_typed_array_return_sync.err.txt" = "move-only resource handles cannot be aggregated"
    "sem_task_race_typed_array_return_thread.err.txt" = "move-only resource handles cannot be aggregated"
    "sem_task_cancel_channel.err.txt" = "task.cancel"
    "sem_task_cancel_generator.err.txt" = "task.cancel"
    "sem_task_cancel_sync.err.txt" = "task.cancel"
    "sem_task_cancel_thread.err.txt" = "task.cancel"
    "sem_channel_class_element.err.txt" = $diagChannelPlainValueElement
    "sem_channel_function_element.err.txt" = $diagChannelPlainValueElement
    "sem_channel_generator_element.err.txt" = $diagChannelPlainValueElement
    "sem_channel_typed_array_element.err.txt" = $diagChannelPlainValueElement
    "sem_channel_recv_target_type.err.txt" = $diagChannelRecvTargetType
    "sem_channel_try_recv_target_type.err.txt" = $diagChannelTryRecvTargetType
    "sem_channel_recv_timeout_target_type.err.txt" = "channel.recvTimeoutInto"
    "sem_channel_recv_const_target.err.txt" = $diagConstAssign
    "sem_channel_try_recv_const_target.err.txt" = $diagConstAssign
    "sem_channel_send_type.err.txt" = $diagChannelSendType
    "sem_channel_send_named_argument.err.txt" = "channel.send"
    "sem_channel_recv_named_argument.err.txt" = "channel.recvInto"
    "sem_channel_try_recv_named_argument.err.txt" = "channel.tryRecvInto"
    "sem_channel_recv_timeout_named_argument.err.txt" = "channel.recvTimeoutInto"
    "sem_channel_recv_timeout_type.err.txt" = "timeoutMs"
    "sem_channel_receiver_expr.err.txt" = $diagChannelNamedReceiver
    "sem_channel_unknown_method.err.txt" = "unknown channel method"
    "sem_generator_class_element.err.txt" = $diagGeneratorPlainValueElement
    "sem_generator_function_element.err.txt" = $diagGeneratorPlainValueElement
    "sem_generator_function_param.err.txt" = $diagGeneratorParamPlainValue
    "sem_generator_string_element.err.txt" = $diagGeneratorPlainValueElement
    "sem_generator_typed_array_element.err.txt" = $diagGeneratorPlainValueElement
    "sem_generator_typed_array_param.err.txt" = $diagGeneratorParamPlainValue
    "sem_generator_yield_type.err.txt" = $diagGeneratorYieldType
    "sem_yield_outside_generator.err.txt" = $diagYieldOutsideGenerator
    "sem_generator_return_value.err.txt" = $diagGeneratorReturnValue
    "sem_async_generator_return_value.err.txt" = $diagGeneratorReturnValue
    "sem_async_generator_type_marker_return_value.err.txt" = $diagGeneratorReturnValue
    "sem_sync_cond_mutex_arg.err.txt" = "cond wait first argument must be mutex"
    "sem_sync_cond_wait_timeout_type.err.txt" = "cond wait timeout must be int"
    "sem_sync_copy.err.txt" = $diagNonCopyableInit
    "sem_sync_ctor_args.err.txt" = "sync constructor does not accept arguments"
    "sem_sync_ctor_named_argument.err.txt" = "sync constructor accepts positional arguments only"
    "sem_sync_method_args.err.txt" = "sync method does not accept arguments"
    "sem_sync_wait_timeout_type.err.txt" = "sync method argument must be int"
    "sem_sync_post_multiple_type.err.txt" = "sync method argument must be int"
    "sem_sync_receiver_expr.err.txt" = "sync method receiver must be a named variable"
    "sem_sync_unknown_method.err.txt" = "unknown sync method"
    "sem_thread_copy.err.txt" = $diagNonCopyableInit
    "sem_thread_ctor_args.err.txt" = "thread constructor requires one func() as int entry argument"
    "sem_thread_ctor_named_argument.err.txt" = "thread constructor accepts positional arguments only"
    "sem_thread_entry_signature.err.txt" = "thread entry must match func() as int"
    "sem_thread_method_args.err.txt" = "thread method does not accept arguments"
    "sem_thread_wait_timeout_type.err.txt" = "thread.waitTimeout argument must be int"
    "sem_thread_receiver_expr.err.txt" = "thread method receiver must be a named variable"
    "sem_thread_unknown_method.err.txt" = "unknown thread method"
    "sem_class_unknown_base.err.txt" = $diagUnknownBaseClass
    "sem_class_base_create_missing.err.txt" = $diagBaseCreateMissingInCreate
    "sem_class_base_default_missing.err.txt" = $diagBaseCreateMissingDefault
    "sem_class_base_outside_create.err.txt" = $diagBaseOutsideCreate
    "sem_class_base_overload_missing.err.txt" = $diagBaseCreateMissingInCreate
    "sem_class_ctor_arity.err.txt" = "expected 1, actual 0"
    "sem_class_default_ctor_args.err.txt" = "expected 0, actual 1"
    "sem_class_unknown_field.err.txt" = "unknown class/union field"
    "sem_class_property_readonly.err.txt" = "class property assignment requires matching let"
    "sem_class_private_access.err.txt" = "private class member is not accessible"
    "sem_class_static_self.err.txt" = "static class member cannot use self"
    "sem_class_override_missing.err.txt" = "overriding a base member requires explicit override"
    "sem_class_override_nonvirtual.err.txt" = "override target must be virtual"
    "sem_class_override_final.err.txt" = "cannot override a final class member"
    "sem_var_call_arg_destroy_class.err.txt" = "target var, source Resource"
    "sem_var_call_named_order.err.txt" = (Join-CodePoints @(0x4F4D, 0x7F6E, 0x53C2, 0x6570, 0x4E0D, 0x80FD, 0x51FA, 0x73B0, 0x5728, 0x547D, 0x540D, 0x53C2, 0x6570, 0x4E4B, 0x540E))
    "sem_class_destroy_params.err.txt" = $diagDestroyNoParams
    "sem_class_field_conflict.err.txt" = "class member layout name conflicts with an existing field or union"
    "sem_class_method_overload_conflict.err.txt" = "duplicate or ambiguous class member overload signature"
    "sem_native_class_bad_special.err.txt" = $diagNativeBadSpecial
    "sem_native_class_duplicate_c_symbol.err.txt" = $diagNativeDuplicateCSymbol
    "sem_native_class_bind_non_point.err.txt" = $diagNativeBindPoint
    "sem_native_class_create_point_bind_conflict.err.txt" = $diagCreateOverloadMissing
    "sem_native_class_method_overload_conflict.err.txt" = $diagNativeOverloadConflict
    "sem_closure_capture_destroy_class.err.txt" = $diagClosureCaptureNonCopyable
    "sem_closure_capture_native_class.err.txt" = $diagClosureCaptureNonCopyable
    "sem_closure_capture_native_class_field.err.txt" = $diagClosureCaptureNonCopyable
    "sem_anon_capture_assign.err.txt" = $diagClosureCaptureReadonly
    "sem_noncopyable_class_copy_init.err.txt" = $diagNonCopyableInit
    "sem_noncopyable_assign.err.txt" = $diagNonCopyableAssign
    "sem_noncopyable_native_class_param.err.txt" = $diagNonCopyableParam
    "sem_noncopyable_return.err.txt" = $diagNonCopyableReturnExisting
    "sem_var_destroy_class_init.err.txt" = "target var, value Resource"
    "sem_var_destroy_class_return.err.txt" = "target var, value Resource"
    "sem_generic_arity.err.txt" = $diagGenericArityMismatch
    "sem_generic_param_duplicate.err.txt" = $diagGenericParamDuplicate
    "sem_generic_unknown_type_arg.err.txt" = $diagUnknownType
    "sem_generic_infer_conflict.err.txt" = "target int, value string"
    "sem_generic_func_inst_arity.err.txt" = "expected 1, actual 2"
    "sem_generic_class_ctor_arity.err.txt" = $diagGenericClassArgArity
    "sem_generic_class_open_ctor.err.txt" = $diagOpenGenericClassCtor
    "sem_generic_class_ctor_type_mismatch.err.txt" = $diagCreateOverloadMissing
    "sem_generic_class_arity.err.txt" = $diagGenericClassTypeParamArity
    "sem_generic_class_extends.err.txt" = $diagGenericClassExtendsUnsupported
    "sem_overload_ambiguous.err.txt" = $diagOverloadAmbiguousCall
    "sem_overload_return_only.err.txt" = $diagFunctionOverloadConflict
    "sem_overload_optional_conflict.err.txt" = $diagFunctionOverloadConflict
    "sem_overload_vararg_same_kind.err.txt" = $diagFunctionOverloadConflict
    "sem_alias_cycle.err.txt" = "type alias cycle"
    "sem_alias_overload_conflict.err.txt" = $diagFunctionOverloadConflict
    "sem_const_assign.err.txt" = $diagConstAssign
    "sem_duplicate.err.txt" = $diagDuplicateDecl
    "sem_global_func_value_conflict.err.txt" = $diagDuplicateDecl
    "sem_param_default_type.err.txt" = $diagParamDefaultTypeMismatch
    "sem_param_modes_invalid.err.txt" = $diagParamModeDefault
    "sem_out_read_before_write.err.txt" = $diagOutReadBeforeWrite
    "sem_out_missing_branch.err.txt" = $diagOutMissingWrite
    "sem_out_capture.err.txt" = $diagOutCapture
    "sem_return_type_mismatch.err.txt" = $diagReturnTypeMismatch
    "sem_type_convert_invalid.err.txt" = "target point, source int"
    "sem_self_assign.err.txt" = $diagSelfAssign
    "sem_member_assign_type.err.txt" = "assignment expression type does not match member/index target type"
    "sem_nested_member_assign_type.err.txt" = "assignment expression type does not match member/index target type"
    "sem_multi_member_assign_type.err.txt" = "multi-return assignment item 1 type mismatch: target int, return string"
    "sem_safe_member_assign.err.txt" = $diagSafeAssignNameOnly
    "sem_dynamic_member_assign_container.err.txt" = "member key receiver must be var or dynamic dict"
    "sem_dynamic_member_assign_typed_dict.err.txt" = "member key receiver must be var or dynamic dict"
    "sem_index_array_key_type.err.txt" = $diagArrayListIndexInt
    "sem_var_container_index_key_type.err.txt" = "var container index key must be int, string or var"
    "sem_typed_safe_dict_key_type.err.txt" = $diagDictIndexString
    "sem_typed_literal.err.txt" = $diagArrayLiteralPlainList
    "sem_typed_set_pair.err.txt" = $diagSetLiteralPlainList
    "sem_string_concat_mismatch.err.txt" = $diagStringConcatOperands
    "sem_string_assign_mismatch.err.txt" = "string compound assignment operands must both be string"
    "sem_string_compare_mismatch.err.txt" = "string comparison operands must both be string"
    "sem_time_literal_bad.err.txt" = "time literal format must be #yyyy-mm-dd#, #hh:mm:ss# or #yyyy-mm-dd hh:mm:ss#"
    "sem_time_literal_newline.err.txt" = $diagTimeLiteralUnclosed
    "sem_time_assign_mismatch.err.txt" = $diagInitTypeMismatch
    "sem_time_compare_mismatch.err.txt" = "time comparison operands must both be time"
    "sem_type_value_mismatch.err.txt" = $diagInitTypeMismatch
    "sem_type_compare_int.err.txt" = $diagTypeCompareValue
    "sem_type_order_compare.err.txt" = $diagTypeLiteralEqualityOnly
    "sem_type_call_arity.err.txt" = $diagTypeCallArity
    "sem_union_top_anon.err.txt" = "top-level union must have a name"
    "sem_range_step_zero.err.txt" = $diagRangeStepZero
    "import_no_global_pollution.err.txt" = $diagImportNoGlobalPollution
    "build_missing_output.err.txt" = "build requires -o <output.exe>"
    "run_bad_script.err.txt" = "time literal format must be #yyyy-mm-dd#, #hh:mm:ss# or #yyyy-mm-dd hh:mm:ss#"
}
$minExpectedFailureCommands = 253
$minDiagnosticArtifacts = 252
$maxUnassertedExpectedFailures = 0
$maxArtifactLineCounts = @{
    "sem_channel_class_element.err.txt" = 1
    "sem_channel_function_element.err.txt" = 1
    "sem_channel_generator_element.err.txt" = 1
    "sem_channel_typed_array_element.err.txt" = 1
    "sem_task_spawn_return_channel.err.txt" = 1
    "sem_task_spawn_return_sync.err.txt" = 1
    "sem_task_spawn_return_thread.err.txt" = 1
    "sem_task_spawn_arg_byref.err.txt" = 1
    "sem_task_spawn_arg_out.err.txt" = 1
    "sem_generator_typed_array_element.err.txt" = 1
    "sem_generator_typed_array_param.err.txt" = 1
    "sem_alias_cycle.err.txt" = 2
    "sem_export_duplicate.err.txt" = 2
    "c_abi_mismatch_extern_header.err.txt" = 2
    "sem_class_implicit_upcast.err.txt" = 4
    "sem_multi_return_type_diagnostic.err.txt" = 2
    "sem_var_to_concrete_implicit.err.txt" = 4
    "sem_class_property_vararg.err.txt" = 2
    "sem_container_literal_target_item_mismatch.err.txt" = 4
    "sem_special_collection_contract.err.txt" = 3
    "sem_param_modes_invalid.err.txt" = 2
    "sem_c_vararg_export.err.txt" = 2
    "sem_async_return_channel.err.txt" = 1
    "sem_async_return_future.err.txt" = 1
    "sem_async_return_sync.err.txt" = 1
    "sem_async_return_thread.err.txt" = 1
    "sem_async_param_future.err.txt" = 1
    "sem_async_param_generator.err.txt" = 1
    "sem_async_param_channel.err.txt" = 1
    "sem_async_param_sync.err.txt" = 1
    "sem_async_param_thread.err.txt" = 1
    "sem_task_all_return_channel.err.txt" = 1
    "sem_task_all_return_generator.err.txt" = 1
    "sem_task_all_typed_array_return_channel.err.txt" = 1
    "sem_task_all_typed_array_return_generator.err.txt" = 1
    "sem_task_race_return_sync.err.txt" = 1
    "sem_task_race_return_thread.err.txt" = 1
    "sem_task_race_typed_array_return_sync.err.txt" = 1
    "sem_task_race_typed_array_return_thread.err.txt" = 1
    "sem_task_cancel_channel.err.txt" = 1
    "sem_task_cancel_generator.err.txt" = 1
    "sem_task_cancel_sync.err.txt" = 1
    "sem_task_cancel_thread.err.txt" = 1
    "sem_class_unknown_field.err.txt" = 1
    "sem_doc_naked_text.err.txt" = 1
    "sem_doc_unknown_field.err.txt" = 1
    "sem_dynamic_member_assign_container.err.txt" = 1
    "sem_dynamic_member_assign_typed_dict.err.txt" = 1
    "sem_generic_infer_conflict.err.txt" = 1
    "sem_string_concat_mismatch.err.txt" = 1
    "sem_thread_entry_signature.err.txt" = 1
    "sem_type_duplicate_decl.err.txt" = 1
    "sem_type_order_compare.err.txt" = 1
    "sem_unknown_type.err.txt" = 1
    "sem_union_top_anon.err.txt" = 1
    "sem_vararg_position.err.txt" = 1
    "sem_enum_recursive_mutual_error.err.txt" = 2
}
$allowedMultiLineArtifacts = New-Object System.Collections.Generic.HashSet[string] ([System.StringComparer]::OrdinalIgnoreCase)
$allowedMultiLineArtifacts.Add("sem_alias_cycle.err.txt") | Out-Null
$allowedMultiLineArtifacts.Add("sem_export_duplicate.err.txt") | Out-Null
$allowedMultiLineArtifacts.Add("c_abi_mismatch_extern_header.err.txt") | Out-Null
$allowedMultiLineArtifacts.Add("sem_class_implicit_upcast.err.txt") | Out-Null
$allowedMultiLineArtifacts.Add("sem_multi_return_type_diagnostic.err.txt") | Out-Null
$allowedMultiLineArtifacts.Add("sem_var_to_concrete_implicit.err.txt") | Out-Null
$allowedMultiLineArtifacts.Add("sem_class_property_vararg.err.txt") | Out-Null
$allowedMultiLineArtifacts.Add("sem_container_literal_target_item_mismatch.err.txt") | Out-Null
$allowedMultiLineArtifacts.Add("sem_special_collection_contract.err.txt") | Out-Null
$allowedMultiLineArtifacts.Add("sem_param_modes_invalid.err.txt") | Out-Null
$allowedMultiLineArtifacts.Add("sem_c_vararg_export.err.txt") | Out-Null
$allowedMultiLineArtifacts.Add("sem_enum_recursive_mutual_error.err.txt") | Out-Null

function Add-Failure($message)
{
    $script:failures.Add($message) | Out-Null
}

function Read-StrictDiagnosticText($path, $name)
{
    $bytes = [System.IO.File]::ReadAllBytes($path)
    try {
        return $strictUtf8.GetString($bytes)
    } catch {
        Add-Failure "expected-failure diagnostic artifact is not strict UTF-8: $name"
        return $looseUtf8.GetString($bytes)
    }
}

function Get-TestRegressionLineRecords()
{
    $records = New-Object System.Collections.Generic.List[object]
    $files = New-Object System.Collections.Generic.List[object]
    $files.Add([pscustomobject]@{ Path = $script:testBat; Relative = "test.bat" }) | Out-Null

    if ( Test-Path -LiteralPath $script:testSuiteDir -PathType Container ) {
        $rootPath = (Resolve-Path -LiteralPath $script:Root).Path
        $suiteFiles = @(Get-ChildItem -LiteralPath $script:testSuiteDir -File -Filter "*.bat" | Sort-Object Name)
        foreach ( $suite in $suiteFiles ) {
            $relative = $suite.FullName.Substring($rootPath.Length + 1)
            $files.Add([pscustomobject]@{ Path = $suite.FullName; Relative = $relative }) | Out-Null
        }
    }

    foreach ( $file in $files ) {
        $fileLines = [System.IO.File]::ReadAllLines($file.Path, [System.Text.Encoding]::UTF8)
        for ( $i = 0; $i -lt $fileLines.Count; ++$i ) {
            $records.Add([pscustomobject]@{
                Text = $fileLines[$i]
                Source = $file.Relative
                Line = $i + 1
            }) | Out-Null
        }
        $records.Add([pscustomobject]@{ Text = ""; Source = $file.Relative; Line = $fileLines.Count + 1 }) | Out-Null
    }

    return @($records.ToArray())
}

if ( ! (Test-Path -LiteralPath $testBat -PathType Leaf) ) {
    Write-Host "test.bat missing: $testBat"
    exit 1
}

$lineRecords = Get-TestRegressionLineRecords
$lines = @($lineRecords | ForEach-Object { $_.Text })
$artifactNames = New-Object System.Collections.Generic.SortedSet[string]
$expectedFailureCommands = 0
$assertedExpectedFailures = 0
$unassertedExpectedFailures = 0

function Is-XlangExpectedFailureCommand($line)
{
    $trimmed = $line.Trim()
    return [regex]::IsMatch($trimmed, '^release\\xl\.exe\s+') -or
        [regex]::IsMatch($trimmed, '^build\\test\\[^ \t&|<>]+\.exe(\s|$)')
}

function Get-BatchLoopValues($records, $index, $variableName)
{
    $values = New-Object System.Collections.Generic.List[string]
    $foundBodyStart = $false
    $source = $records[$index].Source
    $forPattern = '^for\s+%%{0}\s+in\s+\($' -f [regex]::Escape($variableName)

    for ( $i = $index - 1; $i -ge 0; --$i ) {
        if ( $records[$i].Source -ne $source ) {
            break
        }

        $line = $records[$i].Text.Trim()
        if ( ! $foundBodyStart ) {
            if ( $line -eq ') do (' ) {
                $foundBodyStart = $true
            }
            continue
        }

        if ( [regex]::IsMatch($line, $forPattern) ) {
            $result = $values.ToArray()
            [array]::Reverse($result)
            return $result
        }

        if ( $line.Length -gt 0 ) {
            if ( ! [regex]::IsMatch($line, '^[A-Za-z0-9_.-]+$') ) {
                return @()
            }
            $values.Add($line)
        }
    }

    return @()
}

function Add-MatchesToSet($matches, $set, $records, $index)
{
    foreach ( $match in $matches ) {
        $name = $match.Groups[1].Value
        $loopVariable = [regex]::Match($name, '%%([A-Za-z])')
        if ( $loopVariable.Success ) {
            $values = @(Get-BatchLoopValues $records $index $loopVariable.Groups[1].Value)
            foreach ( $value in $values ) {
                $set.Add($name.Replace($loopVariable.Value, $value)) | Out-Null
            }
        } elseif ( $name.Length -gt 0 -and ($name.IndexOf('%') -lt 0) ) {
            $set.Add($name) | Out-Null
        }
    }
}

function Get-PreviousCommandLine($lines, $index)
{
    for ( $i = $index - 1; $i -ge 0; --$i ) {
        $line = $lines[$i].Trim()
        if ( $line.Length -gt 0 ) {
            return $lines[$i]
        }
    }
    return ""
}

function Has-LocalDiagnosticAssertion($lines, $index, $artifactNames, $commandLine)
{
    $max = [Math]::Min($lines.Count - 1, $index + 12)
    if ( $max -le $index ) {
        return $false
    }

    $window = $lines[($index + 1)..$max] -join "`n"
    foreach ( $name in $artifactNames ) {
        if ( $window.Contains($name) ) {
            return $true
        }
    }

    foreach ( $match in [regex]::Matches($commandLine, 'build\\test\\([^ \t&|<>]+)') ) {
        $name = $match.Groups[1].Value
        if ( $name.Contains('%%') -and $window.Contains($name) ) {
            return $true
        }
    }
    return $false
}

function Has-CentralDiagnosticExpectation($artifactNames)
{
    foreach ( $name in $artifactNames ) {
        if ( $requiredArtifactSubstrings.ContainsKey($name) ) {
            return $true
        }
    }
    return $false
}

for ( $i = 0; $i -lt $lines.Count; ++$i ) {
    $line = $lines[$i]
    if ( ! [regex]::IsMatch($line, '^\s*if\s+not\s+errorlevel\s+1\s*\(') ) {
        continue
    }

    $commandLine = Get-PreviousCommandLine $lines $i
    if ( ! (Is-XlangExpectedFailureCommand $commandLine) ) {
        continue
    }

    $expectedFailureCommands += 1

    $stderrNames = New-Object System.Collections.Generic.SortedSet[string]
    $stdoutNames = New-Object System.Collections.Generic.SortedSet[string]
    Add-MatchesToSet ([regex]::Matches($commandLine, '2>\s*build\\test\\([^ \t&|<>]+)')) $stderrNames $lineRecords $i
    Add-MatchesToSet ([regex]::Matches($commandLine, '(?<![0-9])>\s*build\\test\\([^ \t&|<>]+)')) $stdoutNames $lineRecords $i

    $usedNames = New-Object System.Collections.Generic.SortedSet[string]
    foreach ( $name in $stderrNames ) {
        if ( ! $allowEmpty.Contains($name) ) {
            $usedNames.Add($name) | Out-Null
        }
    }

    if ( $usedNames.Count -eq 0 ) {
        foreach ( $name in $stdoutNames ) {
            $usedNames.Add($name) | Out-Null
        }
    }

    if ( $usedNames.Count -eq 0 ) {
        $source = $lineRecords[$i].Source
        $lineNumber = $lineRecords[$i].Line
        Add-Failure "expected-failure command does not capture diagnostic artifact at ${source}:${lineNumber}: $commandLine"
        continue
    }

    foreach ( $name in $usedNames ) {
        $artifactNames.Add($name) | Out-Null
    }

    if ( (Has-LocalDiagnosticAssertion $lines $i $usedNames $commandLine) -or
         (Has-CentralDiagnosticExpectation $usedNames) ) {
        $assertedExpectedFailures += 1
    } else {
        $unassertedExpectedFailures += 1
        Add-Failure ("expected-failure command lacks a diagnostic assertion at {0}:{1}: {2}" -f
            $lineRecords[$i].Source, $lineRecords[$i].Line, $commandLine)
    }
}

if ( $artifactNames.Count -eq 0 ) {
    Add-Failure "no expected-failure diagnostic artifacts found in test.bat/test suites"
}

if ( $expectedFailureCommands -lt $minExpectedFailureCommands ) {
    Add-Failure "expected-failure command coverage regressed: $expectedFailureCommands < $minExpectedFailureCommands"
}

if ( $artifactNames.Count -lt $minDiagnosticArtifacts ) {
    Add-Failure "expected-failure diagnostic artifact coverage regressed: $($artifactNames.Count) < $minDiagnosticArtifacts"
}

if ( $unassertedExpectedFailures -gt $maxUnassertedExpectedFailures ) {
    Add-Failure "expected-failure commands without local diagnostic assertions increased: $unassertedExpectedFailures > $maxUnassertedExpectedFailures"
}

if ( $Artifacts ) {
    if ( ! (Test-Path -LiteralPath $tmpDir -PathType Container) ) {
        Add-Failure "release tmp directory missing: $tmpDir"
    } else {
        foreach ( $name in $artifactNames ) {
            $path = Join-Path $tmpDir $name
            if ( ! (Test-Path -LiteralPath $path -PathType Leaf) ) {
                Add-Failure "expected-failure diagnostic artifact missing: $name"
                continue
            }

            $file = Get-Item -LiteralPath $path
            if ( $file.Length -le 0 ) {
                Add-Failure "expected-failure diagnostic artifact is empty: $name"
                continue
            }

            $text = Read-StrictDiagnosticText $path $name
            if ( $text.IndexOf([string][char]0xfffd, [System.StringComparison]::Ordinal) -ge 0 ) {
                Add-Failure "expected-failure diagnostic artifact contains Unicode replacement character: $name"
            }

            if ( $text -match 'CategoryInfo|FullyQualifiedErrorId|NativeCommandError' ) {
                Add-Failure "expected-failure diagnostic artifact contains PowerShell native error wrapper: $name"
            }

            foreach ( $pattern in $forbiddenArtifactPatterns ) {
                if ( $text.IndexOf($pattern, [System.StringComparison]::OrdinalIgnoreCase) -ge 0 ) {
                    Add-Failure "expected-failure diagnostic artifact contains internal fallback diagnostic '$pattern': $name"
                }
            }

            if ( $requiredArtifactSubstrings.ContainsKey($name) ) {
                $required = $requiredArtifactSubstrings[$name]
                if ( $text.IndexOf($required, [System.StringComparison]::Ordinal) -lt 0 ) {
                    Add-Failure "expected-failure diagnostic artifact is missing required diagnostic '$required': $name"
                }
            }

            $lineCount = [System.IO.File]::ReadAllLines($path, [System.Text.Encoding]::UTF8).Count
            if ( $name.EndsWith(".err.txt", [System.StringComparison]::OrdinalIgnoreCase) -and
                 $lineCount -gt 1 -and ! $allowedMultiLineArtifacts.Contains($name) ) {
                Add-Failure "expected-failure diagnostic artifact unexpectedly has multiple lines: $name has $lineCount"
            }

            if ( $maxArtifactLineCounts.ContainsKey($name) ) {
                $maxLineCount = $maxArtifactLineCounts[$name]
                if ( $lineCount -gt $maxLineCount ) {
                    Add-Failure "expected-failure diagnostic artifact has too many lines: $name has $lineCount, max $maxLineCount"
                }
            }
        }
    }
}

if ( $failures.Count -gt 0 ) {
    foreach ( $failure in $failures ) {
        Write-Host $failure
    }
    exit 1
}

if ( $Artifacts ) {
    Write-Host "Negative diagnostics artifacts ok: $($artifactNames.Count) files, $expectedFailureCommands expected-failure commands, $assertedExpectedFailures asserted, $unassertedExpectedFailures baseline-only."
} else {
    Write-Host "Negative diagnostics wiring ok: $($artifactNames.Count) files, $expectedFailureCommands expected-failure commands, $assertedExpectedFailures asserted, $unassertedExpectedFailures baseline-only."
}
exit 0
