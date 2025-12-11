#include "../include/image_scanner.h"
#include <cstring>
#include <memory>
#include <node_api.h>
#include <string>

// Global scanner instance (maintained across calls)
static std::unique_ptr<ImageScanner::Scanner> g_scanner;

// Ensure scanner is initialized
static ImageScanner::Scanner *GetScanner() {
  if (!g_scanner) {
    g_scanner.reset(new ImageScanner::Scanner());
  }
  return g_scanner.get();
}

// Async worker data
struct ScanWork {
  napi_async_work work;
  napi_deferred deferred;
  std::string filepath;
  ImageScanner::ScanResult result;
  std::string error;
};

// Convert ScanResult to JavaScript object
napi_value ResultToJS(napi_env env, const ImageScanner::ScanResult &result) {
  napi_value jsResult, jsIsSafe, jsIssues;

  // Create result object
  napi_create_object(env, &jsResult);

  // Set isSafe
  napi_get_boolean(env, result.isSafe, &jsIsSafe);
  napi_set_named_property(env, jsResult, "isSafe", jsIsSafe);

  // Create issues array
  napi_create_array_with_length(env, result.issues.size(), &jsIssues);

  for (size_t i = 0; i < result.issues.size(); ++i) {
    napi_value jsIssue, jsType, jsDesc, jsSeverity;

    napi_create_object(env, &jsIssue);

    // type
    napi_create_string_utf8(env, result.issues[i].type.c_str(),
                            NAPI_AUTO_LENGTH, &jsType);
    napi_set_named_property(env, jsIssue, "type", jsType);

    // description
    napi_create_string_utf8(env, result.issues[i].description.c_str(),
                            NAPI_AUTO_LENGTH, &jsDesc);
    napi_set_named_property(env, jsIssue, "description", jsDesc);

    // severity
    const char *severity;
    switch (result.issues[i].severity) {
    case ImageScanner::Severity::INFO:
      severity = "info";
      break;
    case ImageScanner::Severity::WARNING:
      severity = "warning";
      break;
    case ImageScanner::Severity::CRITICAL:
      severity = "critical";
      break;
    default:
      severity = "unknown";
    }
    napi_create_string_utf8(env, severity, NAPI_AUTO_LENGTH, &jsSeverity);
    napi_set_named_property(env, jsIssue, "severity", jsSeverity);

    napi_set_element(env, jsIssues, i, jsIssue);
  }

  napi_set_named_property(env, jsResult, "issues", jsIssues);

  return jsResult;
}

// Async work execution (runs in worker thread)
void ExecuteScan(napi_env env, void *data) {
  ScanWork *work = static_cast<ScanWork *>(data);

  try {
    work->result = GetScanner()->scan(work->filepath);
  } catch (const std::exception &e) {
    work->error = e.what();
  } catch (...) {
    work->error = "Unknown error during scan";
  }
}

// Async work completion (runs in main thread)
void CompleteScan(napi_env env, napi_status status, void *data) {
  ScanWork *work = static_cast<ScanWork *>(data);

  if (!work->error.empty()) {
    napi_value error;
    napi_create_string_utf8(env, work->error.c_str(), NAPI_AUTO_LENGTH, &error);
    napi_reject_deferred(env, work->deferred, error);
  } else {
    napi_value result = ResultToJS(env, work->result);
    napi_resolve_deferred(env, work->deferred, result);
  }

  napi_delete_async_work(env, work->work);
  delete work;
}

// scanImage(filepath) - Async version
napi_value ScanImage(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1];
  napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

  if (argc < 1) {
    napi_throw_type_error(env, nullptr, "Expected 1 argument: filepath");
    return nullptr;
  }

  // Get filepath string
  size_t str_len;
  napi_get_value_string_utf8(env, args[0], nullptr, 0, &str_len);

  char *filepath = new char[str_len + 1];
  napi_get_value_string_utf8(env, args[0], filepath, str_len + 1, &str_len);

  // Create async work
  ScanWork *work = new ScanWork();
  work->filepath = filepath;
  delete[] filepath;

  napi_value promise;
  napi_create_promise(env, &work->deferred, &promise);

  napi_value workName;
  napi_create_string_utf8(env, "ScanImage", NAPI_AUTO_LENGTH, &workName);

  napi_create_async_work(env, nullptr, workName, ExecuteScan, CompleteScan,
                         work, &work->work);
  napi_queue_async_work(env, work->work);

  return promise;
}

// scanImageSync(filepath) - Synchronous version
napi_value ScanImageSync(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1];
  napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

  if (argc < 1) {
    napi_throw_type_error(env, nullptr, "Expected 1 argument: filepath");
    return nullptr;
  }

  // Get filepath string
  size_t str_len;
  napi_get_value_string_utf8(env, args[0], nullptr, 0, &str_len);

  char *filepath = new char[str_len + 1];
  napi_get_value_string_utf8(env, args[0], filepath, str_len + 1, &str_len);

  try {
    ImageScanner::ScanResult result = GetScanner()->scan(filepath);
    delete[] filepath;

    return ResultToJS(env, result);
  } catch (const std::exception &e) {
    delete[] filepath;
    napi_throw_error(env, nullptr, e.what());
    return nullptr;
  }
}

// loadSignatures(filepath) - Load signatures from JSON
napi_value LoadSignatures(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1];
  napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

  if (argc < 1) {
    napi_throw_type_error(env, nullptr, "Expected 1 argument: filepath");
    return nullptr;
  }

  // Get filepath string
  size_t str_len;
  napi_get_value_string_utf8(env, args[0], nullptr, 0, &str_len);

  char *filepath = new char[str_len + 1];
  napi_get_value_string_utf8(env, args[0], filepath, str_len + 1, &str_len);

  bool success = GetScanner()->loadSignatures(filepath);
  delete[] filepath;

  // Return object with success flag and error message if any
  napi_value result, jsSuccess, jsError;
  napi_create_object(env, &result);

  napi_get_boolean(env, success, &jsSuccess);
  napi_set_named_property(env, result, "success", jsSuccess);

  if (!success) {
    std::string error = GetScanner()->getLastError();
    napi_create_string_utf8(env, error.c_str(), NAPI_AUTO_LENGTH, &jsError);
    napi_set_named_property(env, result, "error", jsError);
  }

  return result;
}

// Module initialization
napi_value Init(napi_env env, napi_value exports) {
  napi_value scanFn, scanSyncFn, loadSigFn;

  napi_create_function(env, "scanImage", NAPI_AUTO_LENGTH, ScanImage, nullptr,
                       &scanFn);
  napi_create_function(env, "scanImageSync", NAPI_AUTO_LENGTH, ScanImageSync,
                       nullptr, &scanSyncFn);
  napi_create_function(env, "loadSignatures", NAPI_AUTO_LENGTH, LoadSignatures,
                       nullptr, &loadSigFn);

  napi_set_named_property(env, exports, "scanImage", scanFn);
  napi_set_named_property(env, exports, "scanImageSync", scanSyncFn);
  napi_set_named_property(env, exports, "loadSignatures", loadSigFn);

  return exports;
}

NAPI_MODULE(NODE_GYP_MODULE_NAME, Init)
