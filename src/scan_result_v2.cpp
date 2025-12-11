#include "../include/scan_result_v2.h"
#include <sstream>

namespace ImageScanner {

std::string ScanResultV2::toJSON() const {
  std::ostringstream oss;
  oss << "{";

  // isSafe
  oss << "\"isSafe\":" << (isSafe ? "true" : "false") << ",";

  // format
  oss << "\"format\":\"" << format << "\",";

  // realImageSize, extraDataSize
  oss << "\"realImageSize\":" << realImageSize << ",";
  oss << "\"extraDataSize\":" << extraDataSize << ",";

  // scanTimeMs, scannerVersion
  oss << "\"scanTimeMs\":" << scanTimeMs << ",";
  oss << "\"scannerVersion\":\"" << scannerVersion << "\",";

  // metadata
  oss << "\"metadata\":{";
  oss << "\"hasEXIF\":" << (metadata.hasEXIF ? "true" : "false") << ",";
  oss << "\"hasXMP\":" << (metadata.hasXMP ? "true" : "false") << ",";
  oss << "\"hasGPS\":" << (metadata.hasGPS ? "true" : "false") << ",";
  oss << "\"metadataSizeBytes\":" << metadata.metadataSizeBytes << ",";
  oss << "\"oversizedMetadata\":"
      << (metadata.oversizedMetadata ? "true" : "false") << ",";

  // GPS warnings
  oss << "\"gpsWarnings\":[";
  for (size_t i = 0; i < metadata.gpsWarnings.size(); ++i) {
    if (i > 0)
      oss << ",";
    oss << "\"" << metadata.gpsWarnings[i] << "\"";
  }
  oss << "],";

  // EXIF tags
  oss << "\"exifTags\":{";
  size_t exifCount = 0;
  for (const auto &kv : metadata.exifTags) {
    if (exifCount++ > 0)
      oss << ",";
    oss << "\"" << kv.first << "\":\"" << kv.second << "\"";
  }
  oss << "},";

  // XMP content (truncate if too long)
  oss << "\"xmpContent\":\"";
  if (metadata.xmpContent.length() > 200) {
    oss << metadata.xmpContent.substr(0, 200) << "...";
  } else {
    oss << metadata.xmpContent;
  }
  oss << "\"";
  oss << "},"; // End metadata

  // issues array
  oss << "\"issues\":[";
  for (size_t i = 0; i < issues.size(); ++i) {
    if (i > 0)
      oss << ",";

    oss << "{";
    oss << "\"type\":\"" << issues[i].type << "\",";
    oss << "\"description\":\"" << issues[i].description << "\",";
    oss << "\"severity\":\"" << severityToString(issues[i].severity) << "\",";
    oss << "\"category\":\"" << categoryToString(issues[i].category) << "\",";
    oss << "\"offset\":" << issues[i].offset << ",";
    oss << "\"length\":" << issues[i].length << ",";

    // metadata map
    oss << "\"metadata\":{";
    size_t metaCount = 0;
    for (const auto &kv : issues[i].metadata) {
      if (metaCount++ > 0)
        oss << ",";
      oss << "\"" << kv.first << "\":\"" << kv.second << "\"";
    }
    oss << "}";

    oss << "}";
  }
  oss << "]"; // End issues

  oss << "}";
  return oss.str();
}

} // namespace ImageScanner
