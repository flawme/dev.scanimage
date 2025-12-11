#ifndef WEBP_VALIDATOR_H
#define WEBP_VALIDATOR_H

#include "base_validator.h"

namespace ImageScanner {

class WEBPValidator : public BaseValidator {
public:
  WEBPValidator() = default;
  ~WEBPValidator() override = default;

  bool validate(const std::vector<uint8_t> &data, ScanResultV2 &result,
                const ScanOptions &options) override;

  MetadataResult extractMetadata(const std::vector<uint8_t> &data,
                                 const ScanOptions &options) override;

  std::string getFormatName() const override { return "webp"; }

private:
  // WEBP RIFF structure validation
  bool validateWEBPStructure(const std::vector<uint8_t> &data,
                             ScanResultV2 &result);

  // Find EXIF chunk
  size_t findEXIFChunk(const std::vector<uint8_t> &data);

  // Find XMP chunk
  size_t findXMPChunk(const std::vector<uint8_t> &data);
};

} // namespace ImageScanner

#endif // WEBP_VALIDATOR_H
