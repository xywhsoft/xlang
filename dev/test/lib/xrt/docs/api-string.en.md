# String Processing Library

> String manipulation, search, replace, and encoding/decoding functions

[English](api-string.en.md) | [中文](api-string.md) | [Back to Index](README.en.md)

---

## 📑 Table of Contents

- [Constant Definitions](#constant-definitions)
- [String Copying](#string-copying)
- [String Comparison](#string-comparison)
- [Case Conversion](#case-conversion)
- [String Search](#string-search)
- [String Trimming](#string-trimming)
- [String Filtering](#string-filtering)
- [String Operations](#string-operations)
- [Encoding and Decoding](#encoding-and-decoding)
- [Use Cases](#use-cases)
- [Best Practices](#best-practices)

---

## Constant Definitions

### RandStringDefaultTemplate

Default template for random strings (internal use).

**Definition:**
```c
static const str RandStringDefaultTemplate = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz-_";
```

**Description:**
- Used when `sTemplate` parameter of `xrtRandStr` is `NULL`
- Contains 64 characters: digits + uppercase letters + lowercase letters + hyphen + underscore

### Base64EncodeTable

Standard Base64 encoding table (internal use).

**Definition:**
```c
static const str Base64EncodeTable = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
```

**Description:**
- Used when `sTable` parameter of `xrtBase64Encode` / `xrtBase64Decode` is `NULL`

---

## String Copying

### xrtCopyStr

Copy UTF-8 string.

**Prototype:**
```c
XXAPI str xrtCopyStr(str sText, size_t iSize);
```

**Parameters:**
- `sText` - Source string
- `iSize` - Byte length (0 for auto-calculate)

**Return Value:**
- Newly allocated string copy
- Copied byte length stored in `xCore.iRet`
- Returns `xCore.sNull` on failure

**Memory Release:** ✅ Requires `xrtFree` to release

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    str original = (str)"Hello World";
    str copy = xrtCopyStr(original, 0);
    
    printf("Original: %s\n", original);
    printf("Copy: %s\n", copy);
    printf("Length: %" PRId64 "\n", xCore.iRet);
    
    xrtFree(copy);
    xrtUnit();
    return 0;
}
```

**Additional Notes:**
- If `sText` is `NULL` or length is 0, returns `xCore.sNull`
- Returned string has `\0` terminator automatically added

---

### xrtCopyStrU16

Copy UTF-16 string.

**Prototype:**
```c
XXAPI u16str xrtCopyStrU16(u16str sText, size_t iSize);
```

**Parameters:**
- `sText` - Source UTF-16 string
- `iSize` - Character count (not bytes, 0 for auto-calculate)

**Return Value:**
- Newly allocated UTF-16 string copy
- Copied character count stored in `xCore.iRet`

**Memory Release:** ✅ Requires `xrtFree` to release

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    u16str original = (u16str)L"Hello";
    u16str copy = xrtCopyStrU16(original, 0);
    printf("Character count: %" PRId64 "\n", xCore.iRet);
    
    xrtFree(copy);
    xrtUnit();
    return 0;
}
```

---

### xrtCopyStrU32

Copy UTF-32 string.

**Prototype:**
```c
XXAPI u32str xrtCopyStrU32(u32str sText, size_t iSize);
```

**Parameters:**
- `sText` - Source UTF-32 string
- `iSize` - Character count (not bytes, 0 for auto-calculate)

**Return Value:**
- Newly allocated UTF-32 string copy
- Copied character count stored in `xCore.iRet`

**Memory Release:** ✅ Requires `xrtFree` to release

---

### xrtCopyMem

Copy memory block.

**Prototype:**
```c
XXAPI ptr xrtCopyMem(ptr pMem, size_t iSize);
```

**Parameters:**
- `pMem` - Source memory
- `iSize` - Byte count

**Return Value:**
- Newly allocated memory copy
- Copied byte count stored in `xCore.iRet`

**Memory Release:** ✅ Requires `xrtFree` to release

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    uint8 data[] = {0x01, 0x02, 0x03, 0x04, 0x05};
    ptr copy = xrtCopyMem(data, sizeof(data));
    
    printf("Bytes copied: %" PRId64 "\n", xCore.iRet);
    
    // Verify copied content
    uint8* p = (uint8*)copy;
    for (int i = 0; i < 5; i++) {
        printf("%02X ", p[i]);
    }
    printf("\n");
    
    xrtFree(copy);
    xrtUnit();
    return 0;
}
```

---

## String Comparison

### xrtStrComp

Compare strings.

**Prototype:**
```c
XXAPI int xrtStrComp(str s1, str s2, size_t iSize, bool bCase);
```

**Parameters:**
- `s1` - String 1
- `s2` - String 2
- `iSize` - Comparison length (0 for all)
- `bCase` - Whether to ignore case
  - `TRUE` - **Ignore** case (case-insensitive)
  - `FALSE` - **Distinguish** case (case-sensitive)

**Return Value:**
- `0` - Equal
- `<0` - s1 < s2
- `>0` - s1 > s2

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // Case-sensitive (bCase=FALSE)
    int r1 = xrtStrComp((str)"Hello", (str)"hello", 0, FALSE);
    printf("Case-sensitive: %d (not equal)\n", r1);
    
    // Case-insensitive (bCase=TRUE)
    int r2 = xrtStrComp((str)"Hello", (str)"hello", 0, TRUE);
    printf("Case-insensitive: %d (equal)\n", r2);
    
    // Compare first 3 characters
    int r3 = xrtStrComp((str)"Hello", (str)"Help", 3, FALSE);
    printf("First 3 chars: %d (equal)\n", r3);
    
    // Compare first 4 characters
    int r4 = xrtStrComp((str)"Hello", (str)"Help", 4, FALSE);
    printf("First 4 chars: %d (not equal)\n", r4);
    
    xrtUnit();
    return 0;
}
```

**Additional Notes:**
- Windows uses `stricmp`/`strnicmp`
- Linux/macOS uses `strcasecmp`/`strncasecmp`

---

## Case Conversion

### xrtLCase

Convert to lowercase.

**Prototype:**
```c
XXAPI str xrtLCase(str sText, size_t iSize, bool bSrcRevise);
```

**Parameters:**
- `sText` - Source string
- `iSize` - Length (0 for auto)
- `bSrcRevise` - Whether to modify source string
  - `TRUE` - Modify source directly, return source pointer
  - `FALSE` - Create new string

**Return Value:**
- Lowercase string pointer

**Memory Release:**
- `bSrcRevise=TRUE` - ⭕ No release needed
- `bSrcRevise=FALSE` - ✅ Requires `xrtFree` to release

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // Create new string
    str lower = xrtLCase((str)"HELLO WORLD", 0, FALSE);
    printf("New string: %s\n", lower);  // "hello world"
    xrtFree(lower);
    
    // Modify source string
    str text = xrtCopyStr((str)"WORLD", 0);
    xrtLCase(text, 0, TRUE);
    printf("Modified: %s\n", text);   // "world"
    xrtFree(text);
    
    xrtUnit();
    return 0;
}
```

**Additional Notes:**
- Automatically skips OEM encoded characters (bytes with high bit set to 1)
- Only processes letters in ASCII range

---

### xrtUCase

Convert to uppercase.

**Prototype:**
```c
XXAPI str xrtUCase(str sText, size_t iSize, bool bSrcRevise);
```

**Parameters:** Same as `xrtLCase`

**Memory Release:** Same as `xrtLCase`

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    str upper = xrtUCase((str)"hello world", 0, FALSE);
    printf("%s\n", upper);  // "HELLO WORLD"
    xrtFree(upper);
    
    xrtUnit();
    return 0;
}
```

---

## String Search

### xrtFindStr

Find substring.

**Prototype:**
```c
XXAPI str xrtFindStr(str sText, size_t iSize, str sSubText, size_t iSubSize, bool bCase);
```

**Parameters:**
- `sText` - String to search in
- `iSize` - Length (0 for auto)
- `sSubText` - Substring to find
- `iSubSize` - Substring length (0 for auto)
- `bCase` - Whether to ignore case (`TRUE`=ignore)

**Return Value:**
- Found: Returns pointer to substring position
- Not found: Returns `NULL`
- Found position index (1-based) stored in `xCore.iRet`

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    str text = (str)"Hello World, Hello XRT";
    
    // Case-sensitive search
    str found = xrtFindStr(text, 0, (str)"World", 0, FALSE);
    if (found) {
        printf("Found: %s\n", found);  // "World, Hello XRT"
        printf("Position: %" PRId64 "\n", xCore.iRet);  // 7
    }
    
    // Case-insensitive search
    found = xrtFindStr(text, 0, (str)"world", 0, TRUE);
    if (found) {
        printf("Case-insensitive found: %s\n", found);
    }
    
    // Not found
    found = xrtFindStr(text, 0, (str)"xyz", 0, FALSE);
    if (!found) {
        printf("xyz not found\n");
    }
    
    xrtUnit();
    return 0;
}
```

---

### xrtInStr

Find substring position (returns index).

**Prototype:**
```c
XXAPI uint xrtInStr(str sText, size_t iSize, str sSubText, size_t iSubSize, bool bCase);
```

**Return Value:**
- Found: Returns position index (1-based)
- Not found: Returns 0

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    str text = (str)"Hello World";
    uint pos = xrtInStr(text, 0, (str)"World", 0, FALSE);
    printf("Position: %u\n", pos);  // 7
    
    pos = xrtInStr(text, 0, (str)"xyz", 0, FALSE);
    printf("Not found: %u\n", pos);  // 0
    
    xrtUnit();
    return 0;
}
```

**Additional Notes:**
- Internally calls `xrtFindStr`, then returns `xCore.iRet`

---

### xrtCheckStr

Check if string contains specified characters.

**Prototype:**
```c
XXAPI str xrtCheckStr(str sText, size_t iSize, str sSubText, size_t iSubSize);
```

**Parameters:**
- `sText` - String to check
- `iSize` - Length (0 for auto)
- `sSubText` - Character set
- `iSubSize` - Character set length (0 for auto)

**Return Value:**
- Found: Returns position of first matching character
- Not found: Returns `NULL`

**Description:**
- Checks if `sText` contains **any character** from `sSubText`
- Supports UTF-8 multi-byte characters (up to 6 bytes)

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    str text = (str)"Hello123World";
    
    // Check for digits
    str found = xrtCheckStr(text, 0, (str)"0123456789", 0);
    if (found) {
        printf("Found digit: %s\n", found);  // "123World"
    }
    
    // Check for special characters
    found = xrtCheckStr(text, 0, (str)"!@#$%", 0);
    if (!found) {
        printf("No special characters\n");
    }
    
    xrtUnit();
    return 0;
}
```

---

## String Trimming

### xrtLTrim

Left trim.

**Prototype:**
```c
XXAPI str xrtLTrim(str sText, size_t iSize, str sSubText, size_t iSubSize, bool bSrcRevise);
```

**Parameters:**
- `sText` - Source string
- `iSize` - Length (0 for auto)
- `sSubText` - Character set to trim (`NULL` defaults to ` \t\r\n`)
- `iSubSize` - Character set length (0 for auto)
- `bSrcRevise` - Whether to modify source

**Return Value:**
- Trimmed string
- Trimmed length stored in `xCore.iRet`

**Memory Release:**
- `bSrcRevise=FALSE` - ✅ Needs to be released

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    str text = (str)"   Hello World   ";
    
    // Trim left spaces
    str trimmed = xrtLTrim(text, 0, (str)" ", 0, FALSE);
    printf("[%s]\n", trimmed);  // "[Hello World   ]"
    printf("Trimmed length: %" PRId64 "\n", xCore.iRet);
    xrtFree(trimmed);
    
    // Use default character set (space\t\r\n)
    str text2 = (str)"\t\n  Hello";
    str trimmed2 = xrtLTrim(text2, 0, NULL, 0, FALSE);
    printf("[%s]\n", trimmed2);  // "[Hello]"
    xrtFree(trimmed2);
    
    xrtUnit();
    return 0;
}
```

---

### xrtRTrim

Right trim.

**Prototype:**
```c
XXAPI str xrtRTrim(str sText, size_t iSize, str sSubText, size_t iSubSize, bool bSrcRevise);
```

**Parameters:** Same as `xrtLTrim`

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    str text = (str)"   Hello   ";
    str trimmed = xrtRTrim(text, 0, (str)" ", 0, FALSE);
    printf("[%s]\n", trimmed);  // "[   Hello]"
    xrtFree(trimmed);
    
    xrtUnit();
    return 0;
}
```

---

### xrtTrim

Trim both sides.

**Prototype:**
```c
XXAPI str xrtTrim(str sText, size_t iSize, str sSubText, size_t iSubSize, bool bSrcRevise);
```

**Parameters:** Same as `xrtLTrim`

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // Trim whitespace
    str text = (str)"   Hello World   ";
    str trimmed = xrtTrim(text, 0, (str)" \t\r\n", 0, FALSE);
    printf("[%s]\n", trimmed);  // "[Hello World]"
    xrtFree(trimmed);
    
    // Use default character set
    str text2 = (str)"\n\t  Content  \t\n";
    str trimmed2 = xrtTrim(text2, 0, NULL, 0, FALSE);
    printf("[%s]\n", trimmed2);  // "[Content]"
    xrtFree(trimmed2);
    
    // Trim specific characters
    str text3 = (str)"###Title###";
    str trimmed3 = xrtTrim(text3, 0, (str)"#", 0, FALSE);
    printf("[%s]\n", trimmed3);  // "[Title]"
    xrtFree(trimmed3);
    
    xrtUnit();
    return 0;
}
```

**Additional Notes:**
- Supports UTF-8 multi-byte characters
- When `sSubText` is `NULL`, defaults to trimming ` \t\r\n`

---

## String Filtering

### xrtFilterStr

Filter characters.

**Prototype:**
```c
XXAPI str xrtFilterStr(str sText, size_t iSize, str sSubText, size_t iSubSize, bool bSrcRevise);
```

**Parameters:**
- `sText` - Source string
- `iSize` - Length (0 for auto)
- `sSubText` - Character set to filter out
- `iSubSize` - Character set length (0 for auto)
- `bSrcRevise` - Whether to modify source

**Return Value:**
- Filtered string
- Number of filtered characters stored in `xCore.iRet`

**Memory Release:**
- `bSrcRevise=FALSE` - ✅ Needs to be released

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    str text = (str)"Hello123World456";
    
    // Filter digits
    str filtered = xrtFilterStr(text, 0, (str)"0123456789", 0, FALSE);
    printf("%s\n", filtered);  // "HelloWorld"
    printf("Filtered %" PRId64 " characters\n", xCore.iRet);  // 6
    xrtFree(filtered);
    
    // Filter spaces and newlines
    str text2 = (str)"Hello World\nNew Line";
    str filtered2 = xrtFilterStr(text2, 0, (str)" \n", 0, FALSE);
    printf("%s\n", filtered2);  // "HelloWorldNewLine"
    xrtFree(filtered2);
    
    xrtUnit();
    return 0;
}
```

**Additional Notes:**
- Supports UTF-8 multi-byte characters
- When `sSubText` is `NULL`, returns copy of original string directly

---

## String Operations

### xrtFormat

Format string

**Prototype:**
```c
XXAPI str xrtFormat(str sFormat, ...);
```

**Parameters:**
- `sFormat` - Format string (similar to `printf`)
- `...` - Variable arguments

**Return Value:**
- Formatted string

**Memory Release:** ✅ Requires `xrtFree` to release

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // Basic formatting
    str text = xrtFormat((str)"Hello %s, you are %d years old", "Tom", 25);
    printf("%s\n", text);  // "Hello Tom, you are 25 years old"
    xrtFree(text);
    
    // Build path
    str path = xrtFormat((str)"%s\\%s", "C:\\folder", "file.txt");
    printf("%s\n", path);  // "C:\folder\file.txt"
    xrtFree(path);
    
    // Format numbers
    str num = xrtFormat((str)"Value: %.2f, Count: %06d", 3.14159, 42);
    printf("%s\n", num);  // "Value: 3.14, Count: 000042"
    xrtFree(num);
    
    xrtUnit();
    return 0;
}
```

**Additional Notes:**
- Internally uses `vsnprintf`, supports all standard format specifiers
- Result length stored in `xCore.iRet`

---

### xrtReplace

String replacement

**Prototype:**
```c
XXAPI str xrtReplace(str sText, size_t iSize, str sSubText, size_t iSubSize, str sRepText, size_t iRepSize);
```

**Parameters:**
- `sText` - Source string
- `sSubText` - Substring to replace
- `sRepText` - Replacement string

**Return Value:**
- New string after replacement

**Memory Release:** ✅ Requires `xrtFree` to release

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // Basic replacement
    str replaced = xrtReplace((str)"Hello World", 0, (str)"World", 0, (str)"XRT", 0);
    printf("%s\n", replaced);  // "Hello XRT"
    xrtFree(replaced);
    
    // Replace all (all matches)
    str result = xrtReplace((str)"a b a b a", 0, (str)"a", 0, (str)"c", 0);
    printf("%s\n", result);  // "c b c b c"
    xrtFree(result);
    
    // Delete substring (replace with empty)
    str deleted = xrtReplace((str)"Hello World", 0, (str)"World", 0, (str)"", 0);
    printf("[%s]\n", deleted);  // "[Hello ]"
    xrtFree(deleted);
    
    // Replace with different length substring
    str longer = xrtReplace((str)"AB", 0, (str)"B", 0, (str)"XYZ", 0);
    printf("%s (length: %" PRId64 ")\n", longer, xCore.iRet);  // "AXYZ (length: 4)"
    xrtFree(longer);
    
    xrtUnit();
    return 0;
}
```

**Additional Notes:**
- Returned string length stored in `xCore.iRet`
- Replaces **all** matching substrings, not just the first one

---

### xrtSplit

Split string

**Prototype:**
```c
XXAPI str* xrtSplit(str sText, size_t iSize, str sSepText, size_t iSepSize, bool bSrcRevise);
```

**Parameters:**
- `sText` - Source string
- `sSepText` - Separator
- `bSrcRevise` - Whether to modify source
  - `TRUE` - Modify source directly (insert `\0` in original string)
  - `FALSE` - Create new strings

**Return Value:**
- Array of string pointers (NULL-terminated)

**Memory Release:** ✅ **Always requires** `xrtFree` to release array

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // Split and modify source string (bSrcRevise=TRUE)
    str text1 = xrtCopyStr((str)"apple,banana,orange", 0);
    str* parts1 = xrtSplit(text1, 0, (str)",", 0, TRUE);
    printf("Split count: %" PRId64 "\n", xCore.iRet);  // 3
    for (int i = 0; parts1[i] != NULL; i++) {
        printf("%d: %s\n", i, parts1[i]);
    }
    xrtFree(parts1);  // Release array
    xrtFree(text1);   // Release source string
    
    printf("---\n");
    
    // Split without modifying source (bSrcRevise=FALSE)
    str text2 = (str)"one|two|three";
    str* parts2 = xrtSplit(text2, 0, (str)"|", 0, FALSE);
    for (int i = 0; parts2[i] != NULL; i++) {
        printf("%d: %s\n", i, parts2[i]);
    }
    xrtFree(parts2);  // Only need to release array, strings are in array memory
    
    printf("---\n");
    
    // Multi-character separator
    str csv = xrtCopyStr((str)"a::b::c", 0);
    str* parts3 = xrtSplit(csv, 0, (str)"::", 0, TRUE);
    for (int i = 0; parts3[i] != NULL; i++) {
        printf("%d: [%s]\n", i, parts3[i]);
    }
    xrtFree(parts3);
    xrtFree(csv);
    
    xrtUnit();
    return 0;
}
```

**Additional Notes:**
- Split count stored in `xCore.iRet`
- Returned array is NULL-terminated
- When `bSrcRevise=TRUE`, `\0` is inserted into source string, destroying original data
- When `bSrcRevise=FALSE`, string data and array are in same memory block, only need one release

---

### xrtRandStr

Generate random string

**Prototype:**
```c
XXAPI str xrtRandStr(str sTemplate, size_t iSize, size_t iLen);
```

**Parameters:**
- `sTemplate` - Character template (randomly select from)
- `iSize` - Template length (0 for auto)
- `iLen` - Generation length

**Return Value:**
- Random string

**Memory Release:** ✅ Requires `xrtFree` to release

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // Generate 8-digit numeric PIN
    str pin = xrtRandStr((str)"0123456789", 0, 8);
    printf("PIN: %s\n", pin);  // e.g., "57382910"
    xrtFree(pin);
    
    // Generate 16-character mixed password
    str password = xrtRandStr(
        (str)"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789",
        0, 16
    );
    printf("Password: %s\n", password);
    xrtFree(password);
    
    // Use default template (NULL)
    str token = xrtRandStr(NULL, 0, 32);
    printf("Token: %s\n", token);  // Contains digits+letters+-_
    xrtFree(token);
    
    // Generate hexadecimal string
    str hex = xrtRandStr((str)"0123456789ABCDEF", 0, 8);
    printf("Hex: %s\n", hex);  // e.g., "3F7A9C2E"
    xrtFree(hex);
    
    xrtUnit();
    return 0;
}
```

**Additional Notes:**
- When `sTemplate` is `NULL`, uses default template (64 characters: digits+uppercase+lowercase+-_)
- Internally uses `xrtRandRange` to generate random indices

---

## Encoding and Decoding

### xrtHexEncode

HEX encoding

**Prototype:**
```c
XXAPI str xrtHexEncode(ptr pMem, size_t iSize);
```

**Parameters:**
- `pMem` - Binary data
- `iSize` - Data length

**Return Value:**
- HEX string (uppercase)

**Memory Release:** ✅ Requires `xrtFree` to release

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    uint8 data[] = {0x12, 0x34, 0x56, 0xAB, 0xCD};
    str hex = xrtHexEncode(data, sizeof(data));
    
    printf("%s\n", hex);  // "123456ABCD"
    printf("Encoded length: %" PRId64 "\n", xCore.iRet);  // 10
    
    xrtFree(hex);
    xrtUnit();
    return 0;
}
```

**Additional Notes:**
- Output is uppercase hexadecimal characters
- Encoded length stored in `xCore.iRet`, equals input length × 2

---

### xrtHexDecode

HEX decoding

**Prototype:**
```c
XXAPI ptr xrtHexDecode(str pText, size_t iSize);
```

**Parameters:**
- `pText` - HEX string
- `iSize` - String length (0 for auto)

**Return Value:**
- Binary data
- Decoded length stored in `xCore.iRet`

**Memory Release:** ✅ Requires `xrtFree` to release

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // Decode HEX of "Hello"
    str hex = (str)"48656C6C6F";
    ptr data = xrtHexDecode(hex, 0);
    size_t len = xCore.iRet;
    
    printf("Decoded: %.*s\n", (int)len, (char*)data);  // "Hello"
    printf("Decoded length: %zu\n", len);  // 5
    
    xrtFree(data);
    
    // Lowercase hex also supported
    str hex2 = (str)"576f726c64";  // "World"
    ptr data2 = xrtHexDecode(hex2, 0);
    printf("Decoded: %.*s\n", (int)xCore.iRet, (char*)data2);
    xrtFree(data2);
    
    xrtUnit();
    return 0;
}
```

**Additional Notes:**
- Supports both uppercase and lowercase hexadecimal characters
- Decoded length stored in `xCore.iRet`, equals input length / 2

---

### xrtBase64Encode

Base64 encoding

**Prototype:**
```c
XXAPI str xrtBase64Encode(ptr pMem, size_t iSize, str sTable);
```

**Parameters:**
- `pMem` - Binary data
- `iSize` - Data length
- `sTable` - Encoding table (`NULL` for standard table)

**Return Value:**
- Base64 string

**Memory Release:** ✅ Requires `xrtFree` to release

**Example:**
```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

int main() {
    xrtInit();
    
    // Basic encoding
    str text = (str)"Hello World";
    str b64 = xrtBase64Encode(text, strlen((char*)text), NULL);
    printf("Encoded: %s\n", b64);  // "SGVsbG8gV29ybGQ="
    printf("Encoded length: %" PRId64 "\n", xCore.iRet);  // 16
    xrtFree(b64);
    
    // Encode binary data
    uint8 binary[] = {0x00, 0x01, 0x02, 0xFF};
    str b64_bin = xrtBase64Encode(binary, sizeof(binary), NULL);
    printf("Binary encoded: %s\n", b64_bin);  // "AAEC/w=="
    xrtFree(b64_bin);
    
    xrtUnit();
    return 0;
}
```

**Additional Notes:**
- Encoded length stored in `xCore.iRet`
- Automatically adds padding character `=`

---

### xrtBase64Decode

Base64 decoding

**Prototype:**
```c
XXAPI ptr xrtBase64Decode(str sText, size_t iSize, str sTable);
```

**Parameters:**
- `sText` - Base64 string
- `iSize` - Length (0 for auto)
- `sTable` - Decoding table (`NULL` for standard table)

**Return Value:**
- Binary data
- Length stored in `xCore.iRet`

**Memory Release:** ✅ Requires `xrtFree` to release

**Example:**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // Basic decoding
    str b64 = (str)"SGVsbG8gV29ybGQ=";
    ptr data = xrtBase64Decode(b64, 0, NULL);
    size_t len = xCore.iRet;
    
    printf("Decoded: %.*s\n", (int)len, (char*)data);  // "Hello World"
    printf("Decoded length: %zu\n", len);  // 11
    xrtFree(data);
    
    // Decode binary data
    str b64_bin = (str)"AAEC/w==";
    ptr binary = xrtBase64Decode(b64_bin, 0, NULL);
    uint8* bytes = (uint8*)binary;
    printf("Binary: ");
    for (size_t i = 0; i < (size_t)xCore.iRet; i++) {
        printf("%02X ", bytes[i]);
    }
    printf("\n");  // "00 01 02 FF"
    xrtFree(binary);
    
    xrtUnit();
    return 0;
}
```

**Additional Notes:**
- Input length must be multiple of 4, otherwise returns error
- Returns error and sets `xCore.LastError` when containing illegal characters
- Decoded length stored in `xCore.iRet`

---

## Use Cases

### 1. Text Processing

```c
#include "xrt.h"
#include <stdio.h>

// Clean user input
str CleanInput(str input) {
    // Trim whitespace
    str trimmed = xrtTrim(input, 0, (str)" \t\r\n", 0, FALSE);
    // Convert to lowercase
    str lower = xrtLCase(trimmed, 0, TRUE);
    // trimmed has been modified in-place, return directly
    return lower;
}

int main() {
    xrtInit();
    
    str input = (str)"   Hello WORLD   ";
    str clean = CleanInput(input);
    printf("Cleaned: [%s]\n", clean);  // "[hello world]"
    xrtFree(clean);
    
    xrtUnit();
    return 0;
}
```

---

### 2. Config Parsing

```c
#include "xrt.h"
#include <stdio.h>

// Parse key=value
void ParseConfig(str line) {
    str* parts = xrtSplit(line, 0, (str)"=", 0, FALSE);
    if (parts[0] && parts[1]) {
        str key = xrtTrim(parts[0], 0, (str)" ", 0, FALSE);
        str value = xrtTrim(parts[1], 0, (str)" ", 0, FALSE);
        printf("%s = %s\n", key, value);
        xrtFree(key);
        xrtFree(value);
    }
    xrtFree(parts);
}

int main() {
    xrtInit();
    
    ParseConfig((str)"  name = XRT Library  ");
    ParseConfig((str)"version=1.0.0");
    ParseConfig((str)"  path = C:\\Program Files  ");
    
    xrtUnit();
    return 0;
}
```

---

### 3. URL Encoding

```c
#include "xrt.h"
#include <stdio.h>

str UrlEncode(str text) {
    // Simplified: replace common special characters
    str step1 = xrtReplace(text, 0, (str)" ", 0, (str)"%20", 0);
    str step2 = xrtReplace(step1, 0, (str)"&", 0, (str)"%26", 0);
    str step3 = xrtReplace(step2, 0, (str)"=", 0, (str)"%3D", 0);
    xrtFree(step1);
    xrtFree(step2);
    return step3;
}

int main() {
    xrtInit();
    
    str url = (str)"name=Hello World&value=1+2=3";
    str encoded = UrlEncode(url);
    printf("Encoded: %s\n", encoded);
    xrtFree(encoded);
    
    xrtUnit();
    return 0;
}
```

---

## Best Practices

### 1. Memory Management

```c
#include "xrt.h"
#include <stdio.h>

void UseString(str s) {
    printf("Using: %s\n", s);
}

int main() {
    xrtInit();
    
    // ✓ Correct: release promptly
    str lower = xrtLCase((str)"TEXT", 0, FALSE);
    UseString(lower);
    xrtFree(lower);
    
    // ✗ Memory leak example (don't do this)
    // str leaked = xrtLCase((str)"TEXT", 0, FALSE);
    // Forgot to release...
    
    xrtUnit();
    return 0;
}
```

---

### 2. In-place Modification

```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

int main() {
    xrtInit();
    
    // When source data can be modified, in-place modification is more efficient
    str text = xrtMalloc(100);
    strcpy((char*)text, "HELLO WORLD");
    
    printf("Before: %s\n", text);
    xrtLCase(text, 0, TRUE);  // In-place modification
    printf("After: %s\n", text);  // "hello world"
    
    xrtFree(text);
    xrtUnit();
    return 0;
}
```

---

## Related Documents

- [Charset Conversion](api-charset.en.md)
- [Path Processing](api-path.en.md)
- [Main Index](README.en.md)

---

<div align="center">

[⬆️ Back to Top](#string-processing-library)

</div>
