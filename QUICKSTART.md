# Quick Start Guide

## Installation

```bash
cd /home/iota09/Desktop/scanimage
npm install
npm test
```

## Basic Usage

```javascript
const { scanImage } = require('safeimg');

// Scan an image
const result = await scanImage('/path/to/image.jpg');

if (result.isSafe) {
  console.log('✅ Image is safe');
} else {
  console.log('⚠️ Security issues found:');
  result.issues.forEach(issue => {
    console.log(`- ${issue.type}: ${issue.description}`);
  });
}
```

## Try the Demo

```bash
# Run the included demo
node demo.js

# Run the test suite
npm test

# Start the Express server example
node examples/express-server.js
# Then visit http://localhost:3000
```

## API

### scanImage(filepath)

Asynchronously scans an image file.

**Returns:**
```javascript
{
  isSafe: boolean,
  issues: [
    {
      type: string,
      description: string,
      severity: 'info' | 'warning' | 'critical'
    }
  ]
}
```

### scanImageSync(filepath)

Synchronous version. Same return format.

## What Gets Detected

✅ Invalid image formats  
✅ Malformed headers (corrupted JPEG/PNG/WEBP)  
✅ Polyglot files (PNG+ZIP, JPG+PDF, etc.)  
✅ High entropy (encrypted/hidden payloads)  
✅ Embedded scripts and executables  
✅ Common malware signatures  
✅ **Runtime signature loading** (NEW in v1.1.0)  
✅ **Runtime signature loading** (NEW in v1.1.0)  
✅ **Full TypeScript support** with type definitions

## Runtime Signatures (v1.1.0)

Load custom malicious patterns without recompiling:

```javascript
const { scanImage, loadSignatures } = require('safeimg');

// Load your custom signatures
loadSignatures('./custom-signatures.json');

// Scans now use your custom patterns
await scanImage('upload.jpg');
```

Example `custom-signatures.json`:
```json
{
  "signatures": {
    "my_threat": "MALICIOUS",
    "hex_pattern": "\\xDE\\xAD\\xBE\\xEF"
  }
}
```

See [signatures.json](signatures.json) for 17 built-in signatures.

## Integration Example (Express.js)

```javascript
const express = require('express');
const multer = require('multer');
const { scanImage } = require('safeimg');

const app = express();
const upload = multer({ dest: 'uploads/' });

app.post('/upload', upload.single('image'), async (req, res) => {
  const result = await scanImage(req.file.path);
  
  if (!result.isSafe) {
    fs.unlinkSync(req.file.path); // Delete malicious file
    return res.status(400).json({ 
      error: 'Malicious file detected', 
      issues: result.issues 
    });
  }
  
  res.json({ success: true });
});
```

## Adding to Existing Project

1. Copy the `safeimg` directory to your project
2. Add to package.json dependencies:
   ```json
   {
     "dependencies": {
       "safeimg": "file:./safeimg"
     }
   }
   ```
3. Run `npm install`

Alternatively, publish to npm:
```bash
npm publish
```

Then install normally:
```bash
npm install safeimg
```

## Troubleshooting

**Build fails:** Ensure you have a C++ compiler installed
- Linux: `sudo apt-get install build-essential`
- macOS: `xcode-select --install`
- Windows: Install Visual Studio Build Tools

**Module not found:** Run `npm run build` to rebuild the native addon

**Performance:** For CPU-intensive workloads, the async version is recommended

## Documentation

See [README.md](README.md) for complete documentation.

See [walkthrough.md](.gemini/antigravity/brain/31316a96-428a-40a7-bec0-4005c55aa0c9/walkthrough.md) for implementation details.
