project(misc)

add_library(memory_pool memory_pool.cpp memory_pool.h)


#get_property(INCLUDE_DIR GLOBAL PROPERTY "INCLUDE_DIR")
#
#target_include_directories(memory_pool PRIVATE ${INCLUDE_DIR})

add_library(singleton Singleton.cpp Singleton.h)