#include "../../include/validators/webp_validator.h"
#include "../../include/metadata/xmp_parser.h"
#include <sstream>

namespace ImageScanner {

bool WEBPValidator::validate(const std::vector<uint8_t> &data,
                             ScanResultV2 &result, const ScanOptions &options) {
  bool valid = validateWEBPStructure(data, result);

  if (options.extractMetadata) {
    result.metadata = extractMetadata(data, options);
  }

  return valid;
}

bool WEBPValidator::validateWEBPStructure(const std::vector<uint8_t> &data,
                                          ScanResultV2 &result) {
  // Validate minimum size
  if (data.size() < 12) {
    addIssue(result, IssueV2("malformed_webp", "WEBP file too small",
                             Severity::CRITICAL, IssueCategory::STRUCTURE));
    return false;
  }

  // Validate RIFF size
  uint32_t fileSize = read32LE(data, 4);
  if (fileSize + 8 != data.size()) {
    std::ostringstream oss;
    oss << "WEBP RIFF size mismatch (expected " << (fileSize + 8) << ", got "
        << data.size() << ")";
    addIssue(result, IssueV2("malformed_webp", oss.str(), Severity::WARNING,
                             IssueCategory::STRUCTURE, 4, 4));
  }

  // Check for VP8/VP8L/VP8X chunk
  if (data.size() >= 16) {
    bool validChunk = (data[12] == 'V' && data[13] == 'P' && data[14] == '8') &&
                      (data[15] == ' ' || data[15] == 'L' || data[15] == 'X');
    if (!validChunk) {
      addIssue(result,
               IssueV2("malformed_webp", "Invalid WEBP chunk type",
                       Severity::CRITICAL, IssueCategory::STRUCTURE, 12, 4));
      return false;
    }
  }

  return true;
}

size_t WEBPValidator::findEXIFChunk(const std::vector<uint8_t> &data) {
  // WEBP EXIF is in EXIF chunk
  size_t pos = 12; // After RIFF header and WEBP
  while (pos + 8 <= data.size()) {
    if (data[pos] == 'E' && data[pos + 1] == 'X' && data[pos + 2] == 'I' &&
        data[pos + 3] == 'F') {
      return pos + 8; // Return offset to EXIF data
    }
    // Move to next chunk
    uint32_t chunkSize = read32LE(data, pos + 4);
    pos += 8 + chunkSize;
    if (chunkSize % 2)
      pos++; // Padding
  }
  return std::string::npos;
}

size_t WEBPValidator::findXMPChunk(const std::vector<uint8_t> &data) {
  // WEBP XMP is in XMP chunk
  size_t pos = 12;
  while (pos + 8 <= data.size()) {
    if (data[pos] == 'X' && data[pos + 1] == 'M' && data[pos + 2] == 'P' &&
        data[pos + 3] == ' ') {
      return pos + 8;
    }
    uint32_t chunkSize = read32LE(data, pos + 4);
    pos += 8 + chunkSize;
    if (chunkSize % 2)
      pos++;
  }
  return std::string::npos;
}

MetadataResult WEBPValidator::extractMetadata(const std::vector<uint8_t> &data,
                                              const ScanOptions &options) {
  MetadataResult meta;

  // Check for EXIF chunk
  size_t exifOffset = findEXIFChunk(data);
  if (exifOffset != std::string::npos) {
    meta.hasEXIF = true;
    // EXIF parsing with EXIFParser would go here (Phase 3)
  }

  // Check for XMP chunk
  size_t xmpOffset = findXMPChunk(data);
  if (xmpOffset != std::string::npos) {
    meta.hasXMP = true;

    // Use XMPParser to extract and analyze XMP data
    Metadata::XMPParser xmpParser;
    MetadataResult xmpResult = xmpParser.extractFromWEBP(data);

    // Merge XMP results
    meta.xmpSize = xmpResult.xmpSize;
    meta.xmpWarnings = xmpResult.xmpWarnings;
    meta.metadataSizeBytes += xmpResult.xmpSize;
  }

  return meta;
}

} // namespace ImageScanner
