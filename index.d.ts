export interface Issue {
    type: string;
    description: string;
    severity: 'info' | 'warning' | 'critical';
}

export interface ScanResult {
    isSafe: boolean;
    issues: Issue[];
}

/**
 * Scan an image file for security issues (async)
 * @param filepath - Path to the image file
 * @returns Promise resolving to scan result
 */
export function scanImage(filepath: string): Promise<ScanResult>;

/**
 * Scan an image file for security issues (sync)
 * @param filepath - Path to the image file
 * @returns Scan result
 */
export function scanImageSync(filepath: string): ScanResult;

export interface LoadSignaturesResult {
    success: boolean;
    error?: string;
}

/**
 * Load custom signatures from JSON file
 * @param filepath - Path to signatures JSON file
 * @returns Result indicating success and any error message
 */
export function loadSignatures(filepath: string): LoadSignaturesResult;
