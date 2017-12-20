#ifndef INCLUDE_POBLADO_H
#define INCLUDE_POBLADO_H value
#include <uv.h>
#include "rapidjson_writable.h"
#include "rapidjson/error/en.h"

using rapidjson_writable::RapidjsonWritable;

// By default we don't log, but provide the ability for the user
// to supply a log function
#ifndef POBLADO_LOG
#define POBLADO_LOG(msg) /* silence */
#endif

// In some cases it might be useful to override the assert method used,
// however if the asserts being made fail we reached a really bad state
// so aborting makes sense in lots of cases.
#ifndef POBLADO_ASSERT
#define POBLADO_ASSERT(expr)                              \
 do {                                                     \
  if (!(expr)) {                                          \
    fprintf(stderr,                                       \
            "Assertion failed in %s on line %d: %s\n",    \
            __FILE__,                                     \
            __LINE__,                                     \
            #expr);                                       \
    abort();                                              \
  }                                                       \
 } while (0)
#endif


static inline char* scopy(const char* s) {
  size_t len = strlen(s);
  char* cpy = new char[len + 1]();
  strncpy(cpy, s, len + 1);
  return cpy;
}

static inline const char* parserError(rapidjson::Reader& reader) {
  const rapidjson::ParseErrorCode errorCode = reader.GetParseErrorCode();
  const std::string errorMsg = rapidjson::GetParseError_En(errorCode);
  const size_t offset = reader.GetErrorOffset();

  std::string msg(
    "A parser error occurred, this could be due to incomplete or invalid JSON.\n"
    "Error: [ " + errorMsg + " ]\n"
    "The error occured at file offset: " + std::to_string(offset)
  );

  return scopy(msg.c_str());
}

class Poblado : public RapidjsonWritable {
  public:

// main thread {

    Poblado(uv_loop_t& loop)
      : RapidjsonWritable(),
        loop_(loop),
        processingComplete_(false),
        hasError_(false),
        error_(nullptr) {
      uv_mutex_init(&completeMutex_);
      awaitProcessed();
    }

    ~Poblado() {
      if (error_ != nullptr) delete[] error_;
    }

// } main thread

  protected:
// background thread {

    // @override
    void onparserFailure(rapidjson::Reader& reader) {
      POBLADO_LOG("[processor] parser failure");
      hasError_ = true;
      error_ = parserError(reader);
      signalProcessingComplete_();
    }

    // @override
    void onparseComplete() final {
      // process the collected data when parsing completed
      POBLADO_LOG("[processor] parser complete, processing ...");
      processParsedData();
      signalProcessingComplete_();
    }

// } background thread

  private:

// main thread {
    bool isProcessingComplete() {
      bool iscomplete;
      // We use the cheaper lock, which is only locked by processor once
      // to notify us that processing is complete to verify that we can go and
      // get the processing result.
      uv_mutex_lock(&completeMutex_);
      {
        iscomplete = processingComplete_;
      }
      uv_mutex_unlock(&completeMutex_);

      return iscomplete;
    }

    static void onCheckProcessed(uv_idle_t* handle) {
      Poblado* self = static_cast<Poblado*>(handle->data);
      if (!self->isProcessingComplete()) return;

      // At this point we verified that the data is ready.
      // The processor guarantees now that it won't access the data anymore
      // on the background thread.
      // Therefore the user doesn't need to obtain another lock to access it
      // on the main thread.

      POBLADO_LOG("[collector] found processing is complete, reading results");
      uv_idle_stop(handle);
      self->onprocessingComplete();
    }

    void awaitProcessed() {
      int r = uv_idle_init(&loop_, &processed_handler_);
      POBLADO_ASSERT(r == 0);
      processed_handler_.data = this;
      r = uv_idle_start(&processed_handler_, onCheckProcessed);
      POBLADO_ASSERT(r == 0);
    }
// } main thread

// background thread {
    void signalProcessingComplete_() {
      // Signal to main thread that processing is complete and the data can
      // now be accesseed.
      POBLADO_LOG("[processor] signaling processing is complete" );
      uv_mutex_lock(&completeMutex_);
      {
        processingComplete_ = true;
      }
      uv_mutex_unlock(&completeMutex_);
    }
// } background thread

  protected:
    bool& hasError() { return hasError_; }
    const char& error() { return *error_; }

//
// Implemented by User
//

// background thread {
    virtual void onparsedToken(rapidjson_writable::SaxHandler& handler) = 0;
    virtual void processParsedData() = 0;
// } background thread

// main thread {
    virtual void onprocessingComplete() = 0;
// } main thread

  private:
    uv_loop_t& loop_;
    uv_idle_t processed_handler_;
    uv_mutex_t completeMutex_;
    bool processingComplete_;
    bool hasError_;
    const char* error_;
};

#endif
