# File - File Operations Library

The File library provides cross-platform file system operation functionality, including file read/write, directory management, and file information retrieval.

[English](api-file.en.md) | [中文](api-file.md) | [Back to Index](README.en.md)

---

## Table of Contents

- [Constants](#constants)
- [Data Types](#data-types)
- [File Open/Close](#file-openclose)
- [File Positioning](#file-positioning)
- [File Reading](#file-reading)
- [File Writing](#file-writing)
- [Quick Read/Write](#quick-readwrite)
- [File Information](#file-information)
- [File Operations](#file-operations)
- [Directory Operations](#directory-operations)
- [Usage Scenarios](#usage-scenarios)
- [Best Practices](#best-practices)
- [Related Documentation](#related-documentation)

---

## Constants

### XRT_FCEOF

File end-of-file marker, value is `0x7FFFFFFF`.

```c
#define XRT_FCEOF 0x7FFFFFFF
```

**Usage:**
- Used as return value for [xrtGetC](#xrtgetc) to indicate end of file

---

### File Seek Origin Constants

| Constant | Value | Description |
|---------|-------|-------------|
| `XRT_FSEEK_SET` | 0 | Seek from file beginning |
| `XRT_FSEEK_CUR` | 1 | Seek from current position |
| `XRT_FSEEK_END` | 2 | Seek from file end |

---

### Common Character Encoding Constants

For complete list of character encoding constants, see [Charset Character Set Conversion](api-charset.en.md).

| Constant | Value | Description |
|---------|-------|-------------|
| `XRT_CP_UTF8` | 65001 | UTF-8 encoding |
| `XRT_CP_GBK` | 936 | GBK encoding |
| `XRT_CP_GB18030` | 54936 | GB18030 encoding |
| `XRT_CP_UTF16LE` | 1200 | UTF-16 Little Endian |
| `XRT_CP_UTF16BE` | 1201 | UTF-16 Big Endian |

---

## Data Types

### xfile

File handle type, used for file read/write operations.

```c
typedef void* xfile;
```

**Notes:**
- The actual implementation varies by platform
- Should use file operation functions for access

---

## File Open/Close

### xrtOpen

Open or create a file.

**Function Prototype:**
```c
XXAPI xfile xrtOpen(str sPath, bool bRead, int CP);
```

**Parameters:**
- `sPath` - File path
- `bRead` - Open mode
  - `TRUE` - Read-only mode
  - `FALSE` - Write mode (create or truncate)
- `CP` - Character encoding (e.g., `XRT_CP_UTF8`)
  - `-1` - Binary mode

**Return Value:**
- Success: File handle
- Failure: `NULL`

**Notes:**
- Write mode automatically creates the file or truncates existing file
- When encoding is specified, BOM is automatically handled

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // Open file for reading
    xfile f = xrtOpen((str)"test.txt", TRUE, XRT_CP_UTF8);
    if (f) {
        printf("File opened successfully\n");
        xrtClose(f);
    }
    
    // Create file for writing
    f = xrtOpen((str)"output.txt", FALSE, XRT_CP_UTF8);
    if (f) {
        printf("File created successfully\n");
        xrtClose(f);
    }
    
    xrtUnit();
    return 0;
}
```

---

### xrtClose

Close file.

**Function Prototype:**
```c
XXAPI size_t xrtClose(xfile pFile);
```

**Parameters:**
- `pFile` - File handle

**Return Value:**
- Total bytes written

---

### xrtFlush

Flush file buffer to disk.

**Function Prototype:**
```c
XXAPI void xrtFlush(xfile pFile);
```

**Parameters:**
- `pFile` - File handle

**Notes:**
- Forces buffer data to be written to physical disk
- Usually used before important data operations

---

## File Positioning

### xrtSeek

Move file pointer position.

**Function Prototype:**
```c
XXAPI bool xrtSeek(xfile pFile, int64 iPos, int iFrom);
```

**Parameters:**
- `pFile` - File handle
- `iPos` - Offset
- `iFrom` - Seek origin
  - `XRT_FSEEK_SET` - Seek from file beginning
  - `XRT_FSEEK_CUR` - Seek from current position
  - `XRT_FSEEK_END` - Seek from file end

**Return Value:**
- `TRUE` - Success
- `FALSE` - Failure

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xfile f = xrtOpen((str)"test.txt", TRUE, XRT_CP_UTF8);
    if (f) {
        // Move to file beginning
        xrtSeek(f, 0, XRT_FSEEK_SET);
        
        // Move 10 bytes forward from current position
        xrtSeek(f, 10, XRT_FSEEK_CUR);
        
        // Move to position 10 bytes before file end
        xrtSeek(f, -10, XRT_FSEEK_END);
        
        xrtClose(f);
    }
    
    xrtUnit();
    return 0;
}
```

---

### xrtTell

Get current file pointer position.

**Function Prototype:**
```c
XXAPI int64 xrtTell(xfile pFile);
```

**Parameters:**
- `pFile` - File handle

**Return Value:**
- Current position (byte offset)

---

### xrtRewind

Reset file pointer to beginning.

**Function Prototype:**
```c
XXAPI void xrtRewind(xfile pFile);
```

**Parameters:**
- `pFile` - File handle

**Notes:**
- Equivalent to `xrtSeek(pFile, 0, XRT_FSEEK_SET)`

---

### xrtEof

Check if end of file is reached.

**Function Prototype:**
```c
XXAPI bool xrtEof(xfile pFile);
```

**Parameters:**
- `pFile` - File handle

**Return Value:**
- `TRUE` - End of file reached
- `FALSE` - Not at end of file

---

## File Reading

### xrtGetC

Read one character from file.

**Function Prototype:**
```c
XXAPI int32 xrtGetC(xfile pFile);
```

**Parameters:**
- `pFile` - File handle

**Return Value:**
- Character code read
- `XRT_FCEOF` - End of file

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xfile f = xrtOpen((str)"test.txt", TRUE, XRT_CP_UTF8);
    if (f) {
        int32 ch;
        while ((ch = xrtGetC(f)) != XRT_FCEOF) {
            printf("%c", (char)ch);
        }
        printf("\n");
        xrtClose(f);
    }
    
    xrtUnit();
    return 0;
}
```

---

### xrtGetLine

Read one line from file.

**Function Prototype:**
```c
XXAPI str xrtGetLine(xfile pFile);
```

**Parameters:**
- `pFile` - File handle

**Return Value:**
- Read string (needs to be freed with `xrtFree`)
- `NULL` - End of file or read failure

**Notes:**
- Automatically handles different line endings (`\r\n`, `\n`, `\r`)
- Returned string does not include line ending

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xfile f = xrtOpen((str)"test.txt", TRUE, XRT_CP_UTF8);
    if (f) {
        str line;
        int lineNum = 1;
        while ((line = xrtGetLine(f)) != NULL) {
            printf("%d: %s\n", lineNum++, line);
            xrtFree(line);
        }
        xrtClose(f);
    }
    
    xrtUnit();
    return 0;
}
```

---

### xrtGet

Read specified number of bytes from file.

**Function Prototype:**
```c
XXAPI ptr xrtGet(xfile pFile, size_t iLen);
```

**Parameters:**
- `pFile` - File handle
- `iLen` - Number of bytes to read

**Return Value:**
- Data read (needs to be freed with `xrtFree`)
- `NULL` - Read failure

**Notes:**
- Returns binary data, does not process encoding

---

### xrtRead

Read string from file.

**Function Prototype:**
```c
XXAPI str xrtRead(xfile pFile, size_t iLen);
```

**Parameters:**
- `pFile` - File handle
- `iLen` - Number of characters to read

**Return Value:**
- String read (needs to be freed with `xrtFree`)
- `NULL` - Read failure

**Notes:**
- Reads content based on file's character encoding
- `iLen` is character count, not byte count

---

### xrtReadAll

Read entire file content.

**Function Prototype:**
```c
XXAPI str xrtReadAll(xfile pFile);
```

**Parameters:**
- `pFile` - File handle

**Return Value:**
- File content (needs to be freed with `xrtFree`)
- `NULL` - Read failure

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xfile f = xrtOpen((str)"test.txt", TRUE, XRT_CP_UTF8);
    if (f) {
        str content = xrtReadAll(f);
        if (content) {
            printf("File content:\n%s\n", content);
            xrtFree(content);
        }
        xrtClose(f);
    }
    
    xrtUnit();
    return 0;
}
```

---

## File Writing

### xrtPutC

Write one character to file.

**Function Prototype:**
```c
XXAPI bool xrtPutC(xfile pFile, int32 iChar);
```

**Parameters:**
- `pFile` - File handle
- `iChar` - Character to write

**Return Value:**
- `TRUE` - Success
- `FALSE` - Failure

---

### xrtPutLine

Write string and append newline.

**Function Prototype:**
```c
XXAPI size_t xrtPutLine(xfile pFile, str sData);
```

**Parameters:**
- `pFile` - File handle
- `sData` - String to write

**Return Value:**
- Bytes written

**Notes:**
- Automatically appends line ending based on platform (`\r\n` on Windows, `\n` on Linux)

---

### xrtPut

Write binary data to file.

**Function Prototype:**
```c
XXAPI size_t xrtPut(xfile pFile, ptr pData, size_t iLen);
```

**Parameters:**
- `pFile` - File handle
- `pData` - Data to write
- `iLen` - Data length

**Return Value:**
- Bytes written

---

### xrtWrite

Write string to file.

**Function Prototype:**
```c
XXAPI size_t xrtWrite(xfile pFile, str sData, size_t iLen);
```

**Parameters:**
- `pFile` - File handle
- `sData` - String to write
- `iLen` - String length (0 means write entire string)

**Return Value:**
- Bytes written

**Notes:**
- Writes based on file's character encoding

---

### xrtWritef

Write formatted string to file.

**Function Prototype:**
```c
XXAPI size_t xrtWritef(xfile pFile, str sFormat, ...);
```

**Parameters:**
- `pFile` - File handle
- `sFormat` - Format string
- `...` - Format parameters

**Return Value:**
- Bytes written

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xfile f = xrtOpen((str)"output.txt", FALSE, XRT_CP_UTF8);
    if (f) {
        xrtWritef(f, (str)"Name: %s, Age: %d\n", "Alice", 25);
        xrtWritef(f, (str)"Score: %.2f\n", 98.5);
        xrtClose(f);
        printf("Write complete\n");
    }
    
    xrtUnit();
    return 0;
}
```

---

## Quick Read/Write

These functions provide convenient one-call file read/write, no need to manually open and close files.

### xrtFileReadAll

Read entire file content at once.

**Function Prototype:**
```c
XXAPI str xrtFileReadAll(str sPath, int CP);
```

**Parameters:**
- `sPath` - File path
- `CP` - Character encoding

**Return Value:**
- File content (needs to be freed with `xrtFree`)
- `NULL` - Read failure

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    str content = xrtFileReadAll((str)"config.ini", XRT_CP_UTF8);
    if (content) {
        printf("Config content:\n%s\n", content);
        xrtFree(content);
    } else {
        printf("Read config failed\n");
    }
    
    xrtUnit();
    return 0;
}
```

---

### xrtFileReadBin

Read file as binary data.

**Function Prototype:**
```c
XXAPI ptr xrtFileReadBin(str sPath, size_t* pLen);
```

**Parameters:**
- `sPath` - File path
- `pLen` - Output: Data length

**Return Value:**
- Binary data (needs to be freed with `xrtFree`)
- `NULL` - Read failure

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    size_t len;
    ptr data = xrtFileReadBin((str)"image.png", &len);
    if (data) {
        printf("Read %zu bytes\n", len);
        xrtFree(data);
    }
    
    xrtUnit();
    return 0;
}
```

---

### xrtFileWriteAll

Write string to file at once.

**Function Prototype:**
```c
XXAPI int xrtFileWriteAll(str sPath, str sData, size_t iLen, int CP);
```

**Parameters:**
- `sPath` - File path
- `sData` - Data to write
- `iLen` - Data length (0 means write entire string)
- `CP` - Character encoding

**Return Value:**
- Bytes written

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    str content = (str)"Hello, World!\nThis is a test.";
    int written = xrtFileWriteAll((str)"test.txt", content, 0, XRT_CP_UTF8);
    printf("Wrote %d bytes\n", written);
    
    xrtUnit();
    return 0;
}
```

---

### xrtFileWriteBin

Write binary data to file.

**Function Prototype:**
```c
XXAPI size_t xrtFileWriteBin(str sPath, ptr pData, size_t iLen);
```

**Parameters:**
- `sPath` - File path
- `pData` - Data to write
- `iLen` - Data length

**Return Value:**
- Bytes written

---

### xrtFileAppend

Append string content to file.

**Function Prototype:**
```c
XXAPI int xrtFileAppend(str sPath, str sData, size_t iLen, int CP);
```

**Parameters:**
- `sPath` - File path
- `sData` - Data to write
- `iLen` - Data length (0 means write entire string)
- `CP` - Character encoding

**Return Value:**
- Bytes written

**Notes:**
- If file exists, content is appended at end
- If file doesn't exist, a new file is created

---

### xrtFileAppendBin

Append binary data to file.

**Function Prototype:**
```c
XXAPI size_t xrtFileAppendBin(str sPath, ptr pData, size_t iLen);
```

**Parameters:**
- `sPath` - File path
- `pData` - Data to write
- `iLen` - Data length

**Return Value:**
- Bytes written

---

## File Information

### xrtFileExists

Check if file exists.

**Function Prototype:**
```c
XXAPI bool xrtFileExists(str sPath);
```

**Parameters:**
- `sPath` - File path

**Return Value:**
- `TRUE` - File exists
- `FALSE` - File does not exist

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    if (xrtFileExists((str)"config.ini")) {
        printf("Config file exists\n");
    } else {
        printf("Config file not found\n");
    }
    
    xrtUnit();
    return 0;
}
```

---

### xrtDirExists

Check if directory exists.

**Function Prototype:**
```c
XXAPI bool xrtDirExists(str sPath);
```

**Parameters:**
- `sPath` - Directory path

**Return Value:**
- `TRUE` - Directory exists
- `FALSE` - Directory does not exist

---

### xrtFileGetSize

Get file size.

**Function Prototype:**
```c
XXAPI int64 xrtFileGetSize(str sPath);
```

**Parameters:**
- `sPath` - File path

**Return Value:**
- File size (bytes)
- `-1` - Failed to get

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    int64 size = xrtFileGetSize((str)"test.txt");
    if (size >= 0) {
        printf("File size: %" PRId64 " bytes\n", size);
    } else {
        printf("Failed to get file size\n");
    }
    
    xrtUnit();
    return 0;
}
```

---

### xrtFileGetTime

Get file time information.

**Function Prototype:**
```c
XXAPI int64 xrtFileGetTime(str sPath, int iType);
```

**Parameters:**
- `sPath` - File path
- `iType` - Time type
  - `0` - Creation time
  - `1` - Last access time
  - `2` - Last modification time

**Return Value:**
- Timestamp
- `0` - Failed to get

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    int64 createTime = xrtFileGetTime((str)"test.txt", 0);
    int64 modifyTime = xrtFileGetTime((str)"test.txt", 2);
    
    str createStr = xrtTimeToStr(createTime, XRT_TIME_FORMAT_DATETIME);
    str modifyStr = xrtTimeToStr(modifyTime, XRT_TIME_FORMAT_DATETIME);
    
    printf("Created: %s\n", createStr);
    printf("Modified: %s\n", modifyStr);
    
    xrtFree(createStr);
    xrtFree(modifyStr);
    
    xrtUnit();
    return 0;
}
```

---

## File Operations

### xrtFileCopy

Copy file.

**Function Prototype:**
```c
XXAPI bool xrtFileCopy(str sSrc, str sDst, bool bReWrite);
```

**Parameters:**
- `sSrc` - Source file path
- `sDst` - Destination file path
- `bReWrite` - Whether to overwrite existing destination file

**Return Value:**
- `TRUE` - Success
- `FALSE` - Failure

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    bool ok = xrtFileCopy((str)"source.txt", (str)"backup.txt", TRUE);
    printf("Copy result: %s\n", ok ? "Success" : "Failed");
    
    xrtUnit();
    return 0;
}
```

---

### xrtFileMove

Move or rename file.

**Function Prototype:**
```c
XXAPI bool xrtFileMove(str sSrc, str sDst, bool bReWrite);
```

**Parameters:**
- `sSrc` - Source file path
- `sDst` - Destination file path
- `bReWrite` - Whether to overwrite existing destination file

**Return Value:**
- `TRUE` - Success
- `FALSE` - Failure

**Notes:**
- When moving across partitions, copy+delete is automatically used

---

### xrtFileDelete

Delete file.

**Function Prototype:**
```c
XXAPI bool xrtFileDelete(str sPath);
```

**Parameters:**
- `sPath` - File path

**Return Value:**
- `TRUE` - Success
- `FALSE` - Failure

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    if (xrtFileExists((str)"temp.txt")) {
        bool ok = xrtFileDelete((str)"temp.txt");
        printf("Delete result: %s\n", ok ? "Success" : "Failed");
    }
    
    xrtUnit();
    return 0;
}
```

---

## Directory Operations

### xrtDirScan

Scan directory.

**Function Prototype:**
```c
XXAPI int xrtDirScan(str sPath, int bRecu, ptr pProc, ptr Param);
```

**Parameters:**
- `sPath` - Directory path
- `bRecu` - Whether to recursively scan subdirectories
- `pProc` - Callback function
- `Param` - Callback parameter

**Callback Function Prototype:**
```c
int callback(str sPath, size_t iSize, int bDir, ptr pData, ptr Param);
```

**Callback Parameter Description:**
- `sPath` - File/directory path
- `iSize` - Path length
- `bDir` - Type identifier
  - `0` - File
  - `1` - Directory (entering)
  - `2` - Directory (leaving)
- `pData` - Platform-specific data (Windows: `WIN32_FIND_DATAW*`, Linux: `struct dirent*`)
- `Param` - User parameter

**Return Value:**
- Number of files scanned

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int OnFile(str sPath, size_t iSize, int bDir, ptr pData, ptr Param) {
    int* count = (int*)Param;
    if (bDir == 0) {
        printf("File: %s\n", sPath);
        (*count)++;
    } else if (bDir == 1) {
        printf("Entering directory: %s\n", sPath);
    } else if (bDir == 2) {
        printf("Leaving directory: %s\n", sPath);
    }
    return FALSE;  // Return TRUE to stop scanning
}

int main() {
    xrtInit();
    
    int fileCount = 0;
    int total = xrtDirScan((str)".", TRUE, OnFile, &fileCount);
    printf("Total files scanned: %d\n", total);
    
    xrtUnit();
    return 0;
}
```

---

### xrtDirCreate

Create directory.

**Function Prototype:**
```c
XXAPI bool xrtDirCreate(str sPath);
```

**Parameters:**
- `sPath` - Directory path

**Return Value:**
- `TRUE` - Success
- `FALSE` - Failure (directory exists or parent directory doesn't exist)

---

### xrtDirCreateAll

Create multi-level directory.

**Function Prototype:**
```c
XXAPI bool xrtDirCreateAll(str sPath);
```

**Parameters:**
- `sPath` - Directory path

**Return Value:**
- `TRUE` - Success
- `FALSE` - Failure

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // Create multi-level directory
    bool ok = xrtDirCreateAll((str)"data/cache/temp");
    printf("Create result: %s\n", ok ? "Success" : "Failed");
    
    xrtUnit();
    return 0;
}
```

---

### xrtDirCopy

Copy directory.

**Function Prototype:**
```c
XXAPI int xrtDirCopy(str sSrc, str sDst, bool bReWrite);
```

**Parameters:**
- `sSrc` - Source directory path
- `sDst` - Destination directory path
- `bReWrite` - Whether to overwrite existing files

**Return Value:**
- Number of files copied

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    int count = xrtDirCopy((str)"project", (str)"backup/project", TRUE);
    printf("Copied %d files\n", count);
    
    xrtUnit();
    return 0;
}
```

---

### xrtDirMove

Move directory.

**Function Prototype:**
```c
XXAPI int xrtDirMove(str sSrc, str sDst, bool bReWrite);
```

**Parameters:**
- `sSrc` - Source directory path
- `sDst` - Destination directory path
- `bReWrite` - Whether to overwrite existing files

**Return Value:**
- Number of files moved

---

### xrtDirDelete

Delete directory (including all contents).

**Function Prototype:**
```c
XXAPI int xrtDirDelete(str sPath);
```

**Parameters:**
- `sPath` - Directory path

**Return Value:**
- Number of files deleted

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    if (xrtDirExists((str)"temp")) {
        int count = xrtDirDelete((str)"temp");
        printf("Deleted %d files\n", count);
    }
    
    xrtUnit();
    return 0;
}
```

**Warning:**
- This function recursively deletes all contents, use with caution

---

## Usage Scenarios

### 1. Configuration File Reading

```c
#include "xrt.h"
#include <stdio.h>

str LoadConfig(str path) {
    if (!xrtFileExists(path)) {
        return NULL;
    }
    return xrtFileReadAll(path, XRT_CP_UTF8);
}

int main() {
    xrtInit();
    
    str config = LoadConfig((str)"config.ini");
    if (config) {
        printf("Config content:\n%s\n", config);
        xrtFree(config);
    } else {
        printf("Config file does not exist\n");
    }
    
    xrtUnit();
    return 0;
}
```

---

### 2. Log File Append Writing

```c
#include "xrt.h"
#include <stdio.h>

void AppendLog(str message) {
    str timeStr = xrtTimeToStr(xrtNow(), XRT_TIME_FORMAT_DATETIME);
    str line = xrtFormat((str)"[%s] %s\n", timeStr, message);
    xrtFileAppend((str)"app.log", line, 0, XRT_CP_UTF8);
    xrtFree(line);
    xrtFree(timeStr);
}

int main() {
    xrtInit();
    
    AppendLog((str)"Application started");
    AppendLog((str)"Processing data...");
    AppendLog((str)"Application ended");
    
    printf("Log written to app.log\n");
    
    xrtUnit();
    return 0;
}
```

---

### 3. Batch File Processing

```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
    int fileCount;
    int64 totalSize;
} ScanInfo;

int OnFile(str sPath, size_t iSize, int bDir, ptr pData, ptr Param) {
    ScanInfo* info = (ScanInfo*)Param;
    
    if (bDir == 0) {  // File
        info->fileCount++;
        info->totalSize += xrtFileGetSize(sPath);
        printf("File: %s\n", sPath);
    } else if (bDir == 1) {  // Entering directory
        printf("Enter: %s\n", sPath);
    }
    
    return FALSE;  // Continue scanning
}

int main() {
    xrtInit();
    
    ScanInfo info = {0, 0};
    int total = xrtDirScan((str)".", TRUE, OnFile, &info);
    
    printf("\nScan complete:\n");
    printf("File count: %d\n", info.fileCount);
    printf("Total size: %" PRId64 " bytes\n", info.totalSize);
    
    xrtUnit();
    return 0;
}
```

---

### 4. File Backup

```c
#include "xrt.h"
#include <stdio.h>

int BackupFile(str srcPath) {
    if (!xrtFileExists(srcPath)) {
        printf("Source file does not exist: %s\n", srcPath);
        return FALSE;
    }
    
    // Generate backup filename
    str timeStr = xrtTimeToStr(xrtNow(), XRT_TIME_FORMAT_COMPACT);
    str backupPath = xrtFormat((str)"%s.%s.bak", srcPath, timeStr);
    xrtFree(timeStr);
    
    // Copy file
    bool ok = xrtFileCopy(srcPath, backupPath, FALSE);
    if (ok) {
        printf("Backup successful: %s\n", backupPath);
    } else {
        printf("Backup failed\n");
    }
    
    xrtFree(backupPath);
    return ok;
}

int main() {
    xrtInit();
    
    // Create test file
    xrtFileWriteAll((str)"test.txt", (str)"Test content", 0, XRT_CP_UTF8);
    
    // Backup file
    BackupFile((str)"test.txt");
    
    xrtUnit();
    return 0;
}
```

---

### 5. Binary File Read/Write

```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
    int magic;
    int version;
    int dataSize;
} FileHeader;

int main() {
    xrtInit();
    
    // Write binary file
    xfile f = xrtOpen((str)"data.bin", FALSE, -1);  // -1 for binary mode
    if (f) {
        FileHeader header = { 0x12345678, 1, 100 };
        xrtPut(f, &header, sizeof(FileHeader));
        
        // Write data
        uint8 data[100];
        for (int i = 0; i < 100; i++) data[i] = (uint8)i;
        xrtPut(f, data, 100);
        
        xrtClose(f);
        printf("Write complete\n");
    }
    
    // Read binary file
    f = xrtOpen((str)"data.bin", TRUE, -1);
    if (f) {
        FileHeader header;
        ptr headerData = xrtGet(f, sizeof(FileHeader));
        if (headerData) {
            memcpy(&header, headerData, sizeof(FileHeader));
            xrtFree(headerData);
            printf("Magic: 0x%X\n", header.magic);
            printf("Version: %d\n", header.version);
            printf("DataSize: %d\n", header.dataSize);
        }
        xrtClose(f);
    }
    
    xrtUnit();
    return 0;
}
```

---

## Best Practices

### 1. Always Check File Operation Results

```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xfile f = xrtOpen((str)"important.txt", FALSE, XRT_CP_UTF8);
    if (!f) {
        printf("Failed to open file: %s\n", xCore.LastError);
        xrtUnit();
        return 1;
    }
    
    size_t written = xrtWrite(f, (str)"Important data", 0);
    if (written == 0) {
        printf("Write failed: %s\n", xCore.LastError);
    }
    
    xrtClose(f);
    xrtUnit();
    return 0;
}
```

---

### 2. Use Temporary Files for Safe Writing

```c
#include "xrt.h"
#include <stdio.h>

int SafeWriteFile(str path, str content) {
    // Write to temporary file
    str tempPath = xrtPathRandom((str)".", (str)".tmp");
    int ok = xrtFileWriteAll(tempPath, content, 0, XRT_CP_UTF8);
    
    if (ok) {
        // Delete old file and rename
        if (xrtFileExists(path)) {
            xrtFileDelete(path);
        }
        ok = xrtFileMove(tempPath, path, TRUE);
    }
    
    // Cleanup temporary file
    if (xrtFileExists(tempPath)) {
        xrtFileDelete(tempPath);
    }
    
    xrtFree(tempPath);
    return ok;
}

int main() {
    xrtInit();
    
    int ok = SafeWriteFile((str)"config.ini", (str)"[config]\nkey=value");
    printf("Safe write: %s\n", ok ? "Success" : "Failed");
    
    xrtUnit();
    return 0;
}
```

---

## Related Documentation

- [Path Path Processing](api-path.en.md)
- [Charset Character Set Conversion](api-charset.en.md)
- [Main Index](README.en.md)

---

<div align="center">

[Back to Top](#file---file-operations-library)

</div>