#include "../../include/validators/tiff_validator.h"
#include "../../include/metadata/exif_parser.h"
#include <sstream>

namespace ImageScanner {

bool TIFFValidator::validate(const std::vector<uint8_t> &data,
                             ScanResultV2 &result, const ScanOptions &options) {
  bool valid = validateTIFFStructure(data, result);

  if (options.extractMetadata) {
    result.metadata = extractMetadata(data, options);
  }

  return valid;
}

bool TIFFValidator::validateTIFFStructure(const std::vector<uint8_t> &data,
                                          ScanResultV2 &result) {
  // Validate minimum size
  if (data.size() < 8) {
    addIssue(result, IssueV2("malformed_tiff", "TIFF file too small",
                             Severity::CRITICAL, IssueCategory::STRUCTURE));
    return false;
  }

  // Detect endianness
  bool littleEndian = (data[0] == 'I' && data[1] == 'I');
  if (!littleEndian && !(data[0] == 'M' && data[1] == 'M')) {
    addIssue(result,
             IssueV2("malformed_tiff", "Invalid TIFF byte order",
                     Severity::CRITICAL, IssueCategory::STRUCTURE, 0, 2));
    return false;
  }

  // Verify magic number (42)
  uint16_t magic = littleEndian ? (read16LE(data, 2)) : (read16BE(data, 2));
  if (magic != 42) {
    addIssue(result,
             IssueV2("malformed_tiff", "Invalid TIFF magic number",
                     Severity::CRITICAL, IssueCategory::STRUCTURE, 2, 2));
    return false;
  }

  // Get IFD offset
  uint32_t ifdOffset = littleEndian ? (read32LE(data, 4)) : (read32BE(data, 4));

  if (ifdOffset < 8 || ifdOffset >= data.size()) {
    addIssue(result,
             IssueV2("malformed_tiff", "Invalid TIFF IFD offset",
                     Severity::CRITICAL, IssueCategory::STRUCTURE, 4, 4));
    return false;
  }

  // Basic IFD validation
  if (ifdOffset + 2 <= data.size()) {
    uint16_t numEntries = littleEndian ? (read16LE(data, ifdOffset))
                                       : (read16BE(data, ifdOffset));

    if (numEntries > 1000) { // Sanity check
      addIssue(result,
               IssueV2("malformed_tiff", "TIFF has excessive IFD entries",
                       Severity::WARNING, IssueCategory::STRUCTURE, ifdOffset,
                       2));
    }
  }

  return true;
}

MetadataResult TIFFValidator::extractMetadata(const std::vector<uint8_t> &data,
                                              const ScanOptions &options) {
  MetadataResult meta;

  // TIFF files start with TIFF header (which is also EXIF format)
  // Use EXIFParser directly on the file
  Metadata::EXIFParser exifParser;
  MetadataResult exifResult = exifParser.parse(data, 0);

  // Merge results
  meta.hasEXIF = true;
  meta.hasGPS = exifResult.hasGPS;
  meta.gpsWarnings = exifResult.gpsWarnings;
  meta.exifTags = exifResult.exifTags;
  meta.metadataSizeBytes = exifResult.metadataSizeBytes;
  meta.oversizedMetadata = exifResult.oversizedMetadata;

  return meta;
}

} // namespace ImageScanner
