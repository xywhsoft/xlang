# OS 操作系统库

> 进程管理和系统调用功能

[English](api-os.en.md) | [中文](api-os.md) | [返回索引](README.md)

---

## 📑 目录

- [进程执行](#进程执行)
- [使用场景](#使用场景)
- [平台差异](#平台差异)

---

## 进程执行

### xrtRun

运行外部程序（异步）

**函数原型：**
```c
XXAPI ptr xrtRun(str sPath, size_t iSize);
```

**参数：**
- `sPath` - 程序路径或命令行
- `iSize` - 路径长度（0表示自动计算）

**返回值：**
- Windows: 返回进程句柄 (`HANDLE`)
- Linux: 返回进程ID (`pid_t`) 转换为指针

**说明：**
- 异步执行，不等待程序结束
- 立即返回进程句柄/ID
- 可用于启动后台进程

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
#if defined(_WIN32) || defined(_WIN64)
    // Windows: 启动记事本
    ptr handle = xrtRun((str)"notepad.exe", 0);
    if (handle) {
        printf("程序已启动，进程句柄: %p\n", handle);
        // 进程在后台运行，不等待
    }
    
    // 启动带参数的程序
    ptr h2 = xrtRun((str)"cmd.exe /c echo Hello World", 0);
#else
    // Linux/macOS: 启动程序
    ptr pid = xrtRun((str)"ls -la", 0);
    if (pid) {
        printf("程序已启动，PID: %ld\n", (long)(intptr_t)pid);
    }
    
    // 启动后台进程
    ptr pid2 = xrtRun((str)"sleep 10 &", 0);
#endif
    
    xrtUnit();
    return 0;
}
```

**补充说明：**
- Windows 使用 `CreateProcessW` 创建新进程
- Linux 使用 `fork` + `execl` 通过 `/bin/sh` 执行
- 返回的句柄/PID 可用于后续管理（如等待、终止）

---

### xrtStart

打开文件或 URL（使用系统默认程序）

**函数原型：**
```c
XXAPI ptr xrtStart(str sPath, size_t iSize);
```

**参数：**
- `sPath` - 文件路径或 URL
- `iSize` - 路径长度（0表示自动计算）

**返回值：**
- Windows: 返回 ShellExecute 的结果（大于 32 表示成功）
- Linux: 返回进程 ID（成功）或 `NULL`（失败）

**平台支持：**
- Windows: 使用 `ShellExecuteW`
- Linux/macOS: 使用 `xdg-open`

**说明：**
- 使用系统默认程序打开文件
- 可以打开 URL（使用默认浏览器）
- 可以打开文档（使用关联程序）
- 类似于双击文件的效果

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
#if defined(_WIN32) || defined(_WIN64)
    // 用默认程序打开文本文件
    ptr result = xrtStart((str)"C:\\test.txt", 0);
    printf("结果: %p\n", result);
    
    // 用默认浏览器打开网页
    xrtStart((str)"https://www.example.com", 0);
    
    // 打开图片
    xrtStart((str)"C:\\photo.jpg", 0);
    
    // 打开文件夹（资源管理器）
    xrtStart((str)"C:\\Users", 0);
    
    // 打开邮件客户端
    xrtStart((str)"mailto:user@example.com", 0);
#else
    // Linux/macOS: 使用 xdg-open
    ptr pid = xrtStart((str)"/home/user/document.pdf", 0);
    if (pid) {
        printf("xdg-open 启动成功，PID: %ld\n", (long)(intptr_t)pid);
    }
    
    // 打开网页
    xrtStart((str)"https://www.example.com", 0);
#endif
    
    xrtUnit();
    return 0;
}
```

**补充说明：**
- Windows 支持特殊协议：`mailto:`, `file://`, `http://`, `https://` 等
- Linux 需要安装 `xdg-utils` 包（大多数发行版默认包含）

---

### xrtChain

运行程序并等待结束（同步）

**函数原型：**
```c
XXAPI int xrtChain(str sPath, size_t iSize);
```

**参数：**
- `sPath` - 程序路径或命令行
- `iSize` - 路径长度（0表示自动计算）

**返回值：**
- 程序的退出码
- Windows: 程序的返回值
- Linux: 程序的退出状态

**说明：**
- **同步执行**，会阻塞当前线程
- 等待程序执行完毕后返回
- 返回程序的退出码

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
#if defined(_WIN32) || defined(_WIN64)
    // Windows: 执行命令并等待完成
    int exit_code = xrtChain((str)"cmd.exe /c dir C:\\", 0);
    printf("退出码: %d\n", exit_code);
    
    // 执行批处理脚本
    int code = xrtChain((str)"backup.bat", 0);
    if (code == 0) {
        printf("备份成功\n");
    } else {
        printf("备份失败，退出码: %d\n", code);
    }
#else
    // Linux: 执行 shell 命令
    int result = xrtChain((str)"ls -la /home", 0);
    printf("退出码: %d\n", result);
    
    // 执行脚本
    int code = xrtChain((str)"./backup.sh", 0);
    if (code == 0) {
        printf("备份成功\n");
    } else {
        printf("备份失败，退出码: %d\n", code);
    }
#endif
    
    xrtUnit();
    return 0;
}
```

**补充说明：**
- Windows 使用 `CreateProcessW` + `WaitForSingleObject`
- Linux 使用 `fork` + `execl` + `waitpid`
- Linux 返回 `WEXITSTATUS` 的值，异常退出返回 `-1`

---

## 使用场景

### 1. 启动外部工具

```c
#include "xrt.h"
#include <stdio.h>

// 启动文本编辑器
void OpenEditor(str filename) {
#if defined(_WIN32) || defined(_WIN64)
    str cmd = xrtFormat("notepad.exe \"%s\"", filename);
    xrtRun(cmd, 0);
    xrtFree(cmd);
#else
    str cmd = xrtFormat("gedit \"%s\" &", filename);
    xrtRun(cmd, 0);
    xrtFree(cmd);
#endif
}

int main() {
    xrtInit();
    OpenEditor((str)"test.txt");
    xrtUnit();
    return 0;
}
```

---

### 2. 执行系统命令

```c
#include "xrt.h"
#include <stdio.h>

// 清理临时文件
void CleanupTemp() {
    int result;
#if defined(_WIN32) || defined(_WIN64)
    result = xrtChain((str)"cmd.exe /c del /Q C:\\Temp\\*.tmp", 0);
#else
    result = xrtChain((str)"rm -f /tmp/*.tmp", 0);
#endif
    
    if (result == 0) {
        printf("清理完成\n");
    } else {
        printf("清理失败，退出码: %d\n", result);
    }
}

int main() {
    xrtInit();
    CleanupTemp();
    xrtUnit();
    return 0;
}
```

---

### 3. 打开URL或文件

```c
#include "xrt.h"
#include <stdio.h>

// xrtStart 跨平台支持，无需条件编译
void OpenHelp() {
    xrtStart((str)"https://docs.example.com/help", 0);
}

void ViewDocument(str filepath) {
    xrtStart(filepath, 0);
}

int main() {
    xrtInit();
    
    OpenHelp();  // 用默认浏览器打开
    ViewDocument((str)"manual.pdf");  // 用默认 PDF 阅读器打开
    
    xrtUnit();
    return 0;
}
```

---

### 4. 批量处理

```c
#include "xrt.h"
#include <stdio.h>

// 使用外部工具批量转换文件
void ConvertFile(str input_file) {
    str cmd = xrtFormat(
        "ffmpeg -i \"%s\" \"%s.mp4\"",
        input_file, input_file
    );
    
    // 等待转换完成
    int code = xrtChain(cmd, 0);
    if (code == 0) {
        printf("转换成功: %s\n", input_file);
    } else {
        printf("转换失败: %s (code=%d)\n", input_file, code);
    }
    
    xrtFree(cmd);
}

int main() {
    xrtInit();
    
    // 批量转换示例
    str files[] = {(str)"video1.avi", (str)"video2.avi"};
    int count = 2;
    
    for (int i = 0; i < count; i++) {
        ConvertFile(files[i]);
    }
    
    xrtUnit();
    return 0;
}
```

---

### 5. 后台服务启动

```c
#include "xrt.h"
#include <stdio.h>

// 启动后台服务
void StartService() {
#if defined(_WIN32) || defined(_WIN64)
    ptr handle = xrtRun((str)"service.exe -daemon", 0);
    if (handle) {
        printf("服务已启动，句柄: %p\n", handle);
        // 可以保存 handle 用于后续操作
    }
#else
    ptr pid_ptr = xrtRun((str)"./service -d", 0);
    if (pid_ptr) {
        printf("服务 PID: %ld\n", (long)(intptr_t)pid_ptr);
    }
#endif
}

int main() {
    xrtInit();
    StartService();
    xrtUnit();
    return 0;
}
```

---

## 平台差异

### Windows vs Linux

| 功能 | Windows | Linux/macOS |
|------|---------|-------------|
| `xrtRun` | 返回 `HANDLE` | 返回 `pid_t` |
| `xrtStart` | 使用 `ShellExecuteW` | 使用 `xdg-open` |
| `xrtChain` | 返回退出码 | 返回退出码（-1 表示异常）|

### Windows 特有功能

```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
#if defined(_WIN32) || defined(_WIN64)
    // ShellExecute 封装 - Windows 支持更多协议
    xrtStart((str)"mailto:user@example.com", 0);  // 打开邮件客户端
    xrtStart((str)"file:///C:/folder", 0);         // 在资源管理器中打开
    xrtStart((str)"tel:+1234567890", 0);           // 打开电话应用
#endif
    
    xrtUnit();
    return 0;
}
```

### 跨平台统一用法

`xrtStart` 已经是跨平台的，可以直接使用：

```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // 跨平台打开文件 - Windows 用 ShellExecuteW，Linux 用 xdg-open
    xrtStart((str)"document.pdf", 0);
    
    // 跨平台打开 URL
    xrtStart((str)"https://example.com", 0);
    
    xrtUnit();
    return 0;
}
```

---

## 最佳实践

### 1. 跨平台处理

```c
#include "xrt.h"
#include <stdio.h>

