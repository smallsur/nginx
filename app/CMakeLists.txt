project(app)

add_library(solution Solution.cpp)
add_library(ngx_init_setprotitle ngx_init_setprotitle.cpp)
add_library(ngx_log ngx_log.cpp ngx_printf.cpp)

add_executable(nginx nginx.cpp)
add_executable(test test.cpp)
get_property(INCLUDE_DIR GLOBAL PROPERTY "INCLUDE_DIR")
target_include_directories(solution PUBLIC ${INCLUDE_DIR})
target_include_directories(nginx PUBLIC ${INCLUDE_DIR})
target_include_directories(ngx_init_setprotitle PUBLIC ${INCLUDE_DIR})
target_include_directories(ngx_log PUBLIC ${INCLUDE_DIR})

target_link_libraries(nginx PUBLIC ngx_c_conf)
target_link_libraries(nginx PUBLIC solution)
target_link_libraries(nginx PUBLIC ngx_init_setprotitle)
target_link_libraries(nginx PUBLIC ngx_log)
target_link_libraries(nginx PUBLIC ngx_signal)
target_link_libraries(nginx PUBLIC ngx_process_cycle)
target_link_libraries(nginx PUBLIC ngx_daemon)
target_link_libraries(nginx PUBLIC ngx_c_socket)
target_link_libraries(nginx PUBLIC ngx_c_slogic)

target_include_directories(test PUBLIC ${INCLUDE_DIR})

add_executable(clientTest clientTest.cpp)
target_link_libraries(clientTest PUBLIC ngx_c_socket)
target_include_directories(clientTest PUBLIC ${INCLUDE_DIR})


#find_path(HIREDIS_HEADER hiredis)
#find_library(HIREDIS_LIB hiredis)
#target_link_libraries(test ${HIREDIS_LIB})
#target_include_directories(test PUBLIC ${HIREDIS_HEADER})
#
#find_path(REDIS_PLUS_PLUS_HEADER sw)
#target_include_directories(test PUBLIC ${REDIS_PLUS_PLUS_HEADER})
#
#find_library(REDIS_PLUS_PLUS_LIB redis++)
#target_link_libraries(test ${REDIS_PLUS_PLUS_LIB})

target_link_libraries(test PUBLIC solution)


