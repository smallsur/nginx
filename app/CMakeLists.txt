project(app)


add_library(solution Solution.cpp Solution.h)


add_executable(nginx nginx.cpp)

get_property(INCLUDE_DIR GLOBAL PROPERTY "INCLUDE_DIR")


target_include_directories(nginx PUBLIC ${INCLUDE_DIR}/misc)


#target_link_libraries(nginx PUBLIC memory_pool)
#target_link_libraries(nginx PUBLIC singleton)
target_link_libraries(nginx PUBLIC ngx_c_conf)
target_link_libraries(nginx PUBLIC solution)

