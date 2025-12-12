#ifndef XMP_PARSER_H
#define XMP_PARSER_H

#include "scan_result_v2.h"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

// Forward declare ImageFormat
namespace ImageScanner {
enum class ImageFormat;
}

namespace ImageScanner {
namespace Metadata {

// XMP parser with security analysis
class XMPParser {
public:
  XMPParser();

  // Parse XMP from image data (format-aware)
  MetadataResult parse(const std::vector<uint8_t> &data, ImageFormat format);

  // Format-specific extraction
  MetadataResult extractFromJPEG(const std::vector<uint8_t> &data);
  MetadataResult extractFromPNG(const std::vector<uint8_t> &data);
  MetadataResult extractFromWEBP(const std::vector<uint8_t> &data);

  // Security analysis
  bool hasScriptTags(const std::string &xmpData);
  bool hasBase64Payloads(const std::string &xmpData);
  std::vector<std::string> extractScriptWarnings(const std::string &xmpData);
  std::vector<std::string> extractBase64Warnings(const std::string &xmpData);

  // Sanitization
  std::string sanitizeXMP(const std::string &xmpData);

  // Get last error
  std::string getLastError() const { return lastError_; }

private:
  std::string lastError_;

  // XML validation (lightweight, no external parser)
  bool isWellFormedXML(const std::string &xml);
  bool hasDangerousEntities(const std::string &xml);
  bool isOversized(const std::string &xml);

  // Detection methods
  std::vector<std::string> detectScriptTags(const std::string &xml);
  std::vector<std::string> detectDangerousAttributes(const std::string &xml);
  std::vector<std::string> detectBase64Patterns(const std::string &xml);

  // Sanitization helpers
  std::string removeScriptTags(const std::string &xml);
  std::string stripDangerousAttributes(const std::string &xml);
  std::string removeDoctypeDeclarations(const std::string &xml);

  // Format-specific extraction helpers
  std::optional<std::string> findXMPInJPEG(const std::vector<uint8_t> &data);
  std::optional<std::string> findXMPInPNG(const std::vector<uint8_t> &data);
  std::optional<std::string> findXMPInWEBP(const std::vector<uint8_t> &data);

  // Helper to convert bytes to string
  std::string bytesToString(const std::vector<uint8_t> &data, size_t offset,
                            size_t length);

  // Constants
  static constexpr size_t MAX_XMP_SIZE = 1024 * 1024; // 1MB
  static constexpr size_t MIN_BASE64_LENGTH = 100;    // Min length to flag

  // XMP namespace identifiers
  static constexpr const char *JPEG_XMP_NAMESPACE =
      "http://ns.adobe.com/xap/1.0/\0";
  static constexpr const char *PNG_XMP_KEYWORD = "XML:com.adobe.xmp";
};

} // namespace Metadata
} // namespace ImageScanner

#endif // XMP_PARSER_H
