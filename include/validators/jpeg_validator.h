#ifndef JPEG_VALIDATOR_H
#define JPEG_VALIDATOR_H

#include "base_validator.h"

namespace ImageScanner {

class JPEGValidator : public BaseValidator {
public:
  JPEGValidator() = default;
  ~JPEGValidator() override = default;

  bool validate(const std::vector<uint8_t> &data, ScanResultV2 &result,
                const ScanOptions &options) override;

  MetadataResult extractMetadata(const std::vector<uint8_t> &data,
                                 const ScanOptions &options) override;

  std::string getFormatName() const override { return "jpeg"; }

private:
  // JPEG structure validation
  bool validateJPEGStructure(const std::vector<uint8_t> &data,
                             ScanResultV2 &result);

  // Find EXIF APP1 segment
  size_t findEXIFSegment(const std::vector<uint8_t> &data);

  // Find XMP APP1 segment
  size_t findXMPSegment(const std::vector<uint8_t> &data);

  // Check for trailing data after EOI
  void checkTrailingData(const std::vector<uint8_t> &data, ScanResultV2 &result,
                         const ScanOptions &options);
};

} // namespace ImageScanner

#endif // JPEG_VALIDATOR_H
