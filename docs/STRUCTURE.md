# SafeImg v2.0 - Project Structure

## Overview

SafeImg is organized as a **clean C/C++ shared library** with a minimal public API and well-organized internal modules.

---

## Directory Layout

```
scanimage/
├── include/                    # Public API headers
│   ├── safeimg_export.h       # C API (ONLY public header)
│   └── internal/              # Internal headers (implementation details)
│       ├── config.h
│       ├── scan_result_v2.h
│       ├── scanner_v2.h
│       ├── file_io.h
│       ├── entropy_analyzer.h
│       ├── base_validator.h
│       ├── exif_parser.h
│       ├── xmp_parser.h
│       ├── signature_engine_v2.h
│       └── false_positive_reducer.h
│
├── src/                       # Implementation
│   ├── api/                   # C API wrapper
│   │   └── safeimg_export.cpp
│   │
│   ├── core/                  # Main scanning engine
│   │   ├── scanner_v2.cpp     # Core scanner
│   │   ├── scan_result_v2.cpp # Result structures
│   │   ├── image_scanner.cpp  # Legacy scanner
│   │   ├── file_io.cpp        # File I/O (with mmap)
│   │   ├── entropy_analyzer.cpp
│   │   ├── exif_parser.cpp    # EXIF metadata parser
│   │   ├── xmp_parser.cpp     # XMP metadata parser
│   │   └── validators/        # Format validators
│   │       ├── jpeg_validator.cpp
│   │       ├── png_validator.cpp
│   │       ├── webp_validator.cpp
│   │       ├── gif_validator.cpp
│   │       ├── bmp_validator.cpp
│   │       ├── tiff_validator.cpp
│   │       └── svg_validator.cpp
│   │
│   └── signatures/            # Signature matching system
│       ├── signature_engine_v2.cpp
│       └── false_positive_reducer.cpp
│
├── docs/                      # Documentation
│   ├── API.md                 # C API reference
│   ├── BUILD.md               # Build instructions
│   ├── NODE_FFI.md            # Node.js integration
│   ├── PYTHON.md              # Python integration
│   ├── GO.md                  # Go integration
│   ├── RUST.md                # Rust integration
│   └── STRUCTURE.md           # This file
│
├── test/                      # Test programs
│   ├── test_c_api.c           # C API tests
│   ├── test_signature_v2.cpp  # Signature engine tests
│   └── assets/                # Test images
│
├── dist/                      # Build output
│   └── lib/                   # Shared libraries
│       ├── libsafeimg.so      # Linux
│       ├── libsafeimg.dylib   # macOS
│       └── safeimg.dll        # Windows
│
├── signatures_v2.json         # Signature database
├── CMakeLists.txt             # Build configuration
├── LICENSE                    # MIT License
└── README.md                  # Project overview
```

---

## Module Responsibilities

### Public API (`include/safeimg_export.h`)

**Purpose**: Language-independent C interface

**Functions**:
- `safeimg_scan_file()` - Scan image file
- `safeimg_scan_buffer()` - Scan memory buffer
- `safeimg_match_signatures()` - Pattern matching
- `safeimg_free()` - Memory management
- `safeimg_version()` - Version info
- `safeimg_supports_format()` - Format check

**Design Principles**:
- Pure C interface (`extern "C"`)
- JSON input/output for portability
- Manual memory management (malloc/free)
- Thread-safe (no global state)

---

### Core Engine (`src/core/`)

**scanner_v2.cpp**  
High-level scanning coordinator. Orchestrates:
- Format detection
- Validator selection
- Metadata extraction
- Signature matching
- Result aggregation

**file_io.cpp**  
Platform-specific file I/O:
- Linux/macOS: `mmap()` for large files
- Windows: `CreateFileMapping()`
- Fallback: `fread()`
- Performance: Zero-copy when possible

**entropy_analyzer.cpp**  
Shannon entropy calculation:
- Sliding-window analysis
- Detects encrypted/compressed payloads
- Threshold: 7.5 bits/byte

**exif_parser.cpp**  
EXIF metadata parser:
- TIFF IFD navigation
- GPS coordinate extraction
- Privacy warnings
- Malformed data detection

**xmp_parser.cpp**  
XMP metadata parser:
- Format-aware extraction (JPEG/PNG/WEBP)
- XML validation
- Script tag detection
- Base64 payload analysis

---

### Validators (`src/core/validators/`)

Each validator implements `BaseValidator` interface:

```cpp
bool validate(const vector<uint8_t>& data, ScanResultV2& result, const ScanOptions& options);
MetadataResult extractMetadata(const vector<uint8_t>& data, const ScanOptions& options);
string getFormatName() const;
```

