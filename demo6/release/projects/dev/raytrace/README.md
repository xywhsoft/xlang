# raytrace - 光线追踪渲染器（Ray Tracer）

xlang 实现的递归光线追踪渲染器，参考 *"Ray Tracing in One Weekend"* 教程。
综合验证 xlang 的**浮点运算**、**class 运算符协议**、**继承多态**和**递归性能**。

## 功能

- **Vec3**（三维向量）：完整运算符协议 `__add__/__sub__/__neg__/__mul__/__div__`，
  方法 `dot/cross/length/normalize/reflect/refract/nearZero`
- **Color**（RGB）：颜色运算（加法、Hadamard 积、标量乘除）+ gamma 校正 + 字节转换
- **Ray**（光线）：origin + direction，`at(t)` 求射线点
- **Shape 抽象层**：`Shape` → `Sphere` / `Plane`，`virtual hit()` 返回 `(bool, HitRecord)`
- **Material 抽象层**：`Material` → `Lambertian`（漫反射）/ `Metal`（金属反射 + fuzz）/ `Dielectric`（玻璃折射 + Schlick 近似）
- **Camera**：视场、光圈/景深、焦距、抗锯齿多次采样
- **场景**：经典三球、随机小球场景、简化场景
- **输出**：ASCII PPM (P3) 图像文件

## 用法

```bat
cd /d D:\GIT\x-lang\demo6\release

:: 直接运行
xl run projects\dev\raytrace\raytrace.xl -- render [w] [h] [s] [out] [scene]
xl run projects\dev\raytrace\raytrace.xl -- demo
xl run projects\dev\raytrace\raytrace.xl -- bench [w] [h] [s]

:: 编译为 exe
xl build projects\dev\raytrace\raytrace.xl -o projects\dev\raytrace\raytrace.exe
raytrace.exe render 200 150 10 out.ppm 0
```

### 子命令

| 命令 | 参数 | 说明 |
|---|---|---|
| `render` | `[w=200] [h=150] [s=10] [out=raytrace_out.ppm] [scene=0]` | 渲染场景到 PPM |
| `demo` | （无） | 渲染默认 200×150×10 经典三球场景到 `demo.ppm` |
| `bench` | `[w=100] [h=75] [s=5]` | 跑两轮基准测试（warmup + measure） |

**scene 取值**：`0`=简化场景（5 球+地面），`1`=经典三球+大地面球，`2`=随机小球场景。

### 默认渲染参数

| 场景 | 默认尺寸 | 默认采样 | 预期耗时 |
|---|---|---|---|
| render (simple) | 200×150 | 10 | ~1.5s |
| demo (classic) | 200×150 | 10 | ~2.2s |
| bench (simple) | 100×75 | 5 | ~0.35s/run |

实测吞吐约 **10 万根光线/秒**（每条光线平均递归 5 层）。

## 输出示例

PPM (P3) 头：

```
P3
200 150
255
214 232 255 214 232 255 ...
```

PPM 文件可用 GIMP / ImageMagick (`display`) / 在线 PPM 查看器打开。
转换为 PNG：

```bat
magick demo.ppm demo.png
```

## 重要实现说明（多态存储）

经实战验证，xlang 当前对**多态 class 实例的存储**有以下限制（详见根目录 `BUGS.md` 的 BUG-RT1 / BUG-RT2 / BUG-RT3）：

1. `array<Base>` 在 `append(Derived)` 时**静默丢弃**派生类实例。
2. `Base` 类型字段 + `derive value` 会把派生类按值**切片**到 Base，丢失 override。
3. 从函数**返回**带 virtual 方法 + var 字段的 class 实例会**丢失 vtable**，后续 virtual 调用崩溃。

**因此本程序采用以下设计**：

- `World.shapes` 字段使用 `array<var>`（不是 `array<Shape>`）
- `Sphere` / `Plane` 的 `mat` 字段使用 `var`（不是 `Material`）
- `HitRecord.mat` 字段使用 `var`
- 取用时统一 `as Base` 还原类型，vtable 派发到对应 override
- **场景构造中所有 Shape / Material 实例都直接 `new`**，不通过工厂函数返回

另：

- 「class 构造嵌套 class 构造」会触发 statement-level lowering 限制（BUG-RT4），
  每个内层 class 实例必须先单独构造为局部变量，再传入外层构造。
- 复合赋值 `+=` 不派发 class 运算符协议（已记录 BUG-V1），所以累加用 `s = s + x`。
- 局部变量不能命名为 `dir` / `path` / `time` 等标准库名（会被识别为命名空间访问）。

## 验证

```bat
:: xl run 路径
xl run projects\dev\raytrace\raytrace.xl -- render 100 75 5
:: EXIT=0, 输出 raytrace_out.ppm

:: xl build 路径
xl build projects\dev\raytrace\raytrace.xl -o projects\dev\raytrace\raytrace.exe
:: EXIT=0, 生成 raytrace.exe
```

两条路径都必须 EXIT=0。
