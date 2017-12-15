include_directories("${CMAKE_SOURCE_DIR}/deps/rapidjson-writable/include")
file(GLOB rapidjson_writable_headers ${CMAKE_SOURCE_DIR}/deps/rapidjson-writable/include/*)
source_group(deps\\rapidjson-writable FILES ${rapidjson_writable_headers})
