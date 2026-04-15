cmake_minimum_required(VERSION 3.16)
project(HTodo VERSION 0.0.3 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(GNUInstallDirs)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Widgets Network)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Widgets Network)

set(APP_ICON_SVG "${CMAKE_CURRENT_SOURCE_DIR}/icons/htodo.svg")
set(APP_ICON_PNG "${CMAKE_CURRENT_SOURCE_DIR}/icons/htodo.png")
set(APP_ICON_ICO "${CMAKE_CURRENT_SOURCE_DIR}/icons/htodo.ico")
set(APP_ICON_PNG_SIZES 64 128 256)

add_subdirectory(src)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(HTodo
        WIN32
        MANUAL_FINALIZATION
        ${HTODO_SOURCES}
    )
else()
    add_executable(HTodo
        WIN32
        ${HTODO_SOURCES}
    )
endif()

if(WIN32)
    set_target_properties(HTodo PROPERTIES WIN32_EXECUTABLE TRUE)
    target_sources(HTodo PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/packaging/htodo.rc"
    )
endif()

target_include_directories(HTodo PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src/app"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/core"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/widgets"
)

target_link_libraries(HTodo PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Network
)

install(TARGETS HTodo
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

set(CPACK_PACKAGE_NAME "htodo")
set(CPACK_PACKAGE_VENDOR "hyp")
set(CPACK_PACKAGE_CONTACT "hyp")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "HTodo task manager with Pomodoro timer")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "HTodo")
set(CPACK_PACKAGE_EXECUTABLES "HTodo" "HTodo")
set(CPACK_CREATE_DESKTOP_LINKS "HTodo")
set(CPACK_PACKAGE_FILE_NAME
    "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}"
)

if(WIN32 AND QT_VERSION_MAJOR EQUAL 6)
    qt_generate_deploy_app_script(
        TARGET HTodo
        OUTPUT_SCRIPT HTODO_DEPLOY_SCRIPT
        NO_TRANSLATIONS
        NO_COMPILER_RUNTIME
        INCLUDE_PLUGINS qwindows qico qsvg qsvgicon
        EXCLUDE_PLUGINS qgif qjpeg
        EXCLUDE_PLUGIN_TYPES generic networkinformation styles tls
        DEPLOY_TOOL_OPTIONS
            --no-system-d3d-compiler
            --no-system-dxc-compiler
            --no-opengl-sw
    )
    install(SCRIPT ${HTODO_DEPLOY_SCRIPT})
endif()

if(WIN32)
    set(CPACK_GENERATOR "WIX")
    set(CPACK_WIX_VERSION 4)
    set(CPACK_WIX_UPGRADE_GUID "05458688-8C85-4A90-A57A-7D3AF61653D2")
    set(CPACK_WIX_PROGRAM_MENU_FOLDER "HTodo")
    set(CPACK_WIX_UI_REF "WixUI_InstallDir")
    set(CPACK_WIX_INSTALL_SCOPE "perMachine")
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(CPACK_WIX_ARCHITECTURE "x64")
    else()
        set(CPACK_WIX_ARCHITECTURE "x86")
    endif()
elseif(UNIX AND NOT APPLE)
    install(FILES
        "${APP_ICON_SVG}"
        DESTINATION share/icons/hicolor/scalable/apps
    )

    foreach(size IN LISTS APP_ICON_PNG_SIZES)
        install(FILES
            "${APP_ICON_PNG}"
            DESTINATION "share/icons/hicolor/${size}x${size}/apps"
        )
    endforeach()

    install(FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/packaging/htodo.desktop"
        DESTINATION share/applications
    )

    set(CPACK_GENERATOR "DEB")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "hyp")
    set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "hicolor-icon-theme")
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
endif()

include(CPack)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(HTodo)
endif()
