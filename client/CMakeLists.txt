cmake_minimum_required(VERSION 3.16)
project(opensource-communicator VERSION 0.4.0 LANGUAGES CXX)

include(GNUInstallDirs)

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "/opt/opensource-communicator" CACHE PATH "Install prefix" FORCE)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type (Debug or Release)" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release RelWithDebInfo MinSizeRel)
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets WebSockets Network Multimedia)
if(UNIX AND NOT APPLE)
    find_package(Qt6 REQUIRED COMPONENTS DBus)
endif()
find_package(OpenSSL REQUIRED)

find_package(PkgConfig REQUIRED)
pkg_check_modules(OPUS REQUIRED opus)
pkg_check_modules(OPENH264 REQUIRED openh264)

find_package(LibDataChannel CONFIG QUIET)
if(LibDataChannel_FOUND)
    set(DATACHANNEL_LIB LibDataChannel::LibDataChannel)
else()
    find_library(DATACHANNEL_LIBRARY NAMES datachannel libdatachannel REQUIRED)
    if(NOT TARGET datachannel)
        add_library(datachannel UNKNOWN IMPORTED)
        set_target_properties(datachannel PROPERTIES IMPORTED_LOCATION "${DATACHANNEL_LIBRARY}")
    endif()
    set(DATACHANNEL_LIB datachannel)
endif()

add_executable(opensource-communicator
    resources/resources.qrc
    src/main.cpp
    src/logging/SessionLog.cpp
    src/logging/SessionLog.h
    src/protocol/ProtocolTypes.h
    src/protocol/WsConnection.cpp
    src/protocol/WsConnection.h
    src/protocol/WsApiClient.cpp
    src/protocol/WsApiClient.h
    src/protocol/CommunicatorClient.cpp
    src/protocol/CommunicatorClient.h
    src/protocol/AddressBookManager.cpp
    src/protocol/AddressBookManager.h
    src/protocol/CallHistoryParser.cpp
    src/protocol/CallHistoryParser.h
    src/calls/CallTypes.h
    src/calls/CallManager.cpp
    src/calls/CallManager.h
    src/video/VideoCapture.cpp
    src/video/VideoCapture.h
    src/video/H264Encoder.cpp
    src/video/H264Encoder.h
    src/video/H264Decoder.cpp
    src/video/H264Decoder.h
    src/video/ScreenCapture.cpp
    src/video/ScreenCapture.h
    src/ui/VideoRenderer.cpp
    src/ui/VideoRenderer.h
    src/ui/VideoSourceDialog.cpp
    src/ui/VideoSourceDialog.h
    src/audio/AudioBridge.cpp
    src/audio/AudioBridge.h
    src/audio/JabraHeadset.cpp
    src/audio/JabraHeadset.h
    src/audio/CallRecorder.cpp
    src/audio/CallRecorder.h
    src/audio/RingbackPlayer.cpp
    src/audio/RingbackPlayer.h
    src/audio/IncomingRingPlayer.cpp
    src/audio/IncomingRingPlayer.h
    src/audio/ExternalMediaPauser.cpp
    src/audio/ExternalMediaPauser.h
    src/audio/MessageNotifyPlayer.cpp
    src/audio/MessageNotifyPlayer.h
    src/audio/AudioDeviceUtils.cpp
    src/audio/AudioDeviceUtils.h
    src/network/NetworkUtils.cpp
    src/network/NetworkUtils.h
    src/settings/AppSettings.cpp
    src/settings/AppSettings.h
    src/settings/UserDataStore.cpp
    src/settings/UserDataStore.h
    src/chat/ChatManager.cpp
    src/chat/ChatManager.h
    src/demo/DemoData.cpp
    src/demo/DemoData.h
    src/ui/MainWindow.cpp
    src/ui/MainWindow.h
    src/ui/DialKeypadWidget.cpp
    src/ui/DialKeypadWidget.h
    src/ui/AppInstance.cpp
    src/ui/AppInstance.h
    src/ui/ContactRowWidget.cpp
    src/ui/ContactRowWidget.h
    src/ui/HistoryRowWidget.cpp
    src/ui/HistoryRowWidget.h
    src/ui/CallWindow.cpp
    src/ui/CallWindow.h
    src/ui/ChatDialog.cpp
    src/ui/ChatDialog.h
    src/ui/LoginDialog.cpp
    src/ui/LoginDialog.h
    src/ui/HelpDialog.cpp
    src/ui/HelpDialog.h
    src/ui/PresenceSelector.cpp
    src/ui/PresenceSelector.h
    src/ui/SettingsDialog.cpp
    src/ui/SettingsDialog.h
    src/ui/WallpaperCropDialog.cpp
    src/ui/WallpaperCropDialog.h
    src/ui/ThemePreviewDialog.cpp
    src/ui/ThemePreviewDialog.h
    src/ui/RecordingSettingsDialog.cpp
    src/ui/RecordingSettingsDialog.h
    src/ui/AddContactDialog.cpp
    src/ui/AddContactDialog.h
    src/ui/ConferenceDialog.cpp
    src/ui/ConferenceDialog.h
    src/ui/TransferDialog.cpp
    src/ui/TransferDialog.h
    src/ui/ProfileAvatarWidget.cpp
    src/ui/ProfileAvatarWidget.h
    src/ui/NotePopupDialog.cpp
    src/ui/NotePopupDialog.h
    src/ui/NativeScrollBars.cpp
    src/ui/NativeScrollBars.h
    src/ui/StyleHelper.cpp
    src/ui/StyleHelper.h
    src/ui/ThemeHelper.cpp
    src/ui/ThemeHelper.h
)

