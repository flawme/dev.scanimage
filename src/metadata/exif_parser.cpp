#include "../../include/metadata/exif_parser.h"
#include <cstring>
#include <set>
#include <sstream>

namespace ImageScanner {
namespace Metadata {

EXIFParser::EXIFParser() : littleEndian_(true), tiffHeaderOffset_(0) {}

MetadataResult EXIFParser::parse(const std::vector<uint8_t> &data,
                                 size_t offset) {
  MetadataResult result;
  lastError_.clear();
  visitedIFDs_.clear();

  if (offset + 8 > data.size()) {
    lastError_ = "EXIF data too small";
    return result;
  }

  tiffHeaderOffset_ = offset;

  // Read TIFF header (first 8 bytes)
  // Byte order: "II" (0x4949) = little endian, "MM" (0x4D4D) = big endian
  if (data[offset] == 'I' && data[offset + 1] == 'I') {
    littleEndian_ = true;
  } else if (data[offset] == 'M' && data[offset + 1] == 'M') {
    littleEndian_ = false;
  } else {
    lastError_ = "Invalid TIFF header";
    return result;
  }

  // Verify magic number (42)
  uint16_t magic = read16(data, offset + 2);
  if (magic != 42) {
    lastError_ = "Invalid TIFF magic number";
    return result;
  }

  // Get first IFD offset
  uint32_t ifd0Offset = read32(data, offset + 4);

  if (!isValidIFDOffset(ifd0Offset, data.size())) {
    lastError_ = "Invalid IFD0 offset";
    return result;
  }

  result.hasEXIF = true;

  // Parse IFD0 (main image)
  parseMainIFD(data, tiffHeaderOffset_ + ifd0Offset, result);

  return result;
}

void EXIFParser::parseMainIFD(const std::vector<uint8_t> &data,
                              size_t ifdOffset, MetadataResult &result) {
  if (visitedIFDs_.count(ifdOffset)) {
    return; // Circular reference detected
  }
  visitedIFDs_.insert(ifdOffset);

  auto entries = parseIFD(data, ifdOffset);

  for (const auto &entry : entries) {
    // Check for EXIF IFD pointer
    if (entry.tag == TAG_EXIF_IFD_POINTER) {
      uint32_t exifIFDOffset = entry.valueOffset;
      if (isValidIFDOffset(exifIFDOffset, data.size())) {
        parseEXIFIFD(data, tiffHeaderOffset_ + exifIFDOffset, result);
      }
    }
    // Check for GPS IFD pointer
    else if (entry.tag == TAG_GPS_IFD_POINTER) {
      uint32_t gpsIFDOffset = entry.valueOffset;
      if (isValidIFDOffset(gpsIFDOffset, data.size())) {
        result.hasGPS = true;
        parseGPSIFD(data, tiffHeaderOffset_ + gpsIFDOffset, result);
      }
    }
    // Extract key tags
    else if (entry.tag == TAG_MAKE || entry.tag == TAG_MODEL ||
             entry.tag == TAG_DATETIME || entry.tag == TAG_SOFTWARE) {
      std::string value = extractTagValue(data, entry);
      if (!value.empty()) {
        switch (entry.tag) {
        case TAG_MAKE:
          result.exifTags["Make"] = value;
          break;
        case TAG_MODEL:
          result.exifTags["Model"] = value;
          break;
        case TAG_DATETIME:
          result.exifTags["DateTime"] = value;
          break;
        case TAG_SOFTWARE:
          result.exifTags["Software"] = value;
          break;
        }
      }
    }
  }

  // Check for next IFD (IFD1 - thumbnail)
  size_t entryCount = entries.size();
  uint32_t nextIFDOffset = getNextIFDOffset(data, ifdOffset, entryCount);
  if (nextIFDOffset != 0 && isValidIFDOffset(nextIFDOffset, data.size())) {
    // Could parse IFD1 here if needed
  }
}

void EXIFParser::parseEXIFIFD(const std::vector<uint8_t> &data,
                              size_t ifdOffset, MetadataResult &result) {
  if (visitedIFDs_.count(ifdOffset)) {
    return;
  }
  visitedIFDs_.insert(ifdOffset);

  auto entries = parseIFD(data, ifdOffset);

  // Extract EXIF-specific tags
  for (const auto &entry : entries) {
    // Example: ExposureTime, FNumber, ISO, etc.
    // For now, just count metadata size
  }

  result.metadataSizeBytes += entries.size() * 12;
}

void EXIFParser::parseGPSIFD(const std::vector<uint8_t> &data, size_t ifdOffset,
                             MetadataResult &result) {
  if (visitedIFDs_.count(ifdOffset)) {
    return;
  }
  visitedIFDs_.insert(ifdOffset);

  auto entries = parseIFD(data, ifdOffset);

  if (!entries.empty()) {
    result.hasGPS = true;
    result.gpsWarnings.push_back(
        "GPS coordinates detected - may reveal location");
    result.gpsWarnings.push_back("Consider removing GPS data before sharing");
  }

  // Could extract actual lat/lon here
  for (const auto &entry : entries) {
    // GPS tags: 0x0001 (LatitudeRef), 0x0002 (Latitude),
    //           0x0003 (LongitudeRef), 0x0004 (Longitude)
    if (entry.tag == 0x0002 || entry.tag == 0x0004) {
      // Extract coordinates if needed
    }
  }

  result.metadataSizeBytes += entries.size() * 12;
}

std::vector<IFDEntry> EXIFParser::parseIFD(const std::vector<uint8_t> &data,
                                           size_t ifdOffset) {
  std::vector<IFDEntry> entries;

  if (ifdOffset + 2 > data.size()) {
    return entries;
  }

  // Read entry count
  uint16_t entryCount = read16(data, ifdOffset);

  if (entryCount > 1000) { // Sanity check
    return entries;
  }

  size_t entryStart = ifdOffset + 2;

  for (uint16_t i = 0; i < entryCount; ++i) {
    size_t offset = entryStart + (i * 12);

    if (offset + 12 > data.size()) {
      break;
    }

    IFDEntry entry;
    entry.tag = read16(data, offset);
    uint16_t typeValue = read16(data, offset + 2);
    entry.type = static_cast<EXIFDataType>(typeValue);
    entry.count = read32(data, offset + 4);
    entry.valueOffset = read32(data, offset + 8);

    entries.push_back(entry);
  }

  return entries;
}

uint32_t EXIFParser::getNextIFDOffset(const std::vector<uint8_t> &data,
                                      size_t ifdOffset, size_t entryCount) {
  size_t offsetPos = ifdOffset + 2 + (entryCount * 12);

  if (offsetPos + 4 > data.size()) {
    return 0;
  }

  return read32(data, offsetPos);
}

std::string EXIFParser::extractTagValue(const std::vector<uint8_t> &data,
                                        const IFDEntry &entry) {
  // Determine size per element
  size_t elementSize = 1;
  switch (entry.type) {
  case EXIFDataType::BYTE:
  case EXIFDataType::SBYTE:
  case EXIFDataType::UNDEFINED:
    elementSize = 1;
    break;
  case EXIFDataType::SHORT:
  case EXIFDataType::SSHORT:
    elementSize = 2;
    break;
  case EXIFDataType::LONG:
  case EXIFDataType::SLONG:
  case EXIFDataType::FLOAT:
    elementSize = 4;
    break;
  case EXIFDataType::RATIONAL:
  case EXIFDataType::SRATIONAL:
  case EXIFDataType::DOUBLE:
    elementSize = 8;
    break;
  case EXIFDataType::ASCII:
    elementSize = 1;
    break;
  }

  size_t totalSize = elementSize * entry.count;

  // If value fits in 4 bytes, it's stored directly in valueOffset
  if (totalSize <= 4) {
    if (entry.type == EXIFDataType::ASCII) {
      std::string result;
      for (size_t i = 0; i < entry.count && i < 4; ++i) {
        char c = (entry.valueOffset >> (i * 8)) & 0xFF;
        if (c == '\0')
          break;
        result += c;
      }
      return result;
    }
    return "";
  }

  // Otherwise, valueOffset is an actual offset
  size_t valuePos = tiffHeaderOffset_ + entry.valueOffset;

  if (!isValidOffset(valuePos + totalSize, data.size())) {
    return "";
  }

  if (entry.type == EXIFDataType::ASCII) {
    std::string result;
    for (size_t i = 0; i < entry.count; ++i) {
      char c = data[valuePos + i];
      if (c == '\0')
        break;
      result += c;
    }
    return result;
  }

  return "";
}

bool EXIFParser::hasGPS(const std::vector<uint8_t> &data, size_t offset) {
  MetadataResult result = parse(data, offset);
  return result.hasGPS;
}

std::vector<std::string>
EXIFParser::extractGPSWarnings(const std::vector<uint8_t> &data,
                               size_t offset) {
  MetadataResult result = parse(data, offset);
  return result.gpsWarnings;
}

void EXIFParser::detectOversizedMetadata(const MetadataResult &meta,
                                         ScanResultV2 &result,
                                         size_t totalFileSize) {
  if (meta.metadataSizeBytes > 1024 * 1024) { // > 1MB
    std::ostringstream oss;
    oss << "Oversized EXIF metadata: " << (meta.metadataSizeBytes / 1024)
        << " KB";
    result.addIssue(IssueV2("oversized_exif", oss.str(), Severity::WARNING,
                            IssueCategory::METADATA, 0,
                            meta.metadataSizeBytes));
  }

  // Check if metadata is > 10% of file size
  if (totalFileSize > 0 && meta.metadataSizeBytes > (totalFileSize / 10)) {
    std::ostringstream oss;
    oss << "EXIF metadata is "
        << ((meta.metadataSizeBytes * 100) / totalFileSize)
        << "% of file size (suspicious)";
    result.addIssue(IssueV2("suspicious_exif_ratio", oss.str(),
                            Severity::WARNING, IssueCategory::METADATA, 0,
                            meta.metadataSizeBytes));
  }
}

void EXIFParser::detectMalformedOffsets(const std::vector<uint8_t> &data,
                                        size_t offset, ScanResultV2 &result) {
  if (offset + 8 > data.size()) {
    return;
  }

  tiffHeaderOffset_ = offset;

  // Read endianness
  if (data[offset] == 'I' && data[offset + 1] == 'I') {
    littleEndian_ = true;
  } else if (data[offset] == 'M' && data[offset + 1] == 'M') {
    littleEndian_ = false;
  } else {
    return;
  }

  // Get first IFD offset
  uint32_t ifd0Offset = read32(data, offset + 4);

  // Check if offset is out of bounds
  if (ifd0Offset > data.size() - offset) {
    result.addIssue(
        IssueV2("malformed_exif", "EXIF IFD offset points beyond file",
                Severity::CRITICAL, IssueCategory::METADATA, offset + 4, 4));
  }

  // Check for circular references (would be detected during parse)
  visitedIFDs_.clear();
  if (isValidIFDOffset(ifd0Offset, data.size())) {
    parseIFD(data, tiffHeaderOffset_ + ifd0Offset);
    if (visitedIFDs_.size() > 10) {
      result.addIssue(
          IssueV2("suspicious_exif", "Excessive IFD chain (possible loop)",
                  Severity::WARNING, IssueCategory::METADATA, offset, 0));
    }
  }
}

uint16_t EXIFParser::read16(const std::vector<uint8_t> &data, size_t offset) {
  if (offset + 1 >= data.size())
    return 0;

  if (littleEndian_) {
    return static_cast<uint16_t>(data[offset]) |
           (static_cast<uint16_t>(data[offset + 1]) << 8);
  } else {
    return (static_cast<uint16_t>(data[offset]) << 8) |
           static_cast<uint16_t>(data[offset + 1]);
  }
}

uint32_t EXIFParser::read32(const std::vector<uint8_t> &data, size_t offset) {
  if (offset + 3 >= data.size())
    return 0;

  if (littleEndian_) {
    return static_cast<uint32_t>(data[offset]) |
           (static_cast<uint32_t>(data[offset + 1]) << 8) |
           (static_cast<uint32_t>(data[offset + 2]) << 16) |
           (static_cast<uint32_t>(data[offset + 3]) << 24);
  } else {
    return (static_cast<uint32_t>(data[offset]) << 24) |
           (static_cast<uint32_t>(data[offset + 1]) << 16) |
           (static_cast<uint32_t>(data[offset + 2]) << 8) |
           static_cast<uint32_t>(data[offset + 3]);
  }
}

bool EXIFParser::isValidOffset(size_t offset, size_t dataSize) {
  return offset < dataSize;
}

bool EXIFParser::isValidIFDOffset(uint32_t offset, size_t dataSize) {
  size_t absoluteOffset = tiffHeaderOffset_ + offset;
  return absoluteOffset + 2 <= dataSize; // At least entry count readable
}

} // namespace Metadata
} // namespace ImageScanner
