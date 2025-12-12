# SafeImg v2.0 - Shared Library API Documentation

## Overview

SafeImg is a portable, language-independent image security scanning library written in C++17 with a clean C API. It can detect malicious content, polyglot files, embedded scripts, and privacy-leaking metadata in images.

**Supported formats**: JPEG, PNG, WEBP, GIF, BMP, TIFF, SVG

---

## Building the Library

### Prerequisites

- CMake 3.10+
- C++17 compatible compiler (GCC 7+, Clang 5+, MSVC 2017+)
- Make (Linux/macOS) or MSBuild (Windows)

### Linux / macOS

```bash
mkdir build
cd build
cmake ..
make
```

Output: `dist/lib/libsafeimg.so` (Linux) or `dist/lib/libsafeimg.dylib` (macOS)

### Windows

```cmd
mkdir build
cd build
cmake ..
cmake --build . --config Release
```

Output: `dist\lib\safeimg.dll`

---

## API Reference

### Core Functions

#### `safeimg_scan_file()`

Scan an image file for security threats.

```c
const char* safeimg_scan_file(const char* filepath, const char* optionsJson);
```

**Parameters**:
- `filepath`: Path to image file
- `optionsJson`: JSON configuration (or `NULL` for defaults)

**Returns**: JSON string (must be freed with `safeimg_free()`)

**Example**:
```c
const char* result = safeimg_scan_file("photo.jpg", NULL);
printf("%s\n", result);
safeimg_free(result);
```

---

#### `safeimg_scan_buffer()`

Scan image data from memory.

```c
const char* safeimg_scan_buffer(const uint8_t* data, size_t size, 
                                 const char* optionsJson);
```

**Parameters**:
- `data`: Pointer to image data
- `size`: Size in bytes
- `optionsJson`: JSON configuration (or `NULL`)

**Returns**: JSON string (must be freed with `safeimg_free()`)

**Example**:
```c
uint8_t* imageData = ...; // Your image data
size_t imageSize = ...;

const char* result = safeimg_scan_buffer(imageData, imageSize, NULL);
printf("%s\n", result);
safeimg_free(result);
```

---

#### `safeimg_match_signatures()`

Match signature patterns against data (advanced).

```c
const char* safeimg_match_signatures(const uint8_t* data, size_t size,
                                      const char* contextJson);
```

**Parameters**:
- `data`: Data to scan
- `size`: Size in bytes
- `contextJson`: Context and options

**Returns**: JSON array of matches (must be freed with `safeimg_free()`)

---

#### `safeimg_free()`

Free memory allocated by SafeImg functions.

```c
void safeimg_free(const char* ptr);
```

**IMPORTANT**: All strings returned by SafeImg **must** be freed with this function.

---

#### `safeimg_version()`

Get library version.

```c
const char* safeimg_version(void);
```

**Returns**: Version string (e.g., "2.0.0") - **DO NOT FREE**

---

#### `safeimg_supports_format()`

Check if a format is supported.

```c
int safeimg_supports_format(const char* format);
```

**Returns**: `1` if supported, `0` if not

**Example**:
```c
if (safeimg_supports_format("jpeg")) {
    printf("JPEG is supported\n");
}
```

---

## JSON Schemas

### Options JSON (Input)

```json
{
  "strictMetadata": true,
  "allowGPS": false,
  "extractMetadata": true,
  "checkPolyglot": true,
  "checkEntropy": true,
  "checkIntegrity": true,
  "checkSignatures": true,
  "reduceFalsePositives": true,
  "confidenceThreshold": 0.7,
  "signatureFile": "signatures_v2.json"
}
```

All fields are optional. Defaults are used if omitted.

---

### Result JSON (Output)

```json
{
  "isSafe": false,
  "format": "jpeg",
  "issues": [
    {
      "type": "gps_detected",
      "description": "GPS coordinates found",
      "severity": "warning",
      "category": "metadata",
      "offset": 1234,
      "length": 56
    }
  ],
  "metadata": {
    "hasEXIF": true,
    "hasXMP": false,
    "hasGPS": true,
    "exifTags": {
      "Make": "Canon",
      "Model": "EOS 5D"
    },
    "metadataSizeBytes": 2048
  },
  "realImageSize": 102400,
  "extraDataSize": 0,
  "scanTimeMs": 12.5,
  "version": "2.0.0"
}
```

---

### Error JSON

