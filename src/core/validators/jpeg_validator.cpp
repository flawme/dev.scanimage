#include "internal/jpeg_validator.h"
#include "internal/exif_parser.h"
#include "internal/xmp_parser.h"
#include <sstream>

namespace ImageScanner {

bool JPEGValidator::validate(const std::vector<uint8_t> &data,
                             ScanResultV2 &result, const ScanOptions &options) {
  bool valid = validateJPEGStructure(data, result);

  if (options.extractMetadata) {
    result.metadata = extractMetadata(data, options);
  }

  if (options.checkIntegrity) {
    checkTrailingData(data, result, options);
  }

  return valid;
}

bool JPEGValidator::validateJPEGStructure(const std::vector<uint8_t> &data,
                                          ScanResultV2 &result) {
  // Check for EOI marker (FF D9)
  bool hasEOI = false;
  if (data.size() >= 2) {
    for (size_t i = data.size() - 20; i < data.size() - 1; ++i) {
      if (data[i] == 0xFF && data[i + 1] == 0xD9) {
        hasEOI = true;
        break;
      }
    }
  }

  if (!hasEOI) {
    addIssue(result,
             IssueV2("malformed_jpeg", "Missing or invalid JPEG EOI marker",
                     Severity::CRITICAL, IssueCategory::STRUCTURE));
    return false;
  }

  // Validate APP markers
  size_t pos = 2; // After SOI
  while (pos < data.size() - 1 && pos < 1000) {
    if (data[pos] == 0xFF) {
      uint8_t marker = data[pos + 1];
      if (marker == 0xD9)
        break; // EOI
      if (marker == 0x00 || marker == 0xFF) {
        pos++;
        continue;
      }
      if (marker >= 0xD0 && marker <= 0xD7) {
        pos += 2;
        continue;
      }
      // Markers with length
      if (pos + 3 < data.size()) {
        uint16_t length = (data[pos + 2] << 8) | data[pos + 3];
        if (length < 2 || pos + 2 + length > data.size()) {
          addIssue(result,
                   IssueV2("malformed_jpeg", "Invalid JPEG segment length",
                           Severity::CRITICAL, IssueCategory::STRUCTURE, pos,
                           length));
          return false;
        }
        pos += 2 + length;
      } else {
        break;
      }
    } else {
      break;
    }
  }

  return true;
}

size_t JPEGValidator::findEXIFSegment(const std::vector<uint8_t> &data) {
  // EXIF is in APP1 (FFE1) with "Exif\0\0" identifier
  for (size_t i = 0; i < data.size() - 10; ++i) {
    if (data[i] == 0xFF && data[i + 1] == 0xE1) {
      // Check for "Exif\0\0"
      if (i + 10 < data.size() && data[i + 4] == 'E' && data[i + 5] == 'x' &&
          data[i + 6] == 'i' && data[i + 7] == 'f' && data[i + 8] == 0x00 &&
          data[i + 9] == 0x00) {
        return i + 10; // Return offset to TIFF header
      }
    }
  }
  return std::string::npos;
}

size_t JPEGValidator::findXMPSegment(const std::vector<uint8_t> &data) {
  // XMP is in APP1 with "http://ns.adobe.com/xap/1.0/\0" identifier
  for (size_t i = 0; i < data.size() - 35; ++i) {
    if (data[i] == 0xFF && data[i + 1] == 0xE1) {
      std::string identifier(data.begin() + i + 4, data.begin() + i + 33);
      if (identifier == "http://ns.adobe.com/xap/1.0/") {
        return i + 33;
      }
    }
  }
  return std::string::npos;
}

void JPEGValidator::checkTrailingData(const std::vector<uint8_t> &data,
                                      ScanResultV2 &result,
                                      const ScanOptions &options) {
  // Find last EOI marker
  size_t eofPos = std::string::npos;
  for (size_t i = 0; i < data.size() - 1; ++i) {
    if (data[i] == 0xFF && data[i + 1] == 0xD9) {
      eofPos = i + 2;
    }
  }

  if (eofPos != std::string::npos && eofPos < data.size()) {
    size_t appendedSize = data.size() - eofPos;

    if (appendedSize > options.maxExtraData) {
      std::ostringstream oss;
      oss << "JPEG has " << appendedSize << " bytes appended after EOI marker";
      addIssue(result, IssueV2("appended_data", oss.str(), Severity::CRITICAL,
                               IssueCategory::STRUCTURE, eofPos, appendedSize));
      result.extraDataSize = appendedSize;
    }
  }

  result.realImageSize = eofPos != std::string::npos ? eofPos : data.size();
}

MetadataResult JPEGValidator::extractMetadata(const std::vector<uint8_t> &data,
                                              const ScanOptions &options) {
  MetadataResult meta;

  // Check for EXIF
  size_t exifOffset = findEXIFSegment(data);
  if (exifOffset != std::string::npos) {
    meta.hasEXIF = true;

    // Use EXIFParser to extract detailed EXIF data
    Metadata::EXIFParser exifParser;
    MetadataResult exifResult = exifParser.parse(data, exifOffset);

    // Merge results
    meta.hasGPS = exifResult.hasGPS;
    meta.gpsWarnings = exifResult.gpsWarnings;
    meta.exifTags = exifResult.exifTags;
    meta.metadataSizeBytes += exifResult.metadataSizeBytes;
    meta.oversizedMetadata = exifResult.oversizedMetadata;
  }

  // Check for XMP
  size_t xmpOffset = findXMPSegment(data);
  if (xmpOffset != std::string::npos) {
    meta.hasXMP = true;

    // Use XMPParser to extract and analyze XMP data
    Metadata::XMPParser xmpParser;
    MetadataResult xmpResult = xmpParser.extractFromJPEG(data);

    // Merge XMP results
    meta.xmpSize = xmpResult.xmpSize;
    meta.xmpWarnings = xmpResult.xmpWarnings;
    meta.metadataSizeBytes += xmpResult.xmpSize;
  }

  // Calculate total metadata size from APP segments
  size_t totalMetadata = 0;
  for (size_t i = 0; i < data.size() - 4; ++i) {
    if (data[i] == 0xFF) {
      uint8_t marker = data[i + 1];
      // APP0-APP15: 0xE0-0xEF, COM: 0xFE
      if ((marker >= 0xE0 && marker <= 0xEF) || marker == 0xFE) {
        uint16_t segmentLength = (data[i + 2] << 8) | data[i + 3];
        totalMetadata += segmentLength;
        i += segmentLength + 1;
      }
    }
  }

  meta.metadataSizeBytes = totalMetadata;
  meta.oversizedMetadata = (totalMetadata > 1024 * 1024); // > 1MB

  return meta;
}

} // namespace ImageScanner
