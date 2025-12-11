#ifndef BMP_VALIDATOR_H
#define BMP_VALIDATOR_H

#include "base_validator.h"

namespace ImageScanner {

class BMPValidator : public BaseValidator {
public:
  BMPValidator() = default;
  ~BMPValidator() override = default;

  bool validate(const std::vector<uint8_t> &data, ScanResultV2 &result,
                const ScanOptions &options) override;

  MetadataResult extractMetadata(const std::vector<uint8_t> &data,
                                 const ScanOptions &options) override;

  std::string getFormatName() const override { return "bmp"; }

private:
  // BMP structure validation
  bool validateBMPStructure(const std::vector<uint8_t> &data,
                            ScanResultV2 &result);

  // Check for trailing data
  void checkTrailingData(const std::vector<uint8_t> &data, ScanResultV2 &result,
                         const ScanOptions &options);
};

} // namespace ImageScanner

#endif // BMP_VALIDATOR_H
