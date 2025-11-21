#----------------------------------------------------------------
# Generated CMake target import file.
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "ur-mavrouter::ur-mavrouter-shared" for configuration ""
set_property(TARGET ur-mavrouter::ur-mavrouter-shared APPEND PROPERTY IMPORTED_CONFIGURATIONS NOCONFIG)
set_target_properties(ur-mavrouter::ur-mavrouter-shared PROPERTIES
  IMPORTED_LOCATION_NOCONFIG "${_IMPORT_PREFIX}/lib/libur-mavrouter-shared.so.1.0"
  IMPORTED_SONAME_NOCONFIG "libur-mavrouter-shared.so.1"
  )

list(APPEND _IMPORT_CHECK_TARGETS ur-mavrouter::ur-mavrouter-shared )
list(APPEND _IMPORT_CHECK_FILES_FOR_ur-mavrouter::ur-mavrouter-shared "${_IMPORT_PREFIX}/lib/libur-mavrouter-shared.so.1.0" )

# Import target "ur-mavrouter::ur-mavrouter-shared-static" for configuration ""
set_property(TARGET ur-mavrouter::ur-mavrouter-shared-static APPEND PROPERTY IMPORTED_CONFIGURATIONS NOCONFIG)
set_target_properties(ur-mavrouter::ur-mavrouter-shared-static PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_NOCONFIG "CXX"
  IMPORTED_LOCATION_NOCONFIG "${_IMPORT_PREFIX}/lib/libur-mavrouter-shared-static.a"
  )

list(APPEND _IMPORT_CHECK_TARGETS ur-mavrouter::ur-mavrouter-shared-static )
list(APPEND _IMPORT_CHECK_FILES_FOR_ur-mavrouter::ur-mavrouter-shared-static "${_IMPORT_PREFIX}/lib/libur-mavrouter-shared-static.a" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
