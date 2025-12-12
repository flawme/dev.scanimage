#include "../../include/internal/false_positive_reducer.h"
#include <algorithm>
#include <cctype>
#include <regex>
#include <sstream>

namespace ImageScanner {
namespace Metadata {

FalsePositiveReducer::FalsePositiveReducer() : confidenceThreshold_(0.7f) {
  initializeKnownPatterns();
  initializeWhitelistedNamespaces();
  initializeKnownCameraBrands();
}

// Analyze metadata and adjust confidence
void FalsePositiveReducer::analyzeMetadata(MetadataResult &meta,
                                           const std::vector<uint8_t> &data) {
  // Filter XMP warnings if present
  if (meta.hasXMP && !meta.xmpWarnings.empty()) {
    filterXMPWarnings(meta.xmpWarnings, meta.xmpContent);
  }

  // Analyze GPS confidence
  if (meta.hasGPS) {
    float gpsConfidence = analyzeGPSConfidence(meta);
    if (gpsConfidence < confidenceThreshold_) {
      // Downgrade GPS warnings
      for (auto &warning : meta.gpsWarnings) {
        warning = "[INFO] " + warning + " (likely from camera, confidence: " +
                  std::to_string(gpsConfidence) + ")";
      }
    }
  }
}

// Filter XMP warnings based on confidence
void FalsePositiveReducer::filterXMPWarnings(std::vector<std::string> &warnings,
                                             const std::string &xmpContent) {
  std::vector<std::string> filtered;

  for (const auto &warning : warnings) {
    float confidence = calculateConfidence(warning, "xmp", xmpContent);

    if (confidence >= confidenceThreshold_) {
      // Keep high-confidence warnings as-is
      filtered.push_back(warning);
    } else if (confidence >= 0.4f) {
      // Downgrade medium-confidence warnings to INFO
      std::string adjusted = "[INFO] " + warning +
                             " (confidence: " + std::to_string(confidence) +
                             ")";
      filtered.push_back(adjusted);
    }
    // Discard low-confidence warnings (< 0.4)
  }

  warnings = filtered;
}

// Analyze GPS confidence
float FalsePositiveReducer::analyzeGPSConfidence(const MetadataResult &meta) {
  float confidence = 1.0f; // Default: high confidence (real threat)

  // Check if from known camera brand
  if (meta.exifTags.count("Make")) {
    std::string make = meta.exifTags.at("Make");
    if (isKnownCameraBrand(make)) {
      confidence -= 0.3f; // Lower severity for camera GPS
    }
  }

  // Check if has typical EXIF structure (suggests legitimate photo)
  if (hasTypicalEXIFStructure(meta)) {
    confidence -= 0.2f;
  }

  // Check if GPS format is realistic
  if (hasRealisticGPSFormat(meta)) {
    confidence -= 0.1f;
  }

  return std::max(0.0f, confidence);
}

// Analyze base64 confidence
float FalsePositiveReducer::analyzeBase64Confidence(
    const std::string &base64Data, const std::string &context) {
  float confidence = 1.0f;

  // Check length (thumbnails are usually 5KB-50KB)
  size_t length = base64Data.size();
  if (length >= 5000 && length <= 70000) {
    confidence -= 0.3f; // Likely thumbnail
  }

  // Check if in XMP thumbnail context
  if (context.find("thumbnail") != std::string::npos ||
      context.find("preview") != std::string::npos ||
      context.find("xmp") != std::string::npos) {
    confidence -= 0.4f;
  }

  // Check if it's likely an image thumbnail
  if (isLikelyImageThumbnail(base64Data)) {
    confidence -= 0.3f;
  }

  return std::max(0.0f, confidence);
}

// Set confidence threshold
void FalsePositiveReducer::setConfidenceThreshold(float threshold) {
  confidenceThreshold_ = std::max(0.0f, std::min(1.0f, threshold));
}

// ============================================================================
// PRIVATE METHODS - Initialization
// ============================================================================

void FalsePositiveReducer::initializeKnownPatterns() {
  // Base64 thumbnails in XMP
  knownFalsePositives_.push_back(KnownPattern("base64 payload", "xmp:thumbnail",
                                              -0.4f,
                                              "Likely JPEG thumbnail in XMP"));

  knownFalsePositives_.push_back(KnownPattern(
      "base64 payload", "xmp:preview", -0.4f, "Likely preview image in XMP"));

  // GPS from known cameras
  knownFalsePositives_.push_back(
      KnownPattern("gps_detected", "exif:camera", -0.3f,
                   "GPS from camera is typically legitimate"));

  // Adobe XMP namespaces
  knownFalsePositives_.push_back(
      KnownPattern("xml", "xmp:adobe", -0.5f, "Standard Adobe XMP namespace"));
}

void FalsePositiveReducer::initializeWhitelistedNamespaces() {
  whitelistedNamespaces_ = {"http://ns.adobe.com/xap/1.0/",
                            "http://purl.org/dc/elements/1.1/",
                            "http://www.w3.org/1999/02/22-rdf-syntax-ns#",
                            "http://ns.adobe.com/photoshop/1.0/",
                            "http://ns.adobe.com/tiff/1.0/",
                            "http://ns.adobe.com/exif/1.0/",
                            "http://ns.adobe.com/xap/1.0/mm/",
                            "http://ns.adobe.com/xap/1.0/rights/"};
}

void FalsePositiveReducer::initializeKnownCameraBrands() {
  knownCameraBrands_ = {"Canon",     "Nikon",   "Sony",   "Fujifilm",
                        "Panasonic", "Olympus", "Pentax", "Leica",
                        "Samsung",   "Apple",   "Google"};
}

// ============================================================================
// PRIVATE METHODS - Heuristics
// ============================================================================

bool FalsePositiveReducer::isLikelyThumbnail(const std::string &base64) {
  // Thumbnails are typically 5-70KB
  size_t length = base64.size();
  return length >= 5000 && length <= 70000;
}

bool FalsePositiveReducer::isStandardXMPNamespace(const std::string &xmp) {
  for (const auto &ns : whitelistedNamespaces_) {
    if (xmp.find(ns) != std::string::npos) {
      return true;
    }
  }
  return false;
}

bool FalsePositiveReducer::isKnownCameraBrand(const std::string &make) {
  // Case-insensitive search
  for (const auto &brand : knownCameraBrands_) {
    if (make.find(brand) != std::string::npos) {
      return true;
    }
  }
  return false;
}

bool FalsePositiveReducer::hasTypicalEXIFStructure(const MetadataResult &meta) {
  // Check for common EXIF tags that indicate a legitimate photo
  return meta.exifTags.count("Make") > 0 || meta.exifTags.count("Model") > 0 ||
         meta.exifTags.count("DateTime") > 0;
}

bool FalsePositiveReducer::hasRealisticGPSFormat(const MetadataResult &meta) {
  // GPS warnings typically indicate structured GPS data
  return meta.gpsWarnings.size() >= 1 && meta.hasGPS;
}

bool FalsePositiveReducer::isLikelyImageThumbnail(
    const std::string &base64Data) {
  // Try to decode first few bytes to check for JPEG/PNG signature
  // Simplified: check if length suggests image
  size_t length = base64Data.size();

  // Base64 encoded JPEG (FF D8) or PNG (89 50 4E 47) would have specific
  // prefixes For now, use length heuristic
  if (length < 100) {
    return false; // Too small
  }

  // Check for common base64 prefixes of image headers
  // JPEG: /9j/ or /9k/
  // PNG: iVBOR
  if (base64Data.substr(0, 4) == "/9j/" || base64Data.substr(0, 4) == "/9k/" ||
      base64Data.substr(0, 5) == "iVBOR") {
    return true;
  }

  return false;
}

// ============================================================================
// PRIVATE METHODS - Confidence Calculation
// ============================================================================

float FalsePositiveReducer::calculateConfidence(
    const std::string &warning, const std::string &context,
    const std::vector<uint8_t> &data) {
  float confidence = 1.0f;

  // Check against known false-positive patterns
  for (const auto &pattern : knownFalsePositives_) {
    if (warning.find(pattern.pattern) != std::string::npos &&
        context.find(pattern.context) != std::string::npos) {
      confidence += pattern.confidenceAdjustment;
    }
  }

  return std::max(0.0f, std::min(1.0f, confidence));
}

float FalsePositiveReducer::calculateConfidence(const std::string &warning,
                                                const std::string &context,
                                                const std::string &textData) {
  float confidence = 1.0f;

  // Base64 warnings
  if (warning.find("base64") != std::string::npos ||
      warning.find("payload") != std::string::npos) {
    confidence = analyzeBase64Confidence(textData, context);
  }

  // XML/XMP warnings
  if (warning.find("XMP") != std::string::npos ||
      warning.find("XML") != std::string::npos) {
    if (isStandardXMPNamespace(textData)) {
      confidence -= 0.5f; // Standard namespaces are safe
    }
  }

  // Script warnings
  if (warning.find("script") != std::string::npos) {
    // Keep high confidence for script tags
    // Unless they're in whitelisted namespaces
    if (isStandardXMPNamespace(textData)) {
      confidence -= 0.2f;
    }
  }

  return std::max(0.0f, std::min(1.0f, confidence));
}

} // namespace Metadata
} // namespace ImageScanner
