#include <iostream>
#include "common.h"
#include "poblado.h"

//
// Used for JS tests, so we keep the log output to a minimum
// Captures all strings and capitalizes them.
// Finally outputs them comma separated.
//
// That output can then be read by the JS tests and verified for
// correctness.
//

size_t chunkSize;
int32_t processorDelay;

int32_t ticks = 0;
static bool finished = false;
typedef void (*callback_t)(void);

void ontick(uv_idle_t* tick_handler) {
  ++ticks;
  if (finished) uv_idle_stop(tick_handler);
}

void start_tick(uv_loop_t* loop, uv_idle_t& tick_handler) {
  int r = uv_idle_init(loop, &tick_handler);
  TEST_ASSERT(r == 0);
  r = uv_idle_start(&tick_handler, ontick);
  TEST_ASSERT(r == 0);
}

// called when Capitalizer completed
void onfinished() { finished = true; }

class Capitalizer : public Poblado {
  public:
    Capitalizer(uv_loop_t& loop, callback_t onfinished)
      : Poblado(loop), onfinished_(onfinished) {}

  private:
    // @override
    void onparsedToken(rapidjson_writable::SaxHandler& handler) {
      if (
        handler.type != rapidjson_writable::JsonType::Key &&
        handler.type != rapidjson_writable::JsonType::String) return;
      strings_.push_back(scopy(handler.stringVal.c_str()));
    }

    // @override
    void processParsedData() {
      for (auto& s : strings_) {
        capitalizedStrings_.push_back(string_toupper(s));

        // simulate that processing keys is slow (a little)
        uv_sleep(10);
      }
    }

    // @override
    void onprocessingComplete() {
      if (hasError()) {
        fprintf(stderr, "[collector] error %s\n", &error());
        return;
      }

      for (auto& s : capitalizedStrings_) {
        // output checked by JS test runner
        fprintf(stdout, "%s,", s);
      }
      onfinished_();
    }

    std::vector<const char*> strings_;
    std::vector<const char*> capitalizedStrings_;
    callback_t onfinished_;
};

void onwrite_chunk(uv_idle_t* handle) {
  // Read chunks as they come in. We control the speed from the JS test runner.
  RapidjsonWritable* writable = static_cast<RapidjsonWritable*>(handle->data);

  std::vector<char> buffer(chunkSize, 0);
  std::cin.read(buffer.data(), buffer.size());
  writable->write(*buffer.data(), buffer.size());
  if (std::cin.eof()) uv_idle_stop(handle);
}

void start_pipe_stdin(
    uv_loop_t* loop,
    uv_idle_t& write_chunk_handler,
    RapidjsonWritable& writable) {
  int r = uv_idle_init(loop, &write_chunk_handler);
  TEST_ASSERT(r == 0);
  write_chunk_handler.data = &writable;
  r = uv_idle_start(&write_chunk_handler, onwrite_chunk);
  TEST_ASSERT(r == 0);
}

int main(int argc, char *argv[]) {
  if (argc < 3) {
    fprintf(stderr,
      "Usage: cat test.json | "
      "test_capitalize <chunk size> <processor delay>\n"
    );
    exit(1);
  }
  chunkSize = atoi(argv[1]);
  processorDelay = atoi(argv[2]);

  uv_loop_t* loop = uv_default_loop();

  Capitalizer processor(*loop, onfinished);
  {
    const int ok = 1;
    const int* r = processor.init(&ok);
    TEST_ASSERT(*r == ok);
  }

  uv_idle_t write_chunk_handler;
  start_pipe_stdin(loop, write_chunk_handler, processor);

  // Ticker keeps event loop alive until Capitalizer finished
  uv_idle_t tick_handler;
  start_tick(loop, tick_handler);

  uv_run(loop, UV_RUN_DEFAULT);
  uv_loop_close(loop);

  fprintf(stderr, "OK");

  return 0;
}
