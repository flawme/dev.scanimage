const { scanImage, scanImageSync } = require('../index');
const fs = require('fs');
const path = require('path');

// Test helpers
function assert(condition, message) {
    if (!condition) {
        console.error('❌ FAILED:', message);
        process.exit(1);
    }
    console.log('✓', message);
}

async function createTestImages() {
    const testDir = path.join(__dirname, 'images');
    if (!fs.existsSync(testDir)) {
        fs.mkdirSync(testDir, { recursive: true });
    }

    // Valid JPEG
    const validJpeg = Buffer.from([
        0xFF, 0xD8, 0xFF, 0xE0, 0x00, 0x10, 0x4A, 0x46,
        0x49, 0x46, 0x00, 0x01, 0x01, 0x00, 0x00, 0x01,
        0x00, 0x01, 0x00, 0x00, 0xFF, 0xD9
    ]);
    fs.writeFileSync(path.join(testDir, 'valid.jpg'), validJpeg);

    // Malformed JPEG (missing EOI)
    const malformedJpeg = Buffer.from([
        0xFF, 0xD8, 0xFF, 0xE0, 0x00, 0x10, 0x4A, 0x46,
        0x49, 0x46, 0x00, 0x01, 0x01, 0x00, 0x00, 0x01
    ]);
    fs.writeFileSync(path.join(testDir, 'malformed.jpg'), malformedJpeg);

    // Valid PNG
    const validPng = Buffer.from([
        0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A,
        0x00, 0x00, 0x00, 0x0D, 0x49, 0x48, 0x44, 0x52,
        0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01,
        0x08, 0x06, 0x00, 0x00, 0x00, 0x1F, 0x15, 0xC4,
        0x89, 0x00, 0x00, 0x00, 0x0A, 0x49, 0x44, 0x41,
        0x54, 0x78, 0x9C, 0x63, 0x00, 0x01, 0x00, 0x00,
        0x05, 0x00, 0x01, 0x0D, 0x0A, 0x2D, 0xB4, 0x00,
        0x00, 0x00, 0x00, 0x49, 0x45, 0x4E, 0x44, 0xAE,
        0x42, 0x60, 0x82
    ]);
    fs.writeFileSync(path.join(testDir, 'valid.png'), validPng);

    // Polyglot (PNG + ZIP)
    const polyglot = Buffer.concat([
        validPng,
        Buffer.from([0x50, 0x4B, 0x03, 0x04]) // ZIP signature
    ]);
    fs.writeFileSync(path.join(testDir, 'polyglot.png'), polyglot);

    // High entropy (random data)
    const highEntropy = Buffer.concat([
        validPng.slice(0, 50),
        Buffer.from(Array(1000).fill(0).map(() => Math.floor(Math.random() * 256)))
    ]);
    fs.writeFileSync(path.join(testDir, 'high_entropy.png'), highEntropy);

    // Embedded script
    const scriptEmbed = Buffer.concat([
        validPng,
        Buffer.from('<script>alert("xss")</script>')
    ]);
    fs.writeFileSync(path.join(testDir, 'script.png'), scriptEmbed);

    // Invalid format
    fs.writeFileSync(path.join(testDir, 'invalid.txt'), 'This is not an image');

    return testDir;
}

async function runTests() {
    console.log('🧪 Creating test images...\n');
    const testDir = await createTestImages();

    console.log('🧪 Running Image Scanner Tests\n');

    // Test 1: Valid JPEG
    console.log('Test 1: Valid JPEG');
    const result1 = await scanImage(path.join(testDir, 'valid.jpg'));
    assert(result1.isSafe === true, 'Valid JPEG should be safe');
    assert(result1.issues.length === 0, 'Valid JPEG should have no issues');

    // Test 2: Malformed JPEG
    console.log('\nTest 2: Malformed JPEG');
    const result2 = await scanImage(path.join(testDir, 'malformed.jpg'));
    assert(result2.isSafe === false, 'Malformed JPEG should not be safe');
    assert(result2.issues.some(i => i.type === 'malformed_jpeg'),
        'Should detect malformed JPEG');

    // Test 3: Valid PNG
    console.log('\nTest 3: Valid PNG');
    const result3 = await scanImage(path.join(testDir, 'valid.png'));
    assert(result3.isSafe === true, 'Valid PNG should be safe');

    // Test 4: Polyglot detection
    console.log('\nTest 4: Polyglot Detection');
    const result4 = await scanImage(path.join(testDir, 'polyglot.png'));
    assert(result4.isSafe === false, 'Polyglot should not be safe');
    assert(result4.issues.some(i => i.type === 'polyglot'),
        'Should detect polyglot file');

    // Test 5: High entropy
    console.log('\nTest 5: High Entropy Detection');
    const result5 = await scanImage(path.join(testDir, 'high_entropy.png'));
    assert(result5.issues.some(i => i.type === 'high_entropy'),
        'Should detect high entropy');

    // Test 6: Embedded script
    console.log('\nTest 6: Embedded Script Detection');
    const result6 = await scanImage(path.join(testDir, 'script.png'));
    assert(result6.isSafe === false, 'Embedded script should not be safe');
    assert(result6.issues.some(i => i.type === 'embedded_script'),
        'Should detect embedded script');

    // Test 7: Invalid format
    console.log('\nTest 7: Invalid Format');
    const result7 = await scanImage(path.join(testDir, 'invalid.txt'));
    assert(result7.isSafe === false, 'Invalid format should not be safe');
    assert(result7.issues.some(i => i.type === 'invalid_format'),
        'Should detect invalid format');

    // Test 8: Sync version
    console.log('\nTest 8: Synchronous Scan');
    const result8 = scanImageSync(path.join(testDir, 'valid.png'));
    assert(result8.isSafe === true, 'Sync scan should work');

    // Test 9: Error handling
    console.log('\nTest 9: Error Handling');
    const result9 = await scanImage('/nonexistent/file.jpg');
    assert(result9.isSafe === false, 'Nonexistent file should not be safe');
    assert(result9.issues.some(i => i.type === 'file_error'),
        'Should detect file error');

    console.log('\n✅ All tests passed!\n');

    // Print example result
    console.log('Example scan result:');
    console.log(JSON.stringify(result4, null, 2));
}

runTests().catch(err => {
    console.error('Test failed with error:', err);
    process.exit(1);
});
