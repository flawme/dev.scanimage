#ifndef IMAGE_SCANNER_H
#define IMAGE_SCANNER_H

#include <cstdint>
#include <string>
#include <vector>

namespace ImageScanner {

// Issue severity levels
enum class Severity { INFO, WARNING, CRITICAL };

// Detected issue
struct Issue {
  std::string type;
  std::string description;
  Severity severity;

  Issue(const std::string &t, const std::string &d,
        Severity s = Severity::WARNING)
      : type(t), description(d), severity(s) {}
};

// Scan result
struct ScanResult {
  bool isSafe;
  std::vector<Issue> issues;

  ScanResult() : isSafe(true) {}

  void addIssue(const Issue &issue) {
    issues.push_back(issue);
    if (issue.severity == Severity::CRITICAL) {
      isSafe = false;
    }
  }

  std::string toJSON() const;
};

// Image format types
enum class ImageFormat { UNKNOWN, JPEG, PNG, WEBP, GIF, BMP, TIFF, SVG };

// Main scanner class
class Scanner {
public:
  Scanner();
  ~Scanner();

  // Scan an image file
  ScanResult scan(const std::string &filepath);

  // Load signatures from JSON file
  // Returns true on success, false on error (but continues with defaults)
  bool loadSignatures(const std::string &filepath);

  // Add custom signature to detect (programmatic API)
  void addSignature(const std::string &name,
                    const std::vector<uint8_t> &pattern);

  // Get last error message from signature loading
  std::string getLastError() const;

private:
  ImageFormat detectFormat(const std::vector<uint8_t> &data);
  void validateFormat(const std::vector<uint8_t> &data, ImageFormat format,
                      ScanResult &result);
  void detectPolyglot(const std::vector<uint8_t> &data, ScanResult &result);
  void analyzeEntropy(const std::vector<uint8_t> &data, ScanResult &result);
  void scanSignatures(const std::vector<uint8_t> &data, ScanResult &result);
  void checkIntegrity(const std::vector<uint8_t> &data, ImageFormat format,
                      ScanResult &result);

  // Signature management
  void loadDefaultSignatures();
  std::vector<uint8_t> parseSignatureString(const std::string &str);

  struct Impl;
  Impl *pImpl;
};

} // namespace ImageScanner

#endif // IMAGE_SCANNER_H
