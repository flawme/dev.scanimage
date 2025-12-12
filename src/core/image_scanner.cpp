#include "internal/image_scanner.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <map>
#include <sstream>

namespace ImageScanner {

// Implementation details
struct Scanner::Impl {
  std::map<std::string, std::vector<uint8_t>> customSignatures;
  std::string lastError;
  bool signaturesLoaded;

  Impl() : signaturesLoaded(false) {}
};

Scanner::Scanner() : pImpl(new Impl()) { loadDefaultSignatures(); }

Scanner::~Scanner() { delete pImpl; }

std::string ScanResult::toJSON() const {
  std::ostringstream oss;
  oss << "{\n";
  oss << "  \"isSafe\": " << (isSafe ? "true" : "false") << ",\n";
  oss << "  \"issues\": [";

  for (size_t i = 0; i < issues.size(); ++i) {
    if (i > 0)
      oss << ",";
    oss << "\n    {\n";
    oss << "      \"type\": \"" << issues[i].type << "\",\n";
    oss << "      \"description\": \"" << issues[i].description << "\",\n";
    oss << "      \"severity\": \"";
    switch (issues[i].severity) {
    case Severity::INFO:
      oss << "info";
      break;
    case Severity::WARNING:
      oss << "warning";
      break;
    case Severity::CRITICAL:
      oss << "critical";
      break;
    }
    oss << "\"\n    }";
  }

  oss << "\n  ]\n}";
  return oss.str();
}

ScanResult Scanner::scan(const std::string &filepath) {
  ScanResult result;

  // Read file into memory
  std::ifstream file(filepath, std::ios::binary | std::ios::ate);
  if (!file) {
    result.addIssue(
        Issue("file_error", "Cannot open file", Severity::CRITICAL));
    return result;
  }

  std::streamsize size = file.tellg();
  file.seekg(0, std::ios::beg);

  if (size == 0) {
    result.addIssue(Issue("file_error", "Empty file", Severity::CRITICAL));
    return result;
  }

  if (size > 100 * 1024 * 1024) { // 100MB limit
    result.addIssue(
        Issue("file_size", "File exceeds 100MB limit", Severity::CRITICAL));
    return result;
  }

  std::vector<uint8_t> data(size);
  if (!file.read(reinterpret_cast<char *>(data.data()), size)) {
    result.addIssue(
        Issue("file_error", "Failed to read file", Severity::CRITICAL));
    return result;
  }

  // Detect format
  ImageFormat format = detectFormat(data);
  if (format == ImageFormat::UNKNOWN) {
    result.addIssue(Issue("invalid_format", "Not a recognized image format",
                          Severity::CRITICAL));
    return result;
  }

  // Run all checks
  validateFormat(data, format, result);
  detectPolyglot(data, result);
  analyzeEntropy(data, result);
  scanSignatures(data, result);
  checkIntegrity(data, format, result);

  return result;
}

ImageFormat Scanner::detectFormat(const std::vector<uint8_t> &data) {
  if (data.size() < 12)
    return ImageFormat::UNKNOWN;

  // JPEG: FF D8 FF
  if (data[0] == 0xFF && data[1] == 0xD8 && data[2] == 0xFF) {
    return ImageFormat::JPEG;
  }

  // PNG: 89 50 4E 47 0D 0A 1A 0A
  if (data[0] == 0x89 && data[1] == 0x50 && data[2] == 0x4E &&
      data[3] == 0x47 && data[4] == 0x0D && data[5] == 0x0A &&
      data[6] == 0x1A && data[7] == 0x0A) {
    return ImageFormat::PNG;
  }

  // WEBP: RIFF ???? WEBP
  if (data[0] == 'R' && data[1] == 'I' && data[2] == 'F' && data[3] == 'F' &&
      data[8] == 'W' && data[9] == 'E' && data[10] == 'B' && data[11] == 'P') {
    return ImageFormat::WEBP;
  }

  // GIF: GIF87a or GIF89a
  if (data.size() >= 6 && data[0] == 'G' && data[1] == 'I' && data[2] == 'F' &&
      data[3] == '8' && (data[4] == '7' || data[4] == '9') && data[5] == 'a') {
    return ImageFormat::GIF;
  }

  // BMP: BM
  if (data[0] == 'B' && data[1] == 'M') {
    return ImageFormat::BMP;
  }

  // TIFF: II (little-endian) or MM (big-endian) + magic 42
  if (data.size() >= 4) {
    if ((data[0] == 'I' && data[1] == 'I' && data[2] == 42 && data[3] == 0) ||
        (data[0] == 'M' && data[1] == 'M' && data[2] == 0 && data[3] == 42)) {
      return ImageFormat::TIFF;
    }
  }

  // SVG: Starts with < and contains "svg"
  if (data.size() >= 100 && data[0] == '<') {
    std::string beginning(data.begin(),
                          data.begin() + std::min<size_t>(100, data.size()));
    // Convert to lowercase for comparison
    std::string lower = beginning;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    if (lower.find("svg") != std::string::npos) {
      return ImageFormat::SVG;
    }
  }

  return ImageFormat::UNKNOWN;
}

void Scanner::validateFormat(const std::vector<uint8_t> &data,
                             ImageFormat format, ScanResult &result) {
  switch (format) {
  case ImageFormat::JPEG: {
    // Check for EOI marker (FF D9) at the end
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
      result.addIssue(Issue("malformed_jpeg",
                            "Missing or invalid JPEG EOI marker",
                            Severity::CRITICAL));
    }

    // Check for valid APP markers
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
            result.addIssue(Issue("malformed_jpeg",
                                  "Invalid JPEG segment length",
                                  Severity::CRITICAL));
            break;
          }
          pos += 2 + length;
        } else {
          break;
        }
      } else {
        break;
      }
    }
    break;
  }

  case ImageFormat::PNG: {
    // Validate IHDR chunk (must be first)
    if (data.size() < 33) {
      result.addIssue(Issue("malformed_png", "PNG file too small for IHDR",
                            Severity::CRITICAL));
      break;
    }

    // Check IHDR position (offset 8)
    if (!(data[12] == 'I' && data[13] == 'H' && data[14] == 'D' &&
          data[15] == 'R')) {
      result.addIssue(Issue("malformed_png", "IHDR chunk not first in PNG",
                            Severity::CRITICAL));
    }

    // Validate chunk structure
    size_t pos = 8;
    bool hasIEND = false;

    while (pos + 12 <= data.size()) {
      uint32_t length = (data[pos] << 24) | (data[pos + 1] << 16) |
                        (data[pos + 2] << 8) | data[pos + 3];

      if (length > 0x7FFFFFFF || pos + 12 + length > data.size()) {
        result.addIssue(Issue("malformed_png", "Invalid PNG chunk length",
                              Severity::CRITICAL));
        break;
      }

      // Check for IEND
      if (data[pos + 4] == 'I' && data[pos + 5] == 'E' &&
          data[pos + 6] == 'N' && data[pos + 7] == 'D') {
        hasIEND = true;
        break;
      }

      pos += 12 + length;
    }

    if (!hasIEND) {
      result.addIssue(Issue("malformed_png", "Missing IEND chunk in PNG",
                            Severity::CRITICAL));
    }
    break;
  }

  case ImageFormat::WEBP: {
    // Validate RIFF size
    if (data.size() < 12) {
      result.addIssue(
          Issue("malformed_webp", "WEBP file too small", Severity::CRITICAL));
      break;
    }

    uint32_t fileSize =
        (data[4]) | (data[5] << 8) | (data[6] << 16) | (data[7] << 24);
    if (fileSize + 8 != data.size()) {
      result.addIssue(Issue("malformed_webp", "WEBP RIFF size mismatch",
                            Severity::WARNING));
    }

    // Check for VP8/VP8L/VP8X chunk
    if (data.size() >= 16) {
      bool validChunk =
          (data[12] == 'V' && data[13] == 'P' && data[14] == '8') &&
          (data[15] == ' ' || data[15] == 'L' || data[15] == 'X');
      if (!validChunk) {
        result.addIssue(Issue("malformed_webp", "Invalid WEBP chunk type",
                              Severity::CRITICAL));
      }
    }
    break;
  }

  case ImageFormat::GIF: {
    // Validate GIF structure
    if (data.size() < 13) {
      result.addIssue(
          Issue("malformed_gif", "GIF file too small", Severity::CRITICAL));
      break;
    }

    // Logical Screen Descriptor at bytes 6-12
    size_t width = data[6] | (data[7] << 8);
    size_t height = data[8] | (data[9] << 8);

    if (width == 0 || height == 0 || width > 65535 || height > 65535) {
      result.addIssue(
          Issue("malformed_gif", "Invalid GIF dimensions", Severity::CRITICAL));
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
      result.addIssue(
          Issue("malformed_gif", "Missing GIF trailer", Severity::WARNING));
    }
    break;
  }

  case ImageFormat::BMP: {
    // Validate BMP structure
    if (data.size() < 54) { // Minimum BMP header size
      result.addIssue(
          Issue("malformed_bmp", "BMP file too small", Severity::CRITICAL));
      break;
    }

    // File size (bytes 2-5)
    uint32_t fileSize =
        data[2] | (data[3] << 8) | (data[4] << 16) | (data[5] << 24);
    if (fileSize != data.size() && fileSize > 0) {
      result.addIssue(
          Issue("malformed_bmp", "BMP file size mismatch", Severity::WARNING));
    }

    // DIB header size (bytes 14-17)
    uint32_t dibSize =
        data[14] | (data[15] << 8) | (data[16] << 16) | (data[17] << 24);
    if (dibSize != 40 && dibSize != 108 &&
        dibSize != 124) { // Common sizes: BITMAPINFOHEADER, V4, V5
      result.addIssue(
          Issue("malformed_bmp", "Unusual BMP header size", Severity::WARNING));
    }

    // Width and height (bytes 18-25)
    int32_t width =
        data[18] | (data[19] << 8) | (data[20] << 16) | (data[21] << 24);
    int32_t height =
        data[22] | (data[23] << 8) | (data[24] << 16) | (data[25] << 24);

    if (width <= 0 || std::abs(height) == 0 || width > 65535 ||
        std::abs(height) > 65535) {
      result.addIssue(
          Issue("malformed_bmp", "Invalid BMP dimensions", Severity::CRITICAL));
    }
    break;
  }

  case ImageFormat::TIFF: {
    // Validate TIFF structure
    if (data.size() < 8) {
      result.addIssue(
          Issue("malformed_tiff", "TIFF file too small", Severity::CRITICAL));
      break;
    }

    bool littleEndian = (data[0] == 'I' && data[1] == 'I');

    // IFD offset (bytes 4-7)
    uint32_t ifdOffset;
    if (littleEndian) {
      ifdOffset = data[4] | (data[5] << 8) | (data[6] << 16) | (data[7] << 24);
    } else {
      ifdOffset = (data[4] << 24) | (data[5] << 16) | (data[6] << 8) | data[7];
    }

    if (ifdOffset < 8 || ifdOffset >= data.size()) {
      result.addIssue(Issue("malformed_tiff", "Invalid TIFF IFD offset",
                            Severity::CRITICAL));
    }

    // Basic IFD validation
    if (ifdOffset + 2 <= data.size()) {
      uint16_t numEntries;
      if (littleEndian) {
        numEntries = data[ifdOffset] | (data[ifdOffset + 1] << 8);
      } else {
        numEntries = (data[ifdOffset] << 8) | data[ifdOffset + 1];
      }

      if (numEntries > 1000) { // Sanity check
        result.addIssue(Issue("malformed_tiff",
                              "TIFF has excessive IFD entries",
                              Severity::WARNING));
      }
    }
    break;
  }

  case ImageFormat::SVG: {
    // Convert to string for SVG analysis
    std::string content(data.begin(), data.end());
    std::string lower = content;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    // Critical XSS vectors
    if (lower.find("<script") != std::string::npos) {
      result.addIssue(
          Issue("svg_script", "SVG contains <script> tag", Severity::CRITICAL));
    }

    if (lower.find("<iframe") != std::string::npos) {
      result.addIssue(
          Issue("svg_iframe", "SVG contains <iframe> tag", Severity::CRITICAL));
    }

    if (lower.find("javascript:") != std::string::npos) {
      result.addIssue(Issue("svg_javascript", "SVG contains javascript: URI",
                            Severity::CRITICAL));
    }

    if (lower.find("onload=") != std::string::npos ||
        lower.find("onclick=") != std::string::npos ||
        lower.find("onerror=") != std::string::npos ||
        lower.find("onmouseover=") != std::string::npos) {
      result.addIssue(Issue("svg_event_handler", "SVG contains event handlers",
                            Severity::CRITICAL));
    }

    if (lower.find("<foreignobject") != std::string::npos) {
      result.addIssue(Issue("svg_foreignobject", "SVG contains foreignObject",
                            Severity::CRITICAL));
    }

    // Check for data: URIs with potential XSS
    if (lower.find("data:text/html") != std::string::npos ||
        lower.find("data:image/svg+xml") != std::string::npos) {
      result.addIssue(Issue("svg_data_uri", "SVG contains suspicious data: URI",
                            Severity::CRITICAL));
    }

    // Check for embedded base64 (potential payload hiding)
    if (content.find("base64,") != std::string::npos) {
      size_t pos = content.find("base64,");
      // Check if there's a substantial base64 blob (>1000 chars)
      size_t endPos = content.find_first_of("\"'<> \t\n", pos + 7);
      if (endPos != std::string::npos && (endPos - pos) > 1000) {
        result.addIssue(Issue("svg_large_base64",
                              "SVG contains large base64 payload",
                              Severity::WARNING));
      }
    }
    break;
  }

  default:
    break;
  }
}

