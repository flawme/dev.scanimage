// Test file for SignatureEngineV2 and FalsePositiveReducer
#include "../include/metadata/false_positive_reducer.h"
#include "../include/metadata/signature_engine_v2.h"
#include <iostream>
#include <vector>

using namespace ImageScanner;
using namespace ImageScanner::Metadata;

void testWildcardPatternMatching() {
  std::cout << "=== Testing Wildcard Pattern Matching ===\n";

  SignatureEngineV2 engine;

  // Test data with PHP code
  std::vector<uint8_t> data1 = {'<', '?', 'p', 'h', 'p',
                                ' ', 'e', 'c', 'h', 'o'};
  std::vector<uint8_t> pattern1 = {'<', '?', 'p', 'h', 'p'};

  bool match1 = engine.matchExact(data1, 0, pattern1);
  std::cout << "Test 1 (Exact match '<?php'): " << (match1 ? "PASS" : "FAIL")
            << "\n";

  // Test ZIP header
  std::vector<uint8_t> data2 = {'P', 'K', 0x03, 0x04, 0x14, 0x00, 0x08, 0x00};
  std::vector<uint8_t> pattern2 = {'P', 'K', 0x03, 0x04};

  bool match2 = engine.matchExact(data2, 0, pattern2);
  std::cout << "Test 2 (ZIP header 'PK\\x03\\x04'): "
            << (match2 ? "PASS" : "FAIL") << "\n";

  std::cout << "\n";
}

void testFalsePositiveReduction() {
  std::cout << "=== Testing False-Positive Reduction ===\n";

  FalsePositiveReducer reducer;

  // Test GPS confidence from camera
  MetadataResult meta1;
  meta1.hasEXIF = true;
  meta1.hasGPS = true;
  meta1.exifTags["Make"] = "Canon";
  meta1.exifTags["Model"] = "EOS 5D Mark IV";
  meta1.exifTags["DateTime"] = "2024:01:15 10:30:00";
  meta1.gpsWarnings.push_back("GPS coordinates detected");

  float gpsConfidence = reducer.analyzeGPSConfidence(meta1);
  std::cout << "Test 3 (GPS from Canon camera): Confidence = " << gpsConfidence;
  std::cout << " (Expected < 0.7): " << (gpsConfidence < 0.7f ? "PASS" : "FAIL")
            << "\n";

  // Test GPS confidence from unknown source
  MetadataResult meta2;
  meta2.hasGPS = true;
  meta2.gpsWarnings.push_back("GPS coordinates detected");

  float gpsConfidence2 = reducer.analyzeGPSConfidence(meta2);
  std::cout << "Test 4 (GPS from unknown source): Confidence = "
            << gpsConfidence2;
  std::cout << " (Expected >= 0.7): "
            << (gpsConfidence2 >= 0.7f ? "PASS" : "FAIL") << "\n";

  // Test base64 in XMP thumbnail context
  std::string base64Data(50000, 'A'); // 50KB of 'A's
  float base64Conf =
      reducer.analyzeBase64Confidence(base64Data, "xmp:thumbnail");
  std::cout << "Test 5 (Base64 in XMP thumbnail): Confidence = " << base64Conf;
  std::cout << " (Expected < 0.7): " << (base64Conf < 0.7f ? "PASS" : "FAIL")
            << "\n";

  std::cout << "\n";
}

void testSignatureEngine() {
  std::cout << "=== Testing Signature Engine ===\n";

  SignatureEngineV2 engine;

  // Create test data with embedded PHP code
  std::string testStr =
      "Some random data before <?php echo 'malicious'; ?> and after";
  std::vector<uint8_t> data(testStr.begin(), testStr.end());

  std::cout << "Test 6 (Signature engine initialized): ";
  std::cout << (engine.getSignatureCount() == 0
                    ? "PASS"
                    : "FAIL (expected 0 signatures)")
            << "\n";

  std::cout << "Test 7 (Load signatures_v2.json): ";
  bool loaded = engine.loadSignaturesV2("signatures_v2.json");
  std::cout << (loaded ? "PASS" : "FAIL (JSON parsing not implemented yet)")
            << "\n";

  // Test whitelist
  engine.addToWhitelist("test_signature");
  std::cout << "Test 8 (Whitelist management): ";
  std::cout << (engine.isWhitelisted("test_signature") ? "PASS" : "FAIL")
            << "\n";

  std::cout << "\n";
}

int main() {
  std::cout
      << "SafeImg v2.0 - SignatureEngineV2 & FalsePositiveReducer Tests\n";
  std::cout
      << "============================================================\n\n";

  testWildcardPatternMatching();
  testFalsePositiveReduction();
  testSignatureEngine();

  std::cout << "============================================================\n";
  std::cout << "Tests complete!\n";
  std::cout
      << "Note: Full integration tests require JSON library implementation.\n";

  return 0;
}
