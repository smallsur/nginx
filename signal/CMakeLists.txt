project(signal)


get_property(INCLUDE_DIR GLOBAL PROPERTY "INCLUDE_DIR")

add_library(ngx_signal ngx_signal.cpp)
target_include_directories(ngx_signal PUBLIC ${INCLUDE_DIR})


