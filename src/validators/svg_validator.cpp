#include "../../include/validators/svg_validator.h"
#include <algorithm>
#include <cstring>
#include <sstream>

namespace ImageScanner {

bool SVGValidator::validate(const std::vector<uint8_t> &data,
                            ScanResultV2 &result, const ScanOptions &options) {
  return validateSVGStructure(data, result, options);
}

bool SVGValidator::validateSVGStructure(const std::vector<uint8_t> &data,
                                        ScanResultV2 &result,
                                        const ScanOptions &options) {
  // Convert to string
  std::string content(data.begin(), data.end());

  // Detect XSS vectors
  detectSVGXSS(content, result);

  // If sanitization is enabled and we found issues, optionally sanitize
  // (for now just detect)

  return result.isSafe;
}

void SVGValidator::detectSVGXSS(const std::string &svgContent,
                                ScanResultV2 &result) {
  std::string lower = svgContent;
  std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

  // Detect script tags
  auto scriptPositions = findScriptTags(lower);
  if (!scriptPositions.empty()) {
    for (size_t pos : scriptPositions) {
      addIssue(result, IssueV2("svg_script", "SVG contains <script> tag",
                               Severity::CRITICAL, IssueCategory::XML, pos, 7));
    }
  }

  // Detect iframe
  if (lower.find("<iframe") != std::string::npos) {
    size_t pos = lower.find("<iframe");
    addIssue(result, IssueV2("svg_iframe", "SVG contains <iframe> tag",
                             Severity::CRITICAL, IssueCategory::XML, pos, 7));
  }

  // Detect javascript: URIs
  if (lower.find("javascript:") != std::string::npos) {
    size_t pos = lower.find("javascript:");
    addIssue(result,
             IssueV2("svg_javascript", "SVG contains javascript: URI",
                     Severity::CRITICAL, IssueCategory::SCRIPT, pos, 11));
  }

  // Detect event handlers
  auto eventPositions = findEventHandlers(lower);
  if (!eventPositions.empty()) {
    for (size_t pos : eventPositions) {
      addIssue(result,
               IssueV2("svg_event_handler", "SVG contains event handlers",
                       Severity::CRITICAL, IssueCategory::SCRIPT, pos, 7));
    }
  }

  // Detect foreignObject
  if (lower.find("<foreignobject") != std::string::npos) {
    size_t pos = lower.find("<foreignobject");
    addIssue(result, IssueV2("svg_foreignobject", "SVG contains foreignObject",
                             Severity::CRITICAL, IssueCategory::XML, pos, 14));
  }

  // Detect suspicious data URIs
  auto dataURIs = findDataURIs(lower);
  if (!dataURIs.empty()) {
    for (size_t pos : dataURIs) {
      if (lower.find("data:text/html", pos) != std::string::npos ||
          lower.find("data:image/svg+xml", pos) != std::string::npos) {
        addIssue(result,
                 IssueV2("svg_data_uri", "SVG contains suspicious data: URI",
                         Severity::CRITICAL, IssueCategory::XML, pos, 10));
      }
    }
  }

  // Check for large base64 payloads
  if (svgContent.find("base64,") != std::string::npos) {
    size_t pos = svgContent.find("base64,");
    size_t endPos = svgContent.find_first_of("\"'<> \t\n", pos + 7);
    if (endPos != std::string::npos && (endPos - pos) > 1000) {
      addIssue(result,
               IssueV2("svg_large_base64", "SVG contains large base64 payload",
                       Severity::WARNING, IssueCategory::METADATA, pos,
                       endPos - pos));
    }
  }
}

std::string SVGValidator::sanitizeSVG(const std::string &svgContent) {
  // Simple sanitization: remove script tags
  // TODO: More comprehensive sanitization in future
  std::string sanitized = svgContent;

  // Remove <script> tags
  size_t pos = 0;
  while ((pos = sanitized.find("<script", pos)) != std::string::npos) {
    size_t endPos = sanitized.find("</script>", pos);
    if (endPos != std::string::npos) {
      sanitized.erase(pos, endPos - pos + 9);
    } else {
      break;
    }
  }

  return sanitized;
}

std::vector<size_t> SVGValidator::findScriptTags(const std::string &content) {
  std::vector<size_t> positions;
  size_t pos = 0;
  while ((pos = content.find("<script", pos)) != std::string::npos) {
    positions.push_back(pos);
    pos += 7;
  }
  return positions;
}

std::vector<size_t>
SVGValidator::findEventHandlers(const std::string &content) {
  std::vector<size_t> positions;
  const char *handlers[] = {
      "onload=", "onclick=", "onerror=", "onmouseover=", "onfocus=", "onblur="};

  for (const char *handler : handlers) {
    size_t pos = 0;
    while ((pos = content.find(handler, pos)) != std::string::npos) {
      positions.push_back(pos);
      pos += strlen(handler);
    }
  }

  return positions;
}

std::vector<size_t> SVGValidator::findDataURIs(const std::string &content) {
  std::vector<size_t> positions;
  size_t pos = 0;
  while ((pos = content.find("data:", pos)) != std::string::npos) {
    positions.push_back(pos);
    pos += 5;
  }
  return positions;
}

MetadataResult SVGValidator::extractMetadata(const std::vector<uint8_t> &data,
                                             const ScanOptions &options) {
  MetadataResult meta;
  // SVG doesn't have traditional EXIF/XMP, but we can mark if it has scripts
  // This is handled in validate()
  return meta;
}

} // namespace ImageScanner