// xrtStart 已经是跨平台的，无需条件编译
void OpenFile(str path) {
    ptr result = xrtStart(path, 0);
    if (result) {
        printf("文件已打开\n");
    } else {
        printf("打开失败\n");
    }
}

int main() {
    xrtInit();
    OpenFile((str)"test.txt");
    xrtUnit();
    return 0;
}
```

---

### 2. 命令参数转义

```c
#include "xrt.h"
#include <stdio.h>

// 处理含空格的路径
str EscapePath(str path) {
#if defined(_WIN32) || defined(_WIN64)
    return xrtFormat("\"%s\"", path);
#else
    // Linux 需要更复杂的转义，简化版本
    return xrtFormat("'%s'", path);
#endif
}

void RunWithFile(str program, str file) {
    str escaped = EscapePath(file);
    str cmd = xrtFormat("%s %s", program, escaped);
    printf("执行命令: %s\n", cmd);
    xrtRun(cmd, 0);
    xrtFree(cmd);
    xrtFree(escaped);
}

int main() {
    xrtInit();
    
#if defined(_WIN32) || defined(_WIN64)
    RunWithFile((str)"notepad.exe", (str)"C:\\My Documents\\test file.txt");
#else
    RunWithFile((str)"gedit", (str)"/home/user/my documents/test.txt");
#endif
    
    xrtUnit();
    return 0;
}
```

---

### 3. 检查执行结果

```c
#include "xrt.h"
#include <stdio.h>

