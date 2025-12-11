import { scanImage, scanImageSync, ScanResult, Issue } from 'safeimg';

// Example 1: Basic async usage with types
async function validateImage(filepath: string): Promise<boolean> {
    const result: ScanResult = await scanImage(filepath);

    if (!result.isSafe) {
        console.log('Security issues detected:');
        result.issues.forEach((issue: Issue) => {
            console.log(`- [${issue.severity}] ${issue.type}: ${issue.description}`);
        });
        return false;
    }

    return true;
}

// Example 2: Express.js with TypeScript
import express, { Request, Response } from 'express';
import multer from 'multer';

const app = express();
const upload = multer({ dest: 'uploads/' });

app.post('/upload', upload.single('image'), async (req: Request, res: Response) => {
    if (!req.file) {
        return res.status(400).json({ error: 'No file uploaded' });
    }

    const result: ScanResult = await scanImage(req.file.path);

    if (!result.isSafe) {
        return res.status(400).json({
            error: 'Malicious file detected',
            issues: result.issues
        });
    }

    res.json({ success: true });
});

// Example 3: Type-safe issue filtering
async function getCriticalIssues(filepath: string): Promise<Issue[]> {
    const result = await scanImage(filepath);
    return result.issues.filter(issue => issue.severity === 'critical');
}

// Example 4: Synchronous scan with error handling
function scanImageSafely(filepath: string): ScanResult | null {
    try {
        return scanImageSync(filepath);
    } catch (error) {
        console.error('Scan failed:', error);
        return null;
    }
}

export { validateImage, getCriticalIssues, scanImageSafely };
