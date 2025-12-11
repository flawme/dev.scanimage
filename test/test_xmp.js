const scanner = require('../index.js');
const fs = require('fs');
const path = require('path');

console.log('Testing XMP Parser Integration...\n');

// Ensure test directory exists
const testDir = path.join(__dirname, 'xmp_test_files');
if (!fs.existsSync(testDir)) {
    fs.mkdirSync(testDir, { recursive: true });
}

// Test 1: Create a simple JPEG with XMP
console.log('Test 1: JPEG with benign XMP');
const jpegWithXMP = Buffer.from([
    0xFF, 0xD8, // SOI
    0xFF, 0xE1, 0x00, 0x50, // APP1 marker, length 80
    ...Buffer.from('http://ns.adobe.com/xap/1.0/\0'), // XMP namespace
    ...Buffer.from('<x:xmpmeta><rdf:RDF><rdf:Description rdf:about="test"/></rdf:RDF></x:xmpmeta>'),
    0xFF, 0xD9  // EOI
]);

const test1File = path.join(testDir, 'test1_benign_xmp.jpg');
fs.writeFileSync(test1File, jpegWithXMP);

try {
    const result = scanner.scanImageSync(test1File);
    console.log('Result:', JSON.stringify(result, null, 2));
    console.log('✓ Test 1 passed\n');
} catch (err) {
    console.error('✗ Test 1 failed:', err.message, '\n');
}

// Test 2: JPEG with malicious XMP (script tag)
console.log('Test 2: JPEG with malicious XMP (script tag)');
const maliciousXMP = '<x:xmpmeta><script>alert("XSS")</script></x:xmpmeta>';
const jpegWithMaliciousXMP = Buffer.from([
    0xFF, 0xD8, // SOI
    0xFF, 0xE1, 0x00, 0x50, // APP1 marker with enough length
    ...Buffer.from('http://ns.adobe.com/xap/1.0/\0'),
    ...Buffer.from(maliciousXMP),
    0xFF, 0xD9  // EOI
]);

const test2File = path.join(testDir, 'test2_malicious_xmp.jpg');
fs.writeFileSync(test2File, jpegWithMaliciousXMP);

try {
    const result = scanner.scanImageSync(test2File);
    console.log('Result:', JSON.stringify(result, null, 2));

    if (result.metadata && result.metadata.xmpWarnings && result.metadata.xmpWarnings.length > 0) {
        console.log('✓ Test 2 passed - Script detected!');
        console.log('Warnings:', result.metadata.xmpWarnings);
    } else {
        console.log('✗ Test 2 failed - Script not detected');
    }
    console.log();
} catch (err) {
    console.error('✗ Test 2 failed:', err.message, '\n');
}

// Test 3: JPEG without XMP
console.log('Test 3: JPEG without XMP');
const simpleJPEG = Buffer.from([
    0xFF, 0xD8, // SOI
    0xFF, 0xD9  // EOI
]);

const test3File = path.join(testDir, 'test3_no_xmp.jpg');
fs.writeFileSync(test3File, simpleJPEG);

try {
    const result = scanner.scanImageSync(test3File);
    console.log('Result:', JSON.stringify(result, null, 2));
    console.log('✓ Test 3 passed\n');
} catch (err) {
    console.error('✗ Test 3 failed:', err.message, '\n');
}

console.log('XMP Parser tests complete!');
console.log('Test files created in:', testDir);

// Cleanup
//fs.rmSync(testDir, { recursive: true, force: true });
