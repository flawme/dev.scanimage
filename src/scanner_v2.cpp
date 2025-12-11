#include "../include/scanner_v2.h"
#include "../include/entropy_analyzer.h"
#include "../include/file_io.h"
#include "../include/validators/bmp_validator.h"
#include "../include/validators/gif_validator.h"
#include "../include/validators/jpeg_validator.h"
#include "../include/validators/png_validator.h"
#include "../include/validators/svg_validator.h"
#include "../include/validators/tiff_validator.h"
#include "../include/validators/webp_validator.h"
#include <algorithm>
#include <chrono>
#include <fstream>

namespace ImageScanner {

ScannerV2::ScannerV2()
    : options_(), thresholds_(),
      fileIO_(std::make_unique<FileIOManager>(options_)),
      entropyAnalyzer_(std::make_unique<EntropyAnalyzer>(thresholds_)) {}

ScannerV2::ScannerV2(const ScanOptions &options)
    : options_(options), thresholds_(),
      fileIO_(std::make_unique<FileIOManager>(options_)),
      entropyAnalyzer_(std::make_unique<EntropyAnalyzer>(thresholds_)) {}

ScannerV2::~ScannerV2() = default;

ScanResultV2 ScannerV2::scan(const std::string &filepath) {
  return scanDetailed(filepath, options_);
}

ScanResultV2 ScannerV2::scanDetailed(const std::string &filepath,
                                     const ScanOptions &options) {
  auto startTime = std::chrono::high_resolution_clock::now();

  ScanResultV2 result;

  // Load file
  std::vector<uint8_t> data = loadFile(filepath);

  if (data.empty()) {
    result.addIssue(IssueV2("file_error", "Cannot load file or file is empty",
                            Severity::CRITICAL, IssueCategory::STRUCTURE));
    return result;
  }

  // Perform scan
  result = scanInternal(data, options);

  // Record scan time
  auto endTime = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
      endTime - startTime);
  result.scanTimeMs = duration.count() / 1000.0;

  return result;
}

ScanResultV2 ScannerV2::scanInternal(const std::vector<uint8_t> &data,
                                     const ScanOptions &options) {
  ScanResultV2 result;

  // Detect format
  ImageFormat format = detectFormatInternal(data);
  result.format = formatToString(format);

  if (format == ImageFormat::UNKNOWN) {
    result.addIssue(IssueV2("invalid_format", "Not a recognized image format",
                            Severity::CRITICAL, IssueCategory::STRUCTURE));
    return result;
  }

  // Get validator
  BaseValidator *validator = getValidator(format);
  if (validator) {
    validator->validate(data, result, options);
  }

  // TODO: Phase 2 - Additional analysis
  if (options.checkPolyglot) {
    // analyzePolyglot(data, result);
  }

  if (options.checkEntropy) {
    // analyzeEntropy(data, result);
  }

  return result;
}

MetadataResult ScannerV2::getMetadata(const std::string &filepath) {
  std::vector<uint8_t> data = loadFile(filepath);
  ImageFormat format = detectFormatInternal(data);

  if (format == ImageFormat::UNKNOWN) {
    return MetadataResult{};
  }

  BaseValidator *validator = getValidator(format);
  if (validator) {
    return validator->extractMetadata(data, options_);
  }

  return MetadataResult{};
}

std::string ScannerV2::detectFormat(const std::string &filepath) {
  std::vector<uint8_t> data = loadFile(filepath);
  return detectFormatFromData(data);
}

std::string ScannerV2::detectFormatFromData(const std::vector<uint8_t> &data) {
  ImageFormat format = detectFormatInternal(data);
  return formatToString(format);
}

