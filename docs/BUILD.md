# SafeImg v2.0 - Build Instructions

## Prerequisites

- **CMake** 3.10 or higher
- **C++17 compatible compiler**:
  - GCC 7+ (Linux)
  - Clang 5+ (macOS)
  - MSVC 2017+ (Windows)
- **Make** (Linux/macOS) or **MSBuild** (Windows)

---

## Building on Linux

### 1. Install Dependencies

```bash
sudo apt update
sudo apt install cmake g++ make
```

### 2. Build the Library

```bash
cd /path/to/scanimage
mkdir build && cd build
cmake ..
make -j$(nproc)
```

### 3. Output

```
dist/lib/libsafeimg.so.2.0.0
dist/lib/libsafeimg.so.2 -> libsafeimg.so.2.0.0
dist/lib/libsafeimg.so -> libsafeimg.so.2
```

### 4. Install System-Wide (Optional)

```bash
sudo make install
# Installs to /usr/local/lib and /usr/local/include
```

---

## Building on macOS

### 1. Install Dependencies

```bash
# Install Xcode Command Line Tools
xcode-select --install

# Install CMake (via Homebrew)
brew install cmake
```

### 2. Build the Library

```bash
cd /path/to/scanimage
mkdir build && cd build
cmake ..
make -j$(sysctl -n hw.ncpu)
```

### 3. Output

```
dist/lib/libsafeimg.2.0.0.dylib
dist/lib/libsafeimg.2.dylib -> libsafeimg.2.0.0.dylib
dist/lib/libsafeimg.dylib -> libsafeimg.2.dylib
```

---

## Building on Windows

### 1. Install Dependencies

- Install **Visual Studio 2019+** with C++ tools
- Install **CMake** from https://cmake.org/download/

### 2. Build the Library

```cmd
cd C:\\path\\to\\scanimage
mkdir build
cd build
cmake ..
cmake --build . --config Release
```

### 3. Output

```
dist\\lib\\safeimg.dll
dist\\lib\\safeimg.lib (import library)
```

---

## CMake Options

### Custom Install Prefix

```bash
cmake -DCMAKE_INSTALL_PREFIX=/custom/path ..
make install
```

### Debug Build

```bash
cmake -DCMAKE_BUILD_TYPE=Debug ..
make
```

### Specify Compiler

```bash
cmake -DCMAKE_CXX_COMPILER=clang++ ..
```

---

## Verify Build

### Check Library Size

```bash
ls -lh dist/lib/libsafeimg.so*
# Expected: ~1.8MB
```

### Check Symbol Exports

```bash
nm -D dist/lib/libsafeimg.so | grep safeimg
```

Expected output:
```
safeimg_free
safeimg_match_signatures
safeimg_scan_buffer
safeimg_scan_file
safeimg_supports_format
safeimg_version
```

---

## Testing

### Build Test Program

```bash
cd test
gcc -o test_c_api test_c_api.c -I../include -L../dist/lib -lsafeimg
```

### Run Tests

```bash
# Linux
LD_LIBRARY_PATH=../dist/lib ./test_c_api

# macOS
DYLD_LIBRARY_PATH=../dist/lib ./test_c_api

# Windows
set PATH=%PATH%;..\\dist\\lib
test_c_api.exe
```

---

## Troubleshooting

### "cannot find -lsafeimg"

Make sure library was built:
```bash
ls dist/lib/libsafeimg.so
```

### "error while loading shared libraries"

Set library path:
```bash
export LD_LIBRARY_PATH=/path/to/scanimage/dist/lib:$LD_LIBRARY_PATH
```

Or install system-wide:
```bash
sudo make install
sudo ldconfig
```

### Windows DLL not found

Copy `safeimg.dll` to same directory as your executable, or add `dist\\lib` to PATH.

---

## Clean Build

```bash
rm -rf build dist
mkdir build && cd build
cmake ..
make
```

---

## Next Steps

After building, see:
- **API.md** - How to use the C API
- **PYTHON.md** - Python integration via ctypes
- **NODE_FFI.md** - Node.js integration via ffi-napi
- **GO.md** - Go integration via cgo
