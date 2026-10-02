# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

cmake_minimum_required(VERSION 3.16)

# Nix/stdenv passes these on the cmake command line for every project.
# We are CXX-only and do not use the package registry; reference the values
# so CMake does not emit "Manually-specified variables were not used" warnings.
set(_alarmqt_unused_cli "${CMAKE_C_COMPILER}${CMAKE_EXPORT_NO_PACKAGE_REGISTRY}")
unset(_alarmqt_unused_cli)

# Source of truth: top-level VERSION (e.g. "0.1.0-dev"), or
# -DPROJECT_VERSION_FULL=... from packaging (Nix may append .<revCount>+g<rev>).
if(NOT DEFINED PROJECT_VERSION_FULL)
  file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" PROJECT_VERSION_FULL LIMIT_COUNT 1)
endif()

# project() needs a numeric version; strip -dev / +git / similar suffixes.
string(REGEX MATCH "^[0-9]+(\\.[0-9]+)*" _project_version_cmake "${PROJECT_VERSION_FULL}")
if(NOT _project_version_cmake)
  set(_project_version_cmake "0.0.0")
endif()

project(alarmqt VERSION ${_project_version_cmake} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets DBus Svg Multimedia)

add_executable(alarmqt
  src/main.cpp
  src/Alarm.cpp
  src/AlarmManager.cpp
  src/MainWindow.cpp
  src/AlarmDBus.cpp
  src/NotificationDialog.cpp
  resources.qrc
)

target_include_directories(alarmqt PRIVATE src)
target_link_libraries(alarmqt PRIVATE
  Qt6::Core
  Qt6::Gui
  Qt6::Widgets
  Qt6::DBus
  Qt6::Svg
  Qt6::Multimedia
)

target_compile_definitions(alarmqt PRIVATE
  ALARMQT_VERSION="${PROJECT_VERSION_FULL}"
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

install(FILES man/alarmqt.1
        DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)


option(ALARMQT_BUILD_TESTS "Build unit tests" ON)
if(ALARMQT_BUILD_TESTS)
  enable_testing()
  find_package(Qt6 REQUIRED COMPONENTS Test)
  add_executable(test_parser
    tests/test_parser.cpp
    src/Alarm.cpp
    src/AlarmManager.cpp
  )
  target_include_directories(test_parser PRIVATE src)
  target_link_libraries(test_parser PRIVATE
    Qt6::Core
    Qt6::Test
  )
  add_test(NAME parser COMMAND test_parser)
  set_tests_properties(parser PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
  )
endif()
