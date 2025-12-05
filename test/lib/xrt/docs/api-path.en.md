# Path Processing Library

> File path operations and parsing functions

[English](api-path.en.md) | [中文](api-path.md) | [Back to Index](README.en.md)

---

## 📑 Table of Contents

- [Path Information](#path-information)
- [Path Judgment](#path-judgment)
- [Path Operations](#path-operations)
- [Usage Scenarios](#usage-scenarios)
- [Best Practices](#best-practices)

---

## Path Information

### xrtPathGetDir

Get the directory part of a path.

**Function Prototype:**
```c
XXAPI str xrtPathGetDir(str sPath, size_t iSize);
```

**Parameters:**
- `sPath` - File path
- `iSize` - Length (0 for auto-calculation)

**Return Value:**
- Directory path (without trailing separator and filename)
- Directory length stored in `xCore.iRet`
- Returns `xCore.sNull` if path contains no directory separator

**Memory Release:** ✅ Requires `xrtFree` to release

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // Windows path
    str dir1 = xrtPathGetDir((str)"C:\\folder\\file.txt", 0);
    printf("Directory: %s\n", dir1);  // "C:\folder"
    printf("Length: %" PRId64 "\n", xCore.iRet);  // 9
    xrtFree(dir1);
    
    // Linux path
    str dir2 = xrtPathGetDir((str)"/home/user/file.txt", 0);
    printf("Directory: %s\n", dir2);  // "/home/user"
    xrtFree(dir2);
    
    // Path with trailing separator
    str dir3 = xrtPathGetDir((str)"C:\\folder\\", 0);
    printf("Directory: %s\n", dir3);  // "C:\folder"
    xrtFree(dir3);
    
    // Filename only (no directory)
    str dir4 = xrtPathGetDir((str)"file.txt", 0);
    if (dir4 == xCore.sNull) {
        printf("No directory part\n");
    }
    
    xrtUnit();
    return 0;
}
```

**Additional Notes:**
- Supports both `/` and `\\` as path separators
- Does not include trailing separator

---

### xrtPathGetNameExt

Get filename (including extension).

**Function Prototype:**
```c
XXAPI str xrtPathGetNameExt(str sPath, size_t iSize);
```

**Parameters:**
- `sPath` - File path
- `iSize` - Length (0 for auto-calculation)

**Return Value:**
- Filename string (including extension)
- Filename length stored in `xCore.iRet`
- Returns `xCore.sNull` if path ends with separator

**Memory Release:** ✅ Requires `xrtFree` to release

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // Get filename + extension
    str file1 = xrtPathGetNameExt((str)"C:\\folder\\file.txt", 0);
    printf("Filename: %s\n", file1);  // "file.txt"
    printf("Length: %" PRId64 "\n", xCore.iRet);  // 8
    xrtFree(file1);
    
    // Linux path
    str file2 = xrtPathGetNameExt((str)"/home/user/document.pdf", 0);
    printf("Filename: %s\n", file2);  // "document.pdf"
    xrtFree(file2);
    
    // Filename only (no path)
    str file3 = xrtPathGetNameExt((str)"readme.md", 0);
    printf("Filename: %s\n", file3);  // "readme.md"
    xrtFree(file3);
    
    xrtUnit();
    return 0;
}
```

---

### xrtPathGetName

Get filename (without extension).

**Function Prototype:**
```c
XXAPI str xrtPathGetName(str sPath, size_t iSize);
```

**Parameters:**
- `sPath` - File path
- `iSize` - Length (0 for auto-calculation)

**Return Value:**
- Filename string (without extension)
- Filename length stored in `xCore.iRet`

**Memory Release:** ✅ Requires `xrtFree` to release

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // Get filename without extension
    str name1 = xrtPathGetName((str)"C:\\folder\\file.txt", 0);
    printf("Filename: %s\n", name1);  // "file"
    printf("Length: %" PRId64 "\n", xCore.iRet);  // 4
    xrtFree(name1);
    
    // File without extension
    str name2 = xrtPathGetName((str)"Makefile", 0);
    printf("Filename: %s\n", name2);  // "Makefile"
    xrtFree(name2);
    
    // Filename with multiple dots
    str name3 = xrtPathGetName((str)"archive.tar.gz", 0);
    printf("Filename: %s\n", name3);  // "archive.tar"
    xrtFree(name3);
    
    xrtUnit();
    return 0;
}
```

**Additional Notes:**
- Only removes the last `.` and the extension after it

---

### xrtPathGetExt

Get file extension.

**Function Prototype:**
```c
XXAPI str xrtPathGetExt(str sPath, size_t iSize);
```

**Parameters:**
- `sPath` - File path
- `iSize` - Length (0 for auto-calculation)

**Return Value:**
- Extension string (**without** `.`)
- Extension length stored in `xCore.iRet`
- Returns `xCore.sNull` if no extension

**Memory Release:** ✅ Requires `xrtFree` to release

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // Get extension
    str ext1 = xrtPathGetExt((str)"file.txt", 0);
    printf("Extension: %s\n", ext1);  // "txt" (Note: without dot)
    printf("Length: %" PRId64 "\n", xCore.iRet);  // 3
    xrtFree(ext1);
    
    // File with multiple dots
    str ext2 = xrtPathGetExt((str)"archive.tar.gz", 0);
    printf("Extension: %s\n", ext2);  // "gz"
    xrtFree(ext2);
    
    // File without extension
    str ext3 = xrtPathGetExt((str)"Makefile", 0);
    if (ext3 == xCore.sNull) {
        printf("No extension\n");
    }
    
    // Dot in path but no extension in filename
    str ext4 = xrtPathGetExt((str)"/home/user.name/config", 0);
    if (ext4 == xCore.sNull) {
        printf("Dot in directory is not an extension\n");
    }
    
    xrtUnit();
    return 0;
}
```

**Additional Notes:**
- Returned extension **does not** include leading dot `.`
- If dot appears before path separator, it's considered part of directory name, not extension

---

## Path Judgment

### xrtPathIsAbs

Check if path is absolute.

**Function Prototype:**
```c
XXAPI bool xrtPathIsAbs(str sPath, size_t iSize);
```

**Parameters:**
- `sPath` - File path
- `iSize` - Length (0 for auto-calculation)

**Return Value:**
- `TRUE` - Absolute path
- `FALSE` - Relative path

**Judgment Rules:**
- Linux/macOS: Starts with `/` is absolute path
- Windows: Contains `:` is absolute path (e.g., `C:\`)

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // Windows absolute path
    printf("C:\\folder: %s\n", 
        xrtPathIsAbs((str)"C:\\folder", 0) ? "absolute" : "relative");  // absolute
    
    // Linux absolute path
    printf("/home/user: %s\n", 
        xrtPathIsAbs((str)"/home/user", 0) ? "absolute" : "relative");  // absolute
    
    // Relative path
    printf("./folder: %s\n", 
        xrtPathIsAbs((str)"./folder", 0) ? "absolute" : "relative");  // relative
    printf("folder/file: %s\n", 
        xrtPathIsAbs((str)"folder/file", 0) ? "absolute" : "relative");  // relative
    
    xrtUnit();
    return 0;
}
```

**Additional Notes:**
- This function is cross-platform, supports both Windows and Unix style path detection

---

## Path Operations

### xrtPathJoin

Join multiple path segments.

**Function Prototype:**
```c
XXAPI str xrtPathJoin(uint iCount, ...);
```

**Parameters:**
- `iCount` - Number of path segments
- `...` - Path segments (str type variadic arguments)

**Return Value:**
- Complete joined path
- Path length stored in `xCore.iRet`
- Returns `xCore.sNull` if path exceeds 4094 characters

**Memory Release:** ✅ Requires `xrtFree` to release

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // Join multiple path segments
    str path1 = xrtPathJoin(3, (str)"C:\\", (str)"folder", (str)"file.txt");
    printf("Path: %s\n", path1);  // Windows: "C:\folder\file.txt"
    printf("Length: %" PRId64 "\n", xCore.iRet);
    xrtFree(path1);
    
    // Linux path
    str path2 = xrtPathJoin(3, (str)"/home", (str)"user", (str)"document.pdf");
    printf("Path: %s\n", path2);  // "/home/user/document.pdf"
    xrtFree(path2);
    
    // Using application path
    str config = xrtPathJoin(3, xCore.AppPath, (str)"config", (str)"app.ini");
    printf("Config file: %s\n", config);
    xrtFree(config);
    
    // Path segment already has separator (won't duplicate)
    str path3 = xrtPathJoin(2, (str)"C:\\folder\\", (str)"file.txt");
    printf("Path: %s\n", path3);  // "C:\folder\file.txt"
    xrtFree(path3);
    
    xrtUnit();
    return 0;
}
```

**Additional Notes:**
- Automatically adds separator between path segments (`\` for Windows, `/` for Linux)
- Won't duplicate separator if path segment already ends with one
- NULL or empty path segments are skipped
- Maximum supported path length is 4094 characters

---

### xrtPathRandom

Generate random path string.

**Function Prototype:**
```c
XXAPI str xrtPathRandom(str sHead, size_t iHeadSize, str sFoot, size_t iFootSize, size_t iLen);
```

**Parameters:**
- `sHead` - Path prefix (can be NULL)
- `iHeadSize` - Prefix length (0 for auto-calculation)
- `sFoot` - Path suffix (can be NULL, usually extension)
- `iFootSize` - Suffix length (0 for auto-calculation)
- `iLen` - Length of random part

**Return Value:**
- Path string in format `{sHead}{random}{sFoot}`
- Path length stored in `xCore.iRet`

**Memory Release:** ✅ Requires `xrtFree` to release

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // Generate temporary filename
    str temp1 = xrtPathRandom((str)"/tmp/", 0, (str)".tmp", 0, 8);
    printf("Temp file: %s\n", temp1);  // e.g.: "/tmp/A3xK9mPq.tmp"
    xrtFree(temp1);
    
    // Generate random folder name
    str folder = xrtPathRandom((str)"cache_", 0, NULL, 0, 6);
    printf("Cache folder: %s\n", folder);  // e.g.: "cache_Bz7YnL"
    xrtFree(folder);
    
    // Generate unique filename
    str unique = xrtPathRandom(NULL, 0, (str)".dat", 0, 16);
    printf("Unique file: %s\n", unique);  // e.g.: "Kx9pM2nVqR5sT8wL.dat"
    xrtFree(unique);
    
    xrtUnit();
    return 0;
}
```

**Additional Notes:**
- Random part uses alphanumeric characters (A-Z, a-z, 0-9), no special characters
- Commonly used for generating temporary filenames, cache filenames, etc.
- Does not check if file already exists, caller must verify

---

## Usage Scenarios

### 1. Path Parsing

```c
#include "xrt.h"
#include <stdio.h>

void ParsePath(str path) {
    str dir = xrtPathGetDir(path, 0);
    str name = xrtPathGetName(path, 0);
    str ext = xrtPathGetExt(path, 0);
    
    printf("Directory: %s\n", dir != xCore.sNull ? (char*)dir : "(none)");
    printf("Filename: %s\n", name != xCore.sNull ? (char*)name : "(none)");
    printf("Extension: %s\n", ext != xCore.sNull ? (char*)ext : "(none)");
    
    if (dir != xCore.sNull) xrtFree(dir);
    if (name != xCore.sNull) xrtFree(name);
    if (ext != xCore.sNull) xrtFree(ext);
}

int main() {
    xrtInit();
    
    ParsePath((str)"C:\\folder\\file.txt");
    printf("---\n");
    ParsePath((str)"/home/user/document.pdf");
    
    xrtUnit();
    return 0;
}
```

---

### 2. Path Building

```c
#include "xrt.h"
#include <stdio.h>

str BuildConfigPath() {
    return xrtPathJoin(3, xCore.AppPath, (str)"config", (str)"app.ini");
}

int main() {
    xrtInit();
    
    str config = BuildConfigPath();
    printf("Config file: %s\n", config);
    xrtFree(config);
    
    xrtUnit();
    return 0;
}
```

---

### 3. Temporary File Handling

```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // Create temporary file path
#if defined(_WIN32) || defined(_WIN64)
    str temp_dir = (str)"C:\\Temp\\";
#else
    str temp_dir = (str)"/tmp/";
#endif
    
    str temp_file = xrtPathRandom(temp_dir, 0, (str)".tmp", 0, 8);
    printf("Temp file: %s\n", temp_file);
    
    // Use temporary file...
    
    xrtFree(temp_file);
    xrtUnit();
    return 0;
}
```

---

### 4. Path Security Check

```c
#include "xrt.h"
#include <stdio.h>

bool IsValidFilePath(str path) {
    // Must be absolute path
    if (!xrtPathIsAbs(path, 0)) {
        printf("Error: Must use absolute path\n");
        return FALSE;
    }
    
    // Must have extension
    str ext = xrtPathGetExt(path, 0);
    if (ext == xCore.sNull) {
        printf("Error: Missing file extension\n");
        return FALSE;
    }
    xrtFree(ext);
    
    return TRUE;
}

int main() {
    xrtInit();
    
    printf("Check C:\\test.txt: %s\n", 
        IsValidFilePath((str)"C:\\test.txt") ? "valid" : "invalid");
    printf("Check ./relative: %s\n", 
        IsValidFilePath((str)"./relative") ? "valid" : "invalid");
    
    xrtUnit();
    return 0;
}
```

---

## Best Practices

### 1. Check Return Values

```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    str path = (str)"filename_without_dir";
    str dir = xrtPathGetDir(path, 0);
    
    // Always check return value
    if (dir == xCore.sNull) {
        printf("Path does not contain directory\n");
    } else {
        printf("Directory: %s\n", dir);
        xrtFree(dir);
    }
    
    xrtUnit();
    return 0;
}
```

---

### 2. Cross-Platform Path Handling

```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // Use xrtPathJoin to automatically handle separators
    // Don't manually concatenate path strings
    
    // ✓ Correct: Use xrtPathJoin
    str good = xrtPathJoin(3, xCore.AppPath, (str)"data", (str)"file.txt");
    printf("Correct: %s\n", good);
    xrtFree(good);
    
    // ✗ Not recommended: Manual concatenation
    // str bad = xrtFormat("%s\\data\\file.txt", xCore.AppPath);
    
    xrtUnit();
    return 0;
}
```

---

## Related Documentation

- [File Operations](api-file.en.md)
- [String Processing](api-string.en.md)
- [Main Index](README.en.md)

---

<div align="center">

[⬆️ Back to Top](#path-processing-library)

</div>
