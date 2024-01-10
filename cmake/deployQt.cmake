find_package(Qt5 REQUIRED COMPONENTS Core)
get_target_property(qmake_executable Qt5::qmake IMPORTED_LOCATION)
get_filename_component(_qt_bin_dir "${qmake_executable}" DIRECTORY)
if(APPLE)
    find_program(MACDEPLOYQT_EXECUTABLE macdeployqt HINTS "${_qt_bin_dir}")
    function(deployqt target)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND "$<$<CONFIG:Release>:${MACDEPLOYQT_EXECUTABLE}>"
                "$<$<CONFIG:Release>:$<TARGET_FILE_DIR:${target}>/../../>"
            COMMENT "Deploying Qt..."
        )
    endfunction()
elseif(WIN32)
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${_qt_bin_dir}")
    function(deployqt target)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND "$<$<CONFIG:Release>:${WINDEPLOYQT_EXECUTABLE}>"
                "$<$<CONFIG:Release>:$<TARGET_FILE_DIR:${target}>>"
            COMMENT "Deploying Qt..."
        )
    endfunction()
endif()