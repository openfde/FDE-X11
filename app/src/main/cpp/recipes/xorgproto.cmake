file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/X11")
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/X11/XlibConf.h" CONTENT "\n#pragma once\n#define XTHREADS 1\n#define XUSE_MTSAFE_API 1")

add_library(xorgproto INTERFACE)
target_include_directories(xorgproto INTERFACE "xorgproto/include")
set(USE_FDS_BITS 1)
configure_file("xorgproto/include/X11/Xpoll.h.in" "${CMAKE_CURRENT_BINARY_DIR}/X11/Xpoll.h" @ONLY)

# The patch has to be applied before the headers are linked below, otherwise a
# fresh checkout would compile against the unpatched ones.
target_apply_patch(Xtrans "${CMAKE_CURRENT_SOURCE_DIR}/libxtrans" "${CMAKE_CURRENT_SOURCE_DIR}/patches/Xtrans.patch")

# <X11/Xtrans/Xtrans.h> is taken from the build directory. Link the sources there
# as before; creating symlinks is not permitted on a default Windows host (the
# call fails silently, which used to leave the header missing), so fall back to
# copying the headers in that case.
execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink "${CMAKE_CURRENT_SOURCE_DIR}/libxtrans" "${CMAKE_CURRENT_BINARY_DIR}/X11/Xtrans"
        OUTPUT_QUIET ERROR_QUIET)
if(NOT EXISTS "${CMAKE_CURRENT_BINARY_DIR}/X11/Xtrans/Xtrans.h")
    file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/libxtrans/" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/X11/Xtrans")
endif()
