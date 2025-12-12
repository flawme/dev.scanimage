#ifndef SVG_VALIDATOR_H
#define SVG_VALIDATOR_H

#include "base_validator.h"

namespace ImageScanner {

class SVGValidator : public BaseValidator {
public:
  SVGValidator() = default;
  ~SVGValidator() override = default;

  bool validate(const std::vector<uint8_t> &data, ScanResultV2 &result,
                const ScanOptions &options) override;

  MetadataResult extractMetadata(const std::vector<uint8_t> &data,
                                 const ScanOptions &options) override;

  std::string getFormatName() const override { return "svg"; }

private:
  // SVG-specific validation and sanitization
  bool validateSVGStructure(const std::vector<uint8_t> &data,
                            ScanResultV2 &result, const ScanOptions &options);

  // Detect XSS vectors in SVG
  void detectSVGXSS(const std::string &svgContent, ScanResultV2 &result);

  // Sanitize SVG content
  std::string sanitizeSVG(const std::string &svgContent);

  // Find script tags
  std::vector<size_t> findScriptTags(const std::string &content);

  // Find event handlers (onclick, onload, etc.)
  std::vector<size_t> findEventHandlers(const std::string &content);

  // Find data URIs
  std::vector<size_t> findDataURIs(const std::string &content);
};

} // namespace ImageScanner

#endif // SVG_VALIDATOR_H
