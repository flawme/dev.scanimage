# SafeImg v2.0 - Python Integration

## Overview

Use SafeImg shared library from Python via **ctypes** (standard library, no dependencies).

---

## Basic Usage

```python
import ctypes
import json
import platform
from pathlib import Path

# Determine library path
system = platform.system()
lib_names = {
    'Linux': 'libsafeimg.so',
    'Darwin': 'libsafeimg.dylib',
    'Windows': 'safeimg.dll'
}

lib_path = Path(__file__).parent / 'dist' / 'lib' / lib_names[system]
safeimg = ctypes.CDLL(str(lib_path))

# Configure function signatures
safeimg.safeimg_version.restype = ctypes.c_char_p
safeimg.safeimg_version.argtypes = []

safeimg.safeimg_supports_format.restype = ctypes.c_int
safeimg.safeimg_supports_format.argtypes = [ctypes.c_char_p]

safeimg.safeimg_scan_file.restype = ctypes.c_char_p
safeimg.safeimg_scan_file.argtypes = [ctypes.c_char_p, ctypes.c_char_p]

safeimg.safeimg_scan_buffer.restype = ctypes.c_char_p
safeimg.safeimg_scan_buffer.argtypes = [
    ctypes.POINTER(ctypes.c_uint8),
    ctypes.c_size_t,
    ctypes.c_char_p
]

safeimg.safeimg_free.argtypes = [ctypes.c_char_p]

# Simple scan
version = safeimg.safeimg_version().decode('utf-8')
print(f"SafeImg version: {version}")

result_ptr = safeimg.safeimg_scan_file(b"photo.jpg", None)
result_str = ctypes.string_at(result_ptr).decode('utf-8')
safeimg.safeimg_free(result_ptr)

result = json.loads(result_str)
print(f"Is safe: {result['isSafe']}")
```

---

## Python Wrapper Class

### safeimg.py

```python
import ctypes
import json
import platform
from pathlib import Path
from typing import Optional, Dict, List, Any

class SafeImgError(Exception):
    """SafeImg library error"""
    pass

class SafeImg:
    """SafeImg v2.0 Python wrapper"""
    
    def __init__(self, lib_path: Optional[Path] = None):
        """Initialize SafeImg library
        
        Args:
            lib_path: Path to shared library (auto-detected if None)
        """
        if lib_path is None:
            lib_path = self._find_library()
        
        self._lib = ctypes.CDLL(str(lib_path))
        self._configure_functions()
    
    @staticmethod
    def _find_library() -> Path:
        """Auto-detect library path"""
        system = platform.system()
        lib_names = {
            'Linux': 'libsafeimg.so',
            'Darwin': 'libsafeimg.dylib',
            'Windows': 'safeimg.dll'
        }
        
        if system not in lib_names:
            raise SafeImgError(f"Unsupported platform: {system}")
        
        # Try common locations
        possible_paths = [
            Path(__file__).parent / 'dist' / 'lib' / lib_names[system],
            Path('/usr/local/lib') / lib_names[system],
            Path('/usr/lib') / lib_names[system],
        ]
        
        for path in possible_paths:
            if path.exists():
                return path
        
        raise SafeImgError(f"Library not found. Searched: {possible_paths}")
    
    def _configure_functions(self):
        """Configure ctypes function signatures"""
        # safeimg_version
        self._lib.safeimg_version.restype = ctypes.c_char_p
        self._lib.safeimg_version.argtypes = []
        
        # safeimg_supports_format
        self._lib.safeimg_supports_format.restype = ctypes.c_int
        self._lib.safeimg_supports_format.argtypes = [ctypes.c_char_p]
        
        # safeimg_scan_file
        self._lib.safeimg_scan_file.restype = ctypes.c_char_p
        self._lib.safeimg_scan_file.argtypes = [ctypes.c_char_p, ctypes.c_char_p]
        
        # safeimg_scan_buffer
        self._lib.safeimg_scan_buffer.restype = ctypes.c_char_p
        self._lib.safeimg_scan_buffer.argtypes = [
            ctypes.POINTER(ctypes.c_uint8),
            ctypes.c_size_t,
            ctypes.c_char_p
        ]
        
        # safeimg_match_signatures
        self._lib.safeimg_match_signatures.restype = ctypes.c_char_p
        self._lib.safeimg_match_signatures.argtypes = [
            ctypes.POINTER(ctypes.c_uint8),
            ctypes.c_size_t,
            ctypes.c_char_p
        ]
        
        # safeimg_free
        self._lib.safeimg_free.argtypes = [ctypes.c_char_p]
    
    def version(self) -> str:
        """Get library version"""
        return self._lib.safeimg_version().decode('utf-8')
    
    def supports_format(self, format: str) -> bool:
        """Check if format is supported"""
        return self._lib.safeimg_supports_format(format.encode()) == 1
    
    def scan_file(self, filepath: str, options: Optional[Dict] = None) -> Dict:
        """Scan an image file
        
        Args:
            filepath: Path to image file
            options: Scan options dict (optional)
        
        Returns:
            Scan result dict
        
        Raises:
            SafeImgError: If scan fails
        """
        options_json = json.dumps(options).encode() if options else None
        
        result_ptr = self._lib.safeimg_scan_file(
            filepath.encode(),
            options_json
        )
        
        try:
            result_str = ctypes.string_at(result_ptr).decode('utf-8')
            result = json.loads(result_str)
            
            if 'error' in result:
                raise SafeImgError(result['error'])
            
            return result
        finally:
            self._lib.safeimg_free(result_ptr)
    
    def scan_buffer(self, data: bytes, options: Optional[Dict] = None) -> Dict:
        """Scan image data from memory
        
        Args:
            data: Image data bytes
            options: Scan options dict (optional)
        
        Returns:
            Scan result dict
        """
        options_json = json.dumps(options).encode() if options else None
        
        # Convert bytes to ctypes array
        buffer = (ctypes.c_uint8 * len(data)).from_buffer_copy(data)
        
        result_ptr = self._lib.safeimg_scan_buffer(
            buffer,
            len(data),
            options_json
        )
        
        try:
            result_str = ctypes.string_at(result_ptr).decode('utf-8')
            result = json.loads(result_str)
            
            if 'error' in result:
                raise SafeImgError(result['error'])
            
            return result
        finally:
            self._lib.safeimg_free(result_ptr)
    
    def match_signatures(self, data: bytes, context: Optional[Dict] = None) -> List[Dict]:
        """Match signature patterns
        
        Args:
            data: Data to scan
            context: Context and options dict
        
        Returns:
            List of signature matches
        """
        context_json = json.dumps(context).encode() if context else None
        
        buffer = (ctypes.c_uint8 * len(data)).from_buffer_copy(data)
        
        result_ptr = self._lib.safeimg_match_signatures(
            buffer,
            len(data),
            context_json
        )
        
        try:
            result_str = ctypes.string_at(result_ptr).decode('utf-8')
            return json.loads(result_str)
        finally:
            self._lib.safeimg_free(result_ptr)
```

