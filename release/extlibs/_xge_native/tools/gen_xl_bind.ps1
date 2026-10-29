# gen_xl_bind.ps1 - 从 xge.h/xui.h 自动生成 xlang .xl 绑定文件
# 用法: .\gen_xl_bind.ps1

param(
    [string]$PkgRoot = (Split-Path -Parent $PSScriptRoot)
)

$ErrorActionPreference = "Stop"

# ============================================================
# 类型映射
# ============================================================
function Get-XlType([string]$cType) {
    $cType = $cType.Trim()
    # 移除 const 修饰
    $clean = $cType -replace '\bconst\s+', ''
    $clean = $clean.Trim()

    # 指针类型
    if ($clean -match '\*$') {
        $base = ($clean -replace '\*$', '').Trim()
        if ($base -eq 'char' -or $base -eq 'const char') { return 'string' }
        return 'point'
    }
    # 基本类型
    switch -Regex ($clean) {
        '^void$'           { return '' }
        '^int$'            { return 'c_int' }
        '^unsigned int$'   { return 'c_uint' }
        '^uint32_t$'       { return 'c_uint' }
        '^int32_t$'        { return 'c_int' }
        '^uint64_t$'       { return 'c_ulonglong' }
        '^int64_t$'        { return 'c_longlong' }
        '^uint16_t$'       { return 'c_ushort' }
        '^int16_t$'        { return 'c_short' }
        '^uint8_t$'        { return 'c_ubyte' }
        '^int8_t$'         { return 'c_byte' }
        '^size_t$'         { return 'c_size' }
        '^float$'          { return 'c_float' }
        '^double$'         { return 'c_double' }
        '^bool$'           { return 'c_bool' }
        '^unsigned long$'  { return 'c_uint' }
        '^long$'           { return 'c_int' }
        '^unsigned$'       { return 'c_uint' }
        default {
            # 结构体类型、枚举、typedef 等 → point
            if ($clean -match '_t$' -or $clean -match '^xge_' -or $clean -match '^xui_') {
                return 'point'
            }
            return 'point'
        }
    }
}

# ============================================================
# 解析函数声明
# ============================================================
function Parse-Functions([string]$filePath, [string]$apiMacro) {
    $content = Get-Content $filePath -Raw
    $functions = @()

    # 匹配: XGE_API retType funcName(params);
    $pattern = "(?m)^$apiMacro\s+(.+?)\s+(\w+)\s*\(([^)]*)\)\s*;"
    $matches = [regex]::Matches($content, $pattern)

    foreach ($m in $matches) {
        $retType = $m.Groups[1].Value.Trim()
        $funcName = $m.Groups[2].Value.Trim()
        $paramStr = $m.Groups[3].Value.Trim()

        # 跳过 debug-only 函数（需要 XGE_DEBUGMODE）
        if ($funcName -match '^xgedbg' -or $funcName -match '^xgeDebug') { continue }

        # 跳过未实现的函数
        if ($implementedFuncs.Count -gt 0 -and -not $implementedFuncs.ContainsKey($funcName)) { continue }

        # 解析参数
        $params = @()
        if ($paramStr -ne 'void' -and $paramStr -ne '') {
            # 分割参数（注意函数指针参数中有逗号）
            $depth = 0
            $current = ''
            for ($i = 0; $i -lt $paramStr.Length; $i++) {
                $ch = $paramStr[$i]
                if ($ch -eq '(') { $depth++ }
                elseif ($ch -eq ')') { $depth-- }
                elseif ($ch -eq ',' -and $depth -eq 0) {
                    $params += $current.Trim()
                    $current = ''
                    continue
                }
                $current += $ch
            }
            if ($current.Trim()) { $params += $current.Trim() }
        }

        $parsedParams = @()
        foreach ($p in $params) {
            $p = $p.Trim()
            if ($p -eq '' -or $p -eq 'void') { continue }
            # 提取参数名和类型
            # 模式: type name 或 type* name 或 type (*name)(...)
            if ($p -match '\(\s*\*\s*(\w+)\s*\)') {
                # 函数指针参数: retType (*name)(args) → point
                $paramName = $Matches[1]
                $parsedParams += @{ Name = $paramName; XlType = 'point'; IsCallback = $true }
            } elseif ($p -match '^(.+?)\s+(\w+)\s*$') {
                $paramType = $Matches[1].Trim()
                $paramName = $Matches[2].Trim()
                $xlType = Get-XlType $paramType
                $parsedParams += @{ Name = $paramName; XlType = $xlType; IsCallback = $false }
            } elseif ($p -match '^(.+?)\s*\*\s*(\w+)\s*$') {
                $paramType = $Matches[1].Trim() + '*'
                $paramName = $Matches[2].Trim()
                $xlType = Get-XlType $paramType
                $parsedParams += @{ Name = $paramName; XlType = $xlType; IsCallback = $false }
            } else {
                # 只有类型没有名字
                $xlType = Get-XlType $p
                $parsedParams += @{ Name = "arg$($parsedParams.Count)"; XlType = $xlType; IsCallback = $false }
            }
        }

        $xlRetType = Get-XlType $retType
        $functions += @{
            Name = $funcName
            RetType = $xlRetType
            Params = $parsedParams
            HasCallback = ($parsedParams | Where-Object { $_.IsCallback }).Count -gt 0
        }
    }
    return $functions
}

