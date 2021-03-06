include_directories(${CMAKE_SOURCE_DIR}/deps/uv/include)
include_directories(${CMAKE_SOURCE_DIR}/deps/uv/include/*)
include_directories(${CMAKE_SOURCE_DIR}/deps/uv/src)
include_directories(${CMAKE_SOURCE_DIR}/deps/uv/src/*)
file(GLOB uv_headers ${CMAKE_SOURCE_DIR}/deps/uv/include/*)
file(GLOB uv_sources ${CMAKE_SOURCE_DIR}/deps/uv/src/*)
source_group(deps\\uv\\include FILES ${uv_headers})
source_group(deps\\uv\\src FILES ${uv_sources})
