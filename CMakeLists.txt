cmake_minimum_required (VERSION 3.10)

project (ampart C)

add_subdirectory (lib)

add_executable(ampart
    ampart.c
)

target_link_libraries(ampart
    libampart
)

# target_include_directories(ampart PRIVATE
#     "include"
#     "include/priv"
# )

# target_compile_options(ampart PRIVATE
#     -Wall
#     -Wextra
# )
