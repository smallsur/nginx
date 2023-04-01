project(proc)

get_property(INCLUDE_DIR GLOBAL PROPERTY "INCLUDE_DIR")

add_library(ngx_process_cycle ngx_process_cycle.cpp)
target_include_directories(ngx_process_cycle PUBLIC ${INCLUDE_DIR})