# ============================================================
# 解析常量
# ============================================================
function Parse-Constants([string]$filePath, [string]$prefix) {
    $content = Get-Content $filePath -Raw
    $constants = @()
    $pattern = "(?m)^#define\s+($prefix\w+)\s+(.+?)\s*$"
    $matches = [regex]::Matches($content, $pattern)
    foreach ($m in $matches) {
        $name = $m.Groups[1].Value
        $value = $m.Groups[2].Value.Trim()
        # 跳过 include guard 和版本宏中的非数值
        if ($name -eq "${prefix}H") { continue }
        # 跳过多行宏（值以 \ 结尾或仅为 \）
        if ($value -eq '\' -or $value -match '\\$') { continue }
        $constants += @{ Name = $name; Value = $value }
    }
    return $constants
}

# ============================================================
# 模块分组规则
# ============================================================
function Get-XgeModule([string]$funcName) {
    if ($funcName -match '^xgeShape(Ex|Ex_)') { return 'xge_shape_ex' }
    if ($funcName -match '^xgeSvg') { return 'xge_svg' }
    if ($funcName -match '^xgeShape') { return 'xge_shape' }
    if ($funcName -match '^xge(Texture|Image|Offscreen)') { return 'xge_texture' }
    if ($funcName -match '^xge(Font|Text)') { return 'xge_font' }
    if ($funcName -match '^xge(Sound|Music|Stream|Audio)') { return 'xge_audio' }
    if ($funcName -match '^xge(Key|Mouse|Touch|Gamepad|Input|Joystick)') { return 'xge_input' }
    if ($funcName -match '^xge(Scene|Sprite|Mesh|Material|Shader)') { return 'xge_scene' }
    if ($funcName -match '^xgeAsync') { return 'xge_async' }
    if ($funcName -match '^xge(Camera|Pass|Clear|Begin|End|Draw|Render)') { return 'xge_render' }
    if ($funcName -match '^xge(Window|Platform|Display|Monitor)') { return 'xge_platform' }
    return 'xge_core'
}

function Get-XuiModule([string]$funcName) {
    if ($funcName -match '^xui(Button|Checkbox|Radio|Toggle|Switch|Slider|Spin|Input|TextEdit|CodeEdit)') { return 'xui_controls' }
    if ($funcName -match '^xui(Panel|Tabs|Window|Scroll|Split|Stack|Group|Accordion|Breadcrumb|Page|Dock)') { return 'xui_containers' }
    if ($funcName -match '^xui(List|Tree|Table|Chart|Grid|DataView|Property)') { return 'xui_data' }
    if ($funcName -match '^xui(Popup|Menu|Toolbar|Msgbox|Toast|Tooltip|Dialog|Message)') { return 'xui_overlay' }
    if ($funcName -match '^xui(Combo|Cascader|ColorPicker|DatePicker|TimePicker|IconPicker)') { return 'xui_picker' }
    if ($funcName -match '^xui(Terminal|Timeline|Workflow|FlowGraph|Inventory|Carousel|Virtual)') { return 'xui_advanced' }
    if ($funcName -match '^xui(Style|Theme|Token)') { return 'xui_style' }
    if ($funcName -match '^xui(Surface|Painter|Path|TextLayout|DrawContext)') { return 'xui_surface' }
    if ($funcName -match '^xui(Input|Focus|Hotkey|Pointer|Key)') { return 'xui_input' }
    if ($funcName -match '^xui(Icon)') { return 'xui_icon' }
    if ($funcName -match '^xui(Language|I18n|Locale)') { return 'xui_i18n' }
    if ($funcName -match '^xuiWidget') { return 'xui_widget' }
    return 'xui_core'
}

# ============================================================
# 结构体按值传递 bridge 映射表
# xge_native.c 为这些函数提供了 __xl6_ 前缀的标量参数 bridge，
# 把结构体参数拆成标量，避免 xlang 侧直接传递 C 结构体。
# key = 原始 C 函数名, value = bridge 标量参数列表（名称和 xlang 类型）
# ============================================================
$BridgeFuncs = @{
    'xgeShapeRectFill'           = @(@{Name='fX';XlType='c_float'},@{Name='fY';XlType='c_float'},@{Name='fW';XlType='c_float'},@{Name='fH';XlType='c_float'},@{Name='iColor';XlType='c_uint'})
    'xgeShapeRectFillPx'         = @(@{Name='fX';XlType='c_float'},@{Name='fY';XlType='c_float'},@{Name='fW';XlType='c_float'},@{Name='fH';XlType='c_float'},@{Name='iColor';XlType='c_uint'})
    'xgeShapeRectStroke'         = @(@{Name='fX';XlType='c_float'},@{Name='fY';XlType='c_float'},@{Name='fW';XlType='c_float'},@{Name='fH';XlType='c_float'},@{Name='fWidth';XlType='c_float'},@{Name='iColor';XlType='c_uint'})
    'xgeShapeRectStrokePx'       = @(@{Name='fX';XlType='c_float'},@{Name='fY';XlType='c_float'},@{Name='fW';XlType='c_float'},@{Name='fH';XlType='c_float'},@{Name='fWidth';XlType='c_float'},@{Name='iColor';XlType='c_uint'})
    'xgeShapeRoundRectFill'      = @(@{Name='fX';XlType='c_float'},@{Name='fY';XlType='c_float'},@{Name='fW';XlType='c_float'},@{Name='fH';XlType='c_float'},@{Name='fRadius';XlType='c_float'},@{Name='iColor';XlType='c_uint'})
    'xgeShapeRoundRectStroke'    = @(@{Name='fX';XlType='c_float'},@{Name='fY';XlType='c_float'},@{Name='fW';XlType='c_float'},@{Name='fH';XlType='c_float'},@{Name='fRadius';XlType='c_float'},@{Name='fWidth';XlType='c_float'},@{Name='iColor';XlType='c_uint'})
    'xgeShapeRoundRectDraw'      = @(@{Name='fX';XlType='c_float'},@{Name='fY';XlType='c_float'},@{Name='fW';XlType='c_float'},@{Name='fH';XlType='c_float'},@{Name='fRadius';XlType='c_float'},@{Name='iFillColor';XlType='c_uint'},@{Name='fStrokeWidth';XlType='c_float'},@{Name='iStrokeColor';XlType='c_uint'})
    'xgeShapeRoundRectFillEx'    = @(@{Name='fX';XlType='c_float'},@{Name='fY';XlType='c_float'},@{Name='fW';XlType='c_float'},@{Name='fH';XlType='c_float'},@{Name='fTL';XlType='c_float'},@{Name='fTR';XlType='c_float'},@{Name='fBR';XlType='c_float'},@{Name='fBL';XlType='c_float'},@{Name='iMode';XlType='c_int'},@{Name='iColor';XlType='c_uint'})
    'xgeShapeRoundRectStrokeEx'  = @(@{Name='fX';XlType='c_float'},@{Name='fY';XlType='c_float'},@{Name='fW';XlType='c_float'},@{Name='fH';XlType='c_float'},@{Name='fTL';XlType='c_float'},@{Name='fTR';XlType='c_float'},@{Name='fBR';XlType='c_float'},@{Name='fBL';XlType='c_float'},@{Name='iMode';XlType='c_int'},@{Name='fWidth';XlType='c_float'},@{Name='iColor';XlType='c_uint'})
    'xgeShapeCapsuleFill'        = @(@{Name='fX';XlType='c_float'},@{Name='fY';XlType='c_float'},@{Name='fW';XlType='c_float'},@{Name='fH';XlType='c_float'},@{Name='iColor';XlType='c_uint'})
    'xgeShapeCapsuleStroke'      = @(@{Name='fX';XlType='c_float'},@{Name='fY';XlType='c_float'},@{Name='fW';XlType='c_float'},@{Name='fH';XlType='c_float'},@{Name='fWidth';XlType='c_float'},@{Name='iColor';XlType='c_uint'})
    'xgeShapeTriangleFill'       = @(@{Name='fX0';XlType='c_float'},@{Name='fY0';XlType='c_float'},@{Name='fX1';XlType='c_float'},@{Name='fY1';XlType='c_float'},@{Name='fX2';XlType='c_float'},@{Name='fY2';XlType='c_float'},@{Name='iColor';XlType='c_uint'})
    'xgeShapeTriangleStroke'     = @(@{Name='fX0';XlType='c_float'},@{Name='fY0';XlType='c_float'},@{Name='fX1';XlType='c_float'},@{Name='fY1';XlType='c_float'},@{Name='fX2';XlType='c_float'},@{Name='fY2';XlType='c_float'},@{Name='fWidth';XlType='c_float'},@{Name='iColor';XlType='c_uint'})
    'xgeClipSet'                 = @(@{Name='fX';XlType='c_float'},@{Name='fY';XlType='c_float'},@{Name='fW';XlType='c_float'},@{Name='fH';XlType='c_float'})
    'xgeViewportSet'             = @(@{Name='fX';XlType='c_float'},@{Name='fY';XlType='c_float'},@{Name='fW';XlType='c_float'},@{Name='fH';XlType='c_float'})
    'xgeInvalidateRect'          = @(@{Name='fX';XlType='c_float'},@{Name='fY';XlType='c_float'},@{Name='fW';XlType='c_float'},@{Name='fH';XlType='c_float'})
    # 像素精确几何（xge_rect_i_t / xge_edges_i_t 按值参数拆标量，bridge 在 xge_native.c）
    'xgeShapeRectFillPixels'     = @(@{Name='iX';XlType='c_int'},@{Name='iY';XlType='c_int'},@{Name='iW';XlType='c_int'},@{Name='iH';XlType='c_int'},@{Name='iColor';XlType='c_uint'})
    'xgeShapeRectBorderPixels'   = @(@{Name='iX';XlType='c_int'},@{Name='iY';XlType='c_int'},@{Name='iW';XlType='c_int'},@{Name='iH';XlType='c_int'},@{Name='iLeft';XlType='c_int'},@{Name='iTop';XlType='c_int'},@{Name='iRight';XlType='c_int'},@{Name='iBottom';XlType='c_int'},@{Name='iColor';XlType='c_uint'})
    'xgeViewportSetPixels'       = @(@{Name='iX';XlType='c_int'},@{Name='iY';XlType='c_int'},@{Name='iW';XlType='c_int'},@{Name='iH';XlType='c_int'})
    'xgeClipSetPixels'           = @(@{Name='iX';XlType='c_int'},@{Name='iY';XlType='c_int'},@{Name='iW';XlType='c_int'},@{Name='iH';XlType='c_int'})
}

# ============================================================
# 生成 .xl 文件内容
# ============================================================
function Generate-XlFile([string]$moduleName, [array]$functions, [array]$constants) {
    $sb = [System.Text.StringBuilder]::new()
    [void]$sb.AppendLine("// Auto-generated by gen_xl_bind.ps1 - DO NOT EDIT")
    [void]$sb.AppendLine("// Module: $moduleName")
    [void]$sb.AppendLine("")

    # 常量
    if ($constants.Count -gt 0) {
        [void]$sb.AppendLine("// === Constants ===")
        foreach ($c in $constants) {
            $val = $c.Value
            # 去掉 unsigned 后缀 u/U
            $val = $val -replace '[uU]$', ''
            if ($val -match '^\d+$' -or $val -match '^0x[0-9a-fA-F]+$') {
                # 判断值范围选择类型
                $numVal = [uint64]0
                try {
                    if ($val -match '^0x') { $numVal = [System.Convert]::ToUInt64($val, 16) }
                    else { $numVal = [System.Convert]::ToUInt64($val, 10) }
                } catch { $numVal = [uint64]0 }
                if ($numVal -gt 4294967295) {
                    [void]$sb.AppendLine("export const c_ulonglong $($c.Name) = $val")
                } elseif ($numVal -gt 2147483647) {
                    [void]$sb.AppendLine("export const c_uint $($c.Name) = $val")
                } else {
                    [void]$sb.AppendLine("export const c_int $($c.Name) = $val")
                }
            } elseif ($val -match '^\(' -or $val -match '[a-zA-Z_]') {
                # 复合表达式或引用其他宏 - 注释掉
                [void]$sb.AppendLine("// export const c_int $($c.Name) = $($c.Value)")
            } else {
                [void]$sb.AppendLine("export const c_int $($c.Name) = $val")
            }
        }
        [void]$sb.AppendLine("")
    }

    # 函数
    if ($functions.Count -gt 0) {
        [void]$sb.AppendLine("// === Functions ===")
        foreach ($fn in $functions) {
            # 检查是否有结构体 bridge（参数拆标量）
            $useBridge = $BridgeFuncs.ContainsKey($fn.Name)
            if ($useBridge) {
                $bridgeParams = $BridgeFuncs[$fn.Name]
                $externName = "__xl6_$($fn.Name)"
            } else {
                $bridgeParams = $fn.Params
                $externName = $fn.Name
            }

            # 生成 extern 声明
            $paramParts = @()
            $callArgs = @()
            foreach ($p in $bridgeParams) {
                $xlType = if ($p.XlType -ne '') { $p.XlType } else { 'point' }
                $paramParts += "$xlType $($p.Name)"
                $callArgs += $p.Name
            }
            $paramStr = ($paramParts -join ', ')
            $callStr = ($callArgs -join ', ')
            $retStr = if ($fn.RetType -ne '') { " as $($fn.RetType)" } else { '' }
            $internName = "__xl6_$($fn.Name)"

            [void]$sb.AppendLine("extern `"$externName`" func $internName($paramStr)$retStr")
            # 生成 export 包装函数
            [void]$sb.AppendLine("export func $($fn.Name)($paramStr)$retStr")
            if ($fn.RetType -ne '') {
                [void]$sb.AppendLine("    return $internName($callStr)")
            } else {
                [void]$sb.AppendLine("    $internName($callStr)")
            }
            [void]$sb.AppendLine("end")
            [void]$sb.AppendLine("")
        }
    }

    return $sb.ToString()
}

# ============================================================
# 主流程
# ============================================================
Write-Host "[gen_xl_bind] Package root: $PkgRoot"

$xgeH = "$PkgRoot\xge.h"
$xuiH = "$PkgRoot\xui.h"
$implList = "$PkgRoot\implemented_funcs.txt"

if (-not (Test-Path $xgeH)) { Write-Host "[ERROR] xge.h not found" -ForegroundColor Red; exit 1 }
if (-not (Test-Path $xuiH)) { Write-Host "[ERROR] xui.h not found" -ForegroundColor Red; exit 1 }

# 加载已实现函数列表
$implementedFuncs = @{}
if (Test-Path $implList) {
    Get-Content $implList | ForEach-Object { $implementedFuncs[$_.Trim()] = $true }
    Write-Host "[gen_xl_bind] Loaded $($implementedFuncs.Count) implemented functions"
} else {
    Write-Host "[WARN] implemented_funcs.txt not found, generating all functions" -ForegroundColor Yellow
}

# 解析
Write-Host "[gen_xl_bind] Parsing xge.h ..."
$xgeFuncs = Parse-Functions $xgeH "XGE_API"
Write-Host "  Functions: $($xgeFuncs.Count)"

Write-Host "[gen_xl_bind] Parsing xui.h ..."
$xuiFuncs = Parse-Functions $xuiH "XUI_API"
Write-Host "  Functions: $($xuiFuncs.Count)"

Write-Host "[gen_xl_bind] Parsing constants ..."
$xgeConsts = Parse-Constants $xgeH "XGE_"
$xuiConsts = Parse-Constants $xuiH "XUI_"
Write-Host "  XGE constants: $($xgeConsts.Count)"
Write-Host "  XUI constants: $($xuiConsts.Count)"

# 分组 XGE
$xgeModules = @{}
foreach ($fn in $xgeFuncs) {
    $mod = Get-XgeModule $fn.Name
    if (-not $xgeModules.ContainsKey($mod)) { $xgeModules[$mod] = @() }
    $xgeModules[$mod] += $fn
}

# 分组 XUI
$xuiModules = @{}
foreach ($fn in $xuiFuncs) {
    $mod = Get-XuiModule $fn.Name
    if (-not $xuiModules.ContainsKey($mod)) { $xuiModules[$mod] = @() }
    $xuiModules[$mod] += $fn
}

# 生成 xge 包
$xgeOutDir = "$PkgRoot\..\xge\src"
if (-not (Test-Path $xgeOutDir)) { New-Item -ItemType Directory -Force -Path $xgeOutDir | Out-Null }

Write-Host "[gen_xl_bind] Generating xge package ..."
foreach ($modName in ($xgeModules.Keys | Sort-Object)) {
    # xge_shape_ex 和 xge_svg 属于 xge_shape_ex 包，不放入 xge 包
    if ($modName -eq 'xge_shape_ex' -or $modName -eq 'xge_svg') { continue }
    $funcs = $xgeModules[$modName]
    # 常量分配到对应模块
    $modConsts = @()
    if ($modName -eq 'xge_core') { $modConsts = $xgeConsts }
    $content = Generate-XlFile $modName $funcs $modConsts
    $outFile = "$xgeOutDir\$modName.xl"
    Set-Content -Path $outFile -Value $content -Encoding UTF8
    Write-Host "  $modName.xl ($($funcs.Count) functions)"
}

# 生成 xui 包
$xuiOutDir = "$PkgRoot\..\xui\src"
if (-not (Test-Path $xuiOutDir)) { New-Item -ItemType Directory -Force -Path $xuiOutDir | Out-Null }

Write-Host "[gen_xl_bind] Generating xui package ..."
foreach ($modName in ($xuiModules.Keys | Sort-Object)) {
    $funcs = $xuiModules[$modName]
    $modConsts = @()
    if ($modName -eq 'xui_core') { $modConsts = $xuiConsts }
    $content = Generate-XlFile $modName $funcs $modConsts
    $outFile = "$xuiOutDir\$modName.xl"
    Set-Content -Path $outFile -Value $content -Encoding UTF8
    Write-Host "  $modName.xl ($($funcs.Count) functions)"
}

# 生成 xge_shape_ex 包（从 xge 模块中提取）
$shapeExDir = "$PkgRoot\..\xge_shape_ex\src"
if (-not (Test-Path $shapeExDir)) { New-Item -ItemType Directory -Force -Path $shapeExDir | Out-Null }

if ($xgeModules.ContainsKey('xge_shape_ex')) {
    $funcs = $xgeModules['xge_shape_ex']
    $svgFuncs = @()
    if ($xgeModules.ContainsKey('xge_svg')) { $svgFuncs = $xgeModules['xge_svg'] }
    $content = Generate-XlFile 'xge_shape_ex' $funcs @()
    Set-Content -Path "$shapeExDir\xge_shape_ex.xl" -Value $content -Encoding UTF8
    Write-Host "  xge_shape_ex.xl ($($funcs.Count) functions)"
    if ($svgFuncs.Count -gt 0) {
        $content = Generate-XlFile 'xge_svg' $svgFuncs @()
        Set-Content -Path "$shapeExDir\xge_svg.xl" -Value $content -Encoding UTF8
        Write-Host "  xge_svg.xl ($($svgFuncs.Count) functions)"
    }
}

# 统计
$totalXge = ($xgeModules.Values | ForEach-Object { $_.Count } | Measure-Object -Sum).Sum
$totalXui = ($xuiModules.Values | ForEach-Object { $_.Count } | Measure-Object -Sum).Sum
Write-Host ""
Write-Host "[gen_xl_bind] Done!"
Write-Host "  XGE: $totalXge functions in $($xgeModules.Count) modules"
Write-Host "  XUI: $totalXui functions in $($xuiModules.Count) modules"
Write-Host "  Constants: $($xgeConsts.Count) XGE + $($xuiConsts.Count) XUI"
