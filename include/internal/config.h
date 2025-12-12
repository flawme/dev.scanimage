#ifndef CONFIG_H
#define CONFIG_H

#include <cstddef>
#include <string>

namespace ImageScanner {

// Scanning options for v2.0
struct ScanOptions {
  // Metadata options
  bool strictMetadata;  // Treat metadata issues as critical
  bool allowGPS;        // Allow GPS without warning
  size_t maxExtraData;  // Max trailing data (bytes)
  bool extractMetadata; // Parse EXIF/XMP

  // SVG options
  bool svgSanitize;     // Strip scripts from SVG
  bool svgAllowDataURI; // Allow data: URIs in SVG

  // Performance options
  bool useMmap;              // Enable mmap for large files
  size_t mmapThreshold;      // File size threshold for mmap (bytes)
  bool slidingWindowEntropy; // Use fast sliding-window entropy

  // Signature options
  bool enableCustomSignatures; // Enable custom signature loading
  bool mergeWithBuiltin;       // Merge custom + builtin signatures
  std::string signatureFile;   // Path to v2 signatures JSON
  bool useSignatureV2;         // Use v2 engine (default: true)
  bool enableWildcards;        // Enable wildcard matching

  // False-positive reduction
  bool reduceFalsePositives;    // Enable FP reduction (default: true)
  float confidenceThreshold;    // Min confidence to report (0.0-1.0)
  bool adjustSeverity;          // Downgrade low-confidence findings
  bool whitelistCommonPatterns; // Whitelist known safe patterns

  // Analysis toggles
  bool checkPolyglot;   // Check for polyglot files
  bool checkEntropy;    // Check entropy
  bool checkIntegrity;  // Check integrity
  bool checkSignatures; // Check signatures

  // Legacy mode
  bool legacyMode; // Return v1.5-compatible results

  // Default constructor with sensible defaults
  ScanOptions()
      : strictMetadata(true), allowGPS(false), maxExtraData(1024),
        extractMetadata(true), svgSanitize(true), svgAllowDataURI(false),
        useMmap(true), mmapThreshold(1024 * 1024), // 1MB
        slidingWindowEntropy(true), enableCustomSignatures(true),
        mergeWithBuiltin(true), signatureFile(""), useSignatureV2(true),
        enableWildcards(true), reduceFalsePositives(true),
        confidenceThreshold(0.7f), adjustSeverity(true),
        whitelistCommonPatterns(true), checkPolyglot(true), checkEntropy(true),
        checkIntegrity(true), checkSignatures(true), legacyMode(false) {}
};

// Threshold configuration
struct ScanThresholds {
  double entropyThreshold; // Entropy threshold (bits/byte)
  size_t chunkSize;        // Chunk size for analysis
  size_t maxFileSize;      // Maximum file size (bytes)

  ScanThresholds()
      : entropyThreshold(7.5), chunkSize(4096), maxFileSize(100 * 1024 * 1024) {
  } // 100MB
};

} // namespace ImageScanner

#endif // CONFIG_H
