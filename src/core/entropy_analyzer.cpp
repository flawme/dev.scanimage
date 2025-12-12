#include "internal/entropy_analyzer.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <sstream>

namespace ImageScanner {

EntropyAnalyzer::EntropyAnalyzer(const ScanThresholds &thresholds)
    : thresholds_(thresholds) {}

void EntropyAnalyzer::analyze(const std::vector<uint8_t> &data,
                              ScanResultV2 &result, bool useSlidingWindow) {
  if (data.empty()) {
    return;
  }

  if (useSlidingWindow) {
    analyzeSlidingWindow(data, result);
  } else {
    // Full file entropy (legacy mode)
    double entropy = calculateEntropy(data);

    if (entropy > thresholds_.entropyThreshold) {
      std::ostringstream oss;
      oss << "Unusually high entropy detected: " << entropy << " bits/byte";
      result.addIssue(IssueV2("high_entropy", oss.str(), Severity::WARNING,
                              IssueCategory::STRUCTURE, 0, data.size()));
    }
  }
}

double EntropyAnalyzer::calculateEntropy(const std::vector<uint8_t> &data) {
  if (data.empty()) {
    return 0.0;
  }

  // Count byte frequencies
  std::map<uint8_t, size_t> freq;
  for (uint8_t byte : data) {
    freq[byte]++;
  }

  // Calculate Shannon entropy
  double entropy = 0.0;
  double size = static_cast<double>(data.size());

  for (const auto &pair : freq) {
    double p = pair.second / size;
    entropy -= p * std::log2(p);
  }

  return entropy;
}

double EntropyAnalyzer::calculateEntropyRegion(const std::vector<uint8_t> &data,
                                               size_t start, size_t length) {
  if (start + length > data.size() || length == 0) {
    return 0.0;
  }

  // Count byte frequencies in region
  std::map<uint8_t, size_t> freq;
  for (size_t i = start; i < start + length; ++i) {
    freq[data[i]]++;
  }

  // Calculate Shannon entropy
  double entropy = 0.0;
  double size = static_cast<double>(length);

  for (const auto &pair : freq) {
    double p = pair.second / size;
    entropy -= p * std::log2(p);
  }

  return entropy;
}

void EntropyAnalyzer::analyzeSlidingWindow(const std::vector<uint8_t> &data,
                                           ScanResultV2 &result) {
  const size_t windowSize = thresholds_.chunkSize; // Default 4096 bytes
  const size_t step = windowSize / 2;              // 50% overlap

  if (data.size() < windowSize) {
    // File too small for sliding window, use full entropy
    double entropy = calculateEntropy(data);
    if (entropy > thresholds_.entropyThreshold) {
      std::ostringstream oss;
      oss << "High entropy: " << entropy << " bits/byte";
      result.addIssue(IssueV2("high_entropy", oss.str(), Severity::WARNING,
                              IssueCategory::STRUCTURE, 0, data.size()));
    }
    return;
  }

  // Sliding window analysis
  std::vector<std::pair<size_t, double>> highEntropyWindows;

  for (size_t offset = 0; offset + windowSize <= data.size(); offset += step) {
    double entropy = calculateEntropyRegion(data, offset, windowSize);

    // Detect suspiciously high entropy
    if (entropy > 7.8) { // Very high threshold for encrypted data
      highEntropyWindows.push_back({offset, entropy});
    }
  }

  // Report high entropy regions
  if (!highEntropyWindows.empty()) {
    // Merge adjacent windows
    for (const auto &window : highEntropyWindows) {
      size_t offset = window.first;
      double entropy = window.second;

      std::ostringstream oss;
      oss << "High entropy region detected (possible encrypted/compressed "
             "payload): "
          << entropy << " bits/byte at offset " << offset;

      result.addIssue(IssueV2("high_entropy", oss.str(), Severity::CRITICAL,
                              IssueCategory::EMBEDDED_FILE, offset,
                              windowSize));
    }
  }

  // Also check file tail (last 1KB) specifically
  if (data.size() > 1024) {
    size_t tailStart = data.size() - 1024;
    double tailEntropy = calculateEntropyRegion(data, tailStart, 1024);

    if (tailEntropy > 7.8) {
      std::ostringstream oss;
      oss << "Suspicious high entropy in file tail (possible hidden payload): "
          << tailEntropy << " bits/byte";
      result.addIssue(IssueV2("high_entropy_tail", oss.str(),
                              Severity::CRITICAL, IssueCategory::STRUCTURE,
                              tailStart, 1024));
    }
  }
}

} // namespace ImageScanner
