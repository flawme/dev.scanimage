#include "internal/bmp_validator.h"
#include <cmath>
#include <sstream>

namespace ImageScanner {

bool BMPValidator::validate(const std::vector<uint8_t> &data,
                            ScanResultV2 &result, const ScanOptions &options) {
  bool valid = validateBMPStructure(data, result);

  if (options.checkIntegrity) {
    checkTrailingData(data, result, options);
  }

  return valid;
}

bool BMPValidator::validateBMPStructure(const std::vector<uint8_t> &data,
                                        ScanResultV2 &result) {
  // Validate minimum size (54 bytes for BITMAPINFOHEADER)
  if (data.size() < 54) {
    addIssue(result, IssueV2("malformed_bmp", "BMP file too small",
                             Severity::CRITICAL, IssueCategory::STRUCTURE));
    return false;
  }

  // File size (bytes 2-5)
  uint32_t fileSize = read32LE(data, 2);
  if (fileSize != data.size() && fileSize > 0) {
    std::ostringstream oss;
    oss << "BMP file size mismatch (header says " << fileSize << ", actual "
        << data.size() << ")";
    addIssue(result, IssueV2("malformed_bmp", oss.str(), Severity::WARNING,
                             IssueCategory::STRUCTURE, 2, 4));
  }

  // DIB header size (bytes 14-17)
  uint32_t dibSize = read32LE(data, 14);
  if (dibSize != 40 && dibSize != 108 &&
      dibSize != 124) { // Common sizes: BITMAPINFOHEADER, V4, V5
    std::ostringstream oss;
    oss << "Unusual BMP header size: " << dibSize;
    addIssue(result, IssueV2("malformed_bmp", oss.str(), Severity::WARNING,
                             IssueCategory::STRUCTURE, 14, 4));
  }

  // Width and height (bytes 18-25)
  int32_t width = static_cast<int32_t>(read32LE(data, 18));
  int32_t height = static_cast<int32_t>(read32LE(data, 22));

  if (width <= 0 || std::abs(height) == 0 || width > 65535 ||
      std::abs(height) > 65535) {
    addIssue(result,
             IssueV2("malformed_bmp", "Invalid BMP dimensions",
                     Severity::CRITICAL, IssueCategory::STRUCTURE, 18, 8));
    return false;
  }

  return true;
}

void BMPValidator::checkTrailingData(const std::vector<uint8_t> &data,
                                     ScanResultV2 &result,
                                     const ScanOptions &options) {
  // BMP files should match their declared file size
  uint32_t declaredSize = read32LE(data, 2);

  if (declaredSize > 0 && declaredSize < data.size()) {
    size_t appendedSize = data.size() - declaredSize;

    if (appendedSize > options.maxExtraData) {
      std::ostringstream oss;
      oss << "BMP has " << appendedSize << " bytes beyond declared size";
      addIssue(result,
               IssueV2("appended_data", oss.str(), Severity::CRITICAL,
                       IssueCategory::STRUCTURE, declaredSize, appendedSize));
      result.extraDataSize = appendedSize;
    }
  }

  result.realImageSize = declaredSize > 0 ? declaredSize : data.size();
}

MetadataResult BMPValidator::extractMetadata(const std::vector<uint8_t> &data,
                                             const ScanOptions &options) {
  MetadataResult meta;
  // BMP doesn't typically have EXIF/XMP
  return meta;
}

} // namespace ImageScanner
