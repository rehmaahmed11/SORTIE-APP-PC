cmake_minimum_required(VERSION 3.21)

project(AMSSortie
    VERSION 1.0.0
    DESCRIPTION "AMS Sortie - a floating dial that sorts your files in one drop"
    HOMEPAGE_URL "https://wa.me/923060706989"
    LANGUAGES CXX)

if(WIN32)
    enable_language(RC)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# ------------------------------------------------------------------ Qt
find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Widgets Svg)
qt_standard_project_setup()

# ------------------------------------------------------------------ sources
set(SORTIE_SOURCES
    src/main.cpp
    src/AboutDialog.cpp       src/AboutDialog.h
    src/Autostart.cpp         src/Autostart.h
    src/Brand.h
    src/Config.cpp            src/Config.h
    src/FloatingWindow.cpp    src/FloatingWindow.h
    src/Format.cpp            src/Format.h
    src/Logging.cpp           src/Logging.h
    src/OrbWidget.cpp         src/OrbWidget.h
    src/PetalPicker.cpp       src/PetalPicker.h
    src/SingleInstance.cpp    src/SingleInstance.h
    src/Theme.cpp             src/Theme.h
    src/Transfer.h
    src/TransferDialog.cpp    src/TransferDialog.h
    src/TransferManager.cpp   src/TransferManager.h
    src/TransferWorker.cpp    src/TransferWorker.h
    src/WhatsAppBubble.cpp    src/WhatsAppBubble.h
)

if(WIN32)
    list(APPEND SORTIE_SOURCES app.rc)
endif()

qt_add_executable(SortieApp
    WIN32
    MACOSX_BUNDLE
    ${SORTIE_SOURCES}
    resources.qrc
)

