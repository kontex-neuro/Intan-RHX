cmake_minimum_required(VERSION 3.16)

set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum OS X deployment version")

project(IntanRHX VERSION 1.0 LANGUAGES CXX)

SET(CMAKE_MODULE_PATH
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake
    ${CMAKE_MODULE_PATH}
)

if(WIN32)
    add_definitions(-D_HAS_STD_BYTE=0)
endif()

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt5 REQUIRED COMPONENTS Core Gui Multimedia Network Widgets Xml)
find_package(OpenCL REQUIRED)
# find_package(okFrontPanel REQUIRED)
find_package(xdaq REQUIRED)

set(ICON_NAME "xdaq-icon.icns")
set(ICON_PATH ${PROJECT_SOURCE_DIR}/images/${ICON_NAME})

option(UseMockOkFrontPanel "Use mock okFrontPanel library" OFF)

add_subdirectory(Engine)
add_subdirectory(GUI)

add_executable(
    IntanRHX WIN32 MACOSX_BUNDLE
    IntanRHX.qrc
    main.cpp
    ${EngineSources}
    ${GUISources}
    ${ICON_PATH}
)

target_include_directories(IntanRHX PRIVATE
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


message(STATUS "Qt5 : ${Qt5_DIR}")

target_compile_features(IntanRHX PRIVATE cxx_std_23)
target_compile_options(IntanRHX PRIVATE $<$<CXX_COMPILER_ID:Clang,GNU>:-Wno-deprecated>)
target_compile_definitions(IntanRHX PUBLIC $<$<CONFIG:Debug>:DEBUG>)
find_package(nlohmann_json REQUIRED)
find_package(fmt REQUIRED)
option(Sanitize "Enable sanitizers" OFF)
# if(Sanitize)
#     target_compile_options(IntanRHX PRIVATE $<$<CXX_COMPILER_ID:Clang,GNU>:-fsanitize=address,undefined>)
#     target_link_options(IntanRHX PRIVATE $<$<CXX_COMPILER_ID:Clang,GNU>:-fsanitize=address,undefined>)
# endif()

target_link_libraries(
    IntanRHX
    PRIVATE
    Qt5::Core
    Qt5::Gui
    Qt5::Multimedia
    Qt5::Network
    Qt5::Widgets
    Qt5::Xml
    OpenCL::OpenCL
    # okFrontPanel::okFrontPanel
    nlohmann_json::nlohmann_json
    fmt::fmt
    xdaq::xdaq_device
)

# copy the bit file and kernel.cl
add_custom_command(TARGET IntanRHX POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${PROJECT_SOURCE_DIR}/FPGA-bitfiles/XDAQRHD.bit"
        "$<TARGET_FILE_DIR:IntanRHX>/XDAQRHD.bit"
)

add_custom_command(TARGET IntanRHX POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${PROJECT_SOURCE_DIR}/FPGA-bitfiles/XDAQRHS.bit"
        "$<TARGET_FILE_DIR:IntanRHX>/XDAQRHS.bit"
)

add_custom_command(TARGET IntanRHX POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${PROJECT_SOURCE_DIR}/kernel.cl"
        "$<TARGET_FILE_DIR:IntanRHX>/kernel.cl"
)

# add_custom_command(TARGET ${PLUGIN_NAME} POST_BUILD
#     COMMAND ${CMAKE_COMMAND} -E copy_if_different -t 
#         $<TARGET_FILE_DIR:${PLUGIN_NAME}> 
#         $<TARGET_FILE:xdaq::ok_device_plugin>
#     COMMAND_EXPAND_LISTS
# )

include(deployQt)

if(APPLE)
    # copy the dylib to the bundle
    add_custom_command(TARGET IntanRHX POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:okFrontPanel::okFrontPanel>"
            "$<TARGET_FILE_DIR:IntanRHX>/../Frameworks/libokFrontPanel.dylib"
    )
    # change dylib path
    add_custom_command(TARGET IntanRHX POST_BUILD
        COMMAND install_name_tool -change
            "$<TARGET_FILE_NAME:okFrontPanel::okFrontPanel>"
            "@loader_path/../Frameworks/libokFrontPanel.dylib"
            "$<TARGET_FILE:IntanRHX>"
    )
    set_source_files_properties(${ICON_PATH} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    set_target_properties(IntanRHX PROPERTIES
        BUNDLE True
        MACOSX_BUNDLE_ICONFILE ${ICON_NAME}
    )
    deployqt(IntanRHX)
elseif(WIN32)
    list(GET XDAQ_PLUGIN_TARGETS 0 first_xdaq_plugin)
    install(DIRECTORY $<TARGET_FILE_DIR:xdaq::${first_xdaq_plugin}>
            DESTINATION .)
    install(DIRECTORY ${XDAQ_RESOURCES_DIR} 
            DESTINATION .)
    install(TARGETS IntanRHX DESTINATION .)
    # copy the dll to the executable directory
    # add_custom_command(TARGET IntanRHX POST_BUILD
    #     COMMAND ${CMAKE_COMMAND} -E copy_if_different
    #         "$<TARGET_FILE:okFrontPanel::okFrontPanel>"
    #         "$<TARGET_FILE_DIR:IntanRHX>"
    # )
    add_custom_command(TARGET IntanRHX POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:OpenCL::OpenCL>"
            "$<TARGET_FILE_DIR:IntanRHX>"
    )
    install(FILES
        "$<TARGET_FILE_DIR:IntanRHX>/kernel.cl"
        "$<TARGET_FILE:OpenCL::OpenCL>"
        DESTINATION .
    )

    install(DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/qtDeploy/"
            DESTINATION .)

    deployqt(IntanRHX)
endif()

include (InstallRequiredSystemLibraries)

set(CPACK_PACKAGE_NAME IntanRHX)
set(CPACK_PACKAGE_VENDOR IntanRHX)

# set(CPACK_PACKAGE_DESCRIPTION "RHX Data Acquisition Software")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "RHX Data Acquisition Software")

set(CPACK_PACKAGE_VERSION_MAJOR "1")
set(CPACK_PACKAGE_VERSION_MINOR "1")
set(CPACK_PACKAGE_VERSION_PATCH "5")
set(CPACK_PACKAGE_VERSION "${CPACK_PACKAGE_VERSION_MAJOR}-${CPACK_PACKAGE_VERSION_MINOR}-${CPACK_PACKAGE_VERSION_PATCH}")

set(CPACK_PACKAGE_INSTALL_DIRECTORY IntanRHX)

# set(CPACK_COMPONENTS_ALL_IN_ONE_PACKAGE true)
set(CPACK_MONOLITHIC_INSTALL true)

# set(CPACK_INSTALL_CMAKE_PROJECTS "build;IntanRHX;ALL;/")

set(CPACK_GENERATOR "NSIS")
set(CPACK_SOURCE_GENERATOR "NSIS")

# set (CPACK_NSIS_MENU_LINKS
#      "doc/cmake-${VERSION_MAJOR}.${VERSION_MINOR}/cmake-gui.html"
#      "cmake-gui Help" "http://www.cmake.org" "CMake Web Site")

# set(CPACK_NSIS_INSTALLED_ICON_NAME "IntanRHX") 
# set(CPACK_PACKAGE_ICON ${ICON_PATH})

set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/COPYING")

list(APPEND CPACK_NSIS_EXTRA_INSTALL_COMMANDS "
    CreateShortCut \\\"$DESKTOP\\\\IntanRHX.lnk\\\" \\\"$INSTDIR\\\\IntanRHX.exe\\\"
")

list(APPEND CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS "
    Delete \\\"$DESKTOP\\\\IntanRHX.lnk\\\"
")

list(APPEND CPACK_NSIS_EXTRA_INSTALL_COMMANDS "
    CreateShortCut \\\"$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\IntanRHX.lnk\\\" \\\"$INSTDIR\\\\IntanRHX.exe\\\"
")

list(APPEND CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS "
    CreateShortCut \\\"$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\Uninstall.lnk\\\" \\\"$INSTDIR\\\\Uninstall.exe\\\"
")

# set(CPACK_CREATE_DESKTOP_LINKS "IntanRHX")

include(CPack)