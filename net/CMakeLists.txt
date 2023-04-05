project(net)
get_property(INCLUDE_DIR GLOBAL PROPERTY "INCLUDE_DIR")

add_library(ngx_c_socket ngx_c_socket.cpp ngx_c_socket_accept.cpp ngx_c_socket_conn.cpp ngx_c_socket_inet.cpp ngx_c_socket_request.cpp)
target_include_directories(ngx_c_socket PUBLIC ${INCLUDE_DIR})