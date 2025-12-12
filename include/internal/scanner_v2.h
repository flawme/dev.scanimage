#ifndef SCANNER_V2_H
#define SCANNER_V2_H

#include "config.h"
#include "scan_result_v2.h"
#include "base_validator.h"
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace ImageScanner {

class ScannerV2 {
public:
  ScannerV2();
  explicit ScannerV2(const ScanOptions &options);
  ~ScannerV2();

  // Main scan API
  ScanResultV2 scan(const std::string &filepath);
  ScanResultV2 scanDetailed(const std::string &filepath,
                            const ScanOptions &options);

  // Metadata extraction only
  MetadataResult getMetadata(const std::string &filepath);

  // Format detection only
  std::string detectFormat(const std::string &filepath);
  std::string detectFormatFromData(const std::vector<uint8_t> &data);

  // Configuration
  void setOptions(const ScanOptions &options);
  ScanOptions getOptions() const;

private:
  ScanOptions options_;
  ScanThresholds thresholds_;

  // Subsystems
  std::unique_ptr<class FileIOManager> fileIO_;
  std::unique_ptr<class EntropyAnalyzer> entropyAnalyzer_;

  // Validators (lazy-loaded via factory pattern)
  std::map<ImageFormat, std::unique_ptr<BaseValidator>> validators_;

  // Internal scan implementation
  ScanResultV2 scanInternal(const std::vector<uint8_t> &data,
                            const ScanOptions &options);

  // Format detection
  ImageFormat detectFormatInternal(const std::vector<uint8_t> &data);

  // Get validator for format (creates if needed)
  BaseValidator *getValidator(ImageFormat format);

  // Analysis engines (to be implemented in later phases)
  void analyzePolyglot(const std::vector<uint8_t> &data, ScanResultV2 &result);
  void analyzeEntropy(const std::vector<uint8_t> &data, ScanResultV2 &result);
  void checkIntegrityV2(const std::vector<uint8_t> &data, ImageFormat format,
                        ScanResultV2 &result, const ScanOptions &options);

  // File I/O (basic version, mmap in Phase 2)
  std::vector<uint8_t> loadFile(const std::string &filepath);
};

} // namespace ImageScanner

#endif // SCANNER_V2_H