void Scanner::detectPolyglot(const std::vector<uint8_t> &data,
                             ScanResult &result) {
  // Check for ZIP signature (PK)
  for (size_t i = 1; i + 3 < data.size(); ++i) {
    if (data[i] == 'P' && data[i + 1] == 'K' && data[i + 2] == 0x03 &&
        data[i + 3] == 0x04) {
      result.addIssue(Issue("polyglot", "ZIP signature detected in image",
                            Severity::CRITICAL));
      break;
    }
  }

  // Check for PDF signature
  for (size_t i = 1; i + 4 < data.size(); ++i) {
    if (data[i] == '%' && data[i + 1] == 'P' && data[i + 2] == 'D' &&
        data[i + 3] == 'F' && data[i + 4] == '-') {
      result.addIssue(Issue("polyglot", "PDF signature detected in image",
                            Severity::CRITICAL));
      break;
    }
  }

  // Check for script tags
  std::string dataStr(data.begin(), data.end());
  if (dataStr.find("<script") != std::string::npos ||
      dataStr.find("javascript:") != std::string::npos) {
    result.addIssue(Issue("embedded_script", "Script content detected in image",
                          Severity::CRITICAL));
  }

  // Check for HTML tags
  if (dataStr.find("<html") != std::string::npos ||
      dataStr.find("<!DOCTYPE") != std::string::npos) {
    result.addIssue(Issue("polyglot", "HTML content detected in image",
                          Severity::CRITICAL));
  }
}

