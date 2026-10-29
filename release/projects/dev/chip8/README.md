# chip8 - CHIP-8 虚拟机/模拟器

用 xlang 实现的 CHIP-8 解释器，验证二进制处理、指令解码和模拟循环。

## 用法

```bat
cd /d D:\GIT\x-lang\release

xl run projects\dev\chip8\chip8.xl -- test
xl run projects\dev\chip8\chip8.xl -- demo
xl run projects\dev\chip8\chip8.xl -- disasm projects\dev\chip8\fontA.ch8
xl run projects\dev\chip8\chip8.xl -- load projects\dev\chip8\fontA.ch8 20

xl build projects\dev\chip8\chip8.xl -o projects\dev\chip8\chip8.exe
chip8.exe test
chip8.exe demo
```

## 命令

| 命令 | 说明 |
|---|---|
| `chip8 load <rom> [N=1000]` | 加载 ROM 执行 N 个周期，转储寄存器和显示 |
| `chip8 disasm <rom>` | 反汇编 ROM 指令 |
| `chip8 test` | 内置 13 项指令测试（覆盖全部 35 条 opcode 系列） |
| `chip8 demo` | 内置 C8 logo 垂直滚动动画（4 帧快照） |
| `chip8 -h / --help` | 帮助 |

## 实现的指令集（35 条，Cowgod's 参考）

```
00E0 CLS    00EE RET    1nnn JP     2nnn CALL   3xkk SE Vx,kk
4xkk SNE    5xy0 SE     6xkk LD     7xkk ADD    8xy0 LD Vx,Vy
8xy1 OR     8xy2 AND    8xy3 XOR    8xy4 ADD    8xy5 SUB
8xy6 SHR    8xy7 SUBN   8xyE SHL    9xy0 SNE    Annn LD I
Bnnn JP V0  Cxkk RND    Dxyn DRW    Ex9E SKP    ExA1 SKNP
Fx07 LD Vx,DT   Fx0A LD Vx,K   Fx15 LD DT,Vx  Fx18 LD ST,Vx
Fx1E ADD I,Vx   Fx29 LD F,Vx   Fx33 LD B,Vx   Fx55 LD [I],Vx
Fx65 LD Vx,[I]
```

## 架构

- **内存**：4096 字节 `array<int>`，标准 hex 字体（0-F）烧入 0x50。
- **寄存器**：V0-VF（`array<int>`）+ I（16 位地址寄存器）。
- **栈**：16 层 `array<int>` 存返回地址 + SP。
- **显示**：64×32 1bpp `array<int>`，XOR 精灵碰撞检测，越界裁剪。
- **定时器**：delay + sound，每条指令自减。
- **按键**：16 键 `array<int>`；无 stdin，`Fx0A`（等键）退化为返回键 0。

## 内置测试覆盖

| 测试 | 覆盖指令 |
|---|---|
| T1 | LD / 7xkk ADD |
| T2 | 8xy4 ADD（进位） |
| T3 | 8xy5 SUB（借位） |
| T4 | 8xy7 SUBN |
| T5 | 8xy1 OR / 8xy2 AND / 8xy3 XOR |
| T6 | 8xy6 SHR / 8xyE SHL（含 VF 标志） |
| T7 | Annn LD I / 2nnn CALL / 00EE RET |
| T8 | 3xkk SE / 4xkk SNE / 5xy0 SE / 9xy0 SNE |
| T9 | Cxkk RND（掩码边界） |
| T10 | Fx55 LD [I],Vx / Fx65 LD Vx,[I] 往返 |
| T11 | Fx33 LD B（BCD） |
| T12 | Dxyn DRW / Fx29 LD F / 00E0 CLS（像素比对） |
| T13 | Fx15 LD DT / Fx07 LD Vx,DT / Bnnn JP V0 |

## 已知绕过（详见 ../BUGS.md BUG-C1/C2）

xlang class 方法不能安全接收 `array<int>` / `Buffer` 形参。ROM 加载逻辑
因此从 class 方法移到全局函数 `loadBytes(Chip8, array<int>)` /
`loadBuffer(Chip8, Buffer)`。