---

## Usage Examples

### Basic Scanning

```python
from safeimg import SafeImg

scanner = SafeImg()

# Check version
print(f"SafeImg v{scanner.version()}")

# Scan a file
result = scanner.scan_file('photo.jpg')

if result['isSafe']:
    print("✓ Image is safe")
else:
    print("⚠ Security issues found:")
    for issue in result['issues']:
        print(f"  [{issue['severity']}] {issue['description']}")
```

### Scan with Options

```python
options = {
    'checkPolyglot': True,
    'checkEntropy': True,
    'reduceFalsePositives': True,
    'confidenceThreshold': 0.7
}

result = scanner.scan_file('photo.jpg', options)
print(json.dumps(result, indent=2))
```

### Scan from Memory

```python
with open('photo.jpg', 'rb') as f:
    image_data = f.read()

result = scanner.scan_buffer(image_data)
print(f"Format: {result['format']}")
print(f"Size: {result['realImageSize']} bytes")
```

### Flask Integration

```python
from flask import Flask, request, jsonify
from safeimg import SafeImg, SafeImgError

app = Flask(__name__)
scanner = SafeImg()

@app.route('/scan', methods=['POST'])
def scan_upload():
    if 'file' not in request.files:
        return jsonify({'error': 'No file provided'}), 400
    
    file = request.files['file']
    
    try:
        # Scan from memory
        result = scanner.scan_buffer(file.read())
        
        if not result['isSafe']:
            return jsonify({
                'safe': False,
                'issues': result['issues']
            }), 400
        
        return jsonify({
            'safe': True,
            'format': result['format'],
            'metadata': result['metadata']
        })
    
    except SafeImgError as e:
        return jsonify({'error': str(e)}), 500

if __name__ == '__main__':
    print(f"SafeImg v{scanner.version()}")
    app.run(debug=True)
```

---

## Advanced Usage

### Batch Scanning

```python
import os
from concurrent.futures import ThreadPoolExecutor
from safeimg import SafeImg

def scan_directory(directory: str):
    scanner = SafeImg()
    results = {}
    
    image_files = [
        f for f in os.listdir(directory)
        if f.lower().endswith(('.jpg', '.png', '.webp'))
    ]
    
    with ThreadPoolExecutor(max_workers=4) as executor:
        futures = {
            executor.submit(scanner.scan_file, os.path.join(directory, f)): f
            for f in image_files
        }
        
        for future in futures:
            filename = futures[future]
            try:
                result = future.result()
                results[filename] = result
            except Exception as e:
                results[filename] = {'error': str(e)}
    
    return results

# Scan all images
results = scan_directory('./uploads')

# Count safe vs unsafe
safe_count = sum(1 for r in results.values() if r.get('isSafe'))
print(f"Safe: {safe_count}/{len(results)}")
```

---

## Type Hints (Python 3.8+)

```python
from typing import TypedDict, List, Dict, Optional

class Issue(TypedDict):
    type: str
    description: str
    severity: str
    category: str
    offset: int
    length: int

class Metadata(TypedDict):
    hasEXIF: bool
    hasXMP: bool
    hasGPS: bool
    exifTags: Dict[str, str]
    metadataSizeBytes: int

class ScanResult(TypedDict):
    isSafe: bool
    format: str
    issues: List[Issue]
    metadata: Metadata
    realImageSize: int
    extraDataSize: int
    scanTimeMs: float
    version: str
```

---

## See Also

- **API.md** - C API reference
- **NODE_FFI.md** - Node.js integration
- **BUILD.md** - Build instructions
