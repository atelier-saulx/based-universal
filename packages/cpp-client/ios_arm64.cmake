set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "")
set(CMAKE_OSX_SYSROOT iphoneos)
set(CMAKE_XCODE_ATTRIBUTE_AD_HOC_CODE_SIGNING_ALLOWED YES)
set(CMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED NO)
set(CMAKE_XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "-")
# set_property(DIRECTORY . PROPERTY XCODE_ATTRIBUTE_ENABLE_BITCODE "NO")
include_directories(lib/curl/artifacts/include)
#set(CMAKE_XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "iPhone Developer")
#set(CMAKE_XCODE_ATTRIBUTE_DEVELOPMENT_TEAM "xyz")
#set(CMAKE_OSX_DEPLOYMENT_TARGET "10" CACHE STRING "")
