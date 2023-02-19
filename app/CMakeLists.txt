project(app)

add_executable(nginx nginx.cpp)

get_property(INCLUDE_DIR GLOBAL PROPERTY "INCLUDE_DIR")
#message(${INCLUDE_DIR})

target_include_directories(nginx PRIVATE ${INCLUDE_DIR})
target_link_libraries(nginx PRIVATE signal)

