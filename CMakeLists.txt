# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

cmake_minimum_required(VERSION 3.16)
project(alarmqt VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network Svg)

add_executable(alarmqt
  src/main.cpp
  src/Alarm.cpp
  src/AlarmManager.cpp
  src/MainWindow.cpp
  src/SingleInstance.cpp
  src/NotificationDialog.cpp
  resources.qrc
)

target_include_directories(alarmqt PRIVATE src)
target_link_libraries(alarmqt PRIVATE
  Qt6::Core
  Qt6::Gui
  Qt6::Widgets
  Qt6::Network
  Qt6::Svg
)

include(GNUInstallDirs)

install(TARGETS alarmqt RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install(FILES share/applications/alarmqt.desktop
        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)

install(FILES icons/alarm.svg
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps
        RENAME alarmqt.svg)

install(FILES share/metainfo/alarmqt.metainfo.xml
        DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo)
