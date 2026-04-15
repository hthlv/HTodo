cmake_minimum_required(VERSION 3.16)
project(HTodo VERSION 0.0.3 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Widgets Network)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Widgets Network)

set(APP_ICON_SVG "${CMAKE_CURRENT_SOURCE_DIR}/icons/htodo.svg")
set(APP_ICON_PNG "${CMAKE_CURRENT_SOURCE_DIR}/icons/htodo.png")
set(APP_ICON_PNG_SIZES 64 128 256)

set(PROJECT_SOURCES
    main.cpp
    MainWindow.cpp
    MainWindow.h
    TaskStorage.cpp
    TaskStorage.h
    PomodoroTimer.cpp
    PomodoroTimer.h
    PomodoroFocusCard.cpp
    PomodoroFocusCard.h
    RoundedComboBox.cpp
    RoundedComboBox.h
    PomodoroDialWidget.cpp
    PomodoroDialWidget.h
    TagAnalysisChartWidget.cpp
    TagAnalysisChartWidget.h
    resources.qrc
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(HTodo
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )
else()
    add_executable(HTodo
        ${PROJECT_SOURCES}
    )
endif()

target_link_libraries(HTodo PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Network
)

foreach(size IN LISTS APP_ICON_PNG_SIZES)
    install(FILES
        "${APP_ICON_PNG}"
        DESTINATION "share/icons/hicolor/${size}x${size}/apps"
    )
endforeach()

install(TARGETS HTodo
    RUNTIME DESTINATION bin
)

install(FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/packaging/htodo.desktop"
    DESTINATION share/applications
)

set(CPACK_GENERATOR "DEB")
set(CPACK_PACKAGE_NAME "htodo")
set(CPACK_PACKAGE_VENDOR "hyp")
set(CPACK_PACKAGE_CONTACT "hyp")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "HTodo task manager with Pomodoro timer")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${CMAKE_SYSTEM_PROCESSOR}")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "hyp")
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_DEBIAN_PACKAGE_DEPENDS "hicolor-icon-theme")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)

include(CPack)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(HTodo)
endif()
