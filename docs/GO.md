# SafeImg v2.0 - Go Integration

## Overview

Use SafeImg shared library from Go via **cgo**.

---

## Basic Usage

### safeimg.go

```go
package safeimg

/*
#cgo CFLAGS: -I../include
#cgo LDFLAGS: -L../dist/lib -lsafeimg
#include <stdlib.h>
#include "safeimg_export.h"
*/
import "C"
import (
	"encoding/json"
	"errors"
	"unsafe"
)

// ScanResult represents the scan result
type ScanResult struct {
	IsSafe        bool              `json:"isSafe"`
	Format        string            `json:"format"`
	Issues        []Issue           `json:"issues"`
	Metadata      Metadata          `json:"metadata"`
	RealImageSize int               `json:"realImageSize"`
	ExtraDataSize int               `json:"extraDataSize"`
	ScanTimeMs    float64           `json:"scanTimeMs"`
	Version       string            `json:"version"`
}

// Issue represents a security issue
type Issue struct {
	Type        string `json:"type"`
	Description string `json:"description"`
	Severity    string `json:"severity"`
	Category    string `json:"category"`
	Offset      int    `json:"offset"`
	Length      int    `json:"length"`
}

// Metadata represents image metadata
type Metadata struct {
	HasEXIF           bool              `json:"hasEXIF"`
	HasXMP            bool              `json:"hasXMP"`
	HasGPS            bool              `json:"hasGPS"`
	ExifTags          map[string]string `json:"exifTags"`
	MetadataSizeBytes int               `json:"metadataSizeBytes"`
}

// Version returns the library version
func Version() string {
	cVersion := C.safeimg_version()
	return C.GoString(cVersion)
}

// SupportsFormat checks if a format is supported
func SupportsFormat(format string) bool {
	cFormat := C.CString(format)
	defer C.free(unsafe.Pointer(cFormat))
	
	return int(C.safeimg_supports_format(cFormat)) == 1
}

// ScanFile scans an image file
func ScanFile(filepath string) (*ScanResult, error) {
	cPath := C.CString(filepath)
	defer C.free(unsafe.Pointer(cPath))
	
	cResult := C.safeimg_scan_file(cPath, nil)
	defer C.safeimg_free(cResult)
	
	resultStr := C.GoString(cResult)
	
	var result ScanResult
	if err := json.Unmarshal([]byte(resultStr), &result); err != nil {
		return nil, err
	}
	
	return &result, nil
}

// ScanBuffer scans image data from memory
func ScanBuffer(data []byte) (*ScanResult, error) {
	if len(data) == 0 {
		return nil, errors.New("empty buffer")
	}
	
	cResult := C.safeimg_scan_buffer(
		(*C.uint8_t)(unsafe.Pointer(&data[0])),
		C.size_t(len(data)),
		nil,
	)
	defer C.safeimg_free(cResult)
	
	resultStr := C.GoString(cResult)
	
	var result ScanResult
	if err := json.Unmarshal([]byte(resultStr), &result); err != nil {
		return nil, err
	}
	
	return &result, nil
}
```

---

## Usage Examples

### main.go

```go
package main

import (
	"fmt"
	"io/ioutil"
	"log"
	
	"./safeimg"
)

func main() {
	// Check version
	fmt.Printf("SafeImg v%s\n", safeimg.Version())
	
	// Check format support
	fmt.Printf("JPEG supported: %v\n", safeimg.SupportsFormat("jpeg"))
	
	// Scan a file
	result, err := safeimg.ScanFile("photo.jpg")
	if err != nil {
		log.Fatal(err)
	}
	
	if result.IsSafe {
		fmt.Println("✓ Image is safe")
	} else {
		fmt.Println("⚠ Security issues found:")
		for _, issue := range result.Issues {
			fmt.Printf("  [%s] %s\n", issue.Severity, issue.Description)
		}
	}
	
	// Print metadata
	fmt.Printf("Format: %s\n", result.Format)
	fmt.Printf("Has EXIF: %v\n", result.Metadata.HasEXIF)
	fmt.Printf("Has GPS: %v\n", result.Metadata.HasGPS)
}
```

