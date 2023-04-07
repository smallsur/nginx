project(misc)

get_property(INCLUDE_DIR GLOBAL PROPERTY "INCLUDE_DIR")

add_library(memory_pool memory_pool.cpp)
add_library(ngx_c_conf  ngx_c_conf.cpp)
add_library(ngx_c_memory ngx_c_memory.cpp)
add_library(ngx_c_threadpool ngx_c_threadpool.cpp)
add_library(ngx_c_crc32 ngx_c_crc32.cpp)


target_include_directories(ngx_c_conf PUBLIC ${INCLUDE_DIR})
target_include_directories(memory_pool PUBLIC ${INCLUDE_DIR})
target_include_directories(ngx_c_memory PUBLIC ${INCLUDE_DIR})
target_include_directories(ngx_c_threadpool PUBLIC ${INCLUDE_DIR})
target_include_directories(ngx_c_crc32 PUBLIC ${INCLUDE_DIR})