**Supported Formats**:
- **JPEG**: SOI/EOI markers, APP segments, EXIF/XMP
- **PNG**: Signature, IHDR/IEND chunks, CRC validation
- **WEBP**: RIFF container, VP8/VP8L chunks
- **GIF**: Header, blocks, transparency
- **BMP**: DIB header, compression
- **TIFF**: IFD validation, multi-image
- **SVG**: XML parsing, XSS detection

---

### Signature System (`src/signatures/`)

**signature_engine_v2.cpp**  
Advanced pattern matching:
- Wildcard support (`?`, `*`, `[range]`, `{one-of}`)
- Context-aware scanning
- Severity levels (info, warning, high, critical)
- Whitelist management

**false_positive_reducer.cpp**  
Intelligent confidence scoring:
- GPS from cameras (low threat)
- Base64 thumbnails (safe)
- Adobe XMP namespaces (whitelisted)
- Confidence threshold: 0.7

---

## Data Flow

```
┌─────────────────┐
│   User Input    │
│  (filepath or   │
│    buffer)      │
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│   C API Layer   │  safeimg_scan_file()
│ safeimg_export  │  (JSON options → ScanOptions)
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│  ScannerV2      │  Detect format, select validator
└────────┬────────┘
         │
         ├─────────────────────────────┐
         │                             │
         ▼                             ▼
┌─────────────────┐          ┌────────────────┐
│   Validator     │          │  File I/O      │
│  (JPEG/PNG/etc) │          │  (mmap)        │
└────────┬────────┘          └────────────────┘
         │
         ├──────────┬──────────┬────────────┐
         │          │          │            │
         ▼          ▼          ▼            ▼
    ┌────────┐ ┌────────┐ ┌─────────┐ ┌─────────┐
    │  EXIF  │ │  XMP   │ │Signature│ │ Entropy │
    │ Parser │ │ Parser │ │ Engine  │ │Analyzer │
    └────────┘ └────────┘ └─────────┘ └─────────┘
         │          │          │            │
         └──────────┴──────────┴────────────┘
                    │
                    ▼
            ┌───────────────┐
            │ FalsePositive │  Adjust confidence
            │   Reducer     │
            └───────┬───────┘
                    │
                    ▼
            ┌───────────────┐
            │ ScanResultV2  │  Issues, metadata, etc.
            └───────┬───────┘
                    │
                    ▼
            ┌───────────────┐
            │  JSON Output  │
            └───────────────┘
```

---

## Build System (CMake)

### Key Variables

- `CMAKE_CXX_STANDARD` = 17
- `CMAKE_LIBRARY_OUTPUT_DIRECTORY` = `dist/lib`
- `SAFEIMG_EXPORTS` (Windows) = DLL export symbol

### Targets

- `safeimg` (shared library)
- Install rules for system-wide deployment

### Platform Handling

```cmake
if(WIN32)
    # Windows: safeimg.dll
elseif(APPLE)
    # macOS: libsafeimg.dylib
else()
    # Linux: libsafeimg.so
endif()
```

---

## Naming Conventions

### Files
- Headers: `snake_case.h`
- Sources: `snake_case.cpp`
- Tests: `test_*.c` or `test_*.cpp`

### Classes
- `PascalCase` (e.g., `ScannerV2`, `EXIFParser`)

### Functions
- C API: `snake_case` (e.g., `safeimg_scan_file`)
- C++ methods: `camelCase` (e.g., `extractMetadata`)

### Constants
- `SCREAMING_SNAKE_CASE` (e.g., `MAX_FILE_SIZE`)

---

## Version Scheme

SafeImg follows **Semantic Versioning** (semver):

- **2.0.0** = Current version
- **Major** (2) = Breaking API changes
- **Minor** (0) = New features, backward compatible
- **Patch** (0) = Bug fixes

Library versioning:
- `libsafeimg.so.2.0.0` (full version)
- `libsafeimg.so.2` (soversion)
- `libsafeimg.so` (symlink)

---

## Design Principles

1. **Clean API**: Only 6 C functions exposed
2. **No globals**: Thread-safe by design
3. **JSON I/O**: Language-independent data exchange
4. **Zero dependencies**: Pure C++17, no external libs (except JSON parser in future)
5. **Platform-agnostic**: Works on Linux, macOS, Windows
6. **Memory safety**: Explicit `safeimg_free()` for all allocations

---

## Future Extensions

Planned additions (maintain backward compatibility):

1. **JSON Library**: Integrate nlohmann/json for signature loading
2. **Async API**: `safeimg_scan_file_async()` with callbacks
3. **Streaming**: `safeimg_scan_stream()` for large files
4. **Plugins**: Dynamic signature modules
5. **CLI Tool**: `safeimg` command-line utility

---

## See Also

- **API.md** - C API documentation
- **BUILD.md** - Build instructions
- **README.md** - Project overview
