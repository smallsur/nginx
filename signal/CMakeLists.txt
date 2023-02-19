project(signal)

add_library(signal ngx_signal.cpp)

get_property(INCLUDE_DIR GLOBAL PROPERTY "INCLUDE_DIR")
#message(${INCLUDE_DIR})

target_include_directories(signal PRIVATE ${INCLUDE_DIR})