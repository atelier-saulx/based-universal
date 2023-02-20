add_compile_options(-target arm64-apple-ios15.0-macabi)
add_link_options(-target arm64-apple-ios15.0-macabi)
set(CMAKE_XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "iPhone Developer")
set(ARCH_FOLDER "Catalyst")
