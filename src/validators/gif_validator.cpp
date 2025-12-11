#include "../../include/validators/gif_validator.h"
#include <sstream>

namespace ImageScanner {

bool GIFValidator::validate(const std::vector<uint8_t> &data,
                            ScanResultV2 &result, const ScanOptions &options) {
  bool valid = validateGIFStructure(data, result);

  if (options.checkIntegrity) {
    checkTrailingData(data, result, options);
  }

  return valid;
}

bool GIFValidator::validateGIFStructure(const std::vector<uint8_t> &data,
                                        ScanResultV2 &result) {
  // Validate minimum size
  if (data.size() < 13) {
    addIssue(result, IssueV2("malformed_gif", "GIF file too small",
                             Severity::CRITICAL, IssueCategory::STRUCTURE));
    return false;
  }

  // Logical Screen Descriptor at bytes 6-12
  size_t width = data[6] | (data[7] << 8);
  size_t height = data[8] | (data[9] << 8);

  if (width == 0 || height == 0 || width > 65535 || height > 65535) {
    addIssue(result,
             IssueV2("malformed_gif", "Invalid GIF dimensions",
                     Severity::CRITICAL, IssueCategory::STRUCTURE, 6, 4));
    return false;
  }

  // Check for GIF trailer (0x3B) - should exist somewhere
  bool hasTrailer = false;
  for (size_t i = 13; i < data.size(); ++i) {
    if (data[i] == 0x3B) {
      hasTrailer = true;
      break;
    }
  }

  if (!hasTrailer) {
    addIssue(result, IssueV2("malformed_gif", "Missing GIF trailer",
                             Severity::WARNING, IssueCategory::STRUCTURE));
  }

  return true;
}

void GIFValidator::checkTrailingData(const std::vector<uint8_t> &data,
                                     ScanResultV2 &result,
                                     const ScanOptions &options) {
  // Find GIF trailer (0x3B)
  size_t eofPos = std::string::npos;
  for (size_t i = 13; i < data.size(); ++i) {
    if (data[i] == 0x3B) {
      eofPos = i + 1;
    }
  }

  if (eofPos != std::string::npos && eofPos < data.size()) {
    size_t appendedSize = data.size() - eofPos;

    if (appendedSize > options.maxExtraData) {
      std::ostringstream oss;
      oss << "GIF has " << appendedSize << " bytes appended after trailer";
      addIssue(result, IssueV2("appended_data", oss.str(), Severity::CRITICAL,
                               IssueCategory::STRUCTURE, eofPos, appendedSize));
      result.extraDataSize = appendedSize;
    }
  }

  result.realImageSize = eofPos != std::string::npos ? eofPos : data.size();
}

MetadataResult GIFValidator::extractMetadata(const std::vector<uint8_t> &data,
                                             const ScanOptions &options) {
  MetadataResult meta;
  // GIF doesn't typically have EXIF/XMP
  return meta;
}

} // namespace ImageScanner
