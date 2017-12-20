#include <uv.h>
#include "rapidjson_writable.h"
#include "common.h"

#include <fstream>
#include <unistd.h>

#define CHUNK_SIZE 64
uint64_t START_TIME = uv_hrtime() / 1E6;

const int32_t LOG_TICK_INTERVAL = 1E6;
const int32_t WRITE_CHUNK_INTERVAL = LOG_TICK_INTERVAL / 2;
int32_t ticks = 0;

//
// API
//
using rapidjson_writable::RapidjsonWritable;

class Poblado : public RapidjsonWritable {
  public:

// main thread {

    Poblado(uv_loop_t& loop)
      : RapidjsonWritable(), loop_(loop), processingComplete_(false) {
      uv_mutex_init(&completeMutex_);
      awaitProcessed();
    }

// } main thread

  protected:
// background thread {

    // @override
    void onparserFailure(rapidjson::Reader& reader) {
      poblado_log("[processor] parser failure");
      // TODO: set failure condition, set completeMutex
      // so that the process function pick up the error on the main thread
      // extract error info and pass on to virtual method
    }

    // @override
    void onparseComplete() final {
      // process the collected data when parsing completed
      poblado_log("[processor] parser complete, processing ...");
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

      poblado_log("[collector] found processing is complete, reading results");
      uv_idle_stop(handle);
      self->onprocessingComplete();
    }

    void awaitProcessed() {
      int r = uv_idle_init(&loop_, &processed_handler_);
      ASSERT(r == 0);
      processed_handler_.data = this;
      r = uv_idle_start(&processed_handler_, onCheckProcessed);
      ASSERT(r == 0);
    }
// } main thread

// background thread {
    void signalProcessingComplete_() {
      // Signal to main thread that processing is complete and the data can
      // now be accesseed.
      poblado_log("[processor] signaling processing is complete" );
      uv_mutex_lock(&completeMutex_);
      {
        processingComplete_ = true;
      }
      uv_mutex_unlock(&completeMutex_);
    }
// } background thread

  protected:

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
};

//
// Test
//

class ExtractKeysProcessor : public Poblado {
  public:
    ExtractKeysProcessor(uv_loop_t& loop) : Poblado(loop) {}

  private:
// background thread {

    // @override
    void onparsedToken(rapidjson_writable::SaxHandler& handler) {
      poblado_log("[processor] parsed a token");
      if (handler.type != rapidjson_writable::JsonType::Key) return;
      keys_.push_back(scopy(handler.stringVal.c_str()));
    }

    // @override
    void processParsedData() {
      for (auto& key : keys_) {
        poblado_log(key);
        processedKeys_.push_back(string_toupper(key));

        // simulate that processing keys is slow
        uv_sleep(200);
      }
    }

  // } background thread

  // main thread {
    void onprocessingComplete() {
      for (auto& key : processedKeys_) {
        poblado_log("[collector] printing key");
        poblado_log(key);
      }
    }
  // main thread }

    std::vector<const char*> keys_;
    std::vector<const char*> processedKeys_;
};

// Ticker
void ontick(uv_idle_t* tick_handler) {
  ticks++;
  if ((ticks % LOG_TICK_INTERVAL) != 0) return;
  poblado_log("[ticker] tick");
}

void start_tick(uv_loop_t* loop, uv_idle_t& tick_handler) {
  int r = uv_idle_init(loop, &tick_handler);
  ASSERT(r == 0);
  r = uv_idle_start(&tick_handler, ontick);
  ASSERT(r == 0);
}

// Chunk Producer
typedef struct {
  std::istream& stream;
  RapidjsonWritable& writable;
} write_chunk_data_t;

void onwrite_chunk(uv_idle_t* handle) {
  // write chunk every 10th iteration to simulate that the chunks are coming in slowly
  if ((ticks % WRITE_CHUNK_INTERVAL) != 0) return;

  write_chunk_data_t* write_chunk_data = static_cast<write_chunk_data_t*>(handle->data);
  std::istream& stream = write_chunk_data->stream;
  RapidjsonWritable& writable = write_chunk_data->writable;

  std::vector<char> buffer(CHUNK_SIZE, 0);
  stream.read(buffer.data(), buffer.size());
  writable.write(*buffer.data(), buffer.size());
  if (stream.eof()) uv_idle_stop(handle);
}

void start_write_chunk(
    uv_loop_t* loop,
    uv_idle_t& write_chunk_handler,
    write_chunk_data_t& write_chunk_data) {
  int r = uv_idle_init(loop, &write_chunk_handler);
  ASSERT(r == 0);
  write_chunk_handler.data = &write_chunk_data;
  r = uv_idle_start(&write_chunk_handler, onwrite_chunk);
  ASSERT(r == 0);
}

int main(int argc, char *argv[]) {
  const char* file = argv[1];
  fprintf(stderr, "[main] processing %s\n", file);
  std::ifstream ifs(file);

  uv_loop_t* loop = uv_default_loop();
  ExtractKeysProcessor processor(*loop);
  {
    const int ok = 1;
    const int* r = processor.init(&ok);
    ASSERT(*r == ok);
  }

  uv_idle_t write_chunk_handler;
  {
    write_chunk_data_t write_chunk_data = { .stream = ifs, .writable = processor };
    start_write_chunk(loop, write_chunk_handler, write_chunk_data);
  }

  // We tick to keep the process from exiting and to give feedback that the
  // loop isn't blocked while we process the json file
  uv_idle_t tick_handler;
  {
    start_tick(loop, tick_handler);
  }

  poblado_log("[main] starting loop");
  uv_run(loop, UV_RUN_DEFAULT);

  return 0;
}
