RAPIDJSON_WRITABLE_DIR      = $(DEPS)/rapidjson-writable
RAPIDJSON_WRITABLE_INCLUDES = $(RAPIDJSON_WRITABLE_DIR)/include/

$(RAPIDJSON_WRITABLE_DIR):
	git clone https://github.com/nodesource/rapidjson-writable.git $@
