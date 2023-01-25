add_library(okFrontPanel::okFrontPanel SHARED IMPORTED)

target_include_directories(okFrontPanel::okFrontPanel INTERFACE
    ${PROJECT_SOURCE_DIR}/includes
)

if(APPLE)
    set_target_properties(okFrontPanel::okFrontPanel PROPERTIES
        IMPORTED_LOCATION ${PROJECT_SOURCE_DIR}/libraries/Mac/libokFrontPanel.dylib
    )
elseif(WIN32)
    set_target_properties(okFrontPanel::okFrontPanel PROPERTIES
        IMPORTED_LOCATION ${PROJECT_SOURCE_DIR}/libraries/Windows/okFrontPanel.dll
        IMPORTED_IMPLIB ${PROJECT_SOURCE_DIR}/libraries/Windows/okFrontPanel.lib
    )
else()
    set_target_properties(okFrontPanel::okFrontPanel PROPERTIES
        IMPORTED_LOCATION ${PROJECT_SOURCE_DIR}/Linux/libokFrontPanel.so
    )
endif()

set(okFrontPanel_LIBRARIES okFrontPanel::okFrontPanel)
set(okFrontPanel_FOUND TRUE)