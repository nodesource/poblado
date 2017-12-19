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

using rapidjson_writable::RapidjsonWritable;

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

class KeyCollectorWritable : public RapidjsonWritable {
  public:
    KeyCollectorWritable()
      : RapidjsonWritable(), processingComplete_(false) {
      uv_mutex_init(&completeMutex_);
    }

    //
    // Processing complete checks and processing results retrieval methods
    // are called from main loop thread
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

    std::vector<const char*>& getProcessingResult() {
      // At this point we verified vis isProcessingComplete (using cheap lock)
      // that the data is ready.
      // The processor guarantees now that it won't access the data anymore, therefore
      // we don't need to obtain another lock to return + access it on the main thread.
      return processedKeys_;  
    }

  protected:
    //
    // The below overrides are called on parsing/processing background thread
    //
    // @override
    void onparserFailure(rapidjson::Reader& reader) {
      poblado_log("[processor] parser failure");
    }

    // @override
    void onparsedToken(rapidjson_writable::SaxHandler& handler) {
      poblado_log("[processor] parsed a token");
      if (handler.type != rapidjson_writable::JsonType::Key) return;
      keys_.push_back(scopy(handler.stringVal.c_str()));
    }

    // @override
    void onparseComplete() {
      // process the collected data when parsing completed
      poblado_log("[processor] parser complete, processing keys");
      for (auto& key : keys_) {
        poblado_log(key);
        processedKeys_.push_back(string_toupper(key));

        // simulate that processing keys is slow
        uv_sleep(200);
      }

      // Signal to main thread that processing is complete and the data can
      // now be accesseed.
      poblado_log("[processor] signaling processing is complete" );
      uv_mutex_lock(&completeMutex_);
      {
        processingComplete_ = true;
      }
      uv_mutex_unlock(&completeMutex_);
    }

  private:
    std::vector<const char*> keys_;
    uv_mutex_t completeMutex_;
    std::vector<const char*> processedKeys_;
    bool processingComplete_;
};

void oncollect_processed_data(uv_idle_t* handle) {
  KeyCollectorWritable* keyCollector = static_cast<KeyCollectorWritable*>(handle->data);
  // Not complete yet, so we'll check again on the next tick
  if (!keyCollector->isProcessingComplete()) return;
  poblado_log("[collector] found processing is complete, reading results");

  std::vector<const char*> processedKeys = keyCollector->getProcessingResult();
  for (auto& key : processedKeys) {
    poblado_log("[collector] printing key");
    poblado_log(key);
  }
  uv_idle_stop(handle);
}

void start_collect_processed_data(
    uv_loop_t* loop,
    uv_idle_t& processed_data_handler,
    KeyCollectorWritable& keyCollector) {
  int r = uv_idle_init(loop, &processed_data_handler);
  ASSERT(r == 0);
  processed_data_handler.data = &keyCollector;
  r = uv_idle_start(&processed_data_handler, oncollect_processed_data);
  ASSERT(r == 0);
}

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

int main(int argc, char *argv[]) {
  const char* file = argv[1];
  fprintf(stderr, "[main] processing %s\n", file);
  std::ifstream ifs(file);

  uv_loop_t* loop = uv_default_loop();
  KeyCollectorWritable writable;
  {
    const int ok = 1;
    const int* r = writable.init(&ok);
    ASSERT(*r == ok);
  }

  uv_idle_t write_chunk_handler;
  {
    write_chunk_data_t write_chunk_data = { .stream = ifs, .writable = writable };
    start_write_chunk(loop, write_chunk_handler, write_chunk_data);
  }

  // We tick to keep the process from exiting and to give feedback that the
  // loop isn't blocked while we process the json file
  uv_idle_t tick_handler;
  {
    start_tick(loop, tick_handler);
  }

  uv_idle_t collect_processed_handler;
  {
    start_collect_processed_data(loop, collect_processed_handler, writable);
  }
  poblado_log("[main] starting loop");
  uv_run(loop, UV_RUN_DEFAULT);

  return 0;
}
