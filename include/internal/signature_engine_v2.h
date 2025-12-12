#ifndef SIGNATURE_ENGINE_V2_H
#define SIGNATURE_ENGINE_V2_H

#include "scan_result_v2.h"
#include <cstddef>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace ImageScanner {
namespace Metadata {

// Signature types
enum class SignatureType { EXACT, WILDCARD, REGEX };

// Wildcard token types for pattern matching
enum class TokenType {
  EXACT,        // Exact byte match
  ANY_BYTE,     // ? wildcard (single byte)
  ANY_SEQUENCE, // * wildcard (0 or more bytes)
  BYTE_RANGE,   // [xx-xx] range
  ONE_OF        // {xx,yy,zz} one of
};

// Wildcard token structure
struct WildcardToken {
  TokenType type;
  std::vector<uint8_t> value; // For EXACT, BYTE_RANGE, ONE_OF

  WildcardToken(TokenType t) : type(t) {}
  WildcardToken(TokenType t, const std::vector<uint8_t> &v)
      : type(t), value(v) {}
};

// Signature match result
struct SignatureMatch {
  std::string name;
  size_t offset;
  size_t length;
  Severity severity;
  IssueCategory category;
  std::string description;
  float confidence; // 0.0-1.0 for false-positive reduction

  SignatureMatch(const std::string &n, size_t off, size_t len, Severity s,
                 IssueCategory c, const std::string &desc, float conf = 1.0f)
      : name(n), offset(off), length(len), severity(s), category(c),
        description(desc), confidence(conf) {}
};

// SignatureEngineV2 - Advanced pattern matching
class SignatureEngineV2 {
public:
  SignatureEngineV2();
  ~SignatureEngineV2();

  // Load signatures from JSON v2
  bool loadSignaturesV2(const std::string &filepath);

  // Scan with context awareness
  std::vector<SignatureMatch> scan(const std::vector<uint8_t> &data,
                                   const std::string &context = "all");

  // Pattern matching
  bool matchExact(const std::vector<uint8_t> &data, size_t offset,
                  const std::vector<uint8_t> &pattern);
  bool matchWildcard(const std::vector<uint8_t> &data, size_t offset,
                     const std::vector<WildcardToken> &tokens);

  // Whitelist management
  void addToWhitelist(const std::string &signatureName);
  bool isWhitelisted(const std::string &signatureName);
  void clearWhitelist();

  // Get loaded signature count
  size_t getSignatureCount() const;

  // Get last error
  std::string getLastError() const { return lastError_; }

private:
  struct Signature {
    std::string name;
    std::vector<uint8_t> exactPattern;      // For EXACT type
    std::vector<WildcardToken> wildcardPattern; // For WILDCARD type
    std::string regexPattern;               // For REGEX type (future)
    SignatureType type;
    Severity severity;
    IssueCategory category;
    std::string description;
    std::set<std::string> contexts; // Where to search
    bool enabled;
    bool falsPositiveProne;

    Signature()
        : type(SignatureType::EXACT), severity(Severity::WARNING),
          category(IssueCategory::STRUCTURE), enabled(true),
          falsPositiveProne(false) {}
  };

  std::vector<Signature> signatures_;
  std::set<std::string> whitelist_;
  std::string lastError_;

  // Wildcard parsing
  std::vector<WildcardToken> parseWildcardPattern(const std::string &pattern);
  bool parseEscapeSequence(const std::string &pattern, size_t &i,
                           uint8_t &byte);

  // Wildcard matching
  bool matchWildcardInternal(const std::vector<uint8_t> &data, size_t offset,
                             const std::vector<WildcardToken> &tokens);

  // Context checking
  bool isValidContext(const Signature &sig, const std::string &context);

  // JSON parsing helpers
  bool parseJSONSignatures(const std::string &jsonContent);
  Severity parseSeverity(const std::string &severityStr);
  IssueCategory parseCategory(const std::string &categoryStr);
  SignatureType parseType(const std::string &typeStr);
};

} // namespace Metadata
} // namespace ImageScanner

#endif // SIGNATURE_ENGINE_V2_H
