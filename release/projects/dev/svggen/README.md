# svggen - SVG 图形生成器

用 string 拼接生成 SVG XML 文件。基本图形、样式属性齐全，并支持从 JSON 数据生成柱状图 / 饼图 / 折线图。

## 用法

```bat
cd /d D:\GIT\x-lang\release
xl run projects\dev\svggen\svggen.xl chart bar  projects\dev\svggen\sample_bar.json  out.svg
xl run projects\dev\svggen\svggen.xl chart pie  projects\dev\svggen\sample_pie.json  out.svg
xl run projects\dev\svggen\svggen.xl chart line projects\dev\svggen\sample_line.json out.svg
xl run projects\dev\svggen\svggen.xl demo
xl run projects\dev\svggen\svggen.xl test
```

`demo` / `test` 可追加可选输出路径，省略则打印到 stdout。

## 能力

| 类别 | 内容 |
|---|---|
| 基本图形 | `rect` / `circle` / `ellipse` / `line` / `polygon` / `polyline` / `path` / `text` |
| 样式属性 | `fill` / `stroke` / `stroke-width` / `opacity`（直接作为元素属性输出）|
| JSON 图表 | 柱状图（bar）、饼图（pie，圆弧 path 切片）、折线图（line，含半透明面积填充）|
| demo | 彩色组合场景：天空、太阳光线、云、房子、树、河流 path |
| test | 全图元 + 全样式属性对照表，兼作视觉回归基准 |

## JSON 数据格式（三种均支持）

```jsonc
// 1. 平行数组
{ "title": "...", "labels": ["A","B"], "values": [10,20] }

// 2. items 记录数组
{ "title": "...", "items": [ {"label":"A","value":10} ] }

// 3. 顶层记录数组
[ {"label":"A","value":10}, {"name":"B","value":20} ]
```

可选 `"width"` / `"height"` 覆盖默认画布 720×420。记录的键名兼容 `label` 与 `name`。

## 设计要点

- XML 完全由 `+` 字符串拼接生成（`svgRect` / `svgCircle` / ... 一族构造器）。
- 所有数值经 `fmtNum` 归一：整数渲染为 `120` 而非 `120.00`。
- JSON 经 `json.parse` → `var` dict 读取；缺失键通过 `string(v) == ""`（json null → ""）判定。
- 坐标轴使用 `niceCeil` 把最大值向上取整到 1/2/2.5/5/10 刻度，刻度线整齐。
- 饼图用 `arcPath` 生成 `M cx,cy L .. A r r 0 large 1 .. Z` 圆弧路径；单值（100%）退化为整圆。
- 折线图面积填充用闭合 path + `opacity=0.15`。
- 文本经 `xmlEscape` 转义 `& < > " '`，避免数据中特殊字符破坏 XML。
