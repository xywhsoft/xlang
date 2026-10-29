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
cd /d D:\GIT\x-lang\release

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

- `World.shapes` 使用 `array<Shape>` 保存 `Sphere`、`Plane` 等派生实例。
- `Material` 和命中记录保持静态类型，容器不会切片派生对象；virtual 调用按实际类型派发。
- class 采用受管理引用语义，场景只传递对象引用，不复制完整几何体或材质。
- 向量、颜色和像素数据使用静态类型，`var` 只留给确实需要动态数据的边界。
- 自测覆盖派生对象装入基类数组、命中材质保留和虚调用结果。

## 验证

```bat
:: xl run 路径
xl run projects\dev\raytrace\raytrace.xl -- render 100 75 5
:: EXIT=0, 输出 raytrace_out.ppm

xl run projects\dev\raytrace\raytrace.xl -- test
:: EXIT=0, 9 passed, 0 failed

:: xl build 路径
xl build projects\dev\raytrace\raytrace.xl -o projects\dev\raytrace\raytrace.exe
:: EXIT=0, 生成 raytrace.exe
```

运行、自测和构建路径都必须返回 0。
