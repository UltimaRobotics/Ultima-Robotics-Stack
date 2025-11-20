foreach(plugin ${ENABLED_PLUGINS})
    add_subdirectory(${plugin})
endforeach()

# Don't forget about mavlink_passthrough which is not auto-generated.
add_subdirectory(mavlink_passthrough)

