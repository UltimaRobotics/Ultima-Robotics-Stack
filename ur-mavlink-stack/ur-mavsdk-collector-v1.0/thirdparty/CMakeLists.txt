# Build static third-party dependencies only
# jsoncpp and zlib-ng are now using system libraries
add_subdirectory(curl)
# add_subdirectory(jsoncpp)  # Using system library
add_subdirectory(mavlink)
add_subdirectory(tinyxml2)
# add_subdirectory(zlib-ng)   # Using system library

