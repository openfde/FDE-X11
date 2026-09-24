file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/values.h" CONTENT "#include <limits.h>")
# Same as the Xtrans headers in xorgproto.cmake: link the header into the build
# directory, and copy it only on hosts where symlinks are not permitted.
execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink "${CMAKE_CURRENT_SOURCE_DIR}/libxshmfence/src/xshmfence.h" "${CMAKE_CURRENT_BINARY_DIR}/X11/xshmfence.h"
        OUTPUT_QUIET ERROR_QUIET)
if(NOT EXISTS "${CMAKE_CURRENT_BINARY_DIR}/X11/xshmfence.h")
    file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/libxshmfence/src/xshmfence.h" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/X11")
endif()
add_library(xshmfence STATIC "libxshmfence/src/xshmfence_alloc.c" "libxshmfence/src/xshmfence_futex.c")
target_include_directories(xshmfence PRIVATE "xorgproto/include" "${CMAKE_CURRENT_BINARY_DIR}")
target_compile_options(xshmfence PRIVATE "-DSHMDIR=\"/\"" "-DHAVE_FUTEX" "-DMAXINT=INT_MAX")
