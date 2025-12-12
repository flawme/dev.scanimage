# SafeImg v2.0 - Rust Integration

## Overview

Use SafeImg shared library from Rust via **bindgen** or manual FFI.

---

## Method 1: Manual FFI

### lib.rs

```rust
use std::ffi::{CStr, CString};
use std::os::raw::{c_char, c_int, c_uint};
use serde::{Deserialize, Serialize};

#[link(name = "safeimg")]
extern "C" {
    fn safeimg_version() -> *const c_char;
    fn safeimg_supports_format(format: *const c_char) -> c_int;
    fn safeimg_scan_file(filepath: *const c_char, options: *const c_char) -> *const c_char;
    fn safeimg_scan_buffer(data: *const c_uint, size: usize, options: *const c_char) -> *const c_char;
    fn safeimg_free(ptr: *const c_char);
}

#[derive(Debug, Serialize, Deserialize)]
pub struct ScanResult {
    #[serde(rename = "isSafe")]
    pub is_safe: bool,
    pub format: String,
    pub issues: Vec<Issue>,
    pub metadata: Metadata,
    #[serde(rename = "realImageSize")]
    pub real_image_size: usize,
    #[serde(rename = "extraDataSize")]
    pub extra_data_size: usize,
    #[serde(rename = "scanTimeMs")]
    pub scan_time_ms: f64,
    pub version: String,
}

#[derive(Debug, Serialize, Deserialize)]
pub struct Issue {
    #[serde(rename = "type")]
    pub issue_type: String,
    pub description: String,
    pub severity: String,
    pub category: String,
    pub offset: usize,
    pub length: usize,
}

#[derive(Debug, Serialize, Deserialize)]
pub struct Metadata {
    #[serde(rename = "hasEXIF")]
    pub has_exif: bool,
    #[serde(rename = "hasXMP")]
    pub has_xmp: bool,
    #[serde(rename = "hasGPS")]
    pub has_gps: bool,
    #[serde(rename = "exifTags")]
    pub exif_tags: std::collections::HashMap<String, String>,
    #[serde(rename = "metadataSizeBytes")]
    pub metadata_size_bytes: usize,
}

pub struct SafeImg;

impl SafeImg {
    pub fn version() -> String {
        unsafe {
            let c_str = safeimg_version();
            CStr::from_ptr(c_str).to_string_lossy().into_owned()
        }
    }
    
    pub fn supports_format(format: &str) -> bool {
        unsafe {
            let c_format = CString::new(format).unwrap();
            safeimg_supports_format(c_format.as_ptr()) == 1
        }
    }
    
    pub fn scan_file(filepath: &str) -> Result<ScanResult, String> {
        unsafe {
            let c_path = CString::new(filepath).unwrap();
            let result_ptr = safeimg_scan_file(c_path.as_ptr(), std::ptr::null());
            
            let result_str = CStr::from_ptr(result_ptr).to_string_lossy();
            let result = serde_json::from_str(&result_str)
                .map_err(|e| format!("JSON parse error: {}", e))?;
            
            safeimg_free(result_ptr);
            
            Ok(result)
        }
    }
    
    pub fn scan_buffer(data: &[u8]) -> Result<ScanResult, String> {
        unsafe {
            let result_ptr = safeimg_scan_buffer(
                data.as_ptr() as *const c_uint,
                data.len(),
                std::ptr::null()
            );
            
            let result_str = CStr::from_ptr(result_ptr).to_string_lossy();
            let result = serde_json::from_str(&result_str)
                .map_err(|e| format!("JSON parse error: {}", e))?;
            
            safeimg_free(result_ptr);
            
            Ok(result)
        }
    }
}
```

---

## Usage Examples

### main.rs

```rust
use std::fs;

mod safeimg;
use safeimg::SafeImg;

fn main() -> Result<(), Box<dyn std::error::Error>> {
    // Check version
    println!("SafeImg v{}", SafeImg::version());
    
    // Check format support
    println!("JPEG supported: {}", SafeImg::supports_format("jpeg"));
    
    // Scan a file
    let result = SafeImg::scan_file("photo.jpg")?;
    
    if result.is_safe {
        println!("✓ Image is safe");
    } else {
        println!("⚠ Security issues found:");
        for issue in &result.issues {
            println!("  [{}] {}", issue.severity, issue.description);
        }
    }
    
    // Print metadata
    println!("Format: {}", result.format);
    println!("Has EXIF: {}", result.metadata.has_exif);
    println!("Has GPS: {}", result.metadata.has_gps);
    
    Ok(())
}
```

