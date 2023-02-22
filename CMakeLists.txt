cmake_minimum_required(VERSION 3.5)
project(nginx LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17) # specify the C++ standard
set(CMAKE_CXX_STANDARD_REQUIRED True)

#message(${PROJECT_BINARY_DIR})
#message(${PROJECT_SOURCE_DIR})
set(ROOT_DIR ${PROJECT_SOURCE_DIR})
message(${ROOT_DIR})
set_property(GLOBAL PROPERTY INCLUDE_DIR ${ROOT_DIR})


add_subdirectory(app)
#add_subdirectory(signal)
add_subdirectory(misc)
