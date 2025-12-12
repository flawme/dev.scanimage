#ifndef FILE_IO_H
#define FILE_IO_H

#include "config.h"
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace ImageScanner {

// File I/O Manager with mmap support
class FileIOManager {
public:
  explicit FileIOManager(const ScanOptions &options);
  ~FileIOManager();

  // Load file with optimal method (mmap or read)
  std::vector<uint8_t> loadFile(const std::string &filepath);

  // Check if last load used mmap
  bool wasMmapUsed() const { return mmapUsed_; }

  // Get last error message
  std::string getLastError() const { return lastError_; }

private:
  const ScanOptions &options_;
  bool mmapUsed_;
  std::string lastError_;

  // Platform-specific mmap data
  void *mmapAddr_;
  size_t mmapSize_;

#ifdef _WIN32
  void *fileHandle_;
  void *mappingHandle_;
#else
  int fd_;
#endif

  // Load methods
  std::vector<uint8_t> loadWithMmap(const std::string &filepath,
                                    size_t fileSize);
  std::vector<uint8_t> loadWithRead(const std::string &filepath,
                                    size_t fileSize);

  // Cleanup
  void unmapFile();

  // Get file size
  size_t getFileSize(const std::string &filepath);
};

} // namespace ImageScanner

#endif // FILE_IO_H
