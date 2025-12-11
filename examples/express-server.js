const express = require('express');
const multer = require('multer');
const { scanImage } = require('../index');
const fs = require('fs');
const path = require('path');

const app = express();
const upload = multer({ dest: 'uploads/' });

// Image upload endpoint with security scanning
app.post('/upload', upload.single('image'), async (req, res) => {
  try {
    if (!req.file) {
      return res.status(400).json({ error: 'No file uploaded' });
    }

    const filepath = req.file.path;

    console.log(`Scanning uploaded file: ${req.file.originalname}`);

    // Scan the image
    const scanResult = await scanImage(filepath);

    // Clean up the file after scanning
    fs.unlinkSync(filepath);

    if (!scanResult.isSafe) {
      console.warn(`⚠️  Malicious file detected: ${req.file.originalname}`);
      console.warn('Issues:', scanResult.issues);

      return res.status(400).json({
        error: 'File rejected: security issues detected',
        scanResult
      });
    }

    console.log(`✓ File is safe: ${req.file.originalname}`);

    res.json({
      message: 'File uploaded and verified successfully',
      filename: req.file.originalname,
      scanResult
    });

  } catch (error) {
    console.error('Error during upload:', error);

    // Clean up file if it exists
    if (req.file && fs.existsSync(req.file.path)) {
      fs.unlinkSync(req.file.path);
    }

    res.status(500).json({ error: 'Internal server error' });
  }
});

// Health check endpoint
app.get('/health', (req, res) => {
  res.json({ status: 'ok' });
});

// Simple upload form for testing
app.get('/', (req, res) => {
  res.send(`
    <!DOCTYPE html>
    <html>
    <head>
      <title>SafeImg Demo</title>
      <style>
        body {
          font-family: Arial, sans-serif;
          max-width: 600px;
          margin: 50px auto;
          padding: 20px;
        }
        .upload-form {
          border: 2px dashed #ccc;
          padding: 40px;
          text-align: center;
          border-radius: 8px;
        }
        input[type="file"] {
          margin: 20px 0;
        }
        button {
          background: #007bff;
          color: white;
          border: none;
          padding: 10px 20px;
          border-radius: 4px;
          cursor: pointer;
          font-size: 16px;
        }
        button:hover {
          background: #0056b3;
        }
        #result {
          margin-top: 20px;
          padding: 15px;
          border-radius: 4px;
          display: none;
        }
        .success {
          background: #d4edda;
          border: 1px solid #c3e6cb;
          color: #155724;
        }
        .error {
          background: #f8d7da;
          border: 1px solid #f5c6cb;
          color: #721c24;
        }
      </style>
    </head>
    <body>
      <h1>🔍 SafeImg Demo</h1>
      <p>Upload an image to scan for security issues</p>
      
      <div class="upload-form">
        <form id="uploadForm" enctype="multipart/form-data">
          <input type="file" name="image" accept="image/*" required>
          <br>
          <button type="submit">Upload & Scan</button>
        </form>
      </div>
      
      <div id="result"></div>
      
      <script>
        document.getElementById('uploadForm').onsubmit = async (e) => {
          e.preventDefault();
          
          const formData = new FormData(e.target);
          const resultDiv = document.getElementById('result');
          
          resultDiv.style.display = 'none';
          resultDiv.textContent = 'Scanning...';
          resultDiv.className = '';
          resultDiv.style.display = 'block';
          
          try {
            const response = await fetch('/upload', {
              method: 'POST',
              body: formData
            });
            
            const data = await response.json();
            
            if (response.ok) {
              resultDiv.className = 'success';
              resultDiv.innerHTML = '<strong>✓ File is safe!</strong><br>' +
                'No security issues detected.';
            } else {
              resultDiv.className = 'error';
              let issuesHtml = data.scanResult.issues
                .map(i => \`<li><strong>\${i.type}</strong>: \${i.description} (\${i.severity})</li>\`)
                .join('');
              resultDiv.innerHTML = '<strong>⚠ Security issues detected:</strong>' +
                \`<ul>\${issuesHtml}</ul>\`;
            }
          } catch (error) {
            resultDiv.className = 'error';
            resultDiv.textContent = '❌ Upload failed: ' + error.message;
          }
        };
      </script>
    </body>
    </html>
  `);
});

const PORT = process.env.PORT || 3000;

app.listen(PORT, () => {
  console.log(`\n🚀 SafeImg Server running on http://localhost:${PORT}`);
  console.log(`\nUsage:`);
  console.log(`  - Web interface: http://localhost:${PORT}`);
  console.log(`  - API endpoint: POST http://localhost:${PORT}/upload`);
  console.log(`\nExample cURL command:`);
  console.log(`  curl -F "image=@/path/to/image.jpg" http://localhost:${PORT}/upload\n`);
});