// xrtChain 在 Linux 上已经处理了 WEXITSTATUS，直接返回退出码
// 返回 -1 表示异常退出（fork 失败或非正常终止）
bool ExecuteCommand(str command) {
    int code = xrtChain(command, 0);
    return (code == 0);  // 跨平台统一判断
}

int main() {
    xrtInit();
    
#if defined(_WIN32) || defined(_WIN64)
    if (ExecuteCommand((str)"cmd.exe /c echo Hello")) {
        printf("命令执行成功\n");
    }
#else
    if (ExecuteCommand((str)"echo Hello")) {
        printf("命令执行成功\n");
    }
#endif
    
    xrtUnit();
    return 0;
}
```

---

### 4. 异步执行管理

```c
#include "xrt.h"
#include <stdio.h>

#if defined(_WIN32) || defined(_WIN64)
    #include <windows.h>
#else
    #include <sys/wait.h>
#endif

// 进程信息结构
typedef struct {
    ptr handle;
    str name;
} ProcessInfo;

// 启动进程并保存信息
ProcessInfo* StartProcess(str command, str name) {
    ptr handle = xrtRun(command, 0);
    if (!handle) {
        return NULL;
    }
    
    ProcessInfo* info = xrtMalloc(sizeof(ProcessInfo));
    info->handle = handle;
    info->name = xrtCopyStr(name, 0);
    return info;
}