```json
{
  "error": "File not found",
  "code": "ENOENT"
}
```

---

## Usage Examples

### C Example

```c
#include "safeimg_export.h"
#include <stdio.h>

int main() {
    // Check version
    printf("SafeImg version: %s\n", safeimg_version());
    
    // Scan a file
    const char* result = safeimg_scan_file("photo.jpg", NULL);
    
    // Parse result (use a JSON library)
    printf("Result: %s\n", result);
    
    // Clean up
    safeimg_free(result);
    
    return 0;
}
```

**Compile**:
```bash
gcc -o scan scan.c -I./include -L./dist/lib -lsafeimg
LD_LIBRARY_PATH=./dist/lib ./scan
```

---

### C++ Example

```cpp
#include "safeimg_export.h"
#include <iostream>
#include <memory>

// RAII wrapper for automatic cleanup
class SafeImgResult {
    const char* ptr_;
public:
    SafeImgResult(const char* p) : ptr_(p) {}
    ~SafeImgResult() { safeimg_free(ptr_); }
    const char* get() const { return ptr_; }
};

int main() {
    SafeImgResult result(safeimg_scan_file("photo.jpg", nullptr));
    std::cout << result.get() << std::endl;
    return 0;
}
```

**Compile**:
```bash
g++ -std=c++17 -o scan scan.cpp -I./include -L./dist/lib -lsafeimg
LD_LIBRARY_PATH=./dist/lib ./scan
```

---

### Python Example (ctypes)

```python
from ctypes import *
import json

# Load library
lib = CDLL("./dist/lib/libsafeimg.so")

# Configure function signatures
lib.safeimg_scan_file.restype = c_char_p
lib.safeimg_scan_file.argtypes = [c_char_p, c_char_p]
lib.safeimg_free.argtypes = [c_char_p]

# Scan file
result = lib.safeimg_scan_file(b"photo.jpg", None)
data = json.loads(result.decode('utf-8'))

print(f"Is safe: {data['isSafe']}")
print(f"Format: {data['format']}")

# Free memory
lib.safeimg_free(result)
```

---

### Go Example (cgo)

```go
package main

/*
#cgo LDFLAGS: -L./dist/lib -lsafeimg
#include "include/safeimg_export.h"
#include <stdlib.h>
*/
import "C"
import (
    "fmt"
    "unsafe"
)

func main() {
    filepath := C.CString("photo.jpg")
    defer C.free(unsafe.Pointer(filepath))
    
    result := C.safeimg_scan_file(filepath, nil)
    defer C.safeimg_free(result)
    
    fmt.Println(C.GoString(result))
}
```

**Run**:
```bash
export LD_LIBRARY_PATH=./dist/lib
go run main.go
```

---

## Memory Management Rules

1. **Always free returned strings**: Use `safeimg_free()` on all results
2. **Never free `safeimg_version()`**: It returns a static string
3. **Thread safety**: Create new scanner instance per call (no global state)
4. **NULL safety**: All functions handle NULL inputs gracefully

---

## Building Your Application

### Linux

```bash
gcc -o myapp myapp.c -I./include -L./dist/lib -lsafeimg
export LD_LIBRARY_PATH=./dist/lib
./myapp
```

### macOS

```bash
gcc -o myapp myapp.c -I./include -L./dist/lib -lsafeimg
export DYLD_LIBRARY_PATH=./dist/lib
./myapp
```

### Windows

```cmd
cl.exe myapp.c /I.\include safeimg.lib /link /LIBPATH:.\dist\lib
set PATH=%PATH%;.\dist\lib
myapp.exe
```

---

## Error Handling

Always check for error JSON:

```c
const char* result = safeimg_scan_file("photo.jpg", NULL);

// Simple error check (look for "error" field)
if (strstr(result, "\"error\"")) {
    fprintf(stderr, "Error: %s\n", result);
}

safeimg_free(result);
```

---

## Performance

- **Typical scan time**: 5-15ms per image
- **Thread-safe**: Can scan multiple images in parallel
- **Memory efficient**: Streaming parsers, no full file load
- **Max file size**: 100MB (configurable)

---

## Platform Support

- ✅ Linux (glibc 2.17+)
- ✅ macOS (10.15+)
- ✅ Windows (10+)
- ✅ x86_64 architecture
- ✅ ARM64 (Linux, macOS)

---

## License

MIT License - See LICENSE file for details.
