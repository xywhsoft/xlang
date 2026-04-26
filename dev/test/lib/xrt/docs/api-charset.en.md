# Charset Conversion Library

> Character encoding conversion, detection and processing functions

[English](api-charset.en.md) | [中文](api-charset.md) | [Back to Index](README.en.md)

---

## 📑 Table of Contents

- [Charset Constants](#charset-constants)
- [UTF Encoding Conversion](#utf-encoding-conversion)
- [Byte Order Conversion](#byte-order-conversion)
- [General Encoding Conversion](#general-encoding-conversion)
- [Encoding Detection](#encoding-detection)
- [Helper Functions](#helper-functions)

---

## Charset Constants

### Charset Code Pages

```c
#define XRT_CP_AUTO         -2      // Auto-detect charset
#define XRT_CP_BINARY       -1      // Binary file
#define XRT_CP_OEM          0       // Native OEM charset
#define XRT_CP_UTF8         65001   // UTF-8
#define XRT_CP_UTF16        1200    // UTF-16 (Little Endian)
#define XRT_CP_UTF16_BE     1201    // UTF-16 (Big Endian)
#define XRT_CP_UTF32        65005   // UTF-32 (Little Endian)
#define XRT_CP_UTF32_BE     65006   // UTF-32 (Big Endian)
```

### BOM Markers

```c
#define XRT_CP_BOM          0x40000000  // With BOM marker
#define XRT_MASK_BOM        0xBFFFFFFF  // BOM mask
```

**Usage:**
```c
// Use UTF-8 with BOM
int charset = XRT_CP_UTF8 | XRT_CP_BOM;

// Remove BOM marker
int pure_charset = charset & XRT_MASK_BOM;
```

---

## UTF Encoding Conversion

### xrtUTF8to16

UTF-8 to UTF-16

**Function Prototype:**
```c
XXAPI u16str xrtUTF8to16(u8str sText, size_t iSize);
```

**Parameters:**
- `sText` - UTF-8 string
- `iSize` - Byte length (0 for auto-calculate to `\0`)

**Return Value:**
- Success: Returns UTF-16 string pointer
- Failure: Returns `NULL`

**Memory Release:** ✅ Requires `xrtFree` to release

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    str utf8 = (str)"Hello, World!";
    u16str utf16 = xrtUTF8to16(utf8, 0);
    if (utf16) {
        printf("UTF-16 character count: %" PRId64 "\n", xCore.iRet);
        xrtFree(utf16);
    }
    
    xrtUnit();
    return 0;
}
```

**Additional Notes:**
- Character count after conversion is stored in `xCore.iRet`
- Characters exceeding UTF-16 range (5-6 byte UTF-8) will be replaced with `0xFFFD`

---

### xrtUTF8to32

UTF-8 to UTF-32

**Function Prototype:**
```c
XXAPI u32str xrtUTF8to32(u8str sText, size_t iSize);
```

**Parameters:**
- `sText` - UTF-8 string
- `iSize` - Byte length (0 for auto-calculate)

**Return Value:**
- Success: Returns UTF-32 string pointer
- Failure: Returns `NULL`

**Memory Release:** ✅ Requires `xrtFree` to release

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    str utf8 = (str)"Hello World";
    u32str utf32 = xrtUTF8to32(utf8, 0);
    if (utf32) {
        printf("UTF-32 character count: %" PRId64 "\n", xCore.iRet);  // 11
        // UTF-32 each character takes 4 bytes
        xrtFree(utf32);
    }
    
    xrtUnit();
    return 0;
}
```

**Additional Notes:**
- UTF-32 can represent all Unicode characters, including 5-6 byte UTF-8 characters
- Character count after conversion is stored in `xCore.iRet`

---

### xrtUTF16to8

UTF-16 to UTF-8

**Function Prototype:**
```c
XXAPI u8str xrtUTF16to8(u16str sText, size_t iSize);
```

**Parameters:**
- `sText` - UTF-16 string
- `iSize` - Character count (0 for auto-calculate to `\0`)

**Return Value:**
- Success: Returns UTF-8 string pointer
- Failure: Returns `NULL`

**Memory Release:** ✅ Requires `xrtFree` to release

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // Note: On Windows L"Hello" is UTF-16
    u16str utf16 = (u16str)L"Hello World";
    u8str utf8 = xrtUTF16to8(utf16, 0);
    if (utf8) {
        printf("UTF-8: %s\n", utf8);
        printf("UTF-8 byte count: %" PRId64 "\n", xCore.iRet);
        xrtFree(utf8);
    }
    
    xrtUnit();
    return 0;
}
```

**Additional Notes:**
- Byte count after conversion is stored in `xCore.iRet`
- Invalid surrogate pairs will be replaced with `0xEFBFBD` (UTF-8 replacement character)

---

### xrtUTF16to32

UTF-16 to UTF-32

**Function Prototype:**
```c
XXAPI u32str xrtUTF16to32(u16str sText, size_t iSize);
```

**Parameters:**
- `sText` - UTF-16 string
- `iSize` - Character count (0 for auto-calculate)

**Return Value:**
- Success: Returns UTF-32 string pointer
- Failure: Returns `NULL`

**Memory Release:** ✅ Requires `xrtFree` to release

---

### xrtUTF32to8

UTF-32 to UTF-8

**Function Prototype:**
```c
XXAPI u8str xrtUTF32to8(u32str sText, size_t iSize);
```

**Parameters:**
- `sText` - UTF-32 string
- `iSize` - Character count (0 for auto-calculate)

**Return Value:**
- Success: Returns UTF-8 string pointer
- Failure: Returns `NULL`

**Memory Release:** ✅ Requires `xrtFree` to release

---

### xrtUTF32to16

UTF-32 to UTF-16

**Function Prototype:**
```c
XXAPI u16str xrtUTF32to16(u32str sText, size_t iSize);
```

**Parameters:**
- `sText` - UTF-32 string
- `iSize` - Character count (0 for auto-calculate)

**Return Value:**
- Success: Returns UTF-16 string pointer
- Failure: Returns `NULL`

**Memory Release:** ✅ Requires `xrtFree` to release

---

## Byte Order Conversion

### xrtUTF16LEtoBE

UTF-16 Little Endian to Big Endian conversion

**Function Prototype:**
```c
XXAPI u16str xrtUTF16LEtoBE(u16str sText, size_t iSize, bool bSrcRevise);
```

**Parameters:**
- `sText` - UTF-16 string
- `iSize` - Character count (0 for auto-calculate)
- `bSrcRevise` - Whether to modify source data directly
  - `TRUE` - Modify source string directly, return source pointer
  - `FALSE` - Create new string, requires `xrtFree` to release

**Return Value:**
- `bSrcRevise=TRUE`: Returns source pointer `sText`
- `bSrcRevise=FALSE`: Returns newly allocated string

**Memory Release:** 
- `bSrcRevise=TRUE` - ⭕ No release needed
- `bSrcRevise=FALSE` - ✅ Requires `xrtFree` to release

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // Don't modify source data, create new string
    u16str le_text = (u16str)L"Hello";
    u16str be_text = xrtUTF16LEtoBE(le_text, 0, FALSE);
    printf("Conversion complete, original data unchanged\n");
    xrtFree(be_text);
    
    // Modify source data directly (requires writable memory)
    u16str text = xrtMalloc(10 * sizeof(u16));
    text[0] = 0x4F60;  // Character
    text[1] = 0x597D;  // Character
    text[2] = 0;
    
    printf("Before: 0x%04X 0x%04X\n", text[0], text[1]);
    xrtUTF16LEtoBE(text, 2, TRUE);  // In-place conversion
    printf("After: 0x%04X 0x%04X\n", text[0], text[1]);
    
    xrtFree(text);
    xrtUnit();
    return 0;
}
```

---

### xrtUTF32LEtoBE

UTF-32 Little Endian to Big Endian conversion

**Function Prototype:**
```c
XXAPI u32str xrtUTF32LEtoBE(u32str sText, size_t iSize, bool bSrcRevise);
```

**Parameters:**
- `sText` - UTF-32 string
- `iSize` - Character count (0 for auto-calculate)
- `bSrcRevise` - Whether to modify source data directly

**Return Value:**
- `bSrcRevise=TRUE`: Returns source pointer
- `bSrcRevise=FALSE`: Returns new string

**Memory Release:**
- `bSrcRevise=TRUE` - ⭕ No release needed
- `bSrcRevise=FALSE` - ✅ Requires `xrtFree` to release

---

## General Encoding Conversion

### xrtConvCharset

Convert between any charsets

**Function Prototype:**
```c
XXAPI ptr xrtConvCharset(ptr sText, size_t iSize, int iInCP, int iOutCP);
```

**Parameters:**
- `sText` - Source string
- `iSize` - Source string byte length (0 for auto-calculate)
- `iInCP` - Input charset code page
- `iOutCP` - Output charset code page

**Return Value:**
- Success: Returns converted string
- Failure: Returns `NULL`

**Memory Release:** ✅ Requires `xrtFree` to release

**Supported Charsets:**
- `XRT_CP_AUTO` - Auto-detect (input only)
- `XRT_CP_BINARY` - Binary (no conversion)
- `XRT_CP_OEM` - Native charset (Windows: ANSI, Linux: UTF-8)
- `XRT_CP_UTF8` - UTF-8
- `XRT_CP_UTF16` - UTF-16 (LE)
- `XRT_CP_UTF16_BE` - UTF-16 (BE)
- `XRT_CP_UTF32` - UTF-32 (LE)
- `XRT_CP_UTF32_BE` - UTF-32 (BE)

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // UTF-8 to UTF-16
    str utf8 = (str)"Hello";
    u16str utf16 = xrtConvCharset(utf8, 0, XRT_CP_UTF8, XRT_CP_UTF16);
    if (utf16) {
        printf("Conversion successful\n");
        xrtFree(utf16);
    }
    
    // Native encoding to UTF-8 (may be GBK on Windows)
    str oem = (str)"Native text";
    str utf8_out = xrtConvCharset(oem, 0, XRT_CP_OEM, XRT_CP_UTF8);
    if (utf8_out) {
        printf("UTF-8: %s\n", utf8_out);
        xrtFree(utf8_out);
    }
    
    // UTF-16 to UTF-32 BE
    u16str src16 = (u16str)L"Test";
    u32str dst32be = xrtConvCharset(src16, 0, XRT_CP_UTF16, XRT_CP_UTF32_BE);
    if (dst32be) {
        printf("UTF-32 BE conversion successful\n");
        xrtFree(dst32be);
    }
    
    xrtUnit();
    return 0;
}
```

**Platform Notes:**
- Windows: Supports all Windows code pages (via Win32 API)
- Linux/macOS: Only supports UTF-8/16/32 interconversion, OEM is fixed to UTF-8

---

## Encoding Detection

### xrtIsUTF8

Check if string is valid UTF-8

**Function Prototype:**
```c
XXAPI bool xrtIsUTF8(str sText, size_t iSize);
```

**Parameters:**
- `sText` - String to check
- `iSize` - Byte length (0 for auto-calculate)

**Return Value:**
- `TRUE` - Is valid UTF-8
- `FALSE` - Not UTF-8 or contains invalid sequences

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    str text1 = (str)"Hello World";
    bool is_utf8 = xrtIsUTF8(text1, 0);  // TRUE
    printf("\"Hello World\" is UTF-8: %s\n", is_utf8 ? "true" : "false");
    
    // Invalid UTF-8 sequence
    unsigned char text2[] = {0xFF, 0xFE, 0x00};
    is_utf8 = xrtIsUTF8((str)text2, 2);  // FALSE
    printf("0xFF 0xFE is UTF-8: %s\n", is_utf8 ? "true" : "false");
    
    // Empty string returns TRUE
    is_utf8 = xrtIsUTF8((str)"", 0);  // TRUE
    printf("Empty string is UTF-8: %s\n", is_utf8 ? "true" : "false");
    
    xrtUnit();
    return 0;
}
```

**Additional Notes:**
- `NULL` input returns `FALSE`
- Empty string returns `TRUE`
- Detection rules: Check if multi-byte sequences start with `10xxxxxx`, check for `0xFE`/`0xFF`

---

### xrtDetectCharset

Auto-detect character encoding

**Function Prototype:**
```c
XXAPI int xrtDetectCharset(ptr sText, size_t iSize, bool bBOM);
```

**Parameters:**
- `sText` - Data to detect
- `iSize` - Data length
- `bBOM` - Detected BOM info (passed via return value)

**Return Value:**
- Detected charset code page (may include `XRT_CP_BOM` flag)
- If has BOM, return value includes `XRT_CP_BOM` bit

**Detection Rules:**
1. First detect BOM markers
2. Then check if valid UTF-8
3. Infer UTF-16/UTF-32 based on `\0` distribution
4. If none match, return `XRT_CP_OEM` or `XRT_CP_BINARY`

**BOM Markers:**
- UTF-8 BOM: `EF BB BF`
- UTF-16 LE BOM: `FF FE`
- UTF-16 BE BOM: `FE FF`
- UTF-32 LE BOM: `FF FE 00 00`
- UTF-32 BE BOM: `00 00 FE FF`

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // Simulate file data (with UTF-8 BOM)
    unsigned char data_with_bom[] = {0xEF, 0xBB, 0xBF, 'H', 'e', 'l', 'l', 'o'};
    size_t size = sizeof(data_with_bom);
    
    int charset = xrtDetectCharset(data_with_bom, size, TRUE);
    
    // Check for BOM
    if (charset & XRT_CP_BOM) {
        printf("BOM marker detected\n");
        int pure_charset = charset & XRT_MASK_BOM;  // Remove BOM flag
        
        switch (pure_charset) {
            case XRT_CP_UTF8:
                printf("Encoding: UTF-8 with BOM\n");
                break;
            case XRT_CP_UTF16:
                printf("Encoding: UTF-16 LE with BOM\n");
                break;
            case XRT_CP_UTF16_BE:
                printf("Encoding: UTF-16 BE with BOM\n");
                break;
        }
    } else {
        printf("No BOM marker\n");
    }
    
    // Detect text without BOM
    str plain_utf8 = (str)"Hello World";
    charset = xrtDetectCharset(plain_utf8, 0, TRUE);
    printf("Plain UTF-8 detection result: %d (XRT_CP_UTF8=%d)\n", charset, XRT_CP_UTF8);
    
    xrtUnit();
    return 0;
}
```

---

### xrtGetCharSize

Get character size for charset

**Function Prototype:**
```c
XXAPI int xrtGetCharSize(int iCP);
```

**Parameters:**
- `iCP` - Charset code page

**Return Value:**
- `1` - Single-byte encoding (UTF-8, OEM, etc.)
- `2` - Double-byte encoding (UTF-16, UTF-16 BE)
- `4` - Four-byte encoding (UTF-32, UTF-32 BE)

**Character Size Notes:**
- `XRT_CP_UTF8` → `1` (Note: this is base unit, actual chars may be 1-6 bytes)
- `XRT_CP_UTF16` / `XRT_CP_UTF16_BE` → `2`
- `XRT_CP_UTF32` / `XRT_CP_UTF32_BE` → `4`
- `XRT_CP_OEM` → `1` (base unit)
- Others → `1`

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    int size_utf8 = xrtGetCharSize(XRT_CP_UTF8);      // 1
    int size_utf16 = xrtGetCharSize(XRT_CP_UTF16);    // 2
    int size_utf32 = xrtGetCharSize(XRT_CP_UTF32);    // 4
    int size_utf16be = xrtGetCharSize(XRT_CP_UTF16_BE); // 2
    
    printf("UTF-8 base unit: %d bytes\n", size_utf8);
    printf("UTF-16 unit: %d bytes\n", size_utf16);
    printf("UTF-32 unit: %d bytes\n", size_utf32);
    
    // Charset with BOM also handled correctly (BOM flag auto-removed)
    int size_with_bom = xrtGetCharSize(XRT_CP_UTF16 | XRT_CP_BOM);  // 2
    printf("UTF-16 with BOM: %d bytes\n", size_with_bom);
    
    xrtUnit();
    return 0;
}
```

---

## Helper Functions

### String Length Functions

These functions are defined in `base.h`, providing UTF-16/32 support:

```c
// UTF-16 string length (character count)
XXAPI size_t u16len(u16str sText);

// UTF-32 string length (character count)
XXAPI size_t u32len(u32str sText);
```

**Example:**
```c
u16str text16 = L"Hello";
size_t len16 = u16len(text16);  // 5

u32str text32 = U"World";
size_t len32 = u32len(text32);  // 5
```

---

## Use Cases

### 1. Cross-Platform Text Processing

```c
#include "xrt.h"
#include <stdio.h>

void DisplayMessage(str utf8_text) {
#if defined(_WIN32) || defined(_WIN64)
    // Windows: Convert to UTF-16 for Win32 API
    u16str wtext = xrtUTF8to16(utf8_text, 0);
    // MessageBoxW(NULL, (LPCWSTR)wtext, L"Title", MB_OK);
    printf("Windows: Converted to UTF-16\n");
    xrtFree(wtext);
#else
    // Linux/macOS: Use UTF-8 directly
    printf("%s\n", utf8_text);
#endif
}

int main() {
    xrtInit();
    DisplayMessage((str)"Hello World");
    xrtUnit();
    return 0;
}
```

---

### 2. File Encoding Conversion

```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // Read file
    ptr file_data = xrtFileGetAll((str)"input.txt");
    if (file_data == NULL) {
        printf("File read failed\n");
        xrtUnit();
        return 1;
    }
    size_t file_size = xCore.iRet;
    
    // Detect encoding
    int detected = xrtDetectCharset(file_data, file_size, TRUE);
    int charset = detected & XRT_MASK_BOM;
    printf("Detected encoding: %d\n", charset);
    
    // Convert to UTF-8
    str utf8_text = xrtConvCharset(file_data, file_size, charset, XRT_CP_UTF8);
    if (utf8_text) {
        printf("Conversion successful, UTF-8 content:\n%s\n", utf8_text);
        
        // Save as UTF-8 file
        xrtFilePutAll((str)"output.txt", utf8_text, 0);
        xrtFree(utf8_text);
    }
    
    xrtFree(file_data);
    xrtUnit();
    return 0;
}
```

---

### 3. Network Data Processing

```c
#include "xrt.h"
#include <stdio.h>

// Simulate receiving UTF-16 data from network
u16str SimulateReceiveFromNetwork() {
    u16str data = xrtMalloc(20 * sizeof(u16));
    data[0] = 0x0048;  // 'H'
    data[1] = 0x0069;  // 'i'
    data[2] = 0;
    return data;
}

void ProcessText(str text) {
    printf("Processing text: %s\n", text);
}

int main() {
    xrtInit();
    
    // Receive UTF-16 data
    u16str received_data = SimulateReceiveFromNetwork();
    
    // Convert to UTF-8 for processing
    str utf8_data = xrtUTF16to8(received_data, 0);
    if (utf8_data) {
        ProcessText(utf8_data);
        xrtFree(utf8_data);
    }
    
    xrtFree(received_data);
    xrtUnit();
    return 0;
}
```

---

### 4. Internationalized Strings

```c
#include "xrt.h"
#include <stdio.h>

void DisplayText(u16str wtext) {
    // Convert back to UTF-8 for display
    str utf8 = xrtUTF16to8(wtext, 0);
    printf("Display: %s\n", utf8);
    xrtFree(utf8);
}

int main() {
    xrtInit();
    
    // Multi-language text processing
    str chinese = (str)"Chinese text";
    str english = (str)"Hello World";
    str japanese = (str)"Japanese text";
    
    // Convert all to UTF-16
    u16str wchinese = xrtUTF8to16(chinese, 0);
    u16str wenglish = xrtUTF8to16(english, 0);
    u16str wjapanese = xrtUTF8to16(japanese, 0);
    
    DisplayText(wchinese);
    DisplayText(wenglish);
    DisplayText(wjapanese);
    
    xrtFree(wchinese);
    xrtFree(wenglish);
    xrtFree(wjapanese);
    
    xrtUnit();
    return 0;
}
```

---

## Best Practices

### 1. Use UTF-8 Uniformly

```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // ✅ Recommended: Use UTF-8 internally
    str internal_text = (str)"Internal text";
    
    // Only convert when needed
#if defined(_WIN32) || defined(_WIN64)
    u16str display_text = xrtUTF8to16(internal_text, 0);
    printf("Converted to UTF-16 for Win32 API\n");
    // ... use display_text with Windows API
    xrtFree(display_text);
#else
    printf("Use UTF-8 directly: %s\n", internal_text);
#endif
    
    xrtUnit();
    return 0;
}
```

---

### 2. Auto Encoding Detection

```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // ✅ Handle files with unknown encoding
    ptr data = xrtFileGetAll((str)"unknown.txt");
    if (data == NULL) {
        printf("File read failed\n");
        xrtUnit();
        return 1;
    }
    size_t size = xCore.iRet;
    
    // Detect encoding
    int charset = xrtDetectCharset(data, size, TRUE);
    int pure_charset = charset & XRT_MASK_BOM;
    printf("Detected encoding: %d\n", pure_charset);
    
    // Convert to UTF-8
    str utf8 = xrtConvCharset(data, size, pure_charset, XRT_CP_UTF8);
    if (utf8) {
        printf("UTF-8 content:\n%s\n", utf8);
        xrtFree(utf8);
    }
    
    xrtFree(data);
    xrtUnit();
    return 0;
}
```

---

### 3. BOM Handling

```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // Simulate file data with BOM
    unsigned char data[] = {0xEF, 0xBB, 0xBF, 'H', 'e', 'l', 'l', 'o', '\0'};
    size_t size = sizeof(data) - 1;
    
    // Detect encoding
    int charset = xrtDetectCharset(data, size, TRUE);
    
    if (charset & XRT_CP_BOM) {
        printf("Has BOM marker\n");
        
        // Skip BOM bytes
        ptr text_start = data;
        size_t bom_size = 0;
        
        switch (charset & XRT_MASK_BOM) {
            case XRT_CP_UTF8:     bom_size = 3; break;  // EF BB BF
            case XRT_CP_UTF16:    bom_size = 2; break;  // FF FE
            case XRT_CP_UTF16_BE: bom_size = 2; break;  // FE FF
            case XRT_CP_UTF32:    bom_size = 4; break;  // FF FE 00 00
            case XRT_CP_UTF32_BE: bom_size = 4; break;  // 00 00 FE FF
        }
        
        printf("BOM size: %zu bytes\n", bom_size);
        printf("Content after BOM: %s\n", (char*)(text_start + bom_size));
    } else {
        printf("No BOM marker\n");
    }
    
    xrtUnit();
    return 0;
}
```

---

### 4. Byte Order Compatibility

```c
#include "xrt.h"
#include <stdio.h>

void SendToNetwork(u16str data) {
    printf("Sending data: 0x%04X 0x%04X\n", data[0], data[1]);
}

int main() {
    xrtInit();
    
    // Simulate local UTF-16 data
    u16str local_text = xrtMalloc(10 * sizeof(u16));
    local_text[0] = 0x0048;  // 'H' LE
    local_text[1] = 0x0069;  // 'i' LE
    local_text[2] = 0;
    
    printf("Local data (LE): 0x%04X 0x%04X\n", local_text[0], local_text[1]);
    
    // Network transmission: use big endian uniformly
    u16str be_text = xrtUTF16LEtoBE(local_text, 2, FALSE);
    printf("Network data (BE): 0x%04X 0x%04X\n", be_text[0], be_text[1]);
    SendToNetwork(be_text);
    xrtFree(be_text);
    
    // Receive big endian data and convert back to native order
    u16str received_be = xrtMalloc(10 * sizeof(u16));
    received_be[0] = 0x4800;  // 'H' BE
    received_be[1] = 0x6900;  // 'i' BE
    received_be[2] = 0;
    
    u16str le_text = xrtUTF16LEtoBE(received_be, 2, FALSE);
    printf("Converted (LE): 0x%04X 0x%04X\n", le_text[0], le_text[1]);
    xrtFree(le_text);
    xrtFree(received_be);
    
    xrtFree(local_text);
    xrtUnit();
    return 0;
}
```

---

## Performance Tips

### 1. Avoid Repeated Conversion

```c
#include "xrt.h"
#include <stdio.h>

void Display(u16str text) {
    str utf8 = xrtUTF16to8(text, 0);
    printf("%s\n", utf8);
    xrtFree(utf8);
}

int main() {
    xrtInit();
    
    // Simulate data
    str utf8_array[3] = {
        (str)"Text1",
        (str)"Text2",
        (str)"Text3"
    };
    int count = 3;
    
    // ❌ Inefficient: repeated conversion
    printf("Inefficient way:\n");
    for (int i = 0; i < count; i++) {
        u16str wtext = xrtUTF8to16(utf8_array[i], 0);
        Display(wtext);
        xrtFree(wtext);
    }
    
    // ✅ Efficient: pre-convert
    printf("Efficient way:\n");
    u16str* wtext_array = xrtMalloc(count * sizeof(u16str));
    for (int i = 0; i < count; i++) {
        wtext_array[i] = xrtUTF8to16(utf8_array[i], 0);
    }
    // Use converted data multiple times
    for (int i = 0; i < count; i++) {
        Display(wtext_array[i]);
    }
    // Release all at once
    for (int i = 0; i < count; i++) {
        xrtFree(wtext_array[i]);
    }
    xrtFree(wtext_array);
    
    xrtUnit();
    return 0;
}
```

---

### 2. Use In-Place Conversion

```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // Allocate writable memory and fill data
    u16str text = xrtMalloc(10 * sizeof(u16));
    text[0] = 0x0048;  // 'H'
    text[1] = 0x0069;  // 'i'
    text[2] = 0;
    
    printf("Before: 0x%04X 0x%04X\n", text[0], text[1]);
    
    // ✅ In-place conversion, no extra memory needed
    xrtUTF16LEtoBE(text, 2, TRUE);
    printf("After: 0x%04X 0x%04X\n", text[0], text[1]);
    
    // Convert back
    xrtUTF16LEtoBE(text, 2, TRUE);
    printf("Converted back: 0x%04X 0x%04X\n", text[0], text[1]);
    
    xrtFree(text);
    xrtUnit();
    return 0;
}
```

---

## Common Errors

### 1. Forgetting to Free Memory

```c
#include "xrt.h"
#include <stdio.h>

void UseText(u16str text) {
    str utf8 = xrtUTF16to8(text, 0);
    printf("%s\n", utf8);
    xrtFree(utf8);
}

int main() {
    xrtInit();
    
    // ❌ Memory leak
    u16str text1 = xrtUTF8to16((str)"text", 0);
    UseText(text1);
    // Forgot xrtFree(text1);
    
    // ✅ Correct
    u16str text2 = xrtUTF8to16((str)"text", 0);
    UseText(text2);
    xrtFree(text2);  // Remember to free
    
    xrtUnit();
    return 0;
}
```

---

### 2. Confusing Character Count and Byte Count

```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

int main() {
    xrtInit();
    
    str utf8 = (str)"Hello";  // 5 bytes, 5 characters
    size_t byte_len = strlen((char*)utf8);
    printf("UTF-8 byte count: %zu\n", byte_len);  // 5
    
    // ❌ Wrong: treating character count as byte count
    // u16str wrong = xrtUTF8to16(utf8, 2);  // Wrong! 2 is byte count, not char count
    
    // ✅ Correct: Use byte count for UTF-8 conversion, or use 0 for auto
    u16str correct1 = xrtUTF8to16(utf8, byte_len);  // Use byte count
    u16str correct2 = xrtUTF8to16(utf8, 0);         // Auto calculate
    
    printf("UTF-16 character count: %" PRId64 "\n", xCore.iRet);  // 5
    
    xrtFree(correct1);
    xrtFree(correct2);
    
    xrtUnit();
    return 0;
}
```

**Notes:**
- UTF-8 conversion function's `iSize` parameter is **byte count**
- UTF-16/UTF-32 conversion function's `iSize` parameter is **character count**
- Recommend using `0` to let function auto-calculate length

---

## Related Documentation

- [Type Definitions](types.en.md) - String type definitions
- [String Processing](api-string.en.md) - String operations
- [File Operations](api-file.en.md) - File encoding handling
- [Main Index](README.en.md) - Return to documentation home

---

<div align="center">

[⬆️ Back to Top](#charset-conversion-library)

</div>
