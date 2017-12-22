#include <uv.h>
#include "common.h"

// initialize Poblado {

// Enable logging inside poblado
#define POBLADO_LOG TEST_LOG

// Use our custom assert inside poblado
#define POBLADO_ASSERT TEST_ASSERT

#include "poblado.h"

// } initialize poblado

#include <fstream>
#include <unistd.h>

#define CHUNK_SIZE 64
uint64_t START_TIME = uv_hrtime() / 1E6;

const int32_t LOG_TICK_INTERVAL = 1E6;
const int32_t WRITE_CHUNK_INTERVAL = LOG_TICK_INTERVAL / 2;
int32_t ticks = 0;
static bool finished = false;

typedef void (*callback_t)(void);

class ExtractKeysProcessor : public Poblado {
  public:
    ExtractKeysProcessor(uv_loop_t& loop, callback_t onfinished)
      : Poblado(loop), onfinished_(onfinished) {}

    ~ExtractKeysProcessor() {
      for (const char* c : keys_) delete [] c;
      for (const char* c : processedKeys_) delete [] c;
    }

  private:
// background thread {

    // @override
    void onparsedToken(rapidjson_writable::SaxHandler& handler) {
      TEST_LOG("[processor] parsed a token");
      if (handler.type != rapidjson_writable::JsonType::Key) return;
      keys_.push_back(scopy(handler.stringVal.c_str()));
    }

    // @override
    void processParsedData() {
      for (auto& key : keys_) {
        TEST_LOG(key);
        processedKeys_.push_back(string_toupper(key));

        // simulate that processing keys is slow
        uv_sleep(200);
      }
    }

// } background thread

// main thread {

    // @override
    void onprocessingComplete() {
      if (hasError()) {
        TEST_LOG("[collector] found error");
        fprintf(stderr, "%s\n", &error());
        return;
      }

      for (auto& key : processedKeys_) {
        TEST_LOG("[collector] printing key");
        TEST_LOG(key);
      }
      onfinished_();
    }

// } main thread

    std::vector<const char*> keys_;
    std::vector<const char*> processedKeys_;
    callback_t onfinished_;
};

// Ticker
void ontick(uv_idle_t* tick_handler) {
  ticks++;
  if ((ticks % LOG_TICK_INTERVAL) != 0) return;
  TEST_LOG("[ticker] tick");
  if (finished) uv_idle_stop(tick_handler);
}

void start_tick(uv_loop_t* loop, uv_idle_t& tick_handler) {
  int r = uv_idle_init(loop, &tick_handler);
  TEST_ASSERT(r == 0);
  r = uv_idle_start(&tick_handler, ontick);
  TEST_ASSERT(r == 0);
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
  TEST_ASSERT(r == 0);
  write_chunk_handler.data = &write_chunk_data;
  r = uv_idle_start(&write_chunk_handler, onwrite_chunk);
  TEST_ASSERT(r == 0);
}

void onfinished() {
  finished = true;
}

int main(int argc, char *argv[]) {
  const char* file = argv[1];
  fprintf(stderr, "[main] processing %s\n", file);
  std::ifstream ifs(file);

  uv_loop_t* loop = uv_default_loop();
  ExtractKeysProcessor processor(*loop, onfinished);
  {
    const int ok = 1;
    const int* r = processor.init(&ok);
    TEST_ASSERT(*r == ok);
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

  TEST_LOG("[main] starting loop");
  uv_run(loop, UV_RUN_DEFAULT);

  return 0;
}
