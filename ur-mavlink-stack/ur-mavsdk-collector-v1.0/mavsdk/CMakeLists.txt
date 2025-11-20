# Core Library
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -g -Wno-maybe-uninitialized -Wno-return-type -Wno-sign-compare -Wno-address-of-packed-member -DLINUX")

# Define MAVSDK include path variables for external use
set(MAVSDK_INCLUDE_PATHS
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/core
    ${CMAKE_CURRENT_SOURCE_DIR}/core/include/mavsdk
    ${CMAKE_CURRENT_SOURCE_DIR}/plugins
)

# Add all plugin directories to include paths
file(GLOB_RECURSE PLUGIN_DIRS 
    LIST_DIRECTORIES true
    "${CMAKE_CURRENT_SOURCE_DIR}/plugins/*"
)

# Filter to only directories (exclude files)
set(PLUGIN_DIRS_ONLY "")
foreach(item ${PLUGIN_DIRS})
    if(IS_DIRECTORY ${item})
        list(APPEND PLUGIN_DIRS_ONLY ${item})
    endif()
endforeach()

# Remove duplicates
list(REMOVE_DUPLICATES PLUGIN_DIRS_ONLY)

# Add plugin directories to MAVSDK include paths
list(APPEND MAVSDK_INCLUDE_PATHS ${PLUGIN_DIRS_ONLY})

# Define third-party include paths
set(MAVSDK_THIRDPARTY_INCLUDE_PATHS
    ${PROJECT_SOURCE_DIR}/thirdparty/curl/include
    ${PROJECT_SOURCE_DIR}/thirdparty/mavlink
    ${PROJECT_SOURCE_DIR}/thirdparty/tinyxml2
    ${JSONCPP_INCLUDE_DIRS}
    ${ZLIB_INCLUDE_DIRS}
)

add_library(mavsdk_core STATIC
    core/connection.cpp
    core/curl_wrapper.cpp
    core/flight_mode.cpp
    core/geometry.cpp
    core/http_loader.cpp
    core/log.cpp
    core/math_conversions.cpp
    core/mavlink_channels.cpp
    core/mavlink_command_receiver.cpp
    core/mavlink_command_sender.cpp
    core/mavlink_ftp_client.cpp
    core/mavlink_ftp_server.cpp
    core/mavlink_message_handler.cpp
    core/mavlink_mission_transfer_client.cpp
    core/mavlink_mission_transfer_server.cpp
    core/mavlink_parameter_cache.cpp
    core/mavlink_parameter_client.cpp
    core/mavlink_parameter_helper.cpp
    core/mavlink_parameter_server.cpp
    core/mavlink_parameter_subscription.cpp
    core/mavlink_receiver.cpp
    core/mavlink_request_message_handler.cpp
    core/mavlink_statustext_handler.cpp
    core/mavsdk.cpp
    core/mavsdk_impl.cpp
    core/mavsdk_time.cpp
    core/param_value.cpp
    core/ping.cpp
    core/plugin_impl_base.cpp
    core/request_message.cpp
    core/serial_connection.cpp
    core/server_component.cpp
    core/server_component_impl.cpp
    core/server_plugin_impl_base.cpp
    core/system.cpp
    core/system_impl.cpp
    core/tcp_connection.cpp
    core/timeout_handler.cpp
    core/timesync.cpp
    core/udp_connection.cpp
    core/crc32.cpp
    core/call_every_handler.cpp
    core/cli_arg.cpp
)

