cmake_minimum_required(VERSION 3.13.0)

project(universal-based VERSION 2.0.0)

if(NOT DEFINED ARCH_FOLDER)
  message(FATAL_ERROR "ARCH_FOLDER is not set, I don't know where to look for libraries")
endif(NOT DEFINED ARCH_FOLDER)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release")
endif()

set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY BOTH)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE BOTH)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE BOTH)

include_directories(lib/websocketpp)
include_directories(lib/nlohmann-json)
include_directories(lib/apply-patch)
include_directories(lib/asio/asio/include)
include_directories(prebuilt-libs/include/)
include_directories(include)

set(LIB_FOLDER "${CMAKE_CURRENT_LIST_DIR}/prebuilt-libs/lib/${ARCH_FOLDER}")

find_library(CURL_LIBRARY NO_CACHE REQUIRED NO_DEFAULT_PATH NAMES curl HINTS "${LIB_FOLDER}")
find_library(SSL_LIBRARY NO_CACHE REQUIRED NO_DEFAULT_PATH NAMES ssl HINTS "${LIB_FOLDER}")
find_library(CRYPTO_LIBRARY NO_CACHE REQUIRED NO_DEFAULT_PATH NAMES crypto HINTS "${LIB_FOLDER}")
find_library(Z_LIBRARY z REQUIRED)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_VERBOSE_MAKEFILE on)

add_library(based SHARED
src/based.cpp
src/connection.cpp
src/basedclient.cpp
src/utility.cpp
include/based.h
)

set_target_properties(based PROPERTIES VERSION ${CMAKE_PROJECT_VERSION} SOVERSION ${CMAKE_PROJECT_VERSION})

add_executable(example example/example.cpp)

add_compile_definitions(ASIO_STANDALONE _WEBSOCKETPP_CPP11_STL_)
add_compile_definitions(BASED_TLS)

add_compile_definitions(BASED_VERBOSE) # Comment this out to disable logs

target_link_libraries(example PRIVATE based)
target_include_directories(example PRIVATE include)

target_link_libraries(based PRIVATE ${Z_LIBRARY})
target_link_libraries(based PRIVATE ${CURL_LIBRARY})
target_link_libraries(based PRIVATE ${SSL_LIBRARY})
target_link_libraries(based PRIVATE ${CRYPTO_LIBRARY})

# This should have no effect on non-apple builds
set_target_properties(based PROPERTIES
  FRAMEWORK TRUE
  MACOSX_FRAMEWORK_IDENTIFIER com.saulx.based-universal
  PUBLIC_HEADER include/based.h
)