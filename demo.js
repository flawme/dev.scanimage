const { scanImage, scanImageSync } = require('./index');
const path = require('path');

console.log('🔍 Image Scanner Demo\n');
console.log('Scanning test images created by the test suite...\n');

async function demo() {
    const testDir = path.join(__dirname, 'test', 'images');

    const files = [
        { name: 'valid.jpg', description: 'Valid JPEG image' },
        { name: 'malformed.jpg', description: 'JPEG with missing EOI marker' },
        { name: 'polyglot.png', description: 'PNG with embedded ZIP signature' },
        { name: 'script.png', description: 'PNG with embedded <script> tag' },
    ];

    for (const file of files) {
        console.log(`\n${'='.repeat(60)}`);
        console.log(`📄 ${file.name} - ${file.description}`);
        console.log('='.repeat(60));

        try {
            const result = await scanImage(path.join(testDir, file.name));

            if (result.isSafe) {
                console.log('✅ STATUS: SAFE');
            } else {
                console.log('⚠️  STATUS: UNSAFE');
            }

            if (result.issues.length > 0) {
                console.log('\n🚨 Issues detected:');
                result.issues.forEach((issue, idx) => {
                    const icon = {
                        'critical': '🔴',
                        'warning': '🟡',
                        'info': '🔵'
                    }[issue.severity] || '⚪';

                    console.log(`${idx + 1}. ${icon} [${issue.severity.toUpperCase()}] ${issue.type}`);
                    console.log(`   ${issue.description}`);
                });
            } else {
                console.log('\n✓ No issues detected');
            }
        } catch (error) {
            console.log('❌ ERROR:', error.message);
        }
    }

    console.log('\n' + '='.repeat(60));
    console.log('\n✨ Demo complete!\n');
    console.log('To integrate into your app:');
    console.log('  const { scanImage } = require(\'safeimg\');');
    console.log('  const result = await scanImage(\'/path/to/image.jpg\');');
    console.log('  if (!result.isSafe) { /* reject upload */ }');
    console.log('\nFor Express.js integration, see: examples/express-server.js\n');
}

demo().catch(console.error);