set(PLUGIN_SOURCES
    plugins/action/action.cpp
    plugins/action/action_impl.cpp
    plugins/action_server/action_server.cpp
    plugins/action_server/action_server_impl.cpp
    plugins/calibration/calibration.cpp
    plugins/calibration/calibration_impl.cpp
    plugins/calibration/calibration_statustext_parser.cpp
    plugins/camera/camera.cpp
    plugins/camera/camera_impl.cpp
    plugins/camera/camera_definition.cpp
    plugins/camera_server/camera_server.cpp
    plugins/camera_server/camera_server_impl.cpp
    plugins/component_information/component_information.cpp
    plugins/component_information/component_information_impl.cpp
    plugins/component_information_server/component_information_server.cpp
    plugins/component_information_server/component_information_server_impl.cpp
    plugins/failure/failure.cpp
    plugins/failure/failure_impl.cpp
    plugins/follow_me/follow_me.cpp
    plugins/follow_me/follow_me_impl.cpp
    plugins/ftp/ftp.cpp
    plugins/ftp/ftp_impl.cpp
    plugins/ftp_server/ftp_server.cpp
    plugins/ftp_server/ftp_server_impl.cpp
    plugins/geofence/geofence.cpp
    plugins/geofence/geofence_impl.cpp
    plugins/gimbal/gimbal.cpp
    plugins/gimbal/gimbal_impl.cpp
    plugins/gimbal/gimbal_protocol_v1.cpp
    plugins/gimbal/gimbal_protocol_v2.cpp
    plugins/gripper/gripper.cpp
    plugins/gripper/gripper_impl.cpp
    plugins/info/info.cpp
    plugins/info/info_impl.cpp
    plugins/log_files/log_files.cpp
    plugins/log_files/log_files_impl.cpp
    plugins/manual_control/manual_control.cpp
    plugins/manual_control/manual_control_impl.cpp
    plugins/mission/mission.cpp
    plugins/mission/mission_impl.cpp
    plugins/mission_raw/mission_raw.cpp
    plugins/mission_raw/mission_raw_impl.cpp
    plugins/mission_raw/mission_import.cpp
    plugins/mission_raw_server/mission_raw_server.cpp
    plugins/mission_raw_server/mission_raw_server_impl.cpp
    plugins/mocap/mocap.cpp
    plugins/mocap/mocap_impl.cpp
    plugins/offboard/offboard.cpp
    plugins/offboard/offboard_impl.cpp
    plugins/param/param.cpp
    plugins/param/param_impl.cpp
    plugins/param_server/param_server.cpp
    plugins/param_server/param_server_impl.cpp
    plugins/rtk/rtk.cpp
    plugins/rtk/rtk_impl.cpp
    plugins/server_utility/server_utility.cpp
    plugins/server_utility/server_utility_impl.cpp
    plugins/shell/shell.cpp
    plugins/shell/shell_impl.cpp
    plugins/telemetry/telemetry.cpp
    plugins/telemetry/telemetry_impl.cpp
    plugins/telemetry_server/telemetry_server.cpp
    plugins/telemetry_server/telemetry_server_impl.cpp
    plugins/tracking_server/tracking_server.cpp
    plugins/tracking_server/tracking_server_impl.cpp
    plugins/transponder/transponder.cpp
    plugins/transponder/transponder_impl.cpp
    plugins/tune/tune.cpp
    plugins/tune/tune_impl.cpp
    plugins/winch/winch.cpp
    plugins/winch/winch_impl.cpp
    plugins/mavlink_passthrough/mavlink_passthrough.cpp
    plugins/mavlink_passthrough/mavlink_passthrough_impl.cpp
)

add_library(mavsdk_plugins STATIC ${PLUGIN_SOURCES})

# Combine core and plugins into final library
add_library(mavsdk STATIC
    $<TARGET_OBJECTS:mavsdk_core>
    $<TARGET_OBJECTS:mavsdk_plugins>
)

# Include directories
target_include_directories(mavsdk_core PUBLIC
    ${MAVSDK_INCLUDE_PATHS}
    ${MAVSDK_THIRDPARTY_INCLUDE_PATHS}
)

# Include directories for plugins
target_include_directories(mavsdk_plugins PUBLIC
    ${MAVSDK_INCLUDE_PATHS}
    ${MAVSDK_THIRDPARTY_INCLUDE_PATHS}
)

# Link dependencies
target_link_libraries(mavsdk_core PRIVATE
    libcurl
    mavlink
    tinyxml2
    ${ZLIB_LIBRARIES}
    ${JSONCPP_LIBRARIES}
)

# Make sure plugins link against core
target_link_libraries(mavsdk_plugins PRIVATE mavsdk_core)
