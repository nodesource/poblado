ROOT        = $(shell pwd)
DEPS        = $(ROOT)/deps

-include rapidjson-writable.mk
-include rapidjson.mk
-include uv.mk

CCFLAGS = $(UV_FLAGS) -std=c++11 -g -Wno-format

INC_DIR = $(ROOT)/include
TST_DIR = $(ROOT)/test
BIN_DIR = $(ROOT)/bin

INCS =-I$(RAPIDJSON_WRITABLE_INCLUDES) -I$(RAPIDJSON_INCLUDES) \
	  -I$(UV_INCLUDES) -I$(INC_DIR)

TEST_DUMP = $(BIN_DIR)/test_dump
TST_DUMP_SRCS=$(TST_DIR)/test_dump.cc
TST_DUMP_OBJS=$(TST_DUMP_SRCS:.cc=.o)

$(TEST_DUMP): $(UV_LIB) $(TST_DUMP_OBJS)
	@mkdir -p $(BIN_DIR)
	$(CXX) $(LIBS) $(TST_DUMP_OBJS) $(UV_LIB) -o $@

test_dump: $(TEST_DUMP)

TEST_CAPITALIZE = $(BIN_DIR)/test_capitalize
TST_CAPITALIZE_SRCS=$(TST_DIR)/test_capitalize.cc
TST_CAPITALIZE_OBJS=$(TST_CAPITALIZE_SRCS:.cc=.o)

$(TEST_CAPITALIZE): $(UV_LIB) $(TST_CAPITALIZE_OBJS)
	@mkdir -p $(BIN_DIR)
	$(CXX) $(LIBS) $(TST_CAPITALIZE_OBJS) $(UV_LIB) -o $@

test_capitalize: $(TEST_CAPITALIZE)

.SUFFIXES: .cc .o
.cc.o:
	@(([ -d $(RAPIDJSON_WRITABLE_DIR) ] || $(MAKE) $(RAPIDJSON_WRITABLE_DIR)) && \
	  ([ -d $(RAPIDJSON_DIR) ] || $(MAKE) $(RAPIDJSON_DIR))) && \
	$(CXX) $< $(CCFLAGS) $(INCS) -c -o $@

clean:
	@rm -f $(TEST_DUMP) $(TST_DUMP_OBJS)
	@rm -f $(TEST_CAPITALIZE) $(TST_CAPITALIZE_OBJS)
	@rm -rf $(ROOT)/build/CMakeFiles $(ROOT)/build/CMakeCache.txt

xcode:
	@mkdir -p build
	(cd build && \
		CC=`xcrun -find cc` CXX=`xcrun -find c++` LDFLAGS='$(UV_LIB)' cmake -G Xcode ..)

ninja:
	@mkdir -p build
	(cd build && \
		CC=`xcrun -find cc` CXX=`xcrun -find c++` LDFLAGS='$(UV_LIB)' cmake -G Ninja ..) && \
	ninja -C build

.PHONY: test_dump test_capitalize xcode ninja
