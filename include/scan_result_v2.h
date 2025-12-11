#ifndef SCAN_RESULT_V2_H
#define SCAN_RESULT_V2_H

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace ImageScanner {

// Severity levels for v2.0
enum class Severity { INFO, WARNING, HIGH, CRITICAL };

// Convert severity to string
inline const char *severityToString(Severity s) {
  switch (s) {
  case Severity::INFO:
    return "info";
  case Severity::WARNING:
    return "warning";
  case Severity::HIGH:
    return "high";
  case Severity::CRITICAL:
    return "critical";
  default:
    return "unknown";
  }
}

// Issue category for v2.0
enum class IssueCategory { METADATA, SCRIPT, EMBEDDED_FILE, XML, STRUCTURE };

// Convert category to string
inline const char *categoryToString(IssueCategory c) {
  switch (c) {
  case IssueCategory::METADATA:
    return "metadata";
  case IssueCategory::SCRIPT:
    return "script";
  case IssueCategory::EMBEDDED_FILE:
    return "embedded-file";
  case IssueCategory::XML:
    return "xml";
  case IssueCategory::STRUCTURE:
    return "structure";
  default:
    return "unknown";
  }
}

// Enhanced Issue v2 with detailed metadata
struct IssueV2 {
  std::string type;        // e.g., "high_entropy", "gps_detected"
  std::string description; // Human-readable description
  Severity severity;       // INFO, WARNING, HIGH, CRITICAL
  IssueCategory category;  // metadata, script, embedded-file, xml, structure
  size_t offset;           // Byte offset where issue was found
  size_t length;           // Length of problematic data
  std::map<std::string, std::string> metadata; // Additional context

  IssueV2(const std::string &t, const std::string &d, Severity s,
          IssueCategory c, size_t off = 0, size_t len = 0)
      : type(t), description(d), severity(s), category(c), offset(off),
        length(len) {}
};

// Metadata analysis result
struct MetadataResult {
  bool hasEXIF;
  bool hasXMP;
  bool hasGPS;
  std::vector<std::string> gpsWarnings;
  std::map<std::string, std::string> exifTags; // Key EXIF fields
  std::string xmpContent;                      // Sanitized XMP content
  size_t metadataSizeBytes;
  bool oversizedMetadata;
  size_t xmpSize;                       // XMP data size
  std::vector<std::string> xmpWarnings; // XMP security warnings

  MetadataResult()
      : hasEXIF(false), hasXMP(false), hasGPS(false), metadataSizeBytes(0),
        oversizedMetadata(false), xmpSize(0) {}
};

// Enhanced ScanResult v2
struct ScanResultV2 {
  bool isSafe;
  std::vector<IssueV2> issues;

  // Enhanced metadata
  std::string format;      // "jpeg", "png", "webp", etc.
  size_t realImageSize;    // Actual image data size
  size_t extraDataSize;    // Trailing/appended data size
  MetadataResult metadata; // Metadata analysis

  // Statistics
  double scanTimeMs;
  std::string scannerVersion; // "2.0.0"

  ScanResultV2()
      : isSafe(true), realImageSize(0), extraDataSize(0), scanTimeMs(0.0),
        scannerVersion("2.0.0") {}

  // Add an issue and update isSafe flag
  void addIssue(const IssueV2 &issue) {
    issues.push_back(issue);
    // CRITICAL or HIGH severity marks as unsafe
    if (issue.severity == Severity::CRITICAL ||
        issue.severity == Severity::HIGH) {
      isSafe = false;
    }
  }

  // Convert to JSON string
  std::string toJSON() const;
};

} // namespace ImageScanner

#endif // SCAN_RESULT_V2_H
