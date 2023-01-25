if(WIN32)
    add_library(OpenCL::OpenCL SHARED IMPORTED)
    set_target_properties(OpenCL::OpenCL PROPERTIES
        IMPORTED_LOCATION ${PROJECT_SOURCE_DIR}/libraries/Windows/OpenCL.dll
        IMPORTED_IMPLIB ${PROJECT_SOURCE_DIR}/libraries/Windows/OpenCL.lib
    )
    target_include_directories(OpenCL::OpenCL INTERFACE
        ${PROJECT_SOURCE_DIR}/includes
    )
    set(OpenCL_LIBRARIES OpenCL::OpenCL)
    set(OpenCL_FOUND TRUE)
else()
    set(CMAKE_MODULE_PATH_OLD_WHEN_FINDING_OPENCL ${CMAKE_MODULE_PATH})
    set(CMAKE_MODULE_PATH "${CMAKE_ROOT}/Modules")
    find_package(OpenCL REQUIRED)
    set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH_OLD_WHEN_FINDING_OPENCL})
    unset(CMAKE_MODULE_PATH_OLD_WHEN_FINDING_OPENCL)
endif()