void Scanner::analyzeEntropy(const std::vector<uint8_t> &data,
                             ScanResult &result) {
  // Calculate Shannon entropy
  std::map<uint8_t, size_t> freq;
  for (uint8_t byte : data) {
    freq[byte]++;
  }

  double entropy = 0.0;
  double size = static_cast<double>(data.size());

  for (const auto &pair : freq) {
    double p = pair.second / size;
    entropy -= p * std::log2(p);
  }

  // High entropy threshold (7.5 bits/byte suggests encryption/compression)
  if (entropy > 7.5) {
    std::ostringstream oss;
    oss << "Unusually high entropy detected: " << entropy << " bits/byte";
    result.addIssue(Issue("high_entropy", oss.str(), Severity::WARNING));
  }

  // Also check entropy of specific regions for hidden payloads
  if (data.size() > 1024) {
    // Check last 1KB
    size_t start = data.size() - 1024;
    std::map<uint8_t, size_t> tailFreq;
    for (size_t i = start; i < data.size(); ++i) {
      tailFreq[data[i]]++;
    }

    double tailEntropy = 0.0;
    for (const auto &pair : tailFreq) {
      double p = pair.second / 1024.0;
      tailEntropy -= p * std::log2(p);
    }

    if (tailEntropy > 7.8) {
      result.addIssue(Issue(
          "high_entropy",
          "Suspicious high entropy in file tail (possible hidden payload)",
          Severity::CRITICAL));
    }
  }
}

