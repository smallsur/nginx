project(net)
get_property(INCLUDE_DIR GLOBAL PROPERTY "INCLUDE_DIR")

add_library(ngx_c_socket ngx_c_socket.cpp)
target_include_directories(ngx_c_socket PUBLIC ${INCLUDE_DIR})