# XGE 游戏范例

本目录是用 xlang + XGE 扩展库 OOP 游戏栈编写的完整小游戏集合。

游戏栈组成（`import xge`）：

- `XgeApp`：引擎初始化、帧循环、`delta()` 帧间隔、`setMaxFrames(n)` 无头冒烟
- `Input`：键盘（`keyPressed` 边沿 / `keyDown` 持续）、鼠标（`mouseX/mouseY/mouseLeftDown`）、手柄
- `Shape`：形状绘制（float 逻辑坐标 + `*Pixels` 像素精确整数坐标两套）
- `Color`：`rgba(r, g, b, a)` 颜色
- `Font`：字体加载 + `drawText`（游戏内 HUD 文本）
- `Texture` / `Sound` / `Music`：精灵纹理与音频（资产缺失时自动降级）

## 游戏清单

| 文件 | 行数 | 类型 | 玩法 / 演示重点 |
|---|---:|---|---|
| `snake.xl` | ~230 | 键盘 | 贪吃蛇：方向键/WASD、像素精确棋盘、SCORE HUD、`--frames` 无头冒烟 |
| `2048.xl` | 344 | 键盘 | 合并滑动（四方向统一索引映射）、按值分档配色、死局检测 |
| `tetris.xl` | 406 | 键盘 | 7 种方块 4 旋转态偏移表、踢墙旋转、消行计分、速度曲线、NEXT 预览 |
| `flappy.xl` | 237 | 键盘 | 重力积分、管道循环复用、亚像素滚动累积器、三态（待机/进行/死亡） |
| `pong.xl` | 262 | 鼠标 | 鼠标挡板 vs 限速 AI、按命中位置反弹角、7 分制 |
| `breakout.xl` | 300 | 鼠标 | 挡板反弹角、砖块 AABB 按穿透深度判轴、双击耐久砖、3 命 |
| `minesweeper.xl` | 338 | 鼠标 | 显式栈洪泛展开（无递归）、右键插旗、首点保护、经典数字配色 |
| `asteroids.xl` | 353 | 键盘 | 旋转推进物理、子弹继承船速、大中小三分裂、屏幕环绕、波次 |
| `shooter.xl` | 376 | 鼠标 | 精灵纹理 + 音效（laser/boom）、难度曲线、无敌闪烁、双层视差星空 |

## 运行

```bat
cd /d D:\GIT\x-lang\demo6
release\xl.exe run release\examples\games\snake.xl
```

无头冒烟（达到 N 帧自动退出，用于快速自检）：

```bat
release\xl.exe run release\examples\games\snake.xl -- 60
```

音频/纹理资产位于 `assets/`（`player.png` 精灵 + `beep/laser/boom/coin.wav`），
使用相对路径加载；缺失时游戏自动降级为形状绘制并静音，不依赖资产即可运行。

## 回归

```bat
release\examples\games\run_games.bat
```

`snake.xl` 做完整 `xl check`（游戏栈全部语义 + native 编译覆盖），其余游戏做
`parse` 语法门禁；`-FullCheck` 可全量 `check`。

## 通用模式（新游戏参照）

1. 帧回调必须成对调用 `XgeApp.beginFrame(bg)` / `XgeApp.endFrame()`（缺省会黑屏）
2. 键盘单步动作用 `keyPressed` 边沿、持续按住用 `keyDown`
3. 鼠标点击边沿检测：`clicked = down and not was; was = down`
4. 数组无 `removeAt`，活跃元素删除用 swap-with-last + 计数压缩
5. 重开键用 ASCII 码 `keyPressed(82)`（R；框架未导出 `XGE_KEY_R`）
6. `main(...)` 中 `arg.length > 1` 时 `setMaxFrames(int(arg[1]))` 支持无头冒烟
