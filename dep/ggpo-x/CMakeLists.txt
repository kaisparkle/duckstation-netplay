include(CMakeSources.cmake)

add_library(ggpo-x ${GGPO_LIB_SRC})

target_include_directories(ggpo-x PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_include_directories(ggpo-x INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/include")

