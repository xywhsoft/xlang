# OS Operating System Library

> Process management and system call functions

[English](api-os.en.md) | [中文](api-os.md) | [Back to Index](README.en.md)

---

## 📑 Table of Contents

- [Process Execution](#process-execution)
- [Usage Scenarios](#usage-scenarios)
- [Platform Differences](#platform-differences)

---

## Process Execution

### xrtRun

Run an external program (asynchronous)

**Function Prototype:**
```c
XXAPI ptr xrtRun(str sPath, size_t iSize);
```

**Parameters:**
- `sPath` - Program path or command line
- `iSize` - Path length (0 for auto-calculation)

**Return Value:**
- Windows: Returns process handle (`HANDLE`)
- Linux: Returns process ID (`pid_t`) converted to pointer

**Notes:**
- Asynchronous execution, does not wait for program to finish
- Returns immediately with process handle/ID
- Can be used to start background processes

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
#if defined(_WIN32) || defined(_WIN64)
    // Windows: Start notepad
    ptr handle = xrtRun((str)"notepad.exe", 0);
    if (handle) {
        printf("Program started, process handle: %p\n", handle);
        // Process runs in background, no waiting
    }
    
    // Start program with arguments
    ptr h2 = xrtRun((str)"cmd.exe /c echo Hello World", 0);
#else
    // Linux/macOS: Start program
    ptr pid = xrtRun((str)"ls -la", 0);
    if (pid) {
        printf("Program started, PID: %ld\n", (long)(intptr_t)pid);
    }
    
    // Start background process
    ptr pid2 = xrtRun((str)"sleep 10 &", 0);
#endif
    
    xrtUnit();
    return 0;
}
```

**Additional Notes:**
- Windows uses `CreateProcessW` to create new process
- Linux uses `fork` + `execl` executed through `/bin/sh`
- Returned handle/PID can be used for subsequent management (wait, terminate, etc.)

---

### xrtStart

Open a file or URL (using system default program)

**Function Prototype:**
```c
XXAPI ptr xrtStart(str sPath, size_t iSize);
```

**Parameters:**
- `sPath` - File path or URL
- `iSize` - Path length (0 for auto-calculation)

**Return Value:**
- Windows: Returns ShellExecute result (greater than 32 indicates success)
- Linux: Returns process ID (success) or `NULL` (failure)

**Platform Support:**
- Windows: Uses `ShellExecuteW`
- Linux/macOS: Uses `xdg-open`

**Notes:**
- Opens file with system default program
- Can open URLs (using default browser)
- Can open documents (using associated program)
- Similar to double-clicking a file

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
#if defined(_WIN32) || defined(_WIN64)
    // Open text file with default program
    ptr result = xrtStart((str)"C:\\test.txt", 0);
    printf("Result: %p\n", result);
    
    // Open webpage with default browser
    xrtStart((str)"https://www.example.com", 0);
    
    // Open image
    xrtStart((str)"C:\\photo.jpg", 0);
    
    // Open folder (Explorer)
    xrtStart((str)"C:\\Users", 0);
    
    // Open email client
    xrtStart((str)"mailto:user@example.com", 0);
#else
    // Linux/macOS: Use xdg-open
    ptr pid = xrtStart((str)"/home/user/document.pdf", 0);
    if (pid) {
        printf("xdg-open started, PID: %ld\n", (long)(intptr_t)pid);
    }
    
    // Open webpage
    xrtStart((str)"https://www.example.com", 0);
#endif
    
    xrtUnit();
    return 0;
}
```

**Additional Notes:**
- Windows supports special protocols: `mailto:`, `file://`, `http://`, `https://`, etc.
- Linux requires `xdg-utils` package (included by default in most distributions)

---

### xrtChain

Run a program and wait for completion (synchronous)

**Function Prototype:**
```c
XXAPI int xrtChain(str sPath, size_t iSize);
```

**Parameters:**
- `sPath` - Program path or command line
- `iSize` - Path length (0 for auto-calculation)

**Return Value:**
- Program's exit code
- Windows: Program's return value
- Linux: Program's exit status

**Notes:**
- **Synchronous execution**, blocks current thread
- Waits for program to finish before returning
- Returns program's exit code

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
#if defined(_WIN32) || defined(_WIN64)
    // Windows: Execute command and wait for completion
    int exit_code = xrtChain((str)"cmd.exe /c dir C:\\", 0);
    printf("Exit code: %d\n", exit_code);
    
    // Execute batch script
    int code = xrtChain((str)"backup.bat", 0);
    if (code == 0) {
        printf("Backup successful\n");
    } else {
        printf("Backup failed, exit code: %d\n", code);
    }
#else
    // Linux: Execute shell command
    int result = xrtChain((str)"ls -la /home", 0);
    printf("Exit code: %d\n", result);
    
    // Execute script
    int code = xrtChain((str)"./backup.sh", 0);
    if (code == 0) {
        printf("Backup successful\n");
    } else {
        printf("Backup failed, exit code: %d\n", code);
    }
#endif
    
    xrtUnit();
    return 0;
}
```

**Additional Notes:**
- Windows uses `CreateProcessW` + `WaitForSingleObject`
- Linux uses `fork` + `execl` + `waitpid`
- Linux returns `WEXITSTATUS` value, returns `-1` for abnormal exit

---

## Usage Scenarios

### 1. Launch External Tools

```c
#include "xrt.h"
#include <stdio.h>

// Open text editor
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

### 2. Execute System Commands

```c
#include "xrt.h"
#include <stdio.h>

// Clean temporary files
void CleanupTemp() {
    int result;
#if defined(_WIN32) || defined(_WIN64)
    result = xrtChain((str)"cmd.exe /c del /Q C:\\Temp\\*.tmp", 0);
#else
    result = xrtChain((str)"rm -f /tmp/*.tmp", 0);
#endif
    
    if (result == 0) {
        printf("Cleanup completed\n");
    } else {
        printf("Cleanup failed, exit code: %d\n", result);
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

### 3. Open URLs or Files

```c
#include "xrt.h"
#include <stdio.h>

// xrtStart is cross-platform, no conditional compilation needed
void OpenHelp() {
    xrtStart((str)"https://docs.example.com/help", 0);
}

void ViewDocument(str filepath) {
    xrtStart(filepath, 0);
}

int main() {
    xrtInit();
    
    OpenHelp();  // Open with default browser
    ViewDocument((str)"manual.pdf");  // Open with default PDF reader
    
    xrtUnit();
    return 0;
}
```

---

### 4. Batch Processing

```c
#include "xrt.h"
#include <stdio.h>

// Use external tool for batch file conversion
void ConvertFile(str input_file) {
    str cmd = xrtFormat(
        "ffmpeg -i \"%s\" \"%s.mp4\"",
        input_file, input_file
    );
    
    // Wait for conversion to complete
    int code = xrtChain(cmd, 0);
    if (code == 0) {
        printf("Conversion successful: %s\n", input_file);
    } else {
        printf("Conversion failed: %s (code=%d)\n", input_file, code);
    }
    
    xrtFree(cmd);
}

int main() {
    xrtInit();
    
    // Batch conversion example
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

### 5. Background Service Startup

```c
#include "xrt.h"
#include <stdio.h>

// Start background service
void StartService() {
#if defined(_WIN32) || defined(_WIN64)
    ptr handle = xrtRun((str)"service.exe -daemon", 0);
    if (handle) {
        printf("Service started, handle: %p\n", handle);
        // Can save handle for subsequent operations
    }
#else
    ptr pid_ptr = xrtRun((str)"./service -d", 0);
    if (pid_ptr) {
        printf("Service PID: %ld\n", (long)(intptr_t)pid_ptr);
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

## Platform Differences

### Windows vs Linux

| Function | Windows | Linux/macOS |
|----------|---------|-------------|
| `xrtRun` | Returns `HANDLE` | Returns `pid_t` |
| `xrtStart` | Uses `ShellExecuteW` | Uses `xdg-open` |
| `xrtChain` | Returns exit code | Returns exit code (-1 for abnormal) |

### Windows-Specific Features

```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
#if defined(_WIN32) || defined(_WIN64)
    // ShellExecute wrapper - Windows supports more protocols
    xrtStart((str)"mailto:user@example.com", 0);  // Open email client
    xrtStart((str)"file:///C:/folder", 0);         // Open in Explorer
    xrtStart((str)"tel:+1234567890", 0);           // Open phone app
#endif
    
    xrtUnit();
    return 0;
}
```

### Cross-Platform Unified Usage

`xrtStart` is already cross-platform, can be used directly:

```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // Cross-platform file opening - Windows uses ShellExecuteW, Linux uses xdg-open
    xrtStart((str)"document.pdf", 0);
    
    // Cross-platform URL opening
    xrtStart((str)"https://example.com", 0);
    
    xrtUnit();
    return 0;
}
```

---

## Best Practices

### 1. Cross-Platform Handling

```c
#include "xrt.h"
#include <stdio.h>