### Scan from Memory

```rust
fn scan_from_memory() -> Result<(), Box<dyn std::error::Error>> {
    let data = fs::read("photo.jpg")?;
    let result = SafeImg::scan_buffer(&data)?;
    
    println!("Is safe: {}", result.is_safe);
    Ok(())
}
```

---

## Actix Web Example

### Cargo.toml

```toml
[dependencies]
actix-web = "4.0"
actix-multipart = "0.6"
serde = { version = "1.0", features = ["derive"] }
serde_json = "1.0"
tokio = { version = "1", features = ["full"] }
```

### main.rs

```rust
use actix_web::{web, App, HttpResponse, HttpServer};
use actix_multipart::Multipart;
use futures_util::StreamExt;

mod safeimg;
use safeimg::SafeImg;

async fn scan_upload(mut payload: Multipart) -> HttpResponse {
    // Read uploaded file
    let mut data = Vec::new();
    
    while let Some(item) = payload.next().await {
        let mut field = match item {
            Ok(f) => f,
            Err(_) => return HttpResponse::BadRequest().finish(),
        };
        
        while let Some(chunk) = field.next().await {
            match chunk {
                Ok(bytes) => data.extend_from_slice(&bytes),
                Err(_) => return HttpResponse::InternalServerError().finish(),
            }
        }
    }
    
    // Scan buffer
    match SafeImg::scan_buffer(&data) {
        Ok(result) => HttpResponse::Ok().json(result),
        Err(e) => HttpResponse::InternalServerError().body(e),
    }
}

#[actix_web::main]
async fn main() -> std::io::Result<()> {
    println!("SafeImg v{}", SafeImg::version());
    println!("Server starting on http://127.0.0.1:8080");
    
    HttpServer::new(|| {
        App::new()
            .route("/scan", web::post().to(scan_upload))
    })
    .bind(("127.0.0.1", 8080))?
    .run()
    .await
}
```

---

## Build Configuration

### Cargo.toml

```toml
[package]
name = "scanner"
version = "0.1.0"
edition = "2021"

[dependencies]
serde = { version = "1.0", features = ["derive"] }
serde_json = "1.0"

[build-dependencies]
# Optional: bindgen for auto-generation
# bindgen = "0.65"
```

### build.rs (Optional)

```rust
fn main() {
    println!("cargo:rustc-link-search=native=../dist/lib");
    println!("cargo:rustc-link-lib=dylib=safeimg");
    
    // For bindgen auto-generation (optional)
    // let bindings = bindgen::Builder::default()
    //     .header("../include/safeimg_export.h")
    //     .generate()
    //     .expect("Unable to generate bindings");
    //
    // bindings
    //     .write_to_file("src/bindings.rs")
    //     .expect("Couldn't write bindings!");
}
```

---

## Building

### Linux

```bash
export LD_LIBRARY_PATH=/path/to/scanimage/dist/lib:$LD_LIBRARY_PATH
cargo build --release
cargo run
```

### macOS

```bash
export DYLD_LIBRARY_PATH=/path/to/scanimage/dist/lib:$DYLD_LIBRARY_PATH
cargo build --release
cargo run
```

### Windows

```cmd
set PATH=%PATH%;C:\\path\\to\\scanimage\\dist\\lib
cargo build --release
cargo run
```

---

## Testing

```rust
#[cfg(test)]
mod tests {
    use super::*;
    
    #[test]
    fn test_version() {
        let version = SafeImg::version();
        assert_eq!(version, "2.0.0");
    }
    
    #[test]
    fn test_supports_format() {
        assert!(SafeImg::supports_format("jpeg"));
        assert!(!SafeImg::supports_format("unknown"));
    }
    
    #[test]
    fn test_scan_file() {
        let result = SafeImg::scan_file("testdata/safe.jpg").unwrap();
        assert!(result.is_safe);
    }
}
```

---

## Method 2: Using Bindgen

Generate bindings automatically:

```bash
bindgen include/safeimg_export.h -o src/bindings.rs
```

Then use:

```rust
mod bindings;
use bindings::*;
```

---

## See Also

- **API.md** - C API reference
- **PYTHON.md** - Python integration
- **GO.md** - Go integration
