cmake_minimum_required(VERSION 3.16)

set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum OS X deployment version")

project(XDAQ-RHX VERSION 1.2.0 LANGUAGES CXX)

SET(CMAKE_MODULE_PATH
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake
    ${CMAKE_MODULE_PATH}
)


# Compiler flags and dependencies
add_compile_definitions($<$<CXX_COMPILER_ID:MSVC>:_HAS_STD_BYTE=0>)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(fmt REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(OpenCL REQUIRED)
find_package(Qt5 REQUIRED COMPONENTS Core Gui Multimedia Network Widgets Xml)
find_package(xdaq REQUIRED)




# Build
add_subdirectory(Engine)
add_subdirectory(GUI)

if(${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
    set(ICON_NAME "xdaq-icon.icns")
else()
    set(ICON_NAME "xdaq-icon.ico")
endif()
set(ICON_PATH ${PROJECT_SOURCE_DIR}/images/${ICON_NAME})

if (${CMAKE_SYSTEM_NAME} MATCHES "Windows")
    enable_language("RC")
    set (WIN32_RESOURCES ${CMAKE_CURRENT_SOURCE_DIR}/XDAQ-RHX.rc)
endif()

add_executable(
    XDAQ-RHX WIN32 MACOSX_BUNDLE
    IntanRHX.qrc
    main.cpp
    ${EngineSources}
    ${GUISources}
    ${ICON_PATH}
    ${WIN32_RESOURCES}
)

target_include_directories(XDAQ-RHX PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Engine/API/Abstract
    ${CMAKE_CURRENT_SOURCE_DIR}/Engine/API/Hardware
    ${CMAKE_CURRENT_SOURCE_DIR}/Engine/API/Synthetic
    ${CMAKE_CURRENT_SOURCE_DIR}/Engine/Processing
    ${CMAKE_CURRENT_SOURCE_DIR}/Engine/Processing/DataFileReaders
    ${CMAKE_CURRENT_SOURCE_DIR}/Engine/Processing/SaveManagers
    ${CMAKE_CURRENT_SOURCE_DIR}/Engine/Processing/XPUInterfaces
    ${CMAKE_CURRENT_SOURCE_DIR}/Engine/Threads
    ${CMAKE_CURRENT_SOURCE_DIR}/GUI/Windows
    ${CMAKE_CURRENT_SOURCE_DIR}/GUI/Widgets
    ${CMAKE_CURRENT_SOURCE_DIR}/GUI/Dialogs
)

target_compile_features(XDAQ-RHX PRIVATE cxx_std_23)
target_compile_options(XDAQ-RHX PRIVATE $<$<CXX_COMPILER_ID:Clang,GNU>:-Wno-deprecated>)
target_compile_definitions(XDAQ-RHX PUBLIC $<$<CONFIG:Debug>:DEBUG>)
option(Sanitize "Enable sanitizers" OFF)
if(Sanitize)
    target_compile_options(XDAQ-RHX PRIVATE -fsanitize=address,undefined)
    target_link_options(XDAQ-RHX PRIVATE -fsanitize=address,undefined)
endif()

target_link_libraries(XDAQ-RHX PRIVATE
    Qt5::Core
    Qt5::Gui
    Qt5::Multimedia
    Qt5::Network
    Qt5::Widgets
    Qt5::Xml
    OpenCL::OpenCL
    nlohmann_json::nlohmann_json
    fmt::fmt
    xdaq::xdaq_device
)

# Copy for development
file(COPY ${XDAQ_DEVICE_MANAGER_DIR} DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
file(COPY ${XDAQ_RESOURCES_DIR} DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
file(COPY ${PROJECT_SOURCE_DIR}/kernel.cl DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
if (${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
    get_target_property(OpenCL_DLL OpenCL::OpenCL IMPORTED_LOCATION)
    file(COPY ${OpenCL_DLL} DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
endif()


# Install
include(deployQt)

install(DIRECTORY ${XDAQ_DEVICE_MANAGER_DIR} DESTINATION .)
install(DIRECTORY ${XDAQ_RESOURCES_DIR} DESTINATION .)

if (${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
    install(TARGETS XDAQ-RHX DESTINATION .)
    install(FILES
        ${PROJECT_SOURCE_DIR}/kernel.cl
        "$<TARGET_FILE:OpenCL::OpenCL>"
        DESTINATION .
    )

    deployqt($<TARGET_FILE_NAME:XDAQ-RHX>)
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
    set_source_files_properties(${ICON_PATH} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    set_target_properties(XDAQ-RHX PROPERTIES
        BUNDLE True
        MACOSX_BUNDLE_ICONFILE ${ICON_NAME}
    )
    deployqt($<TARGET_FILE_NAME:XDAQ-RHX>)
endif()


# Packaging

if (${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
    set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION .)
    include (InstallRequiredSystemLibraries)

	set(CPACK_PACKAGE_DIRECTORY ${CMAKE_BINARY_DIR}/package)

    set(CPACK_PACKAGE_NAME ${PROJECT_NAME})
    set(CPACK_NSIS_DISPLAY_NAME ${PROJECT_NAME})
    set(CPACK_PACKAGE_VENDOR "KonteX Neuroscience")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "IntanRHX Data Acquisition Software for XDAQ")
    set(CPACK_NSIS_UNINSTALL_NAME "Uninstall XDAQ-RHX")
    set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/COPYING")
    set(CPACK_GENERATOR "NSIS")
    set(CPACK_NSIS_MUI_ICON ${ICON_PATH})
    set(CPACK_NSIS_INSTALLED_ICON_NAME "XDAQ-RHX.exe")
    set(CPACK_NSIS_MENU_LINKS "XDAQ-RHX.exe" "XDAQ-RHX")

    include(CPack)
endif()