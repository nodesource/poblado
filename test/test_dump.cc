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

void collect_processed_snapshot(uv_idle_t* handle) {

}

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

class Writable : public RapidjsonWritable {
  public:
    Writable() : RapidjsonWritable() {}

  protected:
    void onparserFailure(rapidjson::Reader& reader) {
      poblado_log("parser failure");
    }

    void onparsedToken(rapidjson_writable::SaxHandler& handler) {
      poblado_log("parsed a token");
    }

    void onparseComplete() {
      poblado_log("parser complete");
    }
};

void ontick(uv_idle_t* tick_handler) {
  ticks++;
  if ((ticks % LOG_TICK_INTERVAL) != 0) return;
  poblado_log("tick");
}

void start_tick(uv_loop_t* loop, uv_idle_t& tick_handler) {
  int r = uv_idle_init(loop, &tick_handler);
  ASSERT(r == 0);
  r = uv_idle_start(&tick_handler, ontick);
  ASSERT(r == 0);
}

int main(int argc, char *argv[]) {
  const char* file = argv[1];
  fprintf(stderr, "Processing %s\n", file);
  std::ifstream ifs(file);

  uv_loop_t* loop = uv_default_loop();
  Writable writable;
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

  poblado_log("starting loop");
  uv_run(loop, UV_RUN_DEFAULT);

  return 0;
}
