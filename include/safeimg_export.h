#ifndef SAFEIMG_EXPORT_H
#define SAFEIMG_EXPORT_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Platform-specific export/import macros
#ifdef _WIN32
#ifdef SAFEIMG_EXPORTS
#define SAFEIMG_API __declspec(dllexport)
#else
#define SAFEIMG_API __declspec(dllimport)
#endif
#else
#define SAFEIMG_API __attribute__((visibility("default")))
#endif

/**
 * SafeImg v2.0 - Shared Library API
 *
 * A portable, language-independent image security scanning engine.
 * All functions are thread-safe and allocate result strings with malloc().
 *
 * @version 2.0.0
 */

/**
 * Scan an image file for security threats.
 *
 * @param filepath Absolute or relative path to image file
 * @param optionsJson JSON string with scan options (or NULL for defaults)
 *
 * Options JSON format (all fields optional):
 * {
 *   "strictMetadata": true,
 *   "allowGPS": false,
 *   "extractMetadata": true,
 *   "checkPolyglot": true,
 *   "checkEntropy": true,
 *   "checkIntegrity": true,
 *   "checkSignatures": true,
 *   "reduceFalsePositives": true,
 *   "confidenceThreshold": 0.7,
 *   "signatureFile": "path/to/signatures.json"
 * }
 *
 * @return JSON string with scan results (must be freed with safeimg_free())
 *
 * Result JSON format:
 * {
 *   "isSafe": true,
 *   "format": "jpeg",
 *   "issues": [
 *     {
 *       "type": "gps_detected",
 *       "description": "GPS coordinates found",
 *       "severity": "warning",
 *       "category": "metadata",
 *       "offset": 1234,
 *       "length": 56
 *     }
 *   ],
 *   "metadata": {
 *     "hasEXIF": true,
 *     "hasXMP": false,
 *     "hasGPS": true,
 *     "exifTags": {"Make": "Canon", "Model": "EOS 5D"}
 *   },
 *   "scanTimeMs": 12.5,
 *   "version": "2.0.0"
 * }
 *
 * On error:
 * {
 *   "error": "File not found",
 *   "code": "ENOENT"
 * }
 */
SAFEIMG_API const char *safeimg_scan_file(const char *filepath,
                                          const char *optionsJson);

/**
 * Scan image data from memory buffer.
 *
 * @param data Pointer to image data
 * @param size Size of image data in bytes
 * @param optionsJson JSON string with scan options (or NULL for defaults)
 *
 * @return JSON string with scan results (must be freed with safeimg_free())
 */
SAFEIMG_API const char *safeimg_scan_buffer(const uint8_t *data, size_t size,
                                            const char *optionsJson);

/**
 * Match signatures against data (advanced mode).
 *
 * @param data Pointer to data
 * @param size Size of data in bytes
 * @param contextJson JSON with context and signature options
 *
 * Context JSON format:
 * {
 *   "context": "xmp",  // or "metadata", "all", etc.
 *   "signatureFile": "path/to/signatures_v2.json"
 * }
 *
 * @return JSON array of matches (must be freed with safeimg_free())
 *
 * Result format:
 * [
 *   {
 *     "name": "php_code",
 *     "offset": 1234,
 *     "length": 5,
 *     "severity": "critical",
 *     "category": "script",
 *     "description": "PHP code detected",
 *     "confidence": 1.0
 *   }
 * ]
 */
SAFEIMG_API const char *safeimg_match_signatures(const uint8_t *data,
                                                 size_t size,
                                                 const char *contextJson);

/**
 * Free memory allocated by SafeImg functions.
 *
 * IMPORTANT: All strings returned by safeimg_* functions MUST be freed
 * with this function to avoid memory leaks.
 *
 * @param ptr Pointer returned by safeimg_scan_file/buffer/match_signatures
 */
SAFEIMG_API void safeimg_free(const char *ptr);

/**
 * Get SafeImg version string.
 *
 * @return Version string (e.g., "2.0.0") - DO NOT FREE
 */
SAFEIMG_API const char *safeimg_version(void);

/**
 * Check if SafeImg supports a given image format.
 *
 * @param format Format name ("jpeg", "png", "webp", "gif", "bmp", "tiff",
 * "svg")
 * @return 1 if supported, 0 if not
 */
SAFEIMG_API int safeimg_supports_format(const char *format);

#ifdef __cplusplus
}
#endif

#endif // SAFEIMG_EXPORT_H
