#ifndef ENTROPY_ANALYZER_H
#define ENTROPY_ANALYZER_H

#include "config.h"
#include "scan_result_v2.h"
#include <cstddef>
#include <cstdint>
#include <vector>

namespace ImageScanner {

class EntropyAnalyzer {
public:
  explicit EntropyAnalyzer(const ScanThresholds &thresholds);

  // Analyze entropy and add issues to result
  void analyze(const std::vector<uint8_t> &data, ScanResultV2 &result,
               bool useSlidingWindow = true);

  // Calculate Shannon entropy for entire data
  double calculateEntropy(const std::vector<uint8_t> &data);

  // Calculate entropy for a specific region
  double calculateEntropyRegion(const std::vector<uint8_t> &data, size_t start,
                                size_t length);

  // Sliding window entropy analysis (fast)
  void analyzeSlidingWindow(const std::vector<uint8_t> &data,
                            ScanResultV2 &result);

private:
  const ScanThresholds &thresholds_;

  // Detect high entropy regions
  void detectHighEntropyRegions(const std::vector<uint8_t> &data,
                                ScanResultV2 &result);
};

} // namespace ImageScanner

#endif // ENTROPY_ANALYZER_H
