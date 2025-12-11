#ifndef PNG_VALIDATOR_H
#define PNG_VALIDATOR_H

#include "base_validator.h"

namespace ImageScanner {

class PNGValidator : public BaseValidator {
public:
  PNGValidator() = default;
  ~PNGValidator() override = default;

  bool validate(const std::vector<uint8_t> &data, ScanResultV2 &result,
                const ScanOptions &options) override;

  MetadataResult extractMetadata(const std::vector<uint8_t> &data,
                                 const ScanOptions &options) override;

  std::string getFormatName() const override { return "png"; }

private:
  // PNG structure validation
  bool validatePNGStructure(const std::vector<uint8_t> &data,
                            ScanResultV2 &result);

  // Validate PNG chunk CRC
  bool validateChunkCRC(const std::vector<uint8_t> &data, size_t chunkStart);

  // Calculate CRC32
  uint32_t crc32(const std::vector<uint8_t> &data, size_t start, size_t length);

  // Find specific chunk type
  size_t findChunk(const std::vector<uint8_t> &data, const char *type);

  // Check for trailing data after IEND
  void checkTrailingData(const std::vector<uint8_t> &data, ScanResultV2 &result,
                         const ScanOptions &options);
};

} // namespace ImageScanner

#endif // PNG_VALIDATOR_H
