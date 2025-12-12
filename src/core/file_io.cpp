#include "internal/file_io.h"
#include <cstring>
#include <fstream>

// Platform-specific includes
#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace ImageScanner {

FileIOManager::FileIOManager(const ScanOptions &options)
    : options_(options), mmapUsed_(false), mmapAddr_(nullptr), mmapSize_(0),
#ifdef _WIN32
      fileHandle_(nullptr), mappingHandle_(nullptr)
#else
      fd_(-1)
#endif
{
}

FileIOManager::~FileIOManager() { unmapFile(); }

std::vector<uint8_t> FileIOManager::loadFile(const std::string &filepath) {
  lastError_.clear();
  mmapUsed_ = false;

  // Get file size first
  size_t fileSize = getFileSize(filepath);
  if (fileSize == 0) {
    lastError_ = "File not found or empty";
    return {};
  }

  // Check if file exceeds max size
  if (fileSize > 100 * 1024 * 1024) { // 100MB limit
    lastError_ = "File exceeds maximum size (100MB)";
    return {};
  }

  // Decide whether to use mmap
  bool useMmap = options_.useMmap && (fileSize >= options_.mmapThreshold);

  if (useMmap) {
    // Try mmap first
    std::vector<uint8_t> data = loadWithMmap(filepath, fileSize);
    if (!data.empty()) {
      mmapUsed_ = true;
      return data;
    }
    // If mmap failed, fall back to read
    lastError_.clear(); // Clear mmap error for fallback
  }

  // Use standard read
  return loadWithRead(filepath, fileSize);
}

size_t FileIOManager::getFileSize(const std::string &filepath) {
#ifdef _WIN32
  WIN32_FILE_ATTRIBUTE_DATA fileInfo;
  if (GetFileAttributesExA(filepath.c_str(), GetFileExInfoStandard,
                           &fileInfo)) {
    LARGE_INTEGER size;
    size.HighPart = fileInfo.nFileSizeHigh;
    size.LowPart = fileInfo.nFileSizeLow;
    return static_cast<size_t>(size.QuadPart);
  }
  return 0;
#else
  struct stat st;
  if (stat(filepath.c_str(), &st) == 0) {
    return static_cast<size_t>(st.st_size);
  }
  return 0;
#endif
}

std::vector<uint8_t> FileIOManager::loadWithMmap(const std::string &filepath,
                                                 size_t fileSize) {
#ifdef _WIN32
  // Windows mmap using CreateFileMapping + MapViewOfFile
  fileHandle_ =
      CreateFileA(filepath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                  OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

  if (fileHandle_ == INVALID_HANDLE_VALUE) {
    lastError_ = "Cannot open file for mmap (Windows)";
    return {};
  }

  mappingHandle_ =
      CreateFileMappingA(fileHandle_, nullptr, PAGE_READONLY, 0, 0, nullptr);

  if (!mappingHandle_) {
    CloseHandle(fileHandle_);
    fileHandle_ = nullptr;
    lastError_ = "CreateFileMapping failed";
    return {};
  }

  mmapAddr_ = MapViewOfFile(mappingHandle_, FILE_MAP_READ, 0, 0, 0);

  if (!mmapAddr_) {
    CloseHandle(mappingHandle_);
    CloseHandle(fileHandle_);
    mappingHandle_ = nullptr;
    fileHandle_ = nullptr;
    lastError_ = "MapViewOfFile failed";
    return {};
  }

  mmapSize_ = fileSize;

  // Copy mapped memory to vector
  std::vector<uint8_t> data(fileSize);
  std::memcpy(data.data(), mmapAddr_, fileSize);

  // Unmap immediately after copying
  unmapFile();

  return data;

#else
  // POSIX mmap
  fd_ = open(filepath.c_str(), O_RDONLY);
  if (fd_ == -1) {
    lastError_ = "Cannot open file for mmap (POSIX)";
    return {};
  }

  mmapAddr_ = mmap(nullptr, fileSize, PROT_READ, MAP_PRIVATE, fd_, 0);

  if (mmapAddr_ == MAP_FAILED) {
    close(fd_);
    fd_ = -1;
    mmapAddr_ = nullptr;
    lastError_ = "mmap failed";
    return {};
  }

  mmapSize_ = fileSize;

  // Copy mapped memory to vector
  std::vector<uint8_t> data(fileSize);
  std::memcpy(data.data(), mmapAddr_, fileSize);

  // Unmap immediately after copying
  unmapFile();

  return data;
#endif
}

std::vector<uint8_t> FileIOManager::loadWithRead(const std::string &filepath,
                                                 size_t fileSize) {
  std::ifstream file(filepath, std::ios::binary);
  if (!file) {
    lastError_ = "Cannot open file";
    return {};
  }

  std::vector<uint8_t> data(fileSize);
  if (!file.read(reinterpret_cast<char *>(data.data()), fileSize)) {
    lastError_ = "Failed to read file";
    return {};
  }

  return data;
}

void FileIOManager::unmapFile() {
  if (!mmapAddr_) {
    return;
  }

#ifdef _WIN32
  if (mmapAddr_) {
    UnmapViewOfFile(mmapAddr_);
    mmapAddr_ = nullptr;
  }
  if (mappingHandle_) {
    CloseHandle(mappingHandle_);
    mappingHandle_ = nullptr;
  }
  if (fileHandle_) {
    CloseHandle(fileHandle_);
    fileHandle_ = nullptr;
  }
#else
  if (mmapAddr_ != MAP_FAILED) {
    munmap(mmapAddr_, mmapSize_);
    mmapAddr_ = nullptr;
  }
  if (fd_ != -1) {
    close(fd_);
    fd_ = -1;
  }
#endif

  mmapSize_ = 0;
}

} // namespace ImageScanner
