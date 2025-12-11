#ifndef GIF_VALIDATOR_H
#define GIF_VALIDATOR_H

#include "base_validator.h"

namespace ImageScanner {

class GIFValidator : public BaseValidator {
public:
  GIFValidator() = default;
  ~GIFValidator() override = default;

  bool validate(const std::vector<uint8_t> &data, ScanResultV2 &result,
                const ScanOptions &options) override;

  MetadataResult extractMetadata(const std::vector<uint8_t> &data,
                                 const ScanOptions &options) override;

  std::string getFormatName() const override { return "gif"; }

private:
  // GIF structure validation
  bool validateGIFStructure(const std::vector<uint8_t> &data,
                            ScanResultV2 &result);

  // Check for trailer
  void checkTrailingData(const std::vector<uint8_t> &data, ScanResultV2 &result,
                         const ScanOptions &options);
};

} // namespace ImageScanner

#endif // GIF_VALIDATOR_H
