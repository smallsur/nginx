project(test)


get_property(INCLUDE_DIR GLOBAL PROPERTY "INCLUDE_DIR")

set(list_tests test_main test_conf)
add_executable(test_main main.cpp)
target_include_directories(test_main PUBLIC ${INCLUDE_DIR})
target_link_libraries(test_main PUBLIC gtest)

add_executable(test_conf test_conf.cpp)
target_include_directories(test_conf PUBLIC ${INCLUDE_DIR})
target_link_libraries(test_conf PUBLIC gtest)
#
#file(GLOB_RECURSE test_lists *.cpp)


