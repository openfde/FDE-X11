
# client side protocol

add_library(xcb STATIC
	libxcb/src/xcb_auth.c
	libxcb/src/xcb_in.c
	libxcb/src/xcb_xid.c
	libxcb/src/xcb_out.c
	libxcb/src/xcb_conn.c
	libxcb/src/xcb_ext.c
	libxcb/src/xcb_list.c
	libxcb/src/xcb_util.c

	libxcb/src/xproto.c
	libxcb/src/bigreq.c
	libxcb/src/xc_misc.c
	libxcb/src/xproto.h
	libxcb/src/bigreq.h
	libxcb/src/xc_misc.h
)


target_compile_options(xcb PRIVATE
	${common_compile_options}
	"-DHAVE_CONFIG_H"
	"-fvisibility=hidden"
	"-DHAVE_ERR_H"
	"-DHAVE_STDINT_H=1"
	"-DHAVE_READLINK"
	"-UHAVE_REALLOCARRAY"
	"-DHAVE_REALPATH"
	"-DHAVE_STRLCPY"
	"-DXFONT_BDFFORMAT=1"
	"-DXFONT_BITMAP=1"
	"-UXFONT_FREETYPE"
	"-DXFONT_PCFFORMAT=1"
	"-UXFONT_SNFFORMAT"
	"-UX_BZIP2_FONT_COMPRESSION"
	"-DX_GZIP_FONT_COMPRESSION=1"
	"-D_GNU_SOURCE=1"
	"-D_DEFAULT_SOURCE=1"
	"-D_BSD_SOURCE=1"
	"-DHAS_FCHOWN"
	"-DHAS_STICKY_DIR_BIT"
	"-D_XOPEN_SOURCE"
	"-DNOFILES_MAX=512")

target_include_directories(xcb PRIVATE "libxcb/src")
target_link_libraries(xcb PUBLIC Xau xorgproto Xdmcp)
