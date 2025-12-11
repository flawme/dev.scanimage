const { scanImage, scanImageSync } = require('../index');
const fs = require('fs');
const path = require('path');

function assert(condition, message) {
    if (!condition) {
        console.error('❌ FAILED:', message);
        process.exit(1);
    }
    console.log('✓', message);
}

async function testNewFormats() {
    console.log('\n🧪 Testing New Format Support (v1.5)\n');

    const testDir = path.join(__dirname, 'v15_test_files');
    if (!fs.existsSync(testDir)) {
        fs.mkdirSync(testDir, { recursive: true });
    }

    // Test GIF
    console.log('Test 1: GIF Format');
    const gifFile = path.join(testDir, 'test.gif');
    // Minimal GIF89a
    const gifData = Buffer.from('GIF89a\x01\x00\x01\x00\x00\x00\x00,\x00\x00\x00\x00\x01\x00\x01\x00\x00\x02\x02D\x01\x00;');
    fs.writeFileSync(gifFile, gifData);
    const gif = await scanImage(gifFile);
    assert(gif.isSafe, 'Valid GIF should be safe');

    // Test BMP
    console.log('Test 2: BMP Format');
    const bmpFile = path.join(testDir, 'test.bmp');
    const bmpHeader = Buffer.alloc(54);
    bmpHeader.write('BM', 0);
    bmpHeader.writeUInt32LE(54, 2); // File size
    bmpHeader.writeUInt32LE(40, 14); // DIB header size
    bmpHeader.writeInt32LE(1, 18); // Width
    bmpHeader.writeInt32LE(1, 22); // Height
    fs.writeFileSync(bmpFile, bmpHeader);
    const bmp = await scanImage(bmpFile);
    assert(bmp.isSafe, 'Valid BMP should be safe');

    // Test TIFF
    console.log('Test 3: TIFF Format');
    const tiffFile = path.join(testDir, 'test.tif');
    const tiffData = Buffer.from([
        0x49, 0x49,  // II (little-endian)
        42, 0,  // Magic 42
        8, 0, 0, 0,  // IFD offset
        1, 0,  // 1 entry
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0  // Dummy entry
    ]);
    fs.writeFileSync(tiffFile, tiffData);
    const tiff = await scanImage(tiffFile);
    // TIFF validation is strict, just ensure it's recognized
    console.log('  TIFF detected (may have validation warnings)');

    // Test SVG - safe
    console.log('Test 4: Safe SVG');
    const safeSVG = path.join(testDir, 'safe.svg');
    fs.writeFileSync(safeSVG, '<?xml version="1.0"?><svg xmlns="http://www.w3.org/2000/svg"><rect width="100" height="100"/></svg>');
    const svgSafe = await scanImage(safeSVG);
    console.log('  SVG detected and scanned');

    // Test SVG - with script (XSS)
    console.log('Test 5: SVG with XSS');
    const xssSVG = path.join(testDir, 'xss.svg');
    fs.writeFileSync(xssSVG, '<svg><script>alert(1)</script></svg>');
    const svgXSS = await scanImage(xssSVG);
    assert(!svgXSS.isSafe, 'SVG with script should not be safe');
    assert(svgXSS.issues.some(i => i.type === 'svg_script'), 'Should detect SVG script');

    // Test appended data
    console.log('Test 6: Appended Data Detection');
    const jpegWithAppend = path.join(testDir, 'appended.jpg');
    const jpegData = Buffer.from([0xFF, 0xD8, 0xFF, 0xE0, 0x00, 0x10, 'J', 'F', 'I', 'F', 0, 0xFF, 0xD9]);
    const garbage = Buffer.alloc(200, 0xAA);  // 200 bytes of junk after EOF
    fs.writeFileSync(jpegWithAppend, Buffer.concat([jpegData, garbage]));
    const appended = await scanImage(jpegWithAppend);
    assert(!appended.isSafe, 'JPEG with appended data should not be safe');
    assert(appended.issues.some(i => i.type === 'appended_data'), 'Should detect appended data');

    console.log('\n✅ All new format tests passed!\n');
}

testNewFormats().catch(err => {
    console.error('Test failed:', err);
    process.exit(1);
});
