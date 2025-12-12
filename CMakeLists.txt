cmake_minimum_required(VERSION 3.10)
project(SafeImg VERSION 2.0.0 LANGUAGES CXX)

# C++17 required
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/dist/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/dist/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/dist/lib)

# Platform-specific settings
if(WIN32)
    add_compile_definitions(SAFEIMG_EXPORTS)
    # Enable exceptions on Windows
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHsc")
else()
    # Unix: export symbols with visibility
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility=hidden")
endif()

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)

# SafeImg shared library source files
set(SAFEIMG_SOURCES
    # C API wrapper
    src/safeimg_export.cpp
    
    # Core engine
    src/image_scanner.cpp
    src/scanner_v2.cpp
    src/scan_result_v2.cpp
    src/file_io.cpp
    src/entropy_analyzer.cpp
    
    # Validators
    src/validators/jpeg_validator.cpp
    src/validators/png_validator.cpp
    src/validators/webp_validator.cpp
    src/validators/gif_validator.cpp
    src/validators/bmp_validator.cpp
    src/validators/tiff_validator.cpp
    src/validators/svg_validator.cpp
    
    # Metadata parsers
    src/metadata/exif_parser.cpp
    src/metadata/xmp_parser.cpp
    src/metadata/signature_engine_v2.cpp
    src/metadata/false_positive_reducer.cpp
)

# Create shared library
add_library(safeimg SHARED ${SAFEIMG_SOURCES})

# Set library properties
set_target_properties(safeimg PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 2
    PUBLIC_HEADER include/safeimg_export.h
)

# Platform-specific library naming
if(WIN32)
    set_target_properties(safeimg PROPERTIES
        PREFIX ""
        OUTPUT_NAME "safeimg"
    )
elseif(APPLE)
    set_target_properties(safeimg PROPERTIES
        OUTPUT_NAME "safeimg"
        SUFFIX ".dylib"
    )
else()
    set_target_properties(safeimg PROPERTIES
        OUTPUT_NAME "safeimg"
        PREFIX "lib"
        SUFFIX ".so"
    )
endif()

# Installation rules
install(TARGETS safeimg
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
    PUBLIC_HEADER DESTINATION include
)

# Install additional headers (optional)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/
    DESTINATION include/safeimg
    FILES_MATCHING PATTERN "*.h"
)

# Print build info
message(STATUS "SafeImg v${PROJECT_VERSION}")
message(STATUS "Building shared library for: ${CMAKE_SYSTEM_NAME}")
message(STATUS "Output directory: ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
message(STATUS "C++ standard: C++${CMAKE_CXX_STANDARD}")
