set(APP_PATH "${CPACK_PACKAGE_DIRECTORY}/_CPack_Packages/Darwin/DragNDrop/${CPACK_PACKAGE_FILE_NAME}/XDAQ-RHX.app")
message(STATUS "Code signing app: ${APP_PATH}")

execute_process(
    COMMAND codesign 
        --deep --force --strict --verbose 
        --timestamp --options runtime 
        --sign "Developer ID Application: KonteX Inc. (XJL74GU2PH)" 
        "${APP_PATH}"
    RESULT_VARIABLE SIGN_RESULT
)

if(SIGN_RESULT)
    message(FATAL_ERROR "Code signing failed with code ${SIGN_RESULT}!")
else()
    message(STATUS "Code signing succeeded.")
endif()