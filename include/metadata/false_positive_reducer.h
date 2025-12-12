#ifndef FALSE_POSITIVE_REDUCER_H
#define FALSE_POSITIVE_REDUCER_H

#include "../scan_result_v2.h"
#include <cstdint>
#include <set>
#include <string>
#include <vector>

namespace ImageScanner {
namespace Metadata {

// Known false-positive pattern
struct KnownPattern {
  std::string pattern;           // Pattern name or keyword
  std::string context;           // Where it appears
  float confidenceAdjustment;    // -0.5 to +0.5
  std::string reason;            // Why it's a false positive

  KnownPattern(const std::string &p, const std::string &c, float adj,
               const std::string &r)
      : pattern(p), context(c), confidenceAdjustment(adj), reason(r) {}
};

// False-positive reduction system
class FalsePositiveReducer {
public:
  FalsePositiveReducer();

  // Analyze metadata result and adjust confidence
  void analyzeMetadata(MetadataResult &meta,
                       const std::vector<uint8_t> &data);

  // Filter XMP warnings with confidence
  void filterXMPWarnings(std::vector<std::string> &warnings,
                         const std::string &xmpContent);

  // Analyze GPS warnings
  float analyzeGPSConfidence(const MetadataResult &meta);

  // Analyze base64 warnings
  float analyzeBase64Confidence(const std::string &base64Data,
                                const std::string &context);

  // Set confidence threshold
  void setConfidenceThreshold(float threshold);

  // Get confidence threshold
  float getConfidenceThreshold() const { return confidenceThreshold_; }

private:
  std::vector<KnownPattern> knownFalsePositives_;
  std::set<std::string> whitelistedNamespaces_;
  std::set<std::string> knownCameraBrands_;
  float confidenceThreshold_; // Default: 0.7

  // Initialize known patterns
  void initializeKnownPatterns();
  void initializeWhitelistedNamespaces();
  void initializeKnownCameraBrands();

  // Heuristics
  bool isLikelyThumbnail(const std::string &base64);
  bool isStandardXMPNamespace(const std::string &xmp);
  bool isKnownCameraBrand(const std::string &make);
  bool hasTypicalEXIFStructure(const MetadataResult &meta);
  bool hasRealisticGPSFormat(const MetadataResult &meta);
  bool isLikelyImageThumbnail(const std::string &base64Data);

  // Confidence calculation
  float calculateConfidence(const std::string &warning,
                            const std::string &context,
                            const std::vector<uint8_t> &data);
  float calculateConfidence(const std::string &warning,
                            const std::string &context,
                            const std::string &textData);
};

} // namespace Metadata
} // namespace ImageScanner

#endif // FALSE_POSITIVE_REDUCER_H
