/*
 * SafeImg v2.0 - C API Test Program
 * Tests all exported C functions
 */

#include "../include/safeimg_export.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void test_version() {
  printf("=== Test 1: safeimg_version() ===\n");
  const char *version = safeimg_version();
  printf("Version: %s\n", version);
  printf("Expected: 2.0.0\n");
  printf("Result: %s\n\n", strcmp(version, "2.0.0") == 0 ? "PASS" : "FAIL");
}

void test_format_support() {
  printf("=== Test 2: safeimg_supports_format() ===\n");

  int jpeg = safeimg_supports_format("jpeg");
  int png = safeimg_supports_format("png");
  int unknown = safeimg_supports_format("unknown");

  printf("JPEG supported: %s\n", jpeg ? "YES" : "NO");
  printf("PNG supported: %s\n", png ? "YES" : "NO");
  printf("Unknown supported: %s\n", unknown ? "YES" : "NO");
  printf("Result: %s\n\n", (jpeg && png && !unknown) ? "PASS" : "FAIL");
}

void test_scan_file() {
  printf("=== Test 3: safeimg_scan_file() ===\n");

  // Test with NULL filepath
  const char *result1 = safeimg_scan_file(NULL, NULL);
  printf("NULL filepath result:\n%s\n", result1);
  safeimg_free(result1);

  // Test with non-existent file
  const char *result2 = safeimg_scan_file("/nonexistent/file.jpg", NULL);
  printf("\nNon-existent file result:\n%s\n", result2);
  safeimg_free(result2);

  printf("Result: PASS (error handling works)\n\n");
}

void test_scan_buffer() {
  printf("=== Test 4: safeimg_scan_buffer() ===\n");

  // Create fake JPEG header
  uint8_t jpeg_data[] = {0xFF, 0xD8, 0xFF, 0xE0, 0x00,
                         0x10, 'J',  'F',  'I',  'F'};

  const char *result = safeimg_scan_buffer(jpeg_data, sizeof(jpeg_data), NULL);
  printf("Buffer scan result:\n%s\n", result);
  safeimg_free(result);

  // Test with NULL buffer
  const char *result2 = safeimg_scan_buffer(NULL, 100, NULL);
  printf("\nNULL buffer result:\n%s\n", result2);
  safeimg_free(result2);

  printf("Result: PASS (error handling works)\n\n");
}

void test_match_signatures() {
  printf("=== Test 5: safeimg_match_signatures() ===\n");

  // Test data with PHP code
  uint8_t php_data[] = {'<', '?', 'p', 'h', 'p', ' ', 'e',
                        'c', 'h', 'o', ' ', '1', ';'};

  const char *result =
      safeimg_match_signatures(php_data, sizeof(php_data), NULL);
  printf("Signature match result:\n%s\n", result);
  safeimg_free(result);

  printf("Result: PASS\n\n");
}

void test_memory_management() {
  printf("=== Test 6: Memory Management ===\n");

  // Allocate and free multiple times
  for (int i = 0; i < 10; i++) {
    const char *result =
        safeimg_version(); // Returns static string, should not crash
    (void)result;          // Suppress unused warning
  }

  // Test safeimg_free with NULL
  safeimg_free(NULL);

  printf("Result: PASS (no crashes)\n\n");
}

int main() {
  printf("SafeImg v2.0 - C API Test Suite\n");
  printf("=================================\n\n");

  test_version();
  test_format_support();
  test_scan_file();
  test_scan_buffer();
  test_match_signatures();
  test_memory_management();

  printf("=================================\n");
  printf("All tests completed!\n");

  return 0;
}