void Scanner::checkIntegrity(const std::vector<uint8_t> &data,
                             ImageFormat format, ScanResult &result) {
  // Detect appended data after EOF markers
  size_t eofPos = std::string::npos;

  switch (format) {
  case ImageFormat::JPEG: {
    // JPEG EOF: FF D9
    for (size_t i = 0; i < data.size() - 1; ++i) {
      if (data[i] == 0xFF && data[i + 1] == 0xD9) {
        eofPos = i + 2;
      }
    }
    break;
  }

  case ImageFormat::PNG: {
    // PNG EOF: IEND chunk
    for (size_t i = 0; i < data.size() - 8; ++i) {
      if (data[i + 4] == 'I' && data[i + 5] == 'E' && data[i + 6] == 'N' &&
          data[i + 7] == 'D') {
        // IEND + length (4) + type (4) + CRC (4) = 12 bytes total
        uint32_t length = (data[i] << 24) | (data[i + 1] << 16) |
                          (data[i + 2] << 8) | data[i + 3];
        eofPos = i + 12 + length;
        break;
      }
    }
    break;
  }

  case ImageFormat::GIF: {
    // GIF EOF: 0x3B trailer
    for (size_t i = 0; i < data.size(); ++i) {
      if (data[i] == 0x3B) {
        eofPos = i + 1;
      }
    }
    break;
  }

  case ImageFormat::WEBP: {
    // WEBP: RIFF header contains the size
    if (data.size() >= 8) {
      uint32_t riffSize =
          data[4] | (data[5] << 8) | (data[6] << 16) | (data[7] << 24);
      eofPos = 8 + riffSize;
    }
    break;
  }

  default:
    return; // Skip integrity check for other formats
  }

  // Check for appended data
  if (eofPos != std::string::npos && eofPos < data.size()) {
    size_t appendedSize = data.size() - eofPos;

    if (appendedSize > 100) { // More than 100 bytes after EOF
      std::ostringstream oss;
      oss << "Image has " << appendedSize << " bytes appended after EOF marker";
      result.addIssue(Issue("appended_data", oss.str(), Severity::CRITICAL));
    }
  }

  // Detect oversized metadata blocks (JPEG APP/COM segments, PNG text chunks)
  size_t totalMetadata = 0;

  if (format == ImageFormat::JPEG) {
    // Scan for APP and COM segments
    for (size_t i = 0; i < data.size() - 4; ++i) {
      if (data[i] == 0xFF) {
        uint8_t marker = data[i + 1];
        // APP0-APP15: 0xE0-0xEF, COM: 0xFE
        if ((marker >= 0xE0 && marker <= 0xEF) || marker == 0xFE) {
          uint16_t segmentLength = (data[i + 2] << 8) | data[i + 3];
          totalMetadata += segmentLength;
          i += segmentLength + 1; // Skip this segment
        }
      }
    }
  } else if (format == ImageFormat::PNG) {
    // Scan for text chunks (tEXt, zTXt, iTXt)
    size_t pos = 8; // After PNG header
    while (pos + 12 <= data.size()) {
      uint32_t length = (data[pos] << 24) | (data[pos + 1] << 16) |
                        (data[pos + 2] << 8) | data[pos + 3];

      char type[5] = {0};
      memcpy(type, &data[pos + 4], 4);

      if (strcmp(type, "tEXt") == 0 || strcmp(type, "zTXt") == 0 ||
          strcmp(type, "iTXt") == 0) {
        totalMetadata += length;
      }

      if (length > 0x7FFFFFFF || pos + 12 + length > data.size())
        break;
      pos += 12 + length;
    }
  }

  if (totalMetadata > 1024 * 1024) { // Over 1MB of metadata
    std::ostringstream oss;
    oss << "Image contains excessive metadata: " << (totalMetadata / 1024)
        << " KB";
    result.addIssue(Issue("oversized_metadata", oss.str(), Severity::WARNING));
  }
}

