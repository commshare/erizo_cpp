cmake_minimum_required(VERSION 2.8)

project (ERIZO_CPP)

set(ERIZO_CPP_VERSION_MAJOR 0)
set(ERIZO_CPP_VERSION_MINOR 1)

set(CMAKE_CXX_FLAGS "-g -Wall  -Wno-deprecated-declarations -DDEBUG -std=c++11 ${ERIZO_CPP_CMAKE_CXX_FLAGS}")

include_directories("${ERIZO_CPP_SOURCE_DIR}" "${LIBDEPS_INCLUDE}")
link_directories("${LIBDEPS_LIBARAYS}")

file(GLOB_RECURSE ERIZO_CPP_SOURCES "${ERIZO_CPP_SOURCE_DIR}/*.h" "${ERIZO_CPP_SOURCE_DIR}/*.c" "${ERIZO_CPP_SOURCE_DIR}/*.cpp" "${ERIZO_CPP_SOURCE_DIR}/*.cc")

add_executable(erizo_cpp ${ERIZO_CPP_SOURCES})

target_link_libraries(erizo_cpp erizo rabbitmq log4cxx pthread jsoncpp boost_system)

install(TARGETS erizo_cpp RUNTIME DESTINATION bin)