// 等待进程结束
void WaitProcess(ProcessInfo* info) {
    if (!info) return;
    
#if defined(_WIN32) || defined(_WIN64)
    WaitForSingleObject((HANDLE)info->handle, INFINITE);
    CloseHandle((HANDLE)info->handle);
#else
    pid_t pid = (pid_t)(intptr_t)info->handle;
    waitpid(pid, NULL, 0);
#endif
    
    printf("进程 %s 已结束\n", info->name);
    xrtFree(info->name);
    xrtFree(info);
}

int main() {
    xrtInit();
    
#if defined(_WIN32) || defined(_WIN64)
    ProcessInfo* proc = StartProcess((str)"cmd.exe /c ping localhost -n 3", (str)"ping");
#else
    ProcessInfo* proc = StartProcess((str)"sleep 2", (str)"sleep");
#endif
    
    if (proc) {
        printf("进程 %s 已启动，等待结束...\n", proc->name);
        WaitProcess(proc);
    }
    
    xrtUnit();
    return 0;
}
```

---

## 安全注意事项

### 1. 命令注入防护

```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

// 简单的文件名验证（仅允许字母数字和点）
bool IsValidFilename(str input) {
    if (!input || input[0] == 0) return FALSE;
    for (size_t i = 0; input[i]; i++) {
        char c = input[i];
        if (!((c >= 'a' && c <= 'z') || 
              (c >= 'A' && c <= 'Z') || 
              (c >= '0' && c <= '9') || 
              c == '.' || c == '_' || c == '-')) {
            return FALSE;
        }
    }
    return TRUE;
}

// 转义 shell 参数
str EscapeShellArg(str input) {
#if defined(_WIN32) || defined(_WIN64)
    return xrtFormat("\"%s\"", input);
#else
    return xrtFormat("'%s'", input);
#endif
}

// ✗ 危险：直接拼接用户输入
void UnsafeRun(str user_input) {
    str cmd = xrtFormat("process %s", user_input);
    // 用户可输入: "; rm -rf /" 进行命令注入!
    xrtRun(cmd, 0);
    xrtFree(cmd);
}

// ✓ 安全：验证和转义
void SafeRun(str user_input) {
    // 1. 验证输入
    if (!IsValidFilename(user_input)) {
        printf("无效的文件名\n");
        return;
    }
    
    // 2. 转义特殊字符
    str escaped = EscapeShellArg(user_input);
    str cmd = xrtFormat("process %s", escaped);
    printf("执行安全命令: %s\n", cmd);
    xrtRun(cmd, 0);
    xrtFree(cmd);
    xrtFree(escaped);
}

