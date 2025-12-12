{
  "targets": [
    {
      "target_name": "image_scanner",
      "sources": [
        "src/node_bindings.cpp",
        "src/image_scanner.cpp",
        "src/scanner_v2.cpp",
        "src/scan_result_v2.cpp",
        "src/file_io.cpp",
        "src/entropy_analyzer.cpp",
        "src/validators/jpeg_validator.cpp",
        "src/validators/png_validator.cpp",
        "src/validators/webp_validator.cpp",
        "src/validators/gif_validator.cpp",
        "src/validators/bmp_validator.cpp",
        "src/validators/tiff_validator.cpp",
        "src/validators/svg_validator.cpp",
        "src/metadata/exif_parser.cpp",
        "src/metadata/xmp_parser.cpp",
        "src/metadata/signature_engine_v2.cpp",
        "src/metadata/false_positive_reducer.cpp"
      ],
      "include_dirs": [
        "<!@(node -p \"require('node-addon-api').include\")",
        "include"
      ],
      "cflags!": [ "-fno-exceptions" ],
      "cflags_cc!": [ "-fno-exceptions" ],
      "cflags_cc": [ "-std=c++17" ],
      "defines": [ "NAPI_DISABLE_CPP_EXCEPTIONS" ],
      "conditions": [
        ["OS=='mac'", {
          "xcode_settings": {
            "GCC_ENABLE_CPP_EXCEPTIONS": "YES",
            "CLANG_CXX_LIBRARY": "libc++",
            "MACOSX_DEPLOYMENT_TARGET": "10.7"
          }
        }],
        ["OS=='win'", {
          "msvs_settings": {
            "VCCLCompilerTool": {
              "ExceptionHandling": 1
            }
          }
        }]
      ]
    }
  ]
}