void Scanner::scanSignatures(const std::vector<uint8_t> &data,
                             ScanResult &result) {
  // Use loaded signatures (defaults loaded in constructor)
  for (const auto &sig : pImpl->customSignatures) {
    const std::string &name = sig.first;
    const std::vector<uint8_t> &pattern = sig.second;

    if (pattern.empty())
      continue;

    // Scan for this pattern
    for (size_t i = 0; i + pattern.size() <= data.size(); ++i) {
      bool match = true;
      for (size_t j = 0; j < pattern.size(); ++j) {
        if (data[i + j] != pattern[j]) {
          match = false;
          break;
        }
      }
      if (match) {
        std::ostringstream oss;
        oss << "Detected signature: " << name;
        result.addIssue(Issue("signature", oss.str(), Severity::CRITICAL));
        break; // Only report first occurrence of this signature
      }
    }
  }
}

void Scanner::addSignature(const std::string &name,
                           const std::vector<uint8_t> &pattern) {
  pImpl->customSignatures[name] = pattern;
}

std::string Scanner::getLastError() const { return pImpl->lastError; }

// Parse signature string with escape sequences
std::vector<uint8_t> Scanner::parseSignatureString(const std::string &str) {
  std::vector<uint8_t> result;
  size_t i = 0;

  while (i < str.size()) {
    if (str[i] == '\\' && i + 1 < str.size()) {
      if (str[i + 1] == 'x' && i + 3 < str.size()) {
        // Hex escape: \xHH
        std::string hex = str.substr(i + 2, 2);
        char *end;
        long value = std::strtol(hex.c_str(), &end, 16);
        if (end != hex.c_str()) {
          result.push_back(static_cast<uint8_t>(value));
          i += 4;
          continue;
        }
      } else if (str[i + 1] == 'n') {
        result.push_back('\n');
        i += 2;
        continue;
      } else if (str[i + 1] == 'r') {
        result.push_back('\r');
        i += 2;
        continue;
      } else if (str[i + 1] == 't') {
        result.push_back('\t');
        i += 2;
        continue;
      } else if (str[i + 1] == '\\') {
        result.push_back('\\');
        i += 2;
        continue;
      }
    }
    result.push_back(static_cast<uint8_t>(str[i]));
    i++;
  }

  return result;
}

