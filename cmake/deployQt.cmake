find_package(Qt5 REQUIRED COMPONENTS Core)
get_target_property(qmake_executable Qt5::qmake IMPORTED_LOCATION)
get_filename_component(_qt_bin_dir "${qmake_executable}" DIRECTORY)

if(${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
    find_program(DEPLOYQT_EXECUTABLE macdeployqt HINTS "${_qt_bin_dir}")
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
    find_program(DEPLOYQT_EXECUTABLE windeployqt HINTS "${_qt_bin_dir}")
endif()

function(deployqt target_install_path)
    install(CODE "execute_process(COMMAND \"${DEPLOYQT_EXECUTABLE}\" \"\${CMAKE_INSTALL_PREFIX}/${target_install_path}\")")
endfunction()