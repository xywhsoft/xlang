# geo - 几何计算器

平面几何命令行计算器，覆盖距离、圆、矩形、三角形、夹角、多边形面积等常用计算。

## 用法

```
geo distance <x1> <y1> <x2> <y2>          两点欧氏距离
geo circle area <r>                        圆面积
geo circle perimeter <r>                   圆周长
geo rect area <w> <h>                      矩形面积
geo rect diagonal <w> <h>                  矩形对角线
geo triangle area <a> <b> <c>              三角形面积（海伦公式）
geo triangle type <a> <b> <c>              三角形类型（等边/等腰/直角/等腰直角/普通）
geo angle <x1> <y1> <x2> <y2>             两向量夹角（度）
geo polygon area <x1,y1> <x2,y2> ...      多边形面积（鞋带公式）
geo selftest                               内部自检
```

## 运行 / 构建

```
cd /d D:\GIT\x-lang\demo6\release
xl run  projects\dev\geo\geo.xl -- distance 0 0 3 4
xl build projects\dev\geo\geo.xl -o projects\dev\geo\geo.exe
```

`xl run` 与 `xl build` 两条路径均 EXIT=0。

## 设计要点

- 用 `class Point` 封装平面点坐标，提供 `distanceTo` / `dot` / `length` 方法，用于
  `distance`、`angle` 与自检路径（Point 作为静态类型局部变量使用）。
- 数值计算全部走 `math` API：`math.hypot`、`math.sqrt`、`math.pi`、`math.acos`、
  `math.deg`、`math.abs`、`math.max`、`math.isNaN`、`math.isInf`、`math.floor`。
- 浮点近似比较用自定义 `approx`（相对容差 1e-9），避免精度误判三角形类型；
  输出用 `formatNum`，整数值显示为整数，否则保留 6 位小数。
- 多边形顶点解析为两个 `float` 动态数组（`xs` / `ys`），鞋带公式求面积。

## 已知限制 / 发现的 BUG

见上级目录 `BUGS.md` 的 **BUG-G1**：带自定义 `create(args)` 的 class 实例不能直接
装箱进 `var`（动态数组 / `var` 变量），否则编译期报
`class constructor with create/arguments cannot be boxed directly as var`。
因此 `polygon` 路径不使用 `Point` 数组，改为把坐标拆进两个 `float` 动态数组。
