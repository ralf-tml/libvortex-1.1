EXTRA_DIST = Makefile.win libvortex-external-1.1.def

if ENABLE_VORTEX_LOG
INCLUDE_VORTEX_LOG=-DENABLE_VORTEX_LOG
endif

INCLUDES = $(compiler_options) -I. -I$(top_srcdir)/src -I$(top_srcdir) -D__COMPILING_VORTEX__ -D__axl_disable_broken_bool_def__  \
	$(AXL_CFLAGS) $(INCLUDE_VORTEX_LOG) $(PTHREAD_CFLAGS) $(NOPOLL_CFLAGS) \
	-DVERSION=\""$(VORTEX_VERSION)"\" \
	-DPACKAGE_DTD_DIR=\""$(datadir)"\" \
	-DPACKAGE_TOP_DIR=\""$(top_srcdir)"\" 

libvortex_external_1_1_includedir = $(includedir)/vortex-1.1

lib_LTLIBRARIES = libvortex-external-1.1.la

libvortex_external_1_1_la_SOURCES = vortex_external.c 
libvortex_external_1_1_include_HEADERS = vortex_external.h

libvortex_external_1_1_la_LIBADD = \
	$(AXL_LIBS) $(NOPOLL_LIBS) $(PTHREAD_LIBS) $(ADDITIONAL_LIBS) $(top_builddir)/src/libvortex-1.1.la

libvortex_external_1_1_la_LDFLAGS = -no-undefined -export-symbols-regex '^(vortex|__vortex|_vortex).*' 

libvortex-external-1.1.def: update-def

update-def:
	echo "EXPORTS" > libvortex-external-1.1.def
	cat .libs/libvortex-external-1.1.exp | grep -v io_waiting_poll | grep -v io_waiting_epoll | grep -v __vortex >> libvortex-external-1.1.def
