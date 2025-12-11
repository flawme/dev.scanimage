const { scanImage, scanImageSync, loadSignatures } = require('../index');
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

async function testSignatureLoading() {
    console.log('\n🧪 Testing Runtime Signature Loading\n');

    const testDir = path.join(__dirname, 'signatures');
    if (!fs.existsSync(testDir)) {
        fs.mkdirSync(testDir, { recursive: true });
    }

    // Test 1: Load valid signatures.json
    console.log('Test 1: Load Default signatures.json');
    const result1 = loadSignatures(path.join(__dirname, '../signatures.json'));
    assert(result1.success === true, 'Should load default signatures successfully');
    console.log('  Loaded default signatures');

    // Test 2: Create custom signatures file
    console.log('\nTest 2: Load Custom Signatures');
    const customSigs = {
        signatures: {
            "test_pattern": "TEST",
            "custom_hex": "\\xDE\\xAD\\xBE\\xEF"
        }
    };
    fs.writeFileSync(path.join(testDir, 'custom.json'), JSON.stringify(customSigs, null, 2));
    const result2 = loadSignatures(path.join(testDir, 'custom.json'));
    assert(result2.success === true, 'Should load custom signatures');

    // Test 3: Test with missing file (should use defaults)
    console.log('\nTest 3: Missing File Fallback');
    const result3 = loadSignatures('/nonexistent/path.json');
    assert(result3.success === false, 'Should return false for missing file');
    assert(result3.error, 'Should have error message');
    console.log(`  Error message: "${result3.error}"`);

    // Test 4: Test with invalid JSON
    console.log('\nTest 4: Invalid JSON Handling');
    fs.writeFileSync(path.join(testDir, 'invalid.json'), 'not valid json{');
    const result4 = loadSignatures(path.join(testDir, 'invalid.json'));
    assert(result4.success === false, 'Should return false for invalid JSON');
    console.log(`  Error message: "${result4.error}"`);

    // Test 5: Test with valid JSON but no signatures key
    console.log('\nTest 5: JSON Without Signatures Key');
    const noSigs = { "other": "data" };
    fs.writeFileSync(path.join(testDir, 'no_sigs.json'), JSON.stringify(noSigs));
    const result5 = loadSignatures(path.join(testDir, 'no_sigs.json'));
    assert(result5.success === false, 'Should return false for missing signatures key');

    console.log('\n✅ All signature loading tests passed!\n');
}

async function testDefaultSignatures() {
    console.log('\n🧪 Testing Default Signature Detection\n');

    // Reload default signatures (in case custom ones were loaded before)
    console.log('Reloading default signatures...');
    loadSignatures(path.join(__dirname, '../signatures.json'));

    const testDir = path.join(__dirname, 'sig_test_files');
    if (!fs.existsSync(testDir)) {
        fs.mkdirSync(testDir, { recursive: true });
    }

    // Create test file with PHP code
    console.log('Test 1: Detect PHP Code Signature');
    const phpFile = path.join(testDir, 'php_test.jpg');
    fs.writeFileSync(phpFile, Buffer.from([
        0xFF, 0xD8, 0xFF, 0xE0, 0x00, 0x10, // Valid JPEG start
        ...Buffer.from('<?php echo "test"; ?>'), // PHP code
        0xFF, 0xD9 // JPEG end
    ]));

    const result1 = await scanImage(phpFile);
    assert(!result1.isSafe, 'Should detect PHP code as unsafe');
    assert(result1.issues.some(i => i.type === 'signature' && i.description.includes('php')),
        'Should detect php signature');
    console.log(`  Detected: ${result1.issues.find(i => i.type === 'signature').description}`);

    // Test ZIP signature
    console.log('\nTest 2: Detect ZIP Signature');
    const zipFile = path.join(testDir, 'zip_test.jpg');
    fs.writeFileSync(zipFile, Buffer.from([
        0xFF, 0xD8, 0xFF, 0xE0, 0x00, 0x10, // Valid JPEG start
        0x50, 0x4B, 0x03, 0x04, // ZIP header
        0xFF, 0xD9 // JPEG end
    ]));

    const result2 = await scanImage(zipFile);
    assert(!result2.isSafe, 'Should detect ZIP header as unsafe');
    assert(result2.issues.some(i => i.type === 'signature' || i.type === 'polyglot'),
        'Should detect ZIP via signature or polyglot detection');
    console.log(`  Detected: ${result2.issues.filter(i => i.type === 'signature' || i.type === 'polyglot').map(i => i.description).join(', ')}`);

    // Test script tag
    console.log('\nTest 3: Detect Script Tag');
    const scriptFile = path.join(testDir, 'script_test.jpg');
    fs.writeFileSync(scriptFile, Buffer.from([
        0xFF, 0xD8, 0xFF, 0xE0, 0x00, 0x10, // Valid JPEG start
        ...Buffer.from('<script>alert(1)</script>'),
        0xFF, 0xD9 // JPEG end
    ]));

    const result3 = await scanImage(scriptFile);
    assert(!result3.isSafe, 'Should detect script tag as unsafe');
    assert(result3.issues.some(i => i.type === 'signature' && i.description.includes('script')),
        'Should detect script tag');

    console.log('\n✅ All default signature tests passed!\n');
}

async function testCustomSignatures() {
    console.log('\n🧪 Testing Custom Signature Scanning\n');

    const testDir = path.join(__dirname, 'sig_test_files');

    // Create custom signatures
    const customSigs = {
        signatures: {
            "my_marker": "CUSTOM_MARKER",
            "hex_pattern": "\\xCA\\xFE\\xBA\\xBE"
        }
    };

    const sigPath = path.join(testDir, 'my_signatures.json');
    fs.writeFileSync(sigPath, JSON.stringify(customSigs, null, 2));

    // Load custom signatures
    console.log('Test 1: Load and Use Custom Signatures');
    loadSignatures(sigPath);

    // Create file with custom marker
    const customFile = path.join(testDir, 'custom_test.jpg');
    fs.writeFileSync(customFile, Buffer.from([
        0xFF, 0xD8, 0xFF, 0xE0, 0x00, 0x10, // Valid JPEG start
        ...Buffer.from('CUSTOM_MARKER'),
        0xFF, 0xD9 // JPEG end
    ]));

    const result1 = await scanImage(customFile);
    assert(!result1.isSafe, 'Should detect custom marker as unsafe');
    assert(result1.issues.some(i => i.description.includes('my_marker')),
        'Should detect custom signature');
    console.log(`  Detected: ${result1.issues.find(i => i.type === 'signature').description}`);

    console.log('\n✅ All custom signature tests passed!\n');
}

async function runAllTests() {
    await testSignatureLoading();
    await testDefaultSignatures();
    await testCustomSignatures();

    console.log('🎉 All signature loading feature tests passed!\n');
}

runAllTests().catch(err => {
    console.error('Test failed with error:', err);
    process.exit(1);
});