int main() {
    xrtInit();
    
    // 测试安全运行
    SafeRun((str)"test.txt");     // ✓ 有效
    SafeRun((str)"; rm -rf /");   // ✗ 拒绝
    
    xrtUnit();
    return 0;
}
```

---

### 2. 路径验证

```c
#include "xrt.h"
#include <stdio.h>

#if !defined(_WIN32) && !defined(_WIN64)
    #include <unistd.h>
#endif

// 验证可执行文件路径
bool IsValidExecutable(str path) {
    // 检查文件存在
    if (!xrtFileExi(path, 0)) {
        printf("文件不存在: %s\n", path);
        return FALSE;
    }
    
#if defined(_WIN32) || defined(_WIN64)
    // Windows: 检查扩展名
    str ext = xrtPathGetExt(path, 0);
    bool valid = (
        xrtStrComp(ext, (str)".exe", 0, FALSE) == 0 ||
        xrtStrComp(ext, (str)".bat", 0, FALSE) == 0 ||
        xrtStrComp(ext, (str)".cmd", 0, FALSE) == 0
    );
    xrtFree(ext);
    if (!valid) {
        printf("无效的可执行文件扩展名\n");
    }
    return valid;
#else
    // Linux: 检查可执行权限
    if (access((char*)path, X_OK) != 0) {
        printf("文件没有执行权限\n");
        return FALSE;
    }
    return TRUE;
#endif
}

int main() {
    xrtInit();
    
#if defined(_WIN32) || defined(_WIN64)
    if (IsValidExecutable((str)"C:\\Windows\\notepad.exe")) {
        printf("有效的可执行文件\n");
    }
#else
    if (IsValidExecutable((str)"/bin/ls")) {
        printf("有效的可执行文件\n");
    }
#endif
    
    xrtUnit();
    return 0;
}
```

---

## 常见问题

### 1. 程序路径问题

```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // ✗ 相对路径可能找不到
    // xrtRun((str)"tool.exe", 0);  // 可能失败
    
    // ✓ 使用绝对路径
    str full_path = xrtPathJoin(2, xCore.AppPath, (str)"tool.exe");
    printf("完整路径: %s\n", full_path);
    
    if (xrtFileExi(full_path, 0)) {
        xrtRun(full_path, 0);
    } else {
        printf("文件不存在\n");
    }
    
    xrtFree(full_path);
    xrtUnit();
    return 0;
}
```

---

### 2. 工作目录

```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // 子进程的工作目录是继承的
    // 如果需要特定工作目录，在命令中指定
    
#if defined(_WIN32) || defined(_WIN64)
    str cmd = xrtFormat("cd /d C:\\project && build.bat");
#else
    str cmd = xrtFormat("cd /home/user/project && ./build.sh");
#endif
    
    int result = xrtChain(cmd, 0);
    printf("执行结果: %d\n", result);
    xrtFree(cmd);
    
    xrtUnit();
    return 0;
}
```

---

### 3. 输出重定向

```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // 将输出重定向到文件
#if defined(_WIN32) || defined(_WIN64)
    str cmd = xrtFormat("cmd.exe /c dir C:\\ > output.log 2>&1");
#else
    str cmd = xrtFormat("ls -la /home > output.log 2>&1");
#endif
    
    int code = xrtChain(cmd, 0);
    printf("命令退出码: %d\n", code);
    xrtFree(cmd);
    
    // 读取输出
    if (xrtFileExi((str)"output.log", 0)) {
        str output = xrtFileGetAll((str)"output.log", XRT_CP_UTF8);
        if (output) {
            printf("输出内容:\n%s\n", output);
            xrtFree(output);
        }
    }
    
    xrtUnit();
    return 0;
}
```

---

## 相关文档

- [File 文件操作](api-file.md) - 文件系统操作
- [Path 路径处理](api-path.md) - 路径拼接和解析
- [Thread 线程管理](api-thread.md) - 多线程功能
- [主索引](README.md) - 返回文档首页

---

<div align="center">

[⬆️ 返回顶部](#os-操作系统库)

</div>
