set (LIB_VERSION_MAJOR 0)
set (LIB_VERSION_MINOR 0)
set (LIB_VERSION_PATCH 1)
set (LIB_VERSION "${LIB_VERSION_MAJOR}.${LIB_VERSION_MINOR}.${LIB_VERSION_PATCH}")

file (GLOB
    LIB_SRC
    CONFIGURE_DEPENDS
        *.c
)

add_library (libampart
    ${LIB_SRC}
)

target_include_directories(libampart
    PUBLIC
        "../include"
)

target_compile_definitions(libampart
    PUBLIC
        LIB_VERSION_MAJOR=${LIB_VERSION_MAJOR}
        LIB_VERSION_MINOR=${LIB_VERSION_MINOR}
        LIB_VERSION_PATCH=${LIB_VERSION_PATCH}
)

target_compile_options(libampart
    PUBLIC
        -Wall
        -Wextra
)

set_target_properties(libampart
    PROPERTIES
        OUTPUT_NAME "ampart"
)

target_link_libraries(libampart
    z
)
