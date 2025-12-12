# SafeImg v2.0 - Node.js FFI Integration

## Overview

Use SafeImg shared library from Node.js via **ffi-napi** without native addons.

---

## Installation

```bash
npm install ffi-napi ref-napi
```

---

## Wrapper Module (safeimg.js)

```javascript
const ffi = require('ffi-napi');
const ref = require('ref-napi');
const path = require('path');
const os = require('os');

// Determine library path based on platform
const libName = {
  linux: 'libsafeimg.so',
  darwin: 'libsafeimg.dylib',
  win32: 'safeimg.dll'
}[os.platform()];

const libPath = path.join(__dirname, 'dist', 'lib', libName);

// Define C types
const charPtr = ref.refType(ref.types.CString);
const uint8Ptr = ref.refType(ref.types.uint8);

// Load SafeImg library
const safeimg = ffi.Library(libPath, {
  'safeimg_version': ['string', []],
  'safeimg_supports_format': ['int', ['string']],
  'safeimg_scan_file': [charPtr, ['string', 'string']],
  'safeimg_scan_buffer': [charPtr, [uint8Ptr, 'size_t', 'string']],
  'safeimg_match_signatures': [charPtr, [uint8Ptr, 'size_t', 'string']],
  'safeimg_free': ['void', [charPtr]]
});

class SafeImg {
  static version() {
    return safeimg.safeimg_version();
  }

  static supportsFormat(format) {
    return safeimg.safeimg_supports_format(format) === 1;
  }

  static scanFile(filepath, options = null) {
    const optionsJson = options ? JSON.stringify(options) : null;
    const resultPtr = safeimg.safeimg_scan_file(filepath, optionsJson);
    const resultStr = ref.readCString(resultPtr);
    safeimg.safeimg_free(resultPtr);
    return JSON.parse(resultStr);
  }

  static scanBuffer(buffer, options = null) {
    const optionsJson = options ? JSON.stringify(options) : null;
    const resultPtr = safeimg.safeimg_scan_buffer(
      buffer, 
      buffer.length, 
      optionsJson
    );
    const resultStr = ref.readCString(resultPtr);
    safeimg.safeimg_free(resultPtr);
    return JSON.parse(resultStr);
  }

  static matchSignatures(buffer, context = {}) {
    const contextJson = JSON.stringify(context);
    const resultPtr = safeimg.safeimg_match_signatures(
      buffer,
      buffer.length,
      contextJson
    );
    const resultStr = ref.readCString(resultPtr);
    safeimg.safeimg_free(resultPtr);
    return JSON.parse(resultStr);
  }
}

module.exports = SafeImg;
```

---

## Usage

### Basic Scanning

```javascript
const SafeImg = require('./safeimg');

// Check version
console.log('SafeImg version:', SafeImg.version());

// Scan a file
const result = SafeImg.scanFile('photo.jpg');

if (result.isSafe) {
  console.log('✓ Image is safe');
} else {
  console.log('⚠ Security issues found:');
  result.issues.forEach(issue => {
    console.log(`  - [${issue.severity}] ${issue.description}`);
  });
}
```

### Scan from Buffer

```javascript
const fs = require('fs');
const SafeImg = require('./safeimg');

const imageBuffer = fs.readFileSync('photo.jpg');
const result = SafeImg.scanBuffer(imageBuffer);

console.log('Format:', result.format);
console.log('Safe:', result.isSafe);
```

### With Options

```javascript
const options = {
  checkPolyglot: true,
  checkEntropy: true,
  reduceFalsePositives: true,
  confidenceThreshold: 0.7
};

const result = SafeImg.scanFile('photo.jpg', options);
```

### Express.js Integration

```javascript
const express = require('express');
const multer = require('multer');
const SafeImg = require('./safeimg');

const app = express();
const upload = multer({ dest: 'uploads/' });

app.post('/upload', upload.single('image'), (req, res) => {
  try {
    const result = SafeImg.scanFile(req.file.path);
    
    if (!result.isSafe) {
      fs.unlinkSync(req.file.path); // Delete malicious file
      return res.status(400).json({
        error: 'Malicious file detected',
        issues: result.issues
      });
    }
    
    res.json({ message: 'Upload successful', result });
  } catch (error) {
    res.status(500).json({ error: error.message });
  }
});

app.listen(3000, () => {
  console.log('Server running on port 3000');
  console.log('SafeImg version:', SafeImg.version());
});
```

---

## TypeScript Support

### Type Definitions (safeimg.d.ts)

```typescript
export interface ScanOptions {
  checkPolyglot?: boolean;
  checkEntropy?: boolean;
  checkIntegrity?: boolean;
  reduceFalsePositives?: boolean;
  confidenceThreshold?: number;
}

export interface Issue {
  type: string;
  description: string;
  severity: 'info' | 'warning' | 'high' | 'critical';
  category: string;
  offset: number;
  length: number;
}

export interface ScanResult {
  isSafe: boolean;
  format: string;
  issues: Issue[];
  metadata: {
    hasEXIF: boolean;
    hasXMP: boolean;
    hasGPS: boolean;
    exifTags: Record<string, string>;
    metadataSizeBytes: number;
  };
  realImageSize: number;
  extraDataSize: number;
  scanTimeMs: number;
  version: string;
}

export default class SafeImg {
  static version(): string;
  static supportsFormat(format: string): boolean;
  static scanFile(filepath: string, options?: ScanOptions): ScanResult;
  static scanBuffer(buffer: Buffer, options?: ScanOptions): ScanResult;
  static matchSignatures(buffer: Buffer, context?: any): any[];
}
```

---

## Platform-Specific Notes

### Linux

Set library path if not installed system-wide:
```bash
export LD_LIBRARY_PATH=/path/to/scanimage/dist/lib:$LD_LIBRARY_PATH
node app.js
```

### macOS

```bash
export DYLD_LIBRARY_PATH=/path/to/scanimage/dist/lib:$DYLD_LIBRARY_PATH
node app.js
```

### Windows

Copy `safeimg.dll` to project root or add to PATH:
```cmd
set PATH=%PATH%;C:\\path\\to\\scanimage\\dist\\lib
node app.js
```

---

## Packaging for Distribution

### package.json

```json
{
  "name": "safeimg-node",
  "version": "2.0.0",
  "main": "safeimg.js",
  "dependencies": {
    "ffi-napi": "^4.0.3",
    "ref-napi": "^3.0.3"
  },
  "files": [
    "safeimg.js",
    "safeimg.d.ts",
    "dist/lib/"
  ]
}
```

---

## Performance

- **No compilation required** - pure FFI calls
- **Same performance** as native addon
- **Cross-platform** - same code for all platforms
- **Easy updates** - just replace shared library

---

## See Also

- **API.md** - C API documentation
- **PYTHON.md** - Python integration
- **BUILD.md** - Build instructions
