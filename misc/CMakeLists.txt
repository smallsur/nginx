project(misc)

add_library(memory_pool memory_pool.cpp memory_pool.h)


get_property(INCLUDE_DIR GLOBAL PROPERTY "INCLUDE_DIR")
#
#target_include_directories(memory_pool PRIVATE ${INCLUDE_DIR})


add_library(ngx_c_conf ngx_c_conf.h ngx_c_conf.cpp)
#add_executable(ngx_c_conf ngx_c_conf.cpp ngx_c_conf.h)
#target_include_directories(ngx_c_conf PUBLIC INCLUDE_DIR/misc)
