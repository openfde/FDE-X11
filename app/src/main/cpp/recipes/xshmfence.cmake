file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/values.h" CONTENT "#include <limits.h>")
# create_symlink silently fails on a Windows host without symlink privileges,
# so copy the header instead (it is a single file).
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/libxshmfence/src/xshmfence.h" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/X11")
add_library(xshmfence STATIC "libxshmfence/src/xshmfence_alloc.c" "libxshmfence/src/xshmfence_futex.c")
target_include_directories(xshmfence PRIVATE "xorgproto/include" "${CMAKE_CURRENT_BINARY_DIR}")
target_compile_options(xshmfence PRIVATE "-DSHMDIR=\"/\"" "-DHAVE_FUTEX" "-DMAXINT=INT_MAX")
