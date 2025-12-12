#include "internal/xmp_parser.h"
#include "internal/base_validator.h"
#include <cctype>
#include <cstring>
#include <regex>

namespace ImageScanner {
namespace Metadata {

XMPParser::XMPParser() : lastError_("") {}

// Main parse function (format-aware)
MetadataResult XMPParser::parse(const std::vector<uint8_t> &data,
                                ImageFormat format) {
  switch (format) {
  case ImageFormat::JPEG:
    return extractFromJPEG(data);
  case ImageFormat::PNG:
    return extractFromPNG(data);
  case ImageFormat::WEBP:
    return extractFromWEBP(data);
  default:
    lastError_ = "Unsupported format for XMP extraction";
    return MetadataResult();
  }
}

// Extract XMP from JPEG (APP1 marker with XMP namespace)
MetadataResult XMPParser::extractFromJPEG(const std::vector<uint8_t> &data) {
  MetadataResult result;
  result.hasXMP = false;

  auto xmpData = findXMPInJPEG(data);
  if (!xmpData.has_value()) {
    return result;
  }

  std::string xmp = xmpData.value();
  result.hasXMP = true;
  result.xmpSize = xmp.size();

  // Security checks
  if (isOversized(xmp)) {
    result.xmpWarnings.push_back("XMP size exceeds 1MB limit (potential DOS)");
  }

  if (!isWellFormedXML(xmp)) {
    result.xmpWarnings.push_back("XMP contains malformed XML");
  }

  if (hasDangerousEntities(xmp)) {
    result.xmpWarnings.push_back(
        "XMP contains DOCTYPE or entity declarations (XXE risk)");
  }

  // Detect scripts
  auto scriptWarnings = extractScriptWarnings(xmp);
  result.xmpWarnings.insert(result.xmpWarnings.end(), scriptWarnings.begin(),
                            scriptWarnings.end());

  // Detect base64
  auto base64Warnings = extractBase64Warnings(xmp);
  result.xmpWarnings.insert(result.xmpWarnings.end(), base64Warnings.begin(),
                            base64Warnings.end());

  return result;
}

// Extract XMP from PNG (iTXt chunk with XMP keyword)
MetadataResult XMPParser::extractFromPNG(const std::vector<uint8_t> &data) {
  MetadataResult result;
  result.hasXMP = false;

  auto xmpData = findXMPInPNG(data);
  if (!xmpData.has_value()) {
    return result;
  }

  std::string xmp = xmpData.value();
  result.hasXMP = true;
  result.xmpSize = xmp.size();

  // Security checks (same as JPEG)
  if (isOversized(xmp)) {
    result.xmpWarnings.push_back("XMP size exceeds 1MB limit (potential DOS)");
  }

  if (!isWellFormedXML(xmp)) {
    result.xmpWarnings.push_back("XMP contains malformed XML");
  }

  if (hasDangerousEntities(xmp)) {
    result.xmpWarnings.push_back(
        "XMP contains DOCTYPE or entity declarations (XXE risk)");
  }

  auto scriptWarnings = extractScriptWarnings(xmp);
  result.xmpWarnings.insert(result.xmpWarnings.end(), scriptWarnings.begin(),
                            scriptWarnings.end());

  auto base64Warnings = extractBase64Warnings(xmp);
  result.xmpWarnings.insert(result.xmpWarnings.end(), base64Warnings.begin(),
                            base64Warnings.end());

  return result;
}

// Extract XMP from WEBP (XMP chunk)
MetadataResult XMPParser::extractFromWEBP(const std::vector<uint8_t> &data) {
  MetadataResult result;
  result.hasXMP = false;

  auto xmpData = findXMPInWEBP(data);
  if (!xmpData.has_value()) {
    return result;
  }

  std::string xmp = xmpData.value();
  result.hasXMP = true;
  result.xmpSize = xmp.size();

  // Security checks
  if (isOversized(xmp)) {
    result.xmpWarnings.push_back("XMP size exceeds 1MB limit (potential DOS)");
  }

  if (!isWellFormedXML(xmp)) {
    result.xmpWarnings.push_back("XMP contains malformed XML");
  }

  if (hasDangerousEntities(xmp)) {
    result.xmpWarnings.push_back(
        "XMP contains DOCTYPE or entity declarations (XXE risk)");
  }

  auto scriptWarnings = extractScriptWarnings(xmp);
  result.xmpWarnings.insert(result.xmpWarnings.end(), scriptWarnings.begin(),
                            scriptWarnings.end());

  auto base64Warnings = extractBase64Warnings(xmp);
  result.xmpWarnings.insert(result.xmpWarnings.end(), base64Warnings.begin(),
                            base64Warnings.end());

  return result;
}

// Security: Check for script tags
bool XMPParser::hasScriptTags(const std::string &xmpData) {
  return !detectScriptTags(xmpData).empty();
}

// Security: Check for base64 payloads
bool XMPParser::hasBase64Payloads(const std::string &xmpData) {
  return !detectBase64Patterns(xmpData).empty();
}

// Security: Extract script warnings
std::vector<std::string>
XMPParser::extractScriptWarnings(const std::string &xmpData) {
  std::vector<std::string> warnings;

  auto scriptTags = detectScriptTags(xmpData);
  for (const auto &tag : scriptTags) {
    warnings.push_back("XMP contains script tag: " + tag);
  }

  auto dangerousAttrs = detectDangerousAttributes(xmpData);
  for (const auto &attr : dangerousAttrs) {
    warnings.push_back("XMP contains dangerous attribute: " + attr);
  }

  return warnings;
}

// Security: Extract base64 warnings
std::vector<std::string>
XMPParser::extractBase64Warnings(const std::string &xmpData) {
  std::vector<std::string> warnings;

  auto base64Patterns = detectBase64Patterns(xmpData);
  for (const auto &pattern : base64Patterns) {
    warnings.push_back("XMP contains base64 payload (" +
                       std::to_string(pattern.size()) + " bytes)");
  }

  return warnings;
}

// Sanitization: Remove all threats
std::string XMPParser::sanitizeXMP(const std::string &xmpData) {
  std::string sanitized = xmpData;

  // Remove DOCTYPE declarations
  sanitized = removeDoctypeDeclarations(sanitized);

  // Remove script tags
  sanitized = removeScriptTags(sanitized);

  // Strip dangerous attributes
  sanitized = stripDangerousAttributes(sanitized);

  return sanitized;
}

// ============================================================================
// PRIVATE METHODS - XML Validation
// ============================================================================

bool XMPParser::isWellFormedXML(const std::string &xml) {
  // Simple check: balanced angle brackets and no unclosed tags
  int bracketCount = 0;
  bool inTag = false;

  for (char c : xml) {
    if (c == '<') {
      bracketCount++;
      inTag = true;
    } else if (c == '>') {
      bracketCount--;
      inTag = false;
      if (bracketCount < 0)
        return false;
    }
  }

  return bracketCount == 0 && !inTag;
}

bool XMPParser::hasDangerousEntities(const std::string &xml) {
  // Check for DOCTYPE or ENTITY declarations (XXE vulnerability)
  std::regex doctypeRegex(R"(<!DOCTYPE)", std::regex::icase);
  std::regex entityRegex(R"(<!ENTITY)", std::regex::icase);

  return std::regex_search(xml, doctypeRegex) ||
         std::regex_search(xml, entityRegex);
}

bool XMPParser::isOversized(const std::string &xml) {
  return xml.size() > MAX_XMP_SIZE;
}

// ============================================================================
// PRIVATE METHODS - Detection
// ============================================================================

std::vector<std::string> XMPParser::detectScriptTags(const std::string &xml) {
  std::vector<std::string> findings;

  // Regex for <script> tags (case-insensitive)
  // Note: . matches newlines in ECMAScript regex by default
  std::regex scriptRegex(R"(<script[^>]*>[\s\S]*?</script>)",
                         std::regex::icase);
  std::smatch match;

  std::string::const_iterator searchStart(xml.cbegin());
  while (std::regex_search(searchStart, xml.cend(), match, scriptRegex)) {
    std::string found = match.str();
    if (found.length() > 100) {
      found = found.substr(0, 97) + "...";
    }
    findings.push_back(found);
    searchStart = match.suffix().first;
  }

  // Also check for javascript: URIs
  std::regex jsUriRegex(R"(javascript:)", std::regex::icase);
  if (std::regex_search(xml, jsUriRegex)) {
    findings.push_back("javascript: URI detected");
  }

  return findings;
}

std::vector<std::string>
XMPParser::detectDangerousAttributes(const std::string &xml) {
  std::vector<std::string> findings;

  // Event handler attributes
  std::vector<std::string> dangerousAttrs = {
      "onclick", "onerror",  "onload",   "onmouseover", "onfocus",
      "onblur",  "onchange", "onsubmit", "onkeypress",  "onkeydown"};

  for (const auto &attr : dangerousAttrs) {
    std::regex attrRegex("\\b" + attr + "\\s*=", std::regex::icase);
    if (std::regex_search(xml, attrRegex)) {
      findings.push_back(attr);
    }
  }

  return findings;
}

std::vector<std::string>
XMPParser::detectBase64Patterns(const std::string &xml) {
  std::vector<std::string> findings;

  // Regex for base64 strings (A-Za-z0-9+/= with min length)
  std::regex base64Regex(R"([A-Za-z0-9+/]{100,}={0,2})");
  std::smatch match;

  std::string::const_iterator searchStart(xml.cbegin());
  while (std::regex_search(searchStart, xml.cend(), match, base64Regex)) {
    std::string found = match.str();
    if (found.length() >= MIN_BASE64_LENGTH) {
      findings.push_back(found);
    }
    searchStart = match.suffix().first;
  }

  return findings;
}

// ============================================================================
// PRIVATE METHODS - Sanitization
// ============================================================================

std::string XMPParser::removeScriptTags(const std::string &xml) {
  std::regex scriptRegex(R"(<script[^>]*>[\s\S]*?</script>)",
                         std::regex::icase);
  return std::regex_replace(xml, scriptRegex, "");
}

std::string XMPParser::stripDangerousAttributes(const std::string &xml) {
  std::string result = xml;

  // Remove event handler attributes
  std::vector<std::string> dangerousAttrs = {
      "onclick", "onerror",  "onload",   "onmouseover", "onfocus",
      "onblur",  "onchange", "onsubmit", "onkeypress",  "onkeydown"};

  for (const auto &attr : dangerousAttrs) {
    std::regex attrRegex("\\s+" + attr + "\\s*=\\s*['\"][^'\"]*['\"]",
                         std::regex::icase);
    result = std::regex_replace(result, attrRegex, "");
  }

  return result;
}

std::string XMPParser::removeDoctypeDeclarations(const std::string &xml) {
  // Remove DOCTYPE declarations
  std::regex doctypeRegex(R"(<!DOCTYPE[^>]*>)", std::regex::icase);
  std::string result = std::regex_replace(xml, doctypeRegex, "");

  // Remove ENTITY declarations
  std::regex entityRegex(R"(<!ENTITY[^>]*>)", std::regex::icase);
  result = std::regex_replace(result, entityRegex, "");

  return result;
}

// ============================================================================
// PRIVATE METHODS - Format-Specific Extraction
// ============================================================================

std::optional<std::string>
XMPParser::findXMPInJPEG(const std::vector<uint8_t> &data) {
  // JPEG XMP is in APP1 marker (0xFFE1) with namespace identifier
  // Format: FF E1 [length:2] "http://ns.adobe.com/xap/1.0/\0" [XMP data]

  if (data.size() < 100)
    return std::nullopt;

  size_t pos = 0;
  while (pos + 4 < data.size()) {
    // Look for marker
    if (data[pos] == 0xFF && data[pos + 1] == 0xE1) {
      // Read length (big-endian)
      uint16_t length = (data[pos + 2] << 8) | data[pos + 3];

      if (pos + 2 + length > data.size())
        return std::nullopt;

      // Check for XMP namespace
      size_t nsStart = pos + 4;
      std::string ns = bytesToString(data, nsStart, 29); // Length of namespace

      if (ns == JPEG_XMP_NAMESPACE) {
        // Found XMP! Extract it
        size_t xmpStart = nsStart + 29;
        size_t xmpLength = length - 2 - 29; // Minus length field and namespace

        if (xmpStart + xmpLength <= data.size()) {
          return bytesToString(data, xmpStart, xmpLength);
        }
      }

      pos += 2 + length;
    } else {
      pos++;
    }
  }

  return std::nullopt;
}

std::optional<std::string>
XMPParser::findXMPInPNG(const std::vector<uint8_t> &data) {
  // PNG XMP is in iTXt chunk with keyword "XML:com.adobe.xmp"
  // Chunk format: [length:4] [type:4] [data] [crc:4]

  if (data.size() < 33)
    return std::nullopt; // PNG signature + IHDR minimum

  size_t pos = 8; // Skip PNG signature

  while (pos + 12 < data.size()) {
    // Read chunk length (big-endian)
    uint32_t length = (data[pos] << 24) | (data[pos + 1] << 16) |
                      (data[pos + 2] << 8) | data[pos + 3];
    pos += 4;

    // Read chunk type
    std::string chunkType = bytesToString(data, pos, 4);
    pos += 4;

    if (pos + length + 4 > data.size())
      break;

    if (chunkType == "iTXt") {
      // iTXt format: keyword\0 compression_flag compression_method
      // language\0 translated_keyword\0 text
      size_t dataStart = pos;
      size_t dataEnd = pos + length;

      // Extract keyword (null-terminated)
      std::string keyword;
      size_t i = dataStart;
      while (i < dataEnd && data[i] != 0) {
        keyword += static_cast<char>(data[i]);
        i++;
      }

      if (keyword == PNG_XMP_KEYWORD) {
        // Skip to text content
        i++; // Skip null terminator
        if (i < dataEnd)
          i++; // Skip compression flag
        if (i < dataEnd)
          i++; // Skip compression method

        // Skip language tag
        while (i < dataEnd && data[i] != 0)
          i++;
        i++; // Skip null

        // Skip translated keyword
        while (i < dataEnd && data[i] != 0)
          i++;
        i++; // Skip null

        // Remaining is XMP data
        if (i < dataEnd) {
          return bytesToString(data, i, dataEnd - i);
        }
      }
    }

    pos += length + 4; // Skip data and CRC

    // Stop at IEND
    if (chunkType == "IEND")
      break;
  }

  return std::nullopt;
}

std::optional<std::string>
XMPParser::findXMPInWEBP(const std::vector<uint8_t> &data) {
  // WEBP XMP is in "XMP " chunk
  // WEBP format: "RIFF" [size:4] "WEBP" [chunks...]
  // Chunk format: [fourcc:4] [size:4] [data]

  if (data.size() < 12)
    return std::nullopt;

  // Verify RIFF header
  std::string riff = bytesToString(data, 0, 4);
  if (riff != "RIFF")
    return std::nullopt;

  std::string webp = bytesToString(data, 8, 4);
  if (webp != "WEBP")
    return std::nullopt;

  size_t pos = 12;

  while (pos + 8 < data.size()) {
    // Read chunk FourCC
    std::string fourcc = bytesToString(data, pos, 4);
    pos += 4;

    // Read chunk size (little-endian)
    uint32_t size = data[pos] | (data[pos + 1] << 8) | (data[pos + 2] << 16) |
                    (data[pos + 3] << 24);
    pos += 4;

    if (pos + size > data.size())
      break;

    if (fourcc == "XMP ") {
      // Found XMP chunk!
      return bytesToString(data, pos, size);
    }

    // Chunks are padded to even size
    pos += size;
    if (size % 2 == 1)
      pos++;
  }

  return std::nullopt;
}

// ============================================================================
// PRIVATE METHODS - Utilities
// ============================================================================

std::string XMPParser::bytesToString(const std::vector<uint8_t> &data,
                                     size_t offset, size_t length) {
  if (offset + length > data.size())
    return "";

  return std::string(reinterpret_cast<const char *>(data.data() + offset),
                     length);
}

} // namespace Metadata
} // namespace ImageScanner
