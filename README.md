# SafeImg

A secure C++ image scanning library with Node.js integration for detecting malicious or suspicious images.

## Features

✅ **Format Validation** - Supports JPEG, PNG, WEBP, GIF, BMP, TIFF, SVG  
✅ **Magic Byte Verification** - Ensures files match their claimed format  
✅ **Header Validation** - Detects corrupted or malformed image headers  
✅ **SVG XSS Detection** - Scans for script tags, event handlers, data URIs (NEW in v1.5)  
✅ **Polyglot Detection** - Identifies files embedding multiple formats  
✅ **Entropy Analysis** - Detects hidden payloads via abnormal entropy patterns  
✅ **Signature Matching** - Extensible pattern detection for malicious content  
✅ **Integrity Checks** - Detects appended data and oversized metadata (NEW in v1.5)  
✅ **Runtime Signature Loading** - Load custom signatures from JSON (v1.1.0)  
✅ **Async & Sync API** - Non-blocking and synchronous scanning options  
✅ **TypeScript Support** - Full type definitions included

## Installation

### Prerequisites

- Node.js 14+
- C++ compiler (g++, clang, or MSVC)
- Python 3 (for node-gyp)

### Install

```bash
npm install
```

This will automatically compile the C++ addon.

## Quick Start

```javascript
const { scanImage } = require('safeimg');

// Async version (recommended)
const result = await scanImage('/path/to/image.jpg');

if (result.isSafe) {
  console.log('✓ Image is safe');
} else {
  console.log('⚠ Security issues detected:');
  result.issues.forEach(issue => {
    console.log(`  - ${issue.type}: ${issue.description}`);
  });
}
```

## API Reference

### `scanImage(filepath: string): Promise<ScanResult>`

Asynchronously scans an image file. **Recommended for production use** as it doesn't block the event loop.

**Parameters:**
- `filepath` - Absolute or relative path to the image file

**Returns:** Promise resolving to `ScanResult`

### `scanImageSync(filepath: string): ScanResult`

Synchronously scans an image file. Blocks the event loop during scanning.

**Parameters:**
- `filepath` - Absolute or relative path to the image file

**Returns:** `ScanResult`

### `loadSignatures(filepath: string): LoadSignaturesResult`

**New in v1.1.0** - Load custom malicious signatures from a JSON file.

**Parameters:**
- `filepath` - Path to JSON file containing signatures

**Returns:** `LoadSignaturesResult`

```typescript
interface LoadSignaturesResult {
  success: boolean;    // true if loaded successfully
  error?: string;      // Error message if failed (still uses defaults)
}
```

**Note:** If loading fails, the scanner automatically falls back to built-in signatures and continues working normally.

### ScanResult

```typescript
interface ScanResult {
  isSafe: boolean;        // true if no critical issues found
  issues: Issue[];        // Array of detected issues
}

interface Issue {
  type: string;           // Issue category (e.g., "polyglot", "high_entropy")
  description: string;    // Human-readable description
  severity: 'info' | 'warning' | 'critical';
}
```

## Detection Capabilities

### 1. Format Validation

Validates that files are proper JPEG, PNG, or WEBP images:

- **JPEG**: Checks SOI marker (FF D8), EOI marker (FF D9), and segment structure
- **PNG**: Validates 8-byte signature, IHDR chunk, IEND chunk, and CRC checksums
- **WEBP**: Verifies RIFF container and VP8/VP8L chunks

### 2. Polyglot Detection

Detects files that embed multiple formats:

- ZIP archives (PK signature)
- PDF documents
- HTML/JavaScript content
- Shell scripts

### 3. Entropy Analysis

Calculates Shannon entropy to detect:

- Encrypted or compressed payloads (>7.5 bits/byte)
- Hidden data in file tails
- Steganographic content

### 4. Signature Matching

Scans for known malicious patterns:

- ELF executables
- Windows PE files
- PHP code
- Shell scripts

**Extensible:** Add custom signatures programmatically (C++ API).

## Integration Examples

### Express.js File Upload

```javascript
const express = require('express');
const multer = require('multer');
const { scanImage } = require('safeimg');

const app = express();
const upload = multer({ dest: 'uploads/' });

app.post('/upload', upload.single('image'), async (req, res) => {
  const result = await scanImage(req.file.path);
  
  if (!result.isSafe) {
    // Delete malicious file
    fs.unlinkSync(req.file.path);
    return res.status(400).json({
      error: 'Malicious file detected',
      issues: result.issues
    });
  }
  
  res.json({ message: 'Upload successful' });
});
```

See [examples/express-server.js](examples/express-server.js) for a complete working example.

## TypeScript Support

Full TypeScript support is included with type definitions:

```typescript
import { scanImage, ScanResult, Issue } from 'safeimg';

async function validateUpload(filepath: string): Promise<boolean> {
    const result: ScanResult = await scanImage(filepath);
    
    if (!result.isSafe) {
        result.issues.forEach((issue: Issue) => {
            console.log(`[${issue.severity}] ${issue.type}: ${issue.description}`);
        });
        return false;
    }
    
    return true;
}
```

