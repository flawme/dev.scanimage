#ifndef TIFF_VALIDATOR_H
#define TIFF_VALIDATOR_H

#include "base_validator.h"

namespace ImageScanner {

class TIFFValidator : public BaseValidator {
public:
  TIFFValidator() = default;
  ~TIFFValidator() override = default;

  bool validate(const std::vector<uint8_t> &data, ScanResultV2 &result,
                const ScanOptions &options) override;

  MetadataResult extractMetadata(const std::vector<uint8_t> &data,
                                 const ScanOptions &options) override;

  std::string getFormatName() const override { return "tiff"; }

private:
  // TIFF structure validation
  bool validateTIFFStructure(const std::vector<uint8_t> &data,
                             ScanResultV2 &result);

  // TIFF has embedded EXIF-like structure
  // Will use EXIFParser when implemented
};

} // namespace ImageScanner

#endif // TIFF_VALIDATOR_H
