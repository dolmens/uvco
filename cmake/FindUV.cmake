find_path(UV_INCLUDE_DIR NAMES uv.h)
find_library(UV_LIBRARY NAMES uv)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(UV
    REQUIRED_VARS UV_LIBRARY UV_INCLUDE_DIR
    )

if(UV_FOUND)
    add_library(UV::uv UNKNOWN IMPORTED)
    set_target_properties(UV::uv PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${UV_INCLUDE_DIR}"
        IMPORTED_LOCATION "${UV_LIBRARY}"
        )
endif()