See [examples/typescript-usage.ts](examples/typescript-usage.ts) for more TypeScript examples.

## Runtime Signature Loading

**New in v1.1.0** - Load custom malicious signatures at runtime without recompiling.

### Default Signatures

SafeImg comes with 17 built-in signatures for common threats:

```javascript
const { scanImage } = require('safeimg');

// Uses built-in signatures automatically
await scanImage('upload.jpg');
```

Built-in patterns detect:
- **Archives**: ZIP, RAR, 7-Zip
- **Executables**: ELF, Windows PE
- **Scripts**: PHP, HTML, JavaScript
- **Documents**: PDF
- **Shell Scripts**: Bash, Python, Perl

### Custom Signatures

Load your own signature database from JSON:

```javascript
const { loadSignatures } = require('safeimg');

// Load custom signatures
const result = loadSignatures('./custom-signatures.json');

if (result.success) {
  console.log('Custom signatures loaded');
} else {
  console.warn('Using defaults:', result.error);
}

// Now scans use your custom patterns
await scanImage('upload.jpg');
```

**Signature JSON Format:**

```json
{
  "signatures": {
    "threat_name": "MALICIOUS_PATTERN",
    "hex_pattern": "\\xDE\\xAD\\xBE\\xEF",
    "mixed": "BAD\\x00DATA"
  }
}
```

Supported escape sequences: `\xHH` (hex), `\n`, `\r`, `\t`, `\\`

**Error Handling:** If the file is missing or invalid, SafeImg automatically falls back to built-in signatures and continues scanning.

See [signatures.json](signatures.json) for the default signature database.


### Fastify

```javascript
const fastify = require('fastify')();
const { scanImage } = require('image-scanner');

fastify.post('/upload', async (request, reply) => {
  const data = await request.file();
  const filepath = `/tmp/${data.filename}`;
  
  await pump(data.file, fs.createWriteStream(filepath));
  
  const result = await scanImage(filepath);
  
  if (!result.isSafe) {
    fs.unlinkSync(filepath);
    return reply.code(400).send({ error: 'Malicious file', issues: result.issues });
  }
  
  return { success: true };
});
```

## Testing

Run the test suite:

```bash
npm test
```

This creates test images with various issues and validates all detection features.

## Performance

- **Average scan time**: <10ms per image (most images <5ms)
- **Async operation**: Non-blocking, suitable for high-throughput servers
- **Memory efficient**: Streams large files, caps at 100MB

## Security Considerations

⚠️ **This is NOT a full antivirus solution**. It detects:
- Common image-based attack vectors
- Malformed files that could exploit parser bugs
- Polyglot files used to bypass upload filters
- Basic steganography indicators

⚠️ **It does NOT**:
- Execute deep malware analysis
- Scan for zero-day exploits
- Provide real-time threat intelligence

**Recommendation**: Use this as a first-line defense, combined with:
- Virus scanning (ClamAV, VirusTotal API)
- File type restrictions
- Size limits
- User permissions

## Architecture

```
┌─────────────────────┐
│   Node.js App       │
│  (index.js)         │
└──────────┬──────────┘
           │
           ▼
┌─────────────────────┐
│   N-API Bindings    │
│ (node_bindings.cpp) │
└──────────┬──────────┘
           │
           ▼
┌─────────────────────┐
│   C++ Scanner Core  │
│ (image_scanner.cpp) │
├─────────────────────┤
│ • Format Validators │
│ • Polyglot Detector │
│ • Entropy Analyzer  │
│ • Signature Matcher │
└─────────────────────┘
```

## Adding Custom Signatures

Currently, custom signatures must be added via the C++ API. Future versions will support runtime signature loading.

```cpp
#include "image_scanner.h"

ImageScanner::Scanner scanner;
scanner.addSignature("custom_threat", {0xDE, 0xAD, 0xBE, 0xEF});
```

## Building from Source

```bash
# Install dependencies
npm install

# Rebuild native addon
npm run build

# Clean build artifacts
npm run clean
```

## Platform Support

- ✅ Linux (tested on Ubuntu 20.04+)
- ✅ macOS (tested on 10.15+)
- ✅ Windows (tested on Windows 10+)

## Troubleshooting

### Build Failures

**Missing compiler:**
```bash
# Ubuntu/Debian
sudo apt-get install build-essential

# macOS
xcode-select --install

# Windows
# Install Visual Studio Build Tools
```

**Python version issues:**
```bash
npm config set python /path/to/python3
```

### Runtime Errors

**Module not found:**
Ensure the addon is built:
```bash
npm run build
```

**Segmentation fault:**
Check that file paths are valid and accessible.

## License

MIT

## Contributing

Contributions welcome! Please:

1. Add tests for new features
2. Follow existing code style
3. Update documentation
4. Submit a pull request

## Roadmap

- [ ] Runtime signature loading from JSON
- [ ] TIFF and GIF support
- [ ] Machine learning-based detection
- [ ] CLI tool
- [ ] Batch scanning API
- [ ] Cloud service integration (AWS S3, etc.)

## Support

For issues, questions, or feature requests, please open an issue on GitHub.

---

## Development

This project was developed with **AI assistance**.
