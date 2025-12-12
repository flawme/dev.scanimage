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
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHsc")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility=hidden")
endif()

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/include/internal)

# SafeImg shared library source files
set(SAFEIMG_SOURCES
    # C API wrapper
    src/api/safeimg_export.cpp
    
    # Core engine
    src/core/image_scanner.cpp
    src/core/scanner_v2.cpp
    src/core/scan_result_v2.cpp
    src/core/file_io.cpp
    src/core/entropy_analyzer.cpp
    
    # EXIF/XMP parsers
    src/core/exif_parser.cpp
    src/core/xmp_parser.cpp
    
    # Validators
    src/core/validators/jpeg_validator.cpp
    src/core/validators/png_validator.cpp
    src/core/validators/webp_validator.cpp
    src/core/validators/gif_validator.cpp
    src/core/validators/bmp_validator.cpp
    src/core/validators/tiff_validator.cpp
    src/core/validators/svg_validator.cpp
    
    # Signature systems
    src/signatures/signature_engine_v2.cpp
    src/signatures/false_positive_reducer.cpp
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

# Print build info
message(STATUS "====================================")
message(STATUS "SafeImg v${PROJECT_VERSION}")
message(STATUS "Language-independent shared library")
message(STATUS "====================================")
message(STATUS "Platform: ${CMAKE_SYSTEM_NAME}")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "Output: ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
message(STATUS "C++ Standard: C++${CMAKE_CXX_STANDARD}")
message(STATUS "====================================")