target_include_directories(SortieApp PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(SortieApp PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Svg)
target_compile_definitions(SortieApp PRIVATE
    SORTIE_VERSION="${PROJECT_VERSION}"
    QT_DISABLE_DEPRECATED_BEFORE=0x060000
)

set_target_properties(SortieApp PROPERTIES
    OUTPUT_NAME "SortieApp"
    MACOSX_BUNDLE_BUNDLE_NAME "AMS Sortie"
    MACOSX_BUNDLE_GUI_IDENTIFIER "ams.sortie"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
)

if(MSVC)
    target_compile_options(SortieApp PRIVATE /W4 /permissive- /utf-8)
    # Per-monitor DPI + long path awareness merged with the linker's own manifest.
    target_link_options(SortieApp PRIVATE
        "/MANIFEST:EMBED"
        "/MANIFESTINPUT:${CMAKE_CURRENT_SOURCE_DIR}/assets/app.manifest")
else()
    target_compile_options(SortieApp PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

# ------------------------------------------------------------------ install
include(GNUInstallDirs)

install(TARGETS SortieApp
    RUNTIME DESTINATION .
    BUNDLE DESTINATION .
    LIBRARY DESTINATION .
)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE" DESTINATION .)
endif()
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/README.md" DESTINATION .)

# Deploy the Qt runtime next to the .exe so the installed app really starts on a
# machine that has never seen a Qt installation.
if(WIN32)
    get_target_property(_qmake_location Qt6::qmake IMPORTED_LOCATION)
    if(NOT _qmake_location)
        set(_qmake_location "${QT6_INSTALL_PREFIX}/bin/qmake.exe")
    endif()
    get_filename_component(_qt_bin_dir "${_qmake_location}" DIRECTORY)
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${_qt_bin_dir}" "${QT6_INSTALL_PREFIX}/bin")
    if(NOT WINDEPLOYQT_EXECUTABLE)
        message(FATAL_ERROR
            "windeployqt was not found next to qmake (${_qt_bin_dir}). "
            "The installer would be missing its Qt runtime, so the build stops here.")
    endif()
    message(STATUS "windeployqt: ${WINDEPLOYQT_EXECUTABLE}")

    install(CODE "
        message(STATUS \"Deploying Qt runtime into \${CMAKE_INSTALL_PREFIX}\")
        execute_process(
            COMMAND \"${WINDEPLOYQT_EXECUTABLE}\"
                    --release
                    --no-translations
                    --compiler-runtime
                    --dir \"\${CMAKE_INSTALL_PREFIX}\"
                    \"\${CMAKE_INSTALL_PREFIX}/SortieApp.exe\"
            RESULT_VARIABLE _sortie_deploy_result)
        if(NOT _sortie_deploy_result EQUAL 0)
            message(FATAL_ERROR \"windeployqt failed with code \${_sortie_deploy_result}\")
        endif()
    ")

    # Every one of these must exist in a healthy package.
    foreach(_required Qt6Core.dll Qt6Gui.dll Qt6Widgets.dll Qt6Svg.dll platforms/qwindows.dll)
        install(CODE "
            if(NOT EXISTS \"\${CMAKE_INSTALL_PREFIX}/${_required}\")
                message(FATAL_ERROR \"Package is incomplete: ${_required} is missing after windeployqt\")
            endif()
        ")
    endforeach()
endif()

# ------------------------------------------------------------------ packaging
set(CPACK_PACKAGE_NAME "AMS Sortie")
set(CPACK_PACKAGE_VENDOR "Rehman Ahmed (AMS)")
set(CPACK_PACKAGE_CONTACT "Rehman Ahmed <rehmanahmed92@yandex.com>")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "A floating dial that sorts your files in one drop.")
set(CPACK_PACKAGE_DESCRIPTION
    "AMS Sortie puts a small dial on your desktop. Drop files or folders on it, pick a petal, "
    "and they are moved (or copied) to that folder. Transfers run in the background with a real "
    "progress window, are cancellable, verified and undoable.")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://wa.me/923060706989")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "AMS/Sortie")
set(CPACK_PACKAGE_EXECUTABLES "SortieApp;AMS Sortie")
set(CPACK_CREATE_DESKTOP_LINKS "SortieApp")
set(CPACK_PACKAGE_ICON "${CMAKE_CURRENT_SOURCE_DIR}/assets/installer/nsis_header.bmp")
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/installer/license.rtf")
    set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/installer/license.rtf")
endif()

# --- MSI (WiX Toolset v4)
set(CPACK_WIX_VERSION 4)
set(CPACK_WIX_UPGRADE_GUID "8F3E1C42-7A6B-4D2E-9B51-A3C9E7D41F60")
set(CPACK_WIX_PRODUCT_ICON "${CMAKE_CURRENT_SOURCE_DIR}/assets/icons/app.ico")
set(CPACK_WIX_UI_BANNER "${CMAKE_CURRENT_SOURCE_DIR}/assets/installer/banner.bmp")
set(CPACK_WIX_UI_DIALOG "${CMAKE_CURRENT_SOURCE_DIR}/assets/installer/dialog.bmp")
set(CPACK_WIX_LICENSE_RTF "${CMAKE_CURRENT_SOURCE_DIR}/installer/license.rtf")
set(CPACK_WIX_PROGRAM_MENU_FOLDER "AMS Sortie")
set(CPACK_WIX_EXTENSIONS "WixToolset.UI.wixext")
set(CPACK_WIX_PROPERTY_ARPHELPLINK "https://wa.me/923060706989")

# --- NSIS setup.exe
set(CPACK_NSIS_PACKAGE_NAME "AMS Sortie")
set(CPACK_NSIS_DISPLAY_NAME "AMS Sortie")
set(CPACK_NSIS_MUI_ICON "${CMAKE_CURRENT_SOURCE_DIR}/assets/icons/app.ico")
set(CPACK_NSIS_MUI_UNIICON "${CMAKE_CURRENT_SOURCE_DIR}/assets/icons/app.ico")
set(CPACK_NSIS_INSTALLED_ICON_NAME "SortieApp.exe")
set(CPACK_NSIS_URL_INFO_ABOUT "https://wa.me/923060706989")
set(CPACK_NSIS_CONTACT "Rehman Ahmed <rehmanahmed92@yandex.com>")
set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
set(CPACK_NSIS_MODIFY_PATH OFF)

set(CPACK_GENERATOR "ZIP")
if(WIN32)
    set(CPACK_GENERATOR "ZIP;NSIS;WIX")
endif()

include(CPack)