### Scan from Memory

```go
func scanFromMemory() {
	data, err := ioutil.ReadFile("photo.jpg")
	if err != nil {
		log.Fatal(err)
	}
	
	result, err := safeimg.ScanBuffer(data)
	if err != nil {
		log.Fatal(err)
	}
	
	fmt.Printf("Is safe: %v\n", result.IsSafe)
}
```

---

## HTTP Server Example

```go
package main

import (
	"encoding/json"
	"fmt"
	"io/ioutil"
	"log"
	"net/http"
	
	"./safeimg"
)

func main() {
	http.HandleFunc("/scan", scanHandler)
	
	fmt.Printf("SafeImg v%s\n", safeimg.Version())
	fmt.Println("Server starting on :8080")
	log.Fatal(http.ListenAndServe(":8080", nil))
}

func scanHandler(w http.ResponseWriter, r *http.Request) {
	if r.Method != "POST" {
		http.Error(w, "Method not allowed", http.StatusMethodNotAllowed)
		return
	}
	
	// Parse multipart form
	err := r.ParseMultipartForm(10 << 20) // 10 MB max
	if err != nil {
		http.Error(w, err.Error(), http.StatusBadRequest)
		return
	}
	
	file, _, err := r.FormFile("image")
	if err != nil {
		http.Error(w, "No file provided", http.StatusBadRequest)
		return
	}
	defer file.Close()
	
	// Read file data
	data, err := ioutil.ReadAll(file)
	if err != nil {
		http.Error(w, err.Error(), http.StatusInternalServerError)
		return
	}
	
	// Scan buffer
	result, err := safeimg.ScanBuffer(data)
	if err != nil {
		http.Error(w, err.Error(), http.StatusInternalServerError)
		return
	}
	
	// Return JSON
	w.Header().Set("Content-Type", "application/json")
	json.NewEncoder(w).Encode(result)
}
```

---

## Building

### Set Library Path

```bash
export LD_LIBRARY_PATH=/path/to/scanimage/dist/lib:$LD_LIBRARY_PATH
export CGO_LDFLAGS="-L/path/to/scanimage/dist/lib"
```

### Build

```bash
go build -o scanner main.go
```

### Run

```bash
LD_LIBRARY_PATH=/path/to/scanimage/dist/lib ./scanner
```

---

## go.mod

```go
module scanner

go 1.19

// No external dependencies needed
```

---

## Platform-Specific Notes

### Linux

```bash
export LD_LIBRARY_PATH=/path/to/scanimage/dist/lib
go run main.go
```

### macOS

```bash
export DYLD_LIBRARY_PATH=/path/to/scanimage/dist/lib
go run main.go
```

### Windows

```cmd
set PATH=%PATH%;C:\\path\\to\\scanimage\\dist\\lib
go run main.go
```

---

## Testing

```go
package safeimg

import (
	"testing"
)

func TestVersion(t *testing.T) {
	version := Version()
	if version != "2.0.0" {
		t.Errorf("Expected version 2.0.0, got %s", version)
	}
}

func TestSupportsFormat(t *testing.T) {
	if !SupportsFormat("jpeg") {
		t.Error("JPEG should be supported")
	}
	
	if SupportsFormat("unknown") {
		t.Error("Unknown format should not be supported")
	}
}

func TestScanFile(t *testing.T) {
	result, err := ScanFile("testdata/safe.jpg")
	if err != nil {
		t.Fatal(err)
	}
	
	if !result.IsSafe {
		t.Error("Expected safe image")
	}
}
```

---

## See Also

- **API.md** - C API reference
- **PYTHON.md** - Python integration
- **NODE_FFI.md** - Node.js integration
