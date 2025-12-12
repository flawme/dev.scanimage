#include "../include/safeimg_export.h"
#include "internal/signature_engine_v2.h"
#include "internal/scanner_v2.h"
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>

using namespace ImageScanner;
using namespace ImageScanner::Metadata;

// Helper: Allocate C string (caller must free with safeimg_free)
static char *allocate_cstring(const std::string &str) {
  char *result = (char *)malloc(str.size() + 1);
  if (result) {
    strcpy(result, str.c_str());
  }
  return result;
}

// Helper: Parse options JSON to ScanOptions
static ScanOptions parseOptions(const char *optionsJson) {
  ScanOptions opts;

  if (!optionsJson || strlen(optionsJson) == 0) {
    return opts; // Use defaults
  }

  // TODO: Implement proper JSON parsing
  // For now, use defaults
  // In production, use nlohmann/json or similar

  return opts;
}

// Helper: Convert ScanResultV2 to JSON string
static std::string resultToJSON(const ScanResultV2 &result) {
  std::ostringstream json;

  json << "{\n";
  json << "  \"isSafe\": " << (result.isSafe ? "true" : "false") << ",\n";
  json << "  \"format\": \"" << result.format << "\",\n";
  json << "  \"issues\": [\n";

  for (size_t i = 0; i < result.issues.size(); ++i) {
    const auto &issue = result.issues[i];
    json << "    {\n";
    json << "      \"type\": \"" << issue.type << "\",\n";
    json << "      \"description\": \"" << issue.description << "\",\n";
    json << "      \"severity\": \"" << severityToString(issue.severity)
         << "\",\n";
    json << "      \"category\": \"" << categoryToString(issue.category)
         << "\",\n";
    json << "      \"offset\": " << issue.offset << ",\n";
    json << "      \"length\": " << issue.length << "\n";
    json << "    }";
    if (i < result.issues.size() - 1)
      json << ",";
    json << "\n";
  }

  json << "  ],\n";
  json << "  \"metadata\": {\n";
  json << "    \"hasEXIF\": " << (result.metadata.hasEXIF ? "true" : "false")
       << ",\n";
  json << "    \"hasXMP\": " << (result.metadata.hasXMP ? "true" : "false")
       << ",\n";
  json << "    \"hasGPS\": " << (result.metadata.hasGPS ? "true" : "false")
       << ",\n";
  json << "    \"exifTags\": {";

  bool first = true;
  for (const auto &tag : result.metadata.exifTags) {
    if (!first)
      json << ", ";
    json << "\"" << tag.first << "\": \"" << tag.second << "\"";
    first = false;
  }

  json << "},\n";
  json << "    \"metadataSizeBytes\": " << result.metadata.metadataSizeBytes
       << "\n";
  json << "  },\n";
  json << "  \"realImageSize\": " << result.realImageSize << ",\n";
  json << "  \"extraDataSize\": " << result.extraDataSize << ",\n";
  json << "  \"scanTimeMs\": " << result.scanTimeMs << ",\n";
  json << "  \"version\": \"" << result.scannerVersion << "\"\n";
  json << "}\n";

  return json.str();
}

// Helper: Create error JSON
static std::string errorJSON(const std::string &message,
                             const std::string &code = "") {
  std::ostringstream json;
  json << "{\n";
  json << "  \"error\": \"" << message << "\"";
  if (!code.empty()) {
    json << ",\n  \"code\": \"" << code << "\"";
  }
  json << "\n}\n";
  return json.str();
}

// ============================================================================
// PUBLIC API IMPLEMENTATION
// ============================================================================

