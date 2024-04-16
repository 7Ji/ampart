add_executable(ampart
    src/cli.c
    src/dtb.c
    src/dts.c
    src/ept.c
    src/gzip.c
    src/io.c
    src/main.c
    src/parg.c
    src/size.c
    src/stringblock.c
    src/util.c
    src/version.c)

target_link_libraries(ampart
    z)
target_include_directories(ampart PRIVATE
    "include")
