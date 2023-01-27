cmake_minimum_required(VERSION 3.13.0)

project(universal-based VERSION 1.0.1)

include_directories(lib/websocketpp)
include_directories(lib/nlohmann-json)
include_directories(lib/apply-patch)
include_directories(lib/asio/asio/include)
include_directories(prebuilt-libs/include/)
include_directories(include)

if(NOT DEFINED ARCH_FOLDER)
  message(FATAL_ERROR "ARCH_FOLDER is not set, I don't know where to look for libraries")
endif(NOT DEFINED ARCH_FOLDER)

set(LIB_FOLDER "${CMAKE_CURRENT_LIST_DIR}/prebuilt-libs/lib/${ARCH_FOLDER}")

find_library(CURL_LIBRARY NO_CACHE REQUIRED NO_DEFAULT_PATH NAMES curl HINTS "${LIB_FOLDER}")
find_library(SSL_LIBRARY NO_CACHE REQUIRED NO_DEFAULT_PATH NAMES ssl HINTS "${LIB_FOLDER}")
find_library(CRYPTO_LIBRARY NO_CACHE REQUIRED NO_DEFAULT_PATH NAMES crypto HINTS "${LIB_FOLDER}")
find_library(NGHTTP2_LIBRARY NO_CACHE REQUIRED NO_DEFAULT_PATH NAMES nghttp2 HINTS "${LIB_FOLDER}")
find_library(Z_LIBRARY z REQUIRED)


set(CMAKE_CXX_STANDARD 11)
set(CMAKE_VERBOSE_MAKEFILE on)


add_library(based SHARED
src/based.cpp
src/connection.cpp
src/basedclient.cpp
src/utility.cpp
)

set_target_properties(based PROPERTIES VERSION ${CMAKE_PROJECT_VERSION} SOVERSION ${CMAKE_PROJECT_VERSION})

add_executable(example example/example.cpp)

add_compile_definitions(ASIO_STANDALONE _WEBSOCKETPP_CPP11_STL_)
add_compile_definitions(BASED_TLS)
add_compile_definitions(BASED_VERBOSE)

target_link_libraries(example PRIVATE based)
target_include_directories(example PRIVATE include)

target_link_libraries(based PRIVATE ${Z_LIBRARY})
target_link_libraries(based PRIVATE ${CURL_LIBRARY})
target_link_libraries(based PRIVATE ${SSL_LIBRARY})
target_link_libraries(based PRIVATE ${CRYPTO_LIBRARY})
target_link_libraries(based PRIVATE ${NGHTTP2_LIBRARY})