// xrtStart is already cross-platform, no conditional compilation needed
void OpenFile(str path) {
    ptr result = xrtStart(path, 0);
    if (result) {
        printf("File opened\n");
    } else {
        printf("Failed to open\n");
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

### 2. Command Argument Escaping

```c
#include "xrt.h"
#include <stdio.h>

// Handle paths with spaces
str EscapePath(str path) {
#if defined(_WIN32) || defined(_WIN64)
    return xrtFormat("\"%s\"", path);
#else
    // Linux needs more complex escaping, simplified version
    return xrtFormat("'%s'", path);
#endif
}

void RunWithFile(str program, str file) {
    str escaped = EscapePath(file);
    str cmd = xrtFormat("%s %s", program, escaped);
    printf("Executing command: %s\n", cmd);
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

### 3. Check Execution Results

```c
#include "xrt.h"
#include <stdio.h>

// xrtChain on Linux already handles WEXITSTATUS, returns exit code directly
// Returns -1 for abnormal exit (fork failure or abnormal termination)
bool ExecuteCommand(str command) {
    int code = xrtChain(command, 0);
    return (code == 0);  // Cross-platform unified check
}

int main() {
    xrtInit();
    
#if defined(_WIN32) || defined(_WIN64)
    if (ExecuteCommand((str)"cmd.exe /c echo Hello")) {
        printf("Command executed successfully\n");
    }
#else
    if (ExecuteCommand((str)"echo Hello")) {
        printf("Command executed successfully\n");
    }
#endif
    
    xrtUnit();
    return 0;
}
```

---

### 4. Asynchronous Execution Management

```c
#include "xrt.h"
#include <stdio.h>

#if defined(_WIN32) || defined(_WIN64)
    #include <windows.h>
#else
    #include <sys/wait.h>
#endif

// Process information structure
typedef struct {
    ptr handle;
    str name;
} ProcessInfo;

// Start process and save information
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

// Wait for process to finish
void WaitProcess(ProcessInfo* info) {
    if (!info) return;
    
#if defined(_WIN32) || defined(_WIN64)
    WaitForSingleObject((HANDLE)info->handle, INFINITE);
    CloseHandle((HANDLE)info->handle);
#else
    pid_t pid = (pid_t)(intptr_t)info->handle;
    waitpid(pid, NULL, 0);
#endif
    
    printf("Process %s finished\n", info->name);
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
        printf("Process %s started, waiting for completion...\n", proc->name);
        WaitProcess(proc);
    }
    
    xrtUnit();
    return 0;
}
```

---

## Security Considerations

### 1. Command Injection Prevention

```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

// Simple filename validation (only allow alphanumeric and dots)
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

// Escape shell argument
str EscapeShellArg(str input) {
#if defined(_WIN32) || defined(_WIN64)
    return xrtFormat("\"%s\"", input);
#else
    return xrtFormat("'%s'", input);
#endif
}

// ✗ Dangerous: Direct concatenation of user input
void UnsafeRun(str user_input) {
    str cmd = xrtFormat("process %s", user_input);
    // User can input: "; rm -rf /" for command injection!
    xrtRun(cmd, 0);
    xrtFree(cmd);
}

// ✓ Safe: Validation and escaping
void SafeRun(str user_input) {
    // 1. Validate input
    if (!IsValidFilename(user_input)) {
        printf("Invalid filename\n");
        return;
    }
    
    // 2. Escape special characters
    str escaped = EscapeShellArg(user_input);
    str cmd = xrtFormat("process %s", escaped);
    printf("Executing safe command: %s\n", cmd);
    xrtRun(cmd, 0);
    xrtFree(cmd);
    xrtFree(escaped);
}

int main() {
    xrtInit();
    
    // Test safe run
    SafeRun((str)"test.txt");     // ✓ Valid
    SafeRun((str)"; rm -rf /");   // ✗ Rejected
    
    xrtUnit();
    return 0;
}
```

---

### 2. Path Validation

```c
#include "xrt.h"
#include <stdio.h>

#if !defined(_WIN32) && !defined(_WIN64)
    #include <unistd.h>
#endif

// Validate executable file path
bool IsValidExecutable(str path) {
    // Check if file exists
    if (!xrtFileExi(path, 0)) {
        printf("File does not exist: %s\n", path);
        return FALSE;
    }
    
#if defined(_WIN32) || defined(_WIN64)
    // Windows: Check extension
    str ext = xrtPathGetExt(path, 0);
    bool valid = (
        xrtStrComp(ext, (str)".exe", 0, FALSE) == 0 ||
        xrtStrComp(ext, (str)".bat", 0, FALSE) == 0 ||
        xrtStrComp(ext, (str)".cmd", 0, FALSE) == 0
    );
    xrtFree(ext);
    if (!valid) {
        printf("Invalid executable extension\n");
    }
    return valid;
#else
    // Linux: Check execute permission
    if (access((char*)path, X_OK) != 0) {
        printf("File does not have execute permission\n");
        return FALSE;
    }
    return TRUE;
#endif
}

int main() {
    xrtInit();
    
#if defined(_WIN32) || defined(_WIN64)
    if (IsValidExecutable((str)"C:\\Windows\\notepad.exe")) {
        printf("Valid executable\n");
    }
#else
    if (IsValidExecutable((str)"/bin/ls")) {
        printf("Valid executable\n");
    }
#endif
    
    xrtUnit();
    return 0;
}
```

---

## Common Issues

### 1. Program Path Issues

```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // ✗ Relative path may not be found
    // xrtRun((str)"tool.exe", 0);  // May fail
    
    // ✓ Use absolute path
    str full_path = xrtPathJoin(2, xCore.AppPath, (str)"tool.exe");
    printf("Full path: %s\n", full_path);
    
    if (xrtFileExi(full_path, 0)) {
        xrtRun(full_path, 0);
    } else {
        printf("File does not exist\n");
    }
    
    xrtFree(full_path);
    xrtUnit();
    return 0;
}
```

---

### 2. Working Directory

```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // Child process inherits working directory
    // If specific working directory needed, specify in command
    
