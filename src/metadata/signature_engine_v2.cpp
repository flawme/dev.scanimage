#include "../../include/metadata/signature_engine_v2.h"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

namespace ImageScanner {
namespace Metadata {

SignatureEngineV2::SignatureEngineV2() : lastError_("") {}

SignatureEngineV2::~SignatureEngineV2() {}

// Load signatures from JSON v2 format
bool SignatureEngineV2::loadSignaturesV2(const std::string &filepath) {
  lastError_.clear();

  std::ifstream file(filepath);
  if (!file.is_open()) {
    lastError_ = "Failed to open signature file: " + filepath;
    return false;
  }

  // Read entire file
  std::stringstream buffer;
  buffer << file.rdbuf();
  std::string jsonContent = buffer.str();
  file.close();

  return parseJSONSignatures(jsonContent);
}

// Scan data with context awareness
std::vector<SignatureMatch>
SignatureEngineV2::scan(const std::vector<uint8_t> &data,
                        const std::string &context) {
  std::vector<SignatureMatch> matches;

  for (const auto &sig : signatures_) {
    // Skip disabled signatures
    if (!sig.enabled) {
      continue;
    }

    // Skip whitelisted signatures
    if (isWhitelisted(sig.name)) {
      continue;
    }

    // Check context
    if (!isValidContext(sig, context)) {
      continue;
    }

    // Scan based on signature type
    if (sig.type == SignatureType::EXACT) {
      // Scan for exact pattern
      for (size_t i = 0; i <= data.size() - sig.exactPattern.size(); ++i) {
        if (matchExact(data, i, sig.exactPattern)) {
          float confidence = sig.falsPositiveProne ? 0.8f : 1.0f;
          matches.push_back(SignatureMatch(sig.name, i, sig.exactPattern.size(),
                                           sig.severity, sig.category,
                                           sig.description, confidence));
        }
      }
    } else if (sig.type == SignatureType::WILDCARD) {
      // Scan for wildcard pattern
      for (size_t i = 0; i < data.size(); ++i) {
        if (matchWildcard(data, i, sig.wildcardPattern)) {
          // Calculate match length (approximate)
          size_t matchLen = sig.wildcardPattern.size();
          float confidence = sig.falsPositiveProne ? 0.8f : 1.0f;
          matches.push_back(SignatureMatch(sig.name, i, matchLen, sig.severity,
                                           sig.category, sig.description,
                                           confidence));
        }
      }
    }
    // REGEX type would go here (future implementation)
  }

  return matches;
}

// Match exact pattern
bool SignatureEngineV2::matchExact(const std::vector<uint8_t> &data,
                                   size_t offset,
                                   const std::vector<uint8_t> &pattern) {
  if (offset + pattern.size() > data.size()) {
    return false;
  }

  for (size_t i = 0; i < pattern.size(); ++i) {
    if (data[offset + i] != pattern[i]) {
      return false;
    }
  }

  return true;
}

// Match wildcard pattern
bool SignatureEngineV2::matchWildcard(
    const std::vector<uint8_t> &data, size_t offset,
    const std::vector<WildcardToken> &tokens) {
  return matchWildcardInternal(data, offset, tokens);
}

// Internal wildcard matching with backtracking
bool SignatureEngineV2::matchWildcardInternal(
    const std::vector<uint8_t> &data, size_t offset,
    const std::vector<WildcardToken> &tokens) {
  size_t dataIdx = offset;
  size_t tokenIdx = 0;

  while (tokenIdx < tokens.size()) {
    if (dataIdx >= data.size() &&
        tokens[tokenIdx].type != TokenType::ANY_SEQUENCE) {
      return false; // Out of data
    }

    const auto &token = tokens[tokenIdx];

    switch (token.type) {
    case TokenType::EXACT:
      if (dataIdx >= data.size() || data[dataIdx] != token.value[0]) {
        return false;
      }
      dataIdx++;
      tokenIdx++;
      break;

    case TokenType::ANY_BYTE:
      if (dataIdx >= data.size()) {
        return false;
      }
      dataIdx++;
      tokenIdx++;
      break;

    case TokenType::ANY_SEQUENCE:
      // Greedy matching: skip to next token
      tokenIdx++;
      if (tokenIdx >= tokens.size()) {
        return true; // Rest of data matches
      }

      // Try to match rest of pattern
      for (size_t i = dataIdx; i <= data.size(); ++i) {
        if (matchWildcardInternal(
                data, i,
                std::vector<WildcardToken>(tokens.begin() + tokenIdx,
                                           tokens.end()))) {
          return true;
        }
      }
      return false;

    case TokenType::BYTE_RANGE:
      if (dataIdx >= data.size()) {
        return false;
      }
      // Check if byte is in range [min, max]
      if (token.value.size() == 2) {
        uint8_t byte = data[dataIdx];
        if (byte < token.value[0] || byte > token.value[1]) {
          return false;
        }
      }
      dataIdx++;
      tokenIdx++;
      break;

    case TokenType::ONE_OF:
      if (dataIdx >= data.size()) {
        return false;
      }
      // Check if byte matches one of the values
      bool found = false;
      for (uint8_t val : token.value) {
        if (data[dataIdx] == val) {
          found = true;
          break;
        }
      }
      if (!found) {
        return false;
      }
      dataIdx++;
      tokenIdx++;
      break;
    }
  }

  return true;
}

// Parse wildcard pattern string into tokens
std::vector<WildcardToken>
SignatureEngineV2::parseWildcardPattern(const std::string &pattern) {
  std::vector<WildcardToken> tokens;

  for (size_t i = 0; i < pattern.size();) {
    if (pattern[i] == '?') {
      // Any single byte
      tokens.push_back(WildcardToken(TokenType::ANY_BYTE));
      i++;
    } else if (pattern[i] == '*') {
      // Any sequence
      tokens.push_back(WildcardToken(TokenType::ANY_SEQUENCE));
      i++;
    } else if (pattern[i] == '[') {
      // Byte range [xx-xx]
      i++;
      if (i + 4 < pattern.size() && pattern[i + 2] == '-') {
        uint8_t min, max;
        if (parseEscapeSequence(pattern, i, min) && i < pattern.size() &&
            pattern[i] == '-') {
          i++;
          if (parseEscapeSequence(pattern, i, max)) {
            std::vector<uint8_t> range = {min, max};
            tokens.push_back(WildcardToken(TokenType::BYTE_RANGE, range));
          }
        }
      }
      // Skip to ]
      while (i < pattern.size() && pattern[i] != ']')
        i++;
      i++;
    } else if (pattern[i] == '{') {
      // One of {xx,yy,zz}
      i++;
      std::vector<uint8_t> values;
      while (i < pattern.size() && pattern[i] != '}') {
        uint8_t byte;
        if (parseEscapeSequence(pattern, i, byte)) {
          values.push_back(byte);
        }
        if (i < pattern.size() && pattern[i] == ',')
          i++;
      }
      tokens.push_back(WildcardToken(TokenType::ONE_OF, values));
      if (i < pattern.size() && pattern[i] == '}')
        i++;
    } else if (pattern[i] == '\\' && i + 1 < pattern.size()) {
      // Escape sequence
      uint8_t byte;
      if (parseEscapeSequence(pattern, i, byte)) {
        tokens.push_back(WildcardToken(TokenType::EXACT, {byte}));
      }
    } else {
      // Literal character
      tokens.push_back(
          WildcardToken(TokenType::EXACT, {static_cast<uint8_t>(pattern[i])}));
      i++;
    }
  }

  return tokens;
}

// Parse escape sequence (e.g., \x41, \n, \t)
bool SignatureEngineV2::parseEscapeSequence(const std::string &pattern,
                                            size_t &i, uint8_t &byte) {
  if (i >= pattern.size())
    return false;

  if (pattern[i] == '\\' && i + 1 < pattern.size()) {
    i++;
    char escapeChar = pattern[i];

    if (escapeChar == 'x' && i + 2 < pattern.size()) {
      // Hex escape \xHH
      i++;
      std::string hexStr = pattern.substr(i, 2);
      try {
        byte = static_cast<uint8_t>(std::stoi(hexStr, nullptr, 16));
        i += 2;
        return true;
      } catch (...) {
        return false;
      }
    } else if (escapeChar == 'n') {
      byte = '\n';
      i++;
      return true;
    } else if (escapeChar == 'r') {
      byte = '\r';
      i++;
      return true;
    } else if (escapeChar == 't') {
      byte = '\t';
      i++;
      return true;
    } else if (escapeChar == '\\') {
      byte = '\\';
      i++;
      return true;
    } else {
      // Unknown escape, treat as literal
      byte = static_cast<uint8_t>(escapeChar);
      i++;
      return true;
    }
  } else {
    // Not an escape sequence
    byte = static_cast<uint8_t>(pattern[i]);
    i++;
    return true;
  }
}

// Whitelist management
void SignatureEngineV2::addToWhitelist(const std::string &signatureName) {
  whitelist_.insert(signatureName);
}

bool SignatureEngineV2::isWhitelisted(const std::string &signatureName) {
  return whitelist_.count(signatureName) > 0;
}

void SignatureEngineV2::clearWhitelist() { whitelist_.clear(); }

size_t SignatureEngineV2::getSignatureCount() const {
  return signatures_.size();
}

// Context validation
bool SignatureEngineV2::isValidContext(const Signature &sig,
                                       const std::string &context) {
  if (sig.contexts.empty() || sig.contexts.count("all") > 0) {
    return true;
  }

  return sig.contexts.count(context) > 0;
}

// JSON parsing (simplified - would use a proper JSON library in production)
bool SignatureEngineV2::parseJSONSignatures(const std::string &jsonContent) {
  // This is a simplified JSON parser for demonstration
  // In production, use a proper JSON library like nlohmann/json

  // For now, return false and set error
  // This would need full JSON parsing implementation
  lastError_ = "JSON parsing not yet implemented - use proper JSON library";
  return false;

  // TODO: Implement proper JSON parsing using nlohmann/json or similar
  // Expected format:
  // {
  //   "version": "2.0",
  //   "signatures": [
  //     {
  //       "name": "php_code",
  //       "pattern": "<?php",
  //       "type": "exact",
  //       "severity": "critical",
  //       "category": "script",
  //       "description": "PHP code detected",
  //       "contexts": ["metadata", "comment"],
  //       "enabled": true,
  //       "false_positive_prone": false
  //     }
  //   ],
  //   "whitelist": ["signature_name1", "signature_name2"]
  // }
}

Severity SignatureEngineV2::parseSeverity(const std::string &severityStr) {
  std::string lower = severityStr;
  std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

  if (lower == "info")
    return Severity::INFO;
  if (lower == "warning")
    return Severity::WARNING;
  if (lower == "high")
    return Severity::HIGH;
  if (lower == "critical")
    return Severity::CRITICAL;

  return Severity::WARNING; // Default
}

IssueCategory SignatureEngineV2::parseCategory(const std::string &categoryStr) {
  std::string lower = categoryStr;
  std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

  if (lower == "metadata")
    return IssueCategory::METADATA;
  if (lower == "script")
    return IssueCategory::SCRIPT;
  if (lower == "embedded-file" || lower == "embedded_file")
    return IssueCategory::EMBEDDED_FILE;
  if (lower == "xml")
    return IssueCategory::XML;
  if (lower == "structure")
    return IssueCategory::STRUCTURE;

  return IssueCategory::STRUCTURE; // Default
}

SignatureType SignatureEngineV2::parseType(const std::string &typeStr) {
  std::string lower = typeStr;
  std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

  if (lower == "exact")
    return SignatureType::EXACT;
  if (lower == "wildcard")
    return SignatureType::WILDCARD;
  if (lower == "regex")
    return SignatureType::REGEX;

  return SignatureType::EXACT; // Default
}

} // namespace Metadata
} // namespace ImageScanner