// Load built-in default signatures
void Scanner::loadDefaultSignatures() {
  pImpl->customSignatures.clear();

  // Archive signatures
  pImpl->customSignatures["zip_header"] = parseSignatureString("PK\\x03\\x04");
  pImpl->customSignatures["zip_eocd"] = parseSignatureString("PK\\x05\\x06");
  pImpl->customSignatures["rar_header"] = parseSignatureString("Rar!");
  pImpl->customSignatures["7zip_header"] =
      parseSignatureString("7z\\xBC\\xAF\\x27\\x1C");

  // Executable headers
  pImpl->customSignatures["elf_header"] = parseSignatureString("\\x7FELF");
  pImpl->customSignatures["exe_header"] = parseSignatureString("MZ");

  // Script injections
  pImpl->customSignatures["php_code"] = parseSignatureString("<?php");
  pImpl->customSignatures["php_short"] = parseSignatureString("<?=");
  pImpl->customSignatures["html_tag"] = parseSignatureString("<html");
  pImpl->customSignatures["doctype"] = parseSignatureString("<!DOCTYPE");
  pImpl->customSignatures["script_tag"] = parseSignatureString("<script");
  pImpl->customSignatures["js_uri"] = parseSignatureString("javascript:");

  // PDF
  pImpl->customSignatures["pdf_header"] = parseSignatureString("%PDF-");

  // Shell scripts
  pImpl->customSignatures["shebang_bash"] = parseSignatureString("#!/bin/bash");
  pImpl->customSignatures["shebang_sh"] = parseSignatureString("#!/bin/sh");
  pImpl->customSignatures["shebang_python"] =
      parseSignatureString("#!/usr/bin/python");
  pImpl->customSignatures["shebang_perl"] =
      parseSignatureString("#!/usr/bin/perl");

  pImpl->signaturesLoaded = true;
}

