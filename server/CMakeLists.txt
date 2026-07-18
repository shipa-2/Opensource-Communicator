cmake_minimum_required(VERSION 3.16)
project(opensource-communicator-server VERSION 0.1.0 LANGUAGES CXX)

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

find_package(Qt6 REQUIRED COMPONENTS Core WebSockets Network Sql)
find_package(OpenSSL REQUIRED)

find_package(PkgConfig QUIET)
find_package(PostgreSQL REQUIRED)

add_executable(communicator-server
    src/main.cpp
    src/server/Server.h
    src/server/Server.cpp
    src/server/Config.h
    src/server/Config.cpp
    src/network/WsServer.h
    src/network/WsServer.cpp
    src/network/WsSession.h
    src/network/WsSession.cpp
    src/session/SessionManager.h
    src/session/SessionManager.cpp
    src/session/UserSession.h
    src/session/UserSession.cpp
    src/auth/AuthManager.h
    src/auth/AuthManager.cpp
    src/protocol/ProtocolTypes.h
    src/protocol/CommandDispatcher.h
    src/protocol/CommandDispatcher.cpp
    src/im/ImManager.h
    src/im/ImManager.cpp
    src/im/PresenceManager.h
    src/im/PresenceManager.cpp
    src/calls/CallManager.h
    src/calls/CallManager.cpp
    src/calls/CallManager.h
    src/calls/CallSession.h
    src/calls/CallSession.cpp
    src/calls/HoldMusicPlayer.h
    src/calls/HoldMusicPlayer.cpp
    src/addressbook/AddressBookManager.h
    src/addressbook/AddressBookManager.cpp
    src/history/HistoryManager.h
    src/history/HistoryManager.cpp
    src/sms/SmsManager.h
    src/sms/SmsManager.cpp
    src/conference/ConferenceManager.h
    src/conference/ConferenceManager.cpp
    src/db/Database.h
    src/db/Database.cpp
)

target_include_directories(communicator-server PRIVATE src)
target_link_libraries(communicator-server PRIVATE
    Qt6::Core
    Qt6::WebSockets
    Qt6::Network
    Qt6::Sql
    PostgreSQL::PostgreSQL
)

if(WIN32)
    target_link_libraries(communicator-server PRIVATE ws2_32 crypt32)
endif()

install(TARGETS communicator-server RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

set(CONFIG_DIR "/etc/communicator-server" CACHE PATH "Config directory")
install(FILES resources/server.conf.example
    DESTINATION ${CONFIG_DIR}
)

install(FILES resources/hold_music.wav
    DESTINATION ${CMAKE_INSTALL_DATADIR}/communicator-server
)
