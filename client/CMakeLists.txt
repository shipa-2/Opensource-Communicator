cmake_minimum_required(VERSION 3.16)
project(opensource-communicator VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets WebSockets Network Multimedia)

find_package(PkgConfig REQUIRED)
pkg_check_modules(OPUS REQUIRED opus)

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
    src/protocol/ProtocolTypes.h
    src/protocol/WsConnection.cpp
    src/protocol/WsConnection.h
    src/protocol/WsApiClient.cpp
    src/protocol/WsApiClient.h
    src/protocol/CommunicatorClient.cpp
    src/protocol/CommunicatorClient.h
    src/calls/CallTypes.h
    src/calls/CallManager.cpp
    src/calls/CallManager.h
    src/audio/AudioBridge.cpp
    src/audio/AudioBridge.h
    src/audio/RingbackPlayer.cpp
    src/audio/RingbackPlayer.h
    src/audio/IncomingRingPlayer.cpp
    src/audio/IncomingRingPlayer.h
    src/audio/AudioDeviceUtils.cpp
    src/audio/AudioDeviceUtils.h
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
    src/ui/ContactRowWidget.cpp
    src/ui/ContactRowWidget.h
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
    src/ui/AddContactDialog.cpp
    src/ui/AddContactDialog.h
    src/ui/ConferenceDialog.cpp
    src/ui/ConferenceDialog.h
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

target_include_directories(opensource-communicator PRIVATE src ${OPUS_INCLUDE_DIRS})

target_link_libraries(opensource-communicator PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::WebSockets
    Qt6::Network
    Qt6::Multimedia
    ${DATACHANNEL_LIB}
    ${OPUS_LIBRARIES}
)

install(TARGETS opensource-communicator RUNTIME DESTINATION bin)
