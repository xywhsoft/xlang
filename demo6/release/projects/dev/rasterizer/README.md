# rasterizer - 软件三角形光栅化器（Software Triangle Rasterizer）

xlang 实现的完整 3D 软件光栅化器。验证 **float 密集运算**、**`array<float>` 大循环**、
**4x4 矩阵运算链**、**class 运算符协议**的综合压测点。

## 功能

- **Vec3**（三维向量）：完整运算符协议 `__add__/__sub__/__neg__/__mul__/__div__`，
  方法 `dot/cross/length/lengthSquared/normalize`
- **Mat4**（4x4 矩阵）：内部 `array<float>`（16 元素，行主序），
  方法 `mul`（矩阵乘法）/ `transformPoint`（点变换，返回 x,y,z,w）/ `transformDir`（方向变换）
- **完整 3D 渲染管线**：model → view → projection → 透视除法 → 视口变换
- **重心坐标三角形光栅化**：包围盒裁剪 + 逐像素边函数测试
- **Z-buffer 深度测试**：每像素 NDC z 线性插值 + `array<float>` 深度缓冲
- **Flat shading**：世界空间面法线 → 光照方向，ambient + diffuse
- **网格**：Cube（8 顶点 12 三角形，6 色面）/ Octahedron（6 顶点 8 三角形，8 色面）/ 二者合并
- **输出**：ASCII PPM (P3) 图像文件

## 矩阵

- 透视投影（OpenGL 风格，`matPerspective(fovyDeg, aspect, zNear, zFar)`）
- LookAt 视图（`matLookAt(eye, target, up)`）
- 平移 / 缩放 / 绕 X/Y/Z 轴旋转
- `Mat4.mul(other)` 自乘

## 用法

```bat
cd /d D:\GIT\x-lang\demo6\release

:: 直接运行
xl run projects\dev\rasterizer\rasterizer.xl -- render [w] [h] [out] [scene]
xl run projects\dev\rasterizer\rasterizer.xl -- test
xl run projects\dev\rasterizer\rasterizer.xl -- bench [w] [h]

:: 编译为 exe
xl build projects\dev\rasterizer\rasterizer.xl -o projects\dev\rasterizer\rasterizer.exe
rasterizer.exe render 320 240 out.ppm 0
```

> **注意**：`xl run` 必须用 `--` 分隔程序参数（位置参数会被当作 C 混编 helper 源文件）。

### 子命令

| 命令 | 参数 | 说明 |
|---|---|---|
| `render` | `[w=320] [h=240] [out=out.ppm] [scene=0]` | 渲染场景到 PPM |
| `test` | （无） | 19 项自检（Vec3 / Mat4 / 投影 / 光栅化 / Z-buffer） |
| `bench` | `[w=320] [h=240]` | 跑两轮基准测试（warmup + measure） |

**scene 取值**：`0`=立方体，`1`=八面体，`2`=二者合并（左移+缩小）。

## 性能

| 场景 | 尺寸 | 三角形 | 耗时 | 吞吐 |
|---|---|---|---|---|
| cube | 320x240 | 12 | ~0.022s | ~3.5M pix/sec |
| octahedron | 320x240 | 8 | ~0.006s | ~13M pix/sec |
| both | 320x240 | 20 | ~0.010s | ~7.5M pix/sec |
| cube | 640x480 | 12 | ~0.089s | ~3.5M pix/sec |

实测约 **3-13M 像素/秒**（瓶颈在像素级的重心坐标 / 深度插值 / 着色浮点运算）。
覆盖率在 cube 场景跨分辨率稳定为 20.5%（正确性一致）。

## 输出示例

PPM (P3) 头：

```
P3
320 240
255
15 20 40 15 20 40 ... 186 54 54 ...
```

PPM 文件可用 GIMP / ImageMagick (`magick out.ppm out.png`) / 在线 PPM 查看器打开。

## 重要实现说明（BUG 绕过）

经实战验证，xlang 当前存在以下限制（详见根目录 `BUGS.md`），本程序已绕过：

1. **`set`/`get`/`let` 不能作为 class 成员方法名**（新发现 BUG-RAS1）：
   `func set(...)` → `[sem] get/let/set cannot be used as class member names`。
   绕过：Mat4 的元素写入方法命名为 `setCell`（实际未使用，构造器直接写 `r.m[i]=...`）。

2. **链式方法调用在临时返回值上失败**（BUG-RT4 族）：
   `tr.mul(scl).mul(rotX)` → `[checked] 找不到 class 方法`。
   绕过：拆成独立语句 `Mat4 a = tr.mul(scl); Mat4 m = a.mul(rotX)`。

3. **class 构造表达式作为方法实参被拒**（BUG-RT4）：
   `s.transformPoint(Vec3(1,1,1))` → `[checked] class constructor expression requires statement-level lowering`。
   绕过：先 `Vec3 p = Vec3(...)`，再 `s.transformPoint(p)`。

4. **`array<float>` 不能作为 class 方法的形参**（BUG-C1）：
   绕过：所有 `array<float>` 读写都在 `self.m` / `self.verts` 等 class 字段上，
   或在**全局函数**里通过 Mat4/Mesh/RenderTarget 实例的 `.m`/`.verts`/`.fb` 字段间接访问。

5. **`math.abs(int)` 重载解析失败**（BUG-XL4）：绕过：自写 `absF(float)`。

6. **`xl run file.xl test`（无 `--`）**：位置参数被当作 C helper 源文件，
   报 `cannot add link library: test`。绕过：用 `xl run file.xl -- test`。

## 验证

```bat
:: xl run 路径
xl run projects\dev\rasterizer\rasterizer.xl -- render
:: EXIT=0, 输出 out.ppm (320x240)

:: xl build 路径
xl build projects\dev\rasterizer\rasterizer.xl -o projects\dev\rasterizer\rasterizer.exe
:: EXIT=0, 生成 rasterizer.exe

:: 自检
xl run projects\dev\rasterizer\rasterizer.xl -- test
:: EXIT=0, 19 passed, 0 failed
```

两条路径都必须 EXIT=0。
