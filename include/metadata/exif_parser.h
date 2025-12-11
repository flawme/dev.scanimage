#ifndef EXIF_PARSER_H
#define EXIF_PARSER_H

#include "../scan_result_v2.h"
#include <cstddef>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace ImageScanner {
namespace Metadata {

// EXIF data types (from TIFF spec)
enum class EXIFDataType {
  BYTE = 1,
  ASCII = 2,
  SHORT = 3,
  LONG = 4,
  RATIONAL = 5,
  SBYTE = 6,
  UNDEFINED = 7,
  SSHORT = 8,
  SLONG = 9,
  SRATIONAL = 10,
  FLOAT = 11,
  DOUBLE = 12
};

// IFD entry structure (12 bytes)
struct IFDEntry {
  uint16_t tag;
  EXIFDataType type;
  uint32_t count;
  uint32_t valueOffset; // Or value itself if fits in 4 bytes

  IFDEntry() : tag(0), type(EXIFDataType::BYTE), count(0), valueOffset(0) {}
};

// EXIF parser with IFD navigation
class EXIFParser {
public:
  EXIFParser();

  // Parse EXIF from JPEG APP1 or TIFF file
  // offset = start of TIFF header
  MetadataResult parse(const std::vector<uint8_t> &data, size_t offset);

  // GPS detection
  bool hasGPS(const std::vector<uint8_t> &data, size_t offset);
  std::vector<std::string> extractGPSWarnings(const std::vector<uint8_t> &data,
                                              size_t offset);

  // Malicious EXIF detection
  void detectOversizedMetadata(const MetadataResult &meta, ScanResultV2 &result,
                               size_t totalFileSize);
  void detectMalformedOffsets(const std::vector<uint8_t> &data, size_t offset,
                              ScanResultV2 &result);

  // Get last error
  std::string getLastError() const { return lastError_; }

private:
  std::string lastError_;
  bool littleEndian_;
  size_t tiffHeaderOffset_;
  std::set<uint32_t> visitedIFDs_; // Track visited IFDs to detect loops

  // IFD navigation
  std::vector<IFDEntry> parseIFD(const std::vector<uint8_t> &data,
                                 size_t ifdOffset);
  uint32_t getNextIFDOffset(const std::vector<uint8_t> &data, size_t ifdOffset,
                            size_t entryCount);

  // Parse specific IFDs
  void parseMainIFD(const std::vector<uint8_t> &data, size_t ifdOffset,
                    MetadataResult &result);
  void parseEXIFIFD(const std::vector<uint8_t> &data, size_t ifdOffset,
                    MetadataResult &result);
  void parseGPSIFD(const std::vector<uint8_t> &data, size_t ifdOffset,
                   MetadataResult &result);

  // Value extraction
  std::string extractTagValue(const std::vector<uint8_t> &data,
                              const IFDEntry &entry);
  std::string formatRational(const std::vector<uint8_t> &data, size_t offset);

  // Endianness handling
  uint16_t read16(const std::vector<uint8_t> &data, size_t offset);
  uint32_t read32(const std::vector<uint8_t> &data, size_t offset);

  // Validation
  bool isValidOffset(size_t offset, size_t dataSize);
  bool isValidIFDOffset(uint32_t offset, size_t dataSize);

  // Well-known EXIF tags
  static const uint16_t TAG_EXIF_IFD_POINTER = 0x8769;
  static const uint16_t TAG_GPS_IFD_POINTER = 0x8825;
  static const uint16_t TAG_MAKE = 0x010F;
  static const uint16_t TAG_MODEL = 0x0110;
  static const uint16_t TAG_DATETIME = 0x0132;
  static const uint16_t TAG_ORIENTATION = 0x0112;
  static const uint16_t TAG_SOFTWARE = 0x0131;
};

} // namespace Metadata
} // namespace ImageScanner

#endif // EXIF_PARSER_H
