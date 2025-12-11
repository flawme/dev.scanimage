const scanner = require('./build/Release/image_scanner');

/**
 * Scan an image file for security issues (async)
 * @param {string} filepath - Path to the image file
 * @returns {Promise<{isSafe: boolean, issues: Array<{type: string, description: string, severity: string}>}>}
 */
async function scanImage(filepath) {
    if (!filepath || typeof filepath !== 'string') {
        throw new Error('filepath must be a non-empty string');
    }

    return scanner.scanImage(filepath);
}

/**
 * Scan an image file for security issues (sync)
 * @param {string} filepath - Path to the image file
 * @returns {{isSafe: boolean, issues: Array<{type: string, description: string, severity: string}>}}
 */
function scanImageSync(filepath) {
    if (!filepath || typeof filepath !== 'string') {
        throw new Error('filepath must be a non-empty string');
    }

    return scanner.scanImageSync(filepath);
}

/**
 * Load custom signatures from JSON file
 * @param {string} filepath - Path to signatures JSON file
 * @returns {{success: boolean, error?: string}}
 */
function loadSignatures(filepath) {
    if (!filepath || typeof filepath !== 'string') {
        throw new Error('filepath must be a non-empty string');
    }

    return scanner.loadSignatures(filepath);
}

module.exports = {
    scanImage,
    scanImageSync,
    loadSignatures
};