ImageFormat ScannerV2::detectFormatInternal(const std::vector<uint8_t> &data) {
  if (data.size() < 12)
    return ImageFormat::UNKNOWN;

  // JPEG: FF D8 FF
  if (data[0] == 0xFF && data[1] == 0xD8 && data[2] == 0xFF) {
    return ImageFormat::JPEG;
  }

  // PNG: 89 50 4E 47 0D 0A 1A 0A
  if (data[0] == 0x89 && data[1] == 0x50 && data[2] == 0x4E &&
      data[3] == 0x47 && data[4] == 0x0D && data[5] == 0x0A &&
      data[6] == 0x1A && data[7] == 0x0A) {
    return ImageFormat::PNG;
  }

  // WEBP: RIFF ???? WEBP
  if (data[0] == 'R' && data[1] == 'I' && data[2] == 'F' && data[3] == 'F' &&
      data[8] == 'W' && data[9] == 'E' && data[10] == 'B' && data[11] == 'P') {
    return ImageFormat::WEBP;
  }

  // GIF: GIF87a or GIF89a
  if (data.size() >= 6 && data[0] == 'G' && data[1] == 'I' && data[2] == 'F' &&
      data[3] == '8' && (data[4] == '7' || data[4] == '9') && data[5] == 'a') {
    return ImageFormat::GIF;
  }

  // BMP: BM
  if (data[0] == 'B' && data[1] == 'M') {
    return ImageFormat::BMP;
  }

  // TIFF: II (little-endian) or MM (big-endian) + magic 42
  if (data.size() >= 4) {
    if ((data[0] == 'I' && data[1] == 'I' && data[2] == 42 && data[3] == 0) ||
        (data[0] == 'M' && data[1] == 'M' && data[2] == 0 && data[3] == 42)) {
      return ImageFormat::TIFF;
    }
  }

  // SVG: Starts with < and contains "svg"
  if (data.size() >= 100 && data[0] == '<') {
    std::string beginning(data.begin(),
                          data.begin() + std::min<size_t>(100, data.size()));
    std::string lower = beginning;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    if (lower.find("svg") != std::string::npos) {
      return ImageFormat::SVG;
    }
  }

  return ImageFormat::UNKNOWN;
}

BaseValidator *ScannerV2::getValidator(ImageFormat format) {
  // Check if validator already exists
  auto it = validators_.find(format);
  if (it != validators_.end()) {
    return it->second.get();
  }

  // Create new validator
  std::unique_ptr<BaseValidator> validator;

  switch (format) {
  case ImageFormat::JPEG:
    validator = std::make_unique<JPEGValidator>();
    break;
  case ImageFormat::PNG:
    validator = std::make_unique<PNGValidator>();
    break;
  case ImageFormat::SVG:
    validator = std::make_unique<SVGValidator>();
    break;
  case ImageFormat::WEBP:
    validator = std::make_unique<WEBPValidator>();
    break;
  case ImageFormat::GIF:
    validator = std::make_unique<GIFValidator>();
    break;
  case ImageFormat::BMP:
    validator = std::make_unique<BMPValidator>();
    break;
  case ImageFormat::TIFF:
    validator = std::make_unique<TIFFValidator>();
    break;
  default:
    return nullptr;
  }

  BaseValidator *ptr = validator.get();
  validators_[format] = std::move(validator);
  return ptr;
}

std::vector<uint8_t> ScannerV2::loadFile(const std::string &filepath) {
  if (fileIO_) {
    return fileIO_->loadFile(filepath);
  }

  // Fallback if fileIO_ is not initialized
  std::ifstream file(filepath, std::ios::binary | std::ios::ate);
  if (!file) {
    return {};
  }

  std::streamsize size = file.tellg();
  file.seekg(0, std::ios::beg);

  if (size == 0 ||
      size > static_cast<std::streamsize>(thresholds_.maxFileSize)) {
    return {};
  }

  std::vector<uint8_t> data(size);
  if (!file.read(reinterpret_cast<char *>(data.data()), size)) {
    return {};
  }

  return data;
}

void ScannerV2::setOptions(const ScanOptions &options) { options_ = options; }

ScanOptions ScannerV2::getOptions() const { return options_; }

// Placeholder implementations for later phases
void ScannerV2::analyzePolyglot(const std::vector<uint8_t> &data,
                                ScanResultV2 &result) {
  // TODO: Implement in Phase 2
}

void ScannerV2::analyzeEntropy(const std::vector<uint8_t> &data,
                               ScanResultV2 &result) {
  if (entropyAnalyzer_) {
    entropyAnalyzer_->analyze(data, result, options_.slidingWindowEntropy);
  }
}

void ScannerV2::checkIntegrityV2(const std::vector<uint8_t> &data,
                                 ImageFormat format, ScanResultV2 &result,
                                 const ScanOptions &options) {
  // TODO: Implement in Phase 2
}

} // namespace ImageScanner
