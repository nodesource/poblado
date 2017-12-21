# poblado [![build status](https://secure.travis-ci.org/nodesource/poblado.svg?branch=master)](http://travis-ci.org/nodesource/poblado)

Parses and processes JSON that is written to it in chunks on a background thread.

## Depends on the following Headers

- [rapidjson](https://github.com/Tencent/rapidjson)
- [rapidjson-writable](https://github.com/nodesource/rapidjson-writable)
- [libuv](https://github.com/libuv/libuv)

Make sure you are including all those headers when building and in the case of libuv you need to either link to the
libuv library as well or make it part of your build.

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
    }

// } main thread

    std::vector<const char*> keys_;
    std::vector<const char*> processedKeys_;
};
```

### Output

```sh
➝  ./bin/test_dump test/fixtures/capitals.json
[main] processing test/fixtures/capitals.json
[00000](test_dump.cc:157)[0x7fff88639340] [main] starting loop
[00604](test_dump.cc:39)[0x700004f9f000] [processor] parsed a token
[00604](test_dump.cc:39)[0x700004f9f000] [processor] parsed a token
[00604](test_dump.cc:39)[0x700004f9f000] [processor] parsed a token
[00604](test_dump.cc:39)[0x700004f9f000] [processor] parsed a token
[00604](test_dump.cc:39)[0x700004f9f000] [processor] parsed a token
[01226](test_dump.cc:85)[0x7fff88639340] [ticker] tick
[01226](test_dump.cc:39)[0x700004f9f000] [processor] parsed a token
[01226](test_dump.cc:39)[0x700004f9f000] [processor] parsed a token
[01226](test_dump.cc:39)[0x700004f9f000] [processor] parsed a token
[01226](test_dump.cc:39)[0x700004f9f000] [processor] parsed a token
[01226](test_dump.cc:39)[0x700004f9f000] [processor] parsed a token
[01226](poblado.h:89)[0x700004f9f000] [processor] parser complete, processing ...
[01226](test_dump.cc:47)[0x700004f9f000] colombia
[01429](test_dump.cc:47)[0x700004f9f000] australia
[01629](test_dump.cc:47)[0x700004f9f000] germany
[01832](test_dump.cc:47)[0x700004f9f000] united states
[02037](poblado.h:141)[0x700004f9f000] [processor] signaling processing is complete
[02037](poblado.h:123)[0x7fff88639340] [collector] found processing is complete, reading results
[02037](test_dump.cc:68)[0x7fff88639340] [collector] printing key
[02037](test_dump.cc:69)[0x7fff88639340] COLOMBIA
[02037](test_dump.cc:68)[0x7fff88639340] [collector] printing key
[02037](test_dump.cc:69)[0x7fff88639340] AUSTRALIA
[02037](test_dump.cc:68)[0x7fff88639340] [collector] printing key
[02037](test_dump.cc:69)[0x7fff88639340] GERMANY
[02037](test_dump.cc:68)[0x7fff88639340] [collector] printing key
[02037](test_dump.cc:69)[0x7fff88639340] UNITED STATES
[02457](test_dump.cc:85)[0x7fff88639340] [ticker] tick
```

## LICENSE

MIT
