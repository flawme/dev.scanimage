#ifndef BASE_VALIDATOR_H
#define BASE_VALIDATOR_H

#include "../config.h"
#include "../scan_result_v2.h"
#include <cstdint>
#include <string>
#include <vector>

namespace ImageScanner {

// Abstract base class for all format validators
class BaseValidator {
public:
  virtual ~BaseValidator() = default;

  // Pure virtual methods - must be implemented by subclasses
  virtual bool validate(const std::vector<uint8_t> &data, ScanResultV2 &result,
                        const ScanOptions &options) = 0;
  virtual MetadataResult extractMetadata(const std::vector<uint8_t> &data,
                                         const ScanOptions &options) = 0;
  virtual std::string getFormatName() const = 0;

protected:
  // Common utility methods for all validators

  // Add an issue to the result
  void addIssue(ScanResultV2 &result, const IssueV2 &issue) {
    result.addIssue(issue);
  }

  // Check if data starts with expected magic bytes
  bool checkMagicBytes(const std::vector<uint8_t> &data,
                       const std::vector<uint8_t> &expected) {
    if (data.size() < expected.size()) {
      return false;
    }
    for (size_t i = 0; i < expected.size(); ++i) {
      if (data[i] != expected[i]) {
        return false;
      }
    }
    return true;
  }

  // Find pattern in data starting from offset
  size_t findPattern(const std::vector<uint8_t> &data,
                     const std::vector<uint8_t> &pattern,
                     size_t startOffset = 0) {
    if (pattern.empty() || data.size() < pattern.size()) {
      return std::string::npos;
    }

    for (size_t i = startOffset; i <= data.size() - pattern.size(); ++i) {
      bool match = true;
      for (size_t j = 0; j < pattern.size(); ++j) {
        if (data[i + j] != pattern[j]) {
          match = false;
          break;
        }
      }
      if (match) {
        return i;
      }
    }
    return std::string::npos;
  }

  // Read 16-bit value (big endian)
  uint16_t read16BE(const std::vector<uint8_t> &data, size_t offset) {
    if (offset + 1 >= data.size())
      return 0;
    return (static_cast<uint16_t>(data[offset]) << 8) |
           static_cast<uint16_t>(data[offset + 1]);
  }

  // Read 16-bit value (little endian)
  uint16_t read16LE(const std::vector<uint8_t> &data, size_t offset) {
    if (offset + 1 >= data.size())
      return 0;
    return static_cast<uint16_t>(data[offset]) |
           (static_cast<uint16_t>(data[offset + 1]) << 8);
  }

  // Read 32-bit value (big endian)
  uint32_t read32BE(const std::vector<uint8_t> &data, size_t offset) {
    if (offset + 3 >= data.size())
      return 0;
    return (static_cast<uint32_t>(data[offset]) << 24) |
           (static_cast<uint32_t>(data[offset + 1]) << 16) |
           (static_cast<uint32_t>(data[offset + 2]) << 8) |
           static_cast<uint32_t>(data[offset + 3]);
  }

  // Read 32-bit value (little endian)
  uint32_t read32LE(const std::vector<uint8_t> &data, size_t offset) {
    if (offset + 3 >= data.size())
      return 0;
    return static_cast<uint32_t>(data[offset]) |
           (static_cast<uint32_t>(data[offset + 1]) << 8) |
           (static_cast<uint32_t>(data[offset + 2]) << 16) |
           (static_cast<uint32_t>(data[offset + 3]) << 24);
  }
};

// Image format enumeration
enum class ImageFormat { UNKNOWN, JPEG, PNG, WEBP, GIF, BMP, TIFF, SVG };

// Convert format to string
inline const char *formatToString(ImageFormat f) {
  switch (f) {
  case ImageFormat::JPEG:
    return "jpeg";
  case ImageFormat::PNG:
    return "png";
  case ImageFormat::WEBP:
    return "webp";
  case ImageFormat::GIF:
    return "gif";
  case ImageFormat::BMP:
    return "bmp";
  case ImageFormat::TIFF:
    return "tiff";
  case ImageFormat::SVG:
    return "svg";
  default:
    return "unknown";
  }
}

} // namespace ImageScanner

#endif // BASE_VALIDATOR_H