target_include_directories(opensource-communicator PRIVATE src ${OPUS_INCLUDE_DIRS} ${OPENH264_INCLUDE_DIRS})

if(OPUS_LIBRARY_DIRS)
    target_link_directories(opensource-communicator PRIVATE ${OPUS_LIBRARY_DIRS})
endif()

target_link_libraries(opensource-communicator PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::WebSockets
    Qt6::Network
    Qt6::Multimedia
    ${DATACHANNEL_LIB}
    ${OPUS_LIBRARIES}
    ${OPENH264_LIBRARIES}
)

if(UNIX AND NOT APPLE)
    target_link_libraries(opensource-communicator PRIVATE Qt6::DBus)
endif()

if(WIN32)
    target_link_libraries(opensource-communicator PRIVATE ws2_32)

    # Release: GUI subsystem (no console). Debug: console for live logs.
    if(CMAKE_CONFIGURATION_TYPES)
        set_target_properties(opensource-communicator PROPERTIES
            WIN32_EXECUTABLE $<NOT:$<CONFIG:Debug>>
        )
    elseif(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set_target_properties(opensource-communicator PROPERTIES WIN32_EXECUTABLE FALSE)
    else()
        set_target_properties(opensource-communicator PROPERTIES WIN32_EXECUTABLE TRUE)
    endif()
endif()

if(CMAKE_CONFIGURATION_TYPES)
    target_compile_definitions(opensource-communicator PRIVATE
        $<$<CONFIG:Debug>:OSC_DEBUG_BUILD>
    )
elseif(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(opensource-communicator PRIVATE OSC_DEBUG_BUILD)
endif()

# Build channel: version carries the git branch suffix (e.g. 0.4.0-videotest),
# main and detached checkouts (CI) stay plain 0.4.0. Non-main branches also embed
# the UTC commit timestamp in the GitHub pre-release tag format
# (videotest-DDMMYYYY-HHMM) so a binary maps to its release.
execute_process(
    COMMAND git -C ${CMAKE_CURRENT_SOURCE_DIR} rev-parse --abbrev-ref HEAD
    OUTPUT_VARIABLE OSC_GIT_BRANCH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
    RESULT_VARIABLE OSC_GIT_BRANCH_RESULT
)
set(OSC_VERSION_SUFFIX "")
if(OSC_GIT_BRANCH_RESULT EQUAL 0 AND NOT OSC_GIT_BRANCH MATCHES "^(main|HEAD|)$")
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E env TZ=UTC git -C ${CMAKE_CURRENT_SOURCE_DIR} log -1
                --format=%cd --date=format:%d%m%Y-%H%M
        OUTPUT_VARIABLE OSC_BUILD_STAMP
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        RESULT_VARIABLE OSC_BUILD_STAMP_RESULT
    )
    # NOTE: CMake regex has no {n} quantifiers.
    if(OSC_BUILD_STAMP_RESULT EQUAL 0 AND OSC_BUILD_STAMP MATCHES "^[0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9]-[0-9][0-9][0-9][0-9]$")
        set(OSC_VERSION_SUFFIX "-${OSC_GIT_BRANCH}-${OSC_BUILD_STAMP}")
    else()
        set(OSC_VERSION_SUFFIX "-${OSC_GIT_BRANCH}")
    endif()
endif()
target_compile_definitions(opensource-communicator PRIVATE
    APP_VERSION="${PROJECT_VERSION}${OSC_VERSION_SUFFIX}"
)

install(TARGETS opensource-communicator RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

set(PACKAGING_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../packaging/linux")
install(FILES
    "${PACKAGING_DIR}/opensource-communicator.desktop"
    DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
)

foreach(_icon_size 16 32 48 64 128 256 512)
    install(FILES
        "${PACKAGING_DIR}/icons/opensource-communicator-${_icon_size}.png"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/${_icon_size}x${_icon_size}/apps"
        RENAME opensource-communicator.png
    )
endforeach()
