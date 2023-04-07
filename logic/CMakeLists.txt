project(logic)

get_property(INCLUDE_DIR GLOBAL PROPERTY "INCLUDE_DIR")


add_library(ngx_c_slogic ngx_c_slogic.cpp)
target_include_directories(ngx_c_slogic PUBLIC ${INCLUDE_DIR})

target_link_libraries(ngx_c_slogic PUBLIC ngx_c_crc32)
#target_link_libraries(ngx_c_slogic PUBLIC ngx_c_crc32)