project(proc)

get_property(INCLUDE_DIR GLOBAL PROPERTY "INCLUDE_DIR")

add_library(ngx_process_cycle ngx_process_cycle.cpp)
target_include_directories(ngx_process_cycle PUBLIC ${INCLUDE_DIR})
target_link_libraries(ngx_process_cycle PUBLIC ngx_event)


add_library(ngx_daemon ngx_daemon.cpp)
target_include_directories(ngx_daemon PUBLIC ${INCLUDE_DIR})



add_library(ngx_event ngx_event.cpp)
target_include_directories(ngx_event PUBLIC ${INCLUDE_DIR})