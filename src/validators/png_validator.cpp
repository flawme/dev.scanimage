#include "../../include/validators/png_validator.h"
#include "../../include/metadata/xmp_parser.h"
#include <cstring>
#include <sstream>

namespace ImageScanner {

bool PNGValidator::validate(const std::vector<uint8_t> &data,
                            ScanResultV2 &result, const ScanOptions &options) {
  bool valid = validatePNGStructure(data, result);

  if (options.extractMetadata) {
    result.metadata = extractMetadata(data, options);
  }

  if (options.checkIntegrity) {
    checkTrailingData(data, result, options);
  }

  return valid;
}

bool PNGValidator::validatePNGStructure(const std::vector<uint8_t> &data,
                                        ScanResultV2 &result) {
  // Validate minimum size
  if (data.size() < 33) {
    addIssue(result, IssueV2("malformed_png", "PNG file too small for IHDR",
                             Severity::CRITICAL, IssueCategory::STRUCTURE));
    return false;
  }

  // Check IHDR position (offset 8, after signature)
  if (!(data[12] == 'I' && data[13] == 'H' && data[14] == 'D' &&
        data[15] == 'R')) {
    addIssue(result,
             IssueV2("malformed_png", "IHDR chunk not first in PNG",
                     Severity::CRITICAL, IssueCategory::STRUCTURE, 12, 4));
    return false;
  }

  // Validate chunk structure
  size_t pos = 8;
  bool hasIEND = false;

  while (pos + 12 <= data.size()) {
    uint32_t length = read32BE(data, pos);

    if (length > 0x7FFFFFFF || pos + 12 + length > data.size()) {
      addIssue(result,
               IssueV2("malformed_png", "Invalid PNG chunk length",
                       Severity::CRITICAL, IssueCategory::STRUCTURE, pos, 4));
      break;
    }

    // Check for IEND
    if (data[pos + 4] == 'I' && data[pos + 5] == 'E' && data[pos + 6] == 'N' &&
        data[pos + 7] == 'D') {
      hasIEND = true;
      break;
    }

    // Validate CRC for critical chunks
    if (!validateChunkCRC(data, pos)) {
      char chunkType[5] = {0};
      memcpy(chunkType, &data[pos + 4], 4);
      std::ostringstream oss;
      oss << "Invalid CRC for PNG chunk: " << chunkType;
      addIssue(result, IssueV2("invalid_crc", oss.str(), Severity::WARNING,
                               IssueCategory::STRUCTURE, pos, 12 + length));
    }

    pos += 12 + length;
  }

  if (!hasIEND) {
    addIssue(result, IssueV2("malformed_png", "Missing IEND chunk in PNG",
                             Severity::CRITICAL, IssueCategory::STRUCTURE));
    return false;
  }

  return true;
}

bool PNGValidator::validateChunkCRC(const std::vector<uint8_t> &data,
                                    size_t chunkStart) {
  if (chunkStart + 12 > data.size()) {
    return false;
  }

  uint32_t length = read32BE(data, chunkStart);
  if (chunkStart + 12 + length > data.size()) {
    return false;
  }

  // Calculate CRC for type + data
  uint32_t calculatedCRC =
      crc32(data, chunkStart + 4, 4 + length); // type (4) + data (length)
  uint32_t storedCRC = read32BE(data, chunkStart + 8 + length);

  return calculatedCRC == storedCRC;
}

uint32_t PNGValidator::crc32(const std::vector<uint8_t> &data, size_t start,
                             size_t length) {
  // PNG CRC-32 polynomial: 0xEDB88320
  static uint32_t crcTable[256];
  static bool tableInitialized = false;

  if (!tableInitialized) {
    for (uint32_t i = 0; i < 256; ++i) {
      uint32_t c = i;
      for (int k = 0; k < 8; ++k) {
        if (c & 1) {
          c = 0xEDB88320 ^ (c >> 1);
        } else {
          c = c >> 1;
        }
      }
      crcTable[i] = c;
    }
    tableInitialized = true;
  }

  uint32_t crc = 0xFFFFFFFF;
  for (size_t i = start; i < start + length && i < data.size(); ++i) {
    crc = crcTable[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
  }

  return crc ^ 0xFFFFFFFF;
}

size_t PNGValidator::findChunk(const std::vector<uint8_t> &data,
                               const char *type) {
  size_t pos = 8; // After PNG signature
  while (pos + 12 <= data.size()) {
    uint32_t length = read32BE(data, pos);

    if (memcmp(&data[pos + 4], type, 4) == 0) {
      return pos;
    }

    if (length > 0x7FFFFFFF || pos + 12 + length > data.size()) {
      break;
    }

    pos += 12 + length;
  }
  return std::string::npos;
}

void PNGValidator::checkTrailingData(const std::vector<uint8_t> &data,
                                     ScanResultV2 &result,
                                     const ScanOptions &options) {
  // Find IEND chunk
  size_t iendPos = findChunk(data, "IEND");

  if (iendPos != std::string::npos) {
    uint32_t iendLength = read32BE(data, iendPos);
    size_t eofPos = iendPos + 12 + iendLength; // IEND end

    if (eofPos < data.size()) {
      size_t appendedSize = data.size() - eofPos;

      if (appendedSize > options.maxExtraData) {
        std::ostringstream oss;
        oss << "PNG has " << appendedSize << " bytes appended after IEND";
        addIssue(result,
                 IssueV2("appended_data", oss.str(), Severity::CRITICAL,
                         IssueCategory::STRUCTURE, eofPos, appendedSize));
        result.extraDataSize = appendedSize;
      }
    }

    result.realImageSize = eofPos;
  }
}

MetadataResult PNGValidator::extractMetadata(const std::vector<uint8_t> &data,
                                             const ScanOptions &options) {
  MetadataResult meta;

  // Check for tEXt, zTXt, iTXt chunks (metadata)
  size_t totalMetadata = 0;
  size_t pos = 8;
  bool foundXMP = false;

  while (pos + 12 <= data.size()) {
    uint32_t length = read32BE(data, pos);

    char type[5] = {0};
    memcpy(type, &data[pos + 4], 4);

    if (strcmp(type, "tEXt") == 0 || strcmp(type, "zTXt") == 0 ||
        strcmp(type, "iTXt") == 0) {
      totalMetadata += length;

      // Check for XMP in iTXt chunk
      if (!foundXMP && strcmp(type, "iTXt") == 0) {
        // Use XMPParser to extract and analyze XMP
        Metadata::XMPParser xmpParser;
        MetadataResult xmpResult = xmpParser.extractFromPNG(data);

        if (xmpResult.hasXMP) {
          foundXMP = true;
          meta.hasXMP = true;
          meta.xmpSize = xmpResult.xmpSize;
          meta.xmpWarnings = xmpResult.xmpWarnings;
        }
      }
    }

    if (length > 0x7FFFFFFF || pos + 12 + length > data.size()) {
      break;
    }

    pos += 12 + length;
  }

  meta.metadataSizeBytes = totalMetadata;
  meta.oversizedMetadata = (totalMetadata > 1024 * 1024); // > 1MB

  return meta;
}

} // namespace ImageScanner