// Simple JSON parser for signatures
bool Scanner::loadSignatures(const std::string &filepath) {
  pImpl->lastError.clear();

  std::ifstream file(filepath);
  if (!file) {
    pImpl->lastError =
        "Cannot open file: " + filepath + " (using default signatures)";
    return false; // Non-fatal, will use defaults
  }

  // Read entire file
  std::string content((std::istreambuf_iterator<char>(file)),
                      std::istreambuf_iterator<char>());
  file.close();

  if (content.empty()) {
    pImpl->lastError = "Empty file (using default signatures)";
    return false;
  }

  // Simple JSON parsing - find "signatures" object
  size_t sigStart = content.find("\"signatures\"");
  if (sigStart == std::string::npos) {
    pImpl->lastError =
        "No 'signatures' key found in JSON (using default signatures)";
    return false;
  }

  size_t objStart = content.find('{', sigStart);
  if (objStart == std::string::npos) {
    pImpl->lastError = "Invalid JSON structure (using default signatures)";
    return false;
  }

  // Clear existing signatures
  pImpl->customSignatures.clear();

  // Parse key-value pairs
  size_t pos = objStart + 1;
  while (pos < content.size()) {
    // Skip whitespace
    while (pos < content.size() && std::isspace(content[pos]))
      pos++;

    if (pos >= content.size() || content[pos] == '}')
      break;

    // Parse key (signature name)
    if (content[pos] != '"') {
      pos++;
      continue;
    }

    size_t keyStart = pos + 1;
    size_t keyEnd = content.find('"', keyStart);
    if (keyEnd == std::string::npos)
      break;

    std::string key = content.substr(keyStart, keyEnd - keyStart);

    // Find colon
    pos = content.find(':', keyEnd);
    if (pos == std::string::npos)
      break;
    pos++;

    // Skip whitespace
    while (pos < content.size() && std::isspace(content[pos]))
      pos++;

    // Parse value (signature pattern)
    if (content[pos] != '"') {
      pos++;
      continue;
    }

    size_t valueStart = pos + 1;
    size_t valueEnd = content.find('"', valueStart);
    if (valueEnd == std::string::npos)
      break;

    std::string value = content.substr(valueStart, valueEnd - valueStart);

    // Parse and store signature
    pImpl->customSignatures[key] = parseSignatureString(value);

    pos = valueEnd + 1;
  }

  if (pImpl->customSignatures.empty()) {
    pImpl->lastError = "No valid signatures parsed, loading defaults";
    loadDefaultSignatures();
    return false;
  }

  pImpl->signaturesLoaded = true;
  return true;
}

} // namespace ImageScanner
