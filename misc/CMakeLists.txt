project(misc)

add_library(memory_pool memory_pool.cpp)


get_property(INCLUDE_DIR GLOBAL PROPERTY "INCLUDE_DIR")


add_library(ngx_c_conf  ngx_c_conf.cpp)
target_include_directories(ngx_c_conf PUBLIC ${INCLUDE_DIR})
target_include_directories(memory_pool PUBLIC ${INCLUDE_DIR})

