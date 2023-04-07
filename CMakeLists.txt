cmake_minimum_required(VERSION 3.5)
project(nginx LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17) # specify the C++ standard
set(CMAKE_CXX_STANDARD_REQUIRED True)

#message(${PROJECT_BINARY_DIR})
#message(${PROJECT_SOURCE_DIR})

#set rootdir and includedir
set(ROOT_DIR ${PROJECT_SOURCE_DIR})
message(${ROOT_DIR})
set_property(GLOBAL PROPERTY INCLUDE_DIR ${ROOT_DIR}/include)

add_subdirectory(misc)

add_subdirectory(lib/googletest)
add_subdirectory(test)
add_subdirectory(proc)
add_subdirectory(signal)
add_subdirectory(app)
add_subdirectory(net)
add_subdirectory(logic)