extern "C" {

SAFEIMG_API const char *safeimg_scan_file(const char *filepath,
                                          const char *optionsJson) {
  if (!filepath) {
    return allocate_cstring(errorJSON("Filepath is NULL", "EINVAL"));
  }

  try {
    // Parse options
    ScanOptions opts = parseOptions(optionsJson);

    // Create scanner instance (thread-safe, no global state)
    ScannerV2 scanner(opts);

    // Perform scan
    ScanResultV2 result = scanner.scan(filepath);

    // Convert to JSON
    std::string jsonResult = resultToJSON(result);

    return allocate_cstring(jsonResult);

  } catch (const std::exception &e) {
    return allocate_cstring(errorJSON(e.what(), "EXCEPTION"));
  } catch (...) {
    return allocate_cstring(errorJSON("Unknown error", "UNKNOWN"));
  }
}

SAFEIMG_API const char *safeimg_scan_buffer(const uint8_t *data, size_t size,
                                            const char *optionsJson) {
  if (!data) {
    return allocate_cstring(errorJSON("Data pointer is NULL", "EINVAL"));
  }

  if (size == 0) {
    return allocate_cstring(errorJSON("Data size is zero", "EINVAL"));
  }

  try {
    // Parse options
    ScanOptions opts = parseOptions(optionsJson);

    // Create scanner instance
    ScannerV2 scanner(opts);

    // Convert buffer to vector
    std::vector<uint8_t> dataVec(data, data + size);

    // Perform scan (would need to add scanBuffer method to ScannerV2)
    // For now, write to temp file and scan
    // TODO: Add direct buffer scanning to ScannerV2

    // Temporary workaround: write to temp file
    std::string tempPath =
        "/tmp/safeimg_buffer_" + std::to_string((uintptr_t)data);
    std::ofstream tempFile(tempPath, std::ios::binary);
    tempFile.write((const char *)data, size);
    tempFile.close();

    ScanResultV2 result = scanner.scan(tempPath);

    // Clean up temp file
    std::remove(tempPath.c_str());

    // Convert to JSON
    std::string jsonResult = resultToJSON(result);

    return allocate_cstring(jsonResult);

  } catch (const std::exception &e) {
    return allocate_cstring(errorJSON(e.what(), "EXCEPTION"));
  } catch (...) {
    return allocate_cstring(errorJSON("Unknown error", "UNKNOWN"));
  }
}

SAFEIMG_API const char *safeimg_match_signatures(const uint8_t *data,
                                                 size_t size,
                                                 const char *contextJson) {
  if (!data) {
    return allocate_cstring(errorJSON("Data pointer is NULL", "EINVAL"));
  }

  if (size == 0) {
    return allocate_cstring(errorJSON("Data size is zero", "EINVAL"));
  }

  try {
    // Parse context JSON
    std::string context = "all";
    std::string sigFile = "";

    // TODO: Parse contextJson properly
    // For now, use defaults

    // Create signature engine
    SignatureEngineV2 engine;

    // Load signatures if specified
    if (!sigFile.empty()) {
      bool loaded = engine.loadSignaturesV2(sigFile);
      if (!loaded) {
        return allocate_cstring(
            errorJSON("Failed to load signatures", "ELOAD"));
      }
    }

    // Convert buffer to vector
    std::vector<uint8_t> dataVec(data, data + size);

    // Scan for signatures
    auto matches = engine.scan(dataVec, context);

    // Convert matches to JSON array
    std::ostringstream json;
    json << "[\n";

    for (size_t i = 0; i < matches.size(); ++i) {
      const auto &match = matches[i];
      json << "  {\n";
      json << "    \"name\": \"" << match.name << "\",\n";
      json << "    \"offset\": " << match.offset << ",\n";
      json << "    \"length\": " << match.length << ",\n";
      json << "    \"severity\": \"" << severityToString(match.severity)
           << "\",\n";
      json << "    \"category\": \"" << categoryToString(match.category)
           << "\",\n";
      json << "    \"description\": \"" << match.description << "\",\n";
      json << "    \"confidence\": " << match.confidence << "\n";
      json << "  }";
      if (i < matches.size() - 1)
        json << ",";
      json << "\n";
    }

    json << "]\n";

    return allocate_cstring(json.str());

  } catch (const std::exception &e) {
    return allocate_cstring(errorJSON(e.what(), "EXCEPTION"));
  } catch (...) {
    return allocate_cstring(errorJSON("Unknown error", "UNKNOWN"));
  }
}

SAFEIMG_API void safeimg_free(const char *ptr) {
  if (ptr) {
    free((void *)ptr);
  }
}

SAFEIMG_API const char *safeimg_version(void) { return "2.0.0"; }

SAFEIMG_API int safeimg_supports_format(const char *format) {
  if (!format)
    return 0;

  std::string fmt(format);
  return (fmt == "jpeg" || fmt == "png" || fmt == "webp" || fmt == "gif" ||
          fmt == "bmp" || fmt == "tiff" || fmt == "svg")
             ? 1
             : 0;
}

} // extern "C"
