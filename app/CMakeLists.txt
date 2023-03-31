project(app)

add_library(solution Solution.cpp)
add_library(ngx_init_setprotitle ngx_init_setprotitle.cpp)

add_executable(nginx nginx.cpp)

get_property(INCLUDE_DIR GLOBAL PROPERTY "INCLUDE_DIR")
target_include_directories(solution PUBLIC ${INCLUDE_DIR})
target_include_directories(nginx PUBLIC ${INCLUDE_DIR})
target_include_directories(ngx_init_setprotitle PUBLIC ${INCLUDE_DIR})


target_link_libraries(nginx PUBLIC ngx_c_conf)
target_link_libraries(nginx PUBLIC solution)
target_link_libraries(nginx PUBLIC ngx_init_setprotitle)