#if defined(_WIN32) || defined(_WIN64)
    str cmd = xrtFormat("cd /d C:\\project && build.bat");
#else
    str cmd = xrtFormat("cd /home/user/project && ./build.sh");
#endif
    
    int result = xrtChain(cmd, 0);
    printf("Execution result: %d\n", result);
    xrtFree(cmd);
    
    xrtUnit();
    return 0;
}
```

---

### 3. Output Redirection

```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // Redirect output to file
#if defined(_WIN32) || defined(_WIN64)
    str cmd = xrtFormat("cmd.exe /c dir C:\\ > output.log 2>&1");
#else
    str cmd = xrtFormat("ls -la /home > output.log 2>&1");
#endif
    
    int code = xrtChain(cmd, 0);
    printf("Command exit code: %d\n", code);
    xrtFree(cmd);
    
    // Read output
    if (xrtFileExi((str)"output.log", 0)) {
        str output = xrtFileGetAll((str)"output.log", XRT_CP_UTF8);
        if (output) {
            printf("Output content:\n%s\n", output);
            xrtFree(output);
        }
    }
    
    xrtUnit();
    return 0;
}
```

---

## Related Documentation

- [File Operations](api-file.en.md) - File system operations
- [Path Processing](api-path.en.md) - Path joining and parsing
- [Thread Management](api-thread.en.md) - Multithreading features
- [Main Index](README.en.md) - Return to documentation homepage

---

<div align="center">

[⬆️ Back to Top](#os-operating-system-library)

</div>
