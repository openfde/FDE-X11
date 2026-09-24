file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/X11")
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/X11/XlibConf.h" CONTENT "\n#pragma once\n#define XTHREADS 1\n#define XUSE_MTSAFE_API 1")

add_library(xorgproto INTERFACE)
target_include_directories(xorgproto INTERFACE "xorgproto/include")
set(USE_FDS_BITS 1)
configure_file("xorgproto/include/X11/Xpoll.h.in" "${CMAKE_CURRENT_BINARY_DIR}/X11/Xpoll.h" @ONLY)

# The patch has to be applied before the headers are copied, otherwise a fresh
# checkout would compile against the unpatched ones.
target_apply_patch(Xtrans "${CMAKE_CURRENT_SOURCE_DIR}/libxtrans" "${CMAKE_CURRENT_SOURCE_DIR}/patches/Xtrans.patch")

# ${CMAKE_COMMAND} -E create_symlink silently fails on a Windows host without
# symlink privileges, which used to leave <X11/Xtrans/Xtrans.h> missing. Copy
# the headers instead; that behaves the same for every consumer of them.
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/libxtrans/" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/X11/Xtrans")
