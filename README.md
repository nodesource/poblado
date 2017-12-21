# poblado [![build status](https://secure.travis-ci.org/nodesource/poblado.svg?branch=master)](http://travis-ci.org/nodesource/poblado)

Parses and processes JSON that is written to it in chunks on a background thread.

<!-- START doctoc generated TOC please keep comment here to allow auto update -->
<!-- DON'T EDIT THIS SECTION, INSTEAD RE-RUN doctoc TO UPDATE -->
**Table of Contents**  *generated with [DocToc](https://github.com/thlorenz/doctoc)*

- [Dependencies](#dependencies)
- [API](#api)
  - [`void onparsedToken(rapidjson_writable::SaxHandler& handler)`](#void-onparsedtokenrapidjson_writablesaxhandler-handler)
  - [`void processParsedData()`](#void-processparseddata)
  - [`void onprocessingComplete()`](#void-onprocessingcomplete)
  - [Overriding `ASSERT`](#overriding-assert)
  - [Logging Diagnostics via `POBLADO_LOG`](#logging-diagnostics-via-poblado_log)
- [Example](#example)
  - [Output](#output)
- [LICENSE](#license)

<!-- END doctoc generated TOC please keep comment here to allow auto update -->


## Dependencies

Poblado depends on the following Headers

- [rapidjson](https://github.com/Tencent/rapidjson)
- [rapidjson-writable](https://github.com/nodesource/rapidjson-writable)
- [libuv](https://github.com/libuv/libuv)

Make sure you are including all those headers when building and in the case of libuv you need to either link to the
libuv library as well or make it part of your build.

## API

In order to use poblado you'll extend the `Poblado` class and override the following methods.

```cc
#include "poblado.h"

class MyPoblado : public Poblado {
  void onparsedToken(rapidjson_writable::SaxHandler& handler);
  void processParsedData();
  void onprocessingComplete();
};
```

The methods are invoked either on the _parser/processor (background) thread_ or the _main thread_ (same as libuv's event
loop).

poblado guarantees that it will not invoke any _background thread_ methods after invoking `onprocessingComplete` on the
_main thread_. Therefore users of the API don't have to use any thread synchronization as it is all handled under the
hood by poblado.

### `void onparsedToken(rapidjson_writable::SaxHandler& handler)`

- invoked on background thread.
- compare the `handler.type` to a `rapidjson_writable::JsonType` and from that decide how to process the respective values.
- for more information have a look at the
  [rapidjson_writable::SaxHandler](https://github.com/nodesource/rapidjson-writable/blob/master/include/rapidjson_saxhandler.h)
  implementation
- store the extracted data in fields on your class so they are available in the following processing steps

### `void processParsedData()`

- invoked on background thread.
- here the user can perform further processing on the data collected via `onparsedToken`

### `void onprocessingComplete()`

- invoked on main thread.
- this method is invoked when both _background_ processing methods have completed
- at this point the processing methods will not be called anymore and thus the process result can safely be accessed
  on the main thread

### Overriding `ASSERT`

poblado uses a built in `ASSERT` method which can be overridden.

```cc
#define POBLADO_ASSERT MY_ASSERT
#include "poblado.h"
```

### Logging Diagnostics via `POBLADO_LOG`

By default poblado is quiet, but has built in support to log diagnostic messages. All you have to do is supply a
`POBLADO_LOG` method that takes a `char*` as an input.

```cc
#define POBLADO_LOG MY_LOG
#include "poblado.h"
```

Have a look at the top of [test/test_dump.cc](test/test_dump.cc) to see an example of that feature.

## Example

Below is a sample implementation of the `Poblado` class. For a more complete example please read
[test/test_dump.cc](test/test_dump.cc) or [test/test_capitalize.cc](test/test_capitalize.cc).

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

Note that the output is using a log method to provide diagnostic messages in order to demonstrate how things work under
the hood.

The output format is as follows:

```
[time](file:line)[thread id] [topic] message
```

```
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
