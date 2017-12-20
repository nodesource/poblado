# poblado

Parses and processes JSON that is written to it in chunks on a background thread.

## Depends on the following Headers

- [rapidjson](https://github.com/Tencent/rapidjson)
- [rapidjson-writable](https://github.com/nodesource/rapidjson-writable)
- [libuv](https://github.com/libuv/libuv)

Make sure you are including all those headers when building and in the case of libuv you need to either link to the
libuv library as well or make it part of your build.

## Status

Functional API, still untested, for now **use at your own risk, tests coming soon :)**.

## Example

Below is a sample implementation of the `Poblado` class. For a more complete example please read
[test/test_dump.cc](test/test_dump.cc).

```cc
class ExtractKeysProcessor : public Poblado {
  public:
    ExtractKeysProcessor(uv_loop_t& loop) : Poblado(loop) {}

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
    }
// main thread }

    std::vector<const char*> keys_;
    std::vector<const char*> processedKeys_;
};
```

## LICENSE

MIT
