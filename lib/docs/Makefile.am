## Process this file with automake to produce Makefile.in
## Stolen from GTK+ 3.10
## FIXME this is just a mock-up, it's full of junk!

AUTOMAKE_OPTIONS = 1.6

# The name of the module.
DOC_MODULE=sandboxutils

# The top-level SGML file.
DOC_MAIN_SGML_FILE=sandboxutils-docs.sgml

# The directory containing the source code. Relative to $(srcdir)
DOC_SOURCE_DIR=$(top_srcdir)/

# Used for dependencies
HFILE_GLOB=$(top_srcdir)/*.h
CFILE_GLOB=$(top_srcdir)/*.c

# Header files to ignore when scanning
IGNORE_HFILES=				

# CFLAGS and LDFLAGS for compiling scan program. Only needed
# if $(DOC_MODULE).types is non-empty.
AM_CPPFLAGS = \
	-I$(top_srcdir) 	\
	-I$(top_builddir) 	\
	$(sandboxutils_DEBUG_FLAGS)	\
	$(sandboxutils_DEP_CFLAGS)


# Extra options to supply to gtkdoc-mkdb
MKDB_OPTIONS=--sgml-mode --output-format=xml --name-space=sandboxutils

# Extra SGML files that are included by $(DOC_MAIN_SGML_FILE)
content_files =			

expand_content_files = 			

# Images to copy into HTML directory
HTML_IMAGES = 
#	$(top_srcdir)/gtk/stock-icons/24/help-about.png			
#	$(srcdir)/images/aboutdialog.png				


# Extra options to supply to gtkdoc-fixref
FIXXREF_OPTIONS=--extra-dir=../gdk/html \
	--extra-dir=$(GLIB_PREFIX)/share/gtk-doc/html/gtk \
	--extra-dir=$(GLIB_PREFIX)/share/gtk-doc/html/gobject \
	--extra-dir=$(GLIB_PREFIX)/share/gtk-doc/html/glib \
	--extra-dir=$(GLIB_PREFIX)/share/gtk-doc/html/gio \
	--extra-dir=$(ATK_PREFIX)/share/gtk-doc/html/atk \
	--extra-dir=$(PANGO_PREFIX)/share/gtk-doc/html/pango \
	--extra-dir=$(CAIRO_PREFIX)/share/gtk-doc/html/cairo

# Other files to distribute
# EXTRA_DIST = version.xml.in 

########################################################################

man_MANS = 				\
	gtk-query-immodules-3.0.1	\
	gtk-update-icon-cache.1		\
	gtk-launch.1			\
	broadwayd.1

if ENABLE_MAN

XSLTPROC_FLAGS = \
        --nonet \
        --stringparam man.output.quietly 1 \
        --stringparam funcsynopsis.style ansi \
        --stringparam man.th.extra1.suppress 1 \
        --stringparam man.authors.section.enabled 0 \
        --stringparam man.copyright.section.enabled 0

.xml.1:
	$(AM_V_GEN) $(XSLTPROC) $(XSLTPROC_FLAGS) http://docbook.sourceforge.net/release/xsl/current/manpages/docbook.xsl $<

dist-local-check-mans-enabled:
	if grep "Man generation disabled" $(man_MANS) >/dev/null; then $(RM) $(man_MANS); fi

else

$(man_MANS):
	echo Man generation disabled.  Creating dummy $@.  Configure with --enable-man to enable it.
	echo Man generation disabled.  Remove this file, configure with --enable-man, and rebuild > $@

dist-local-check-mans-enabled:
	echo "*** --enable-man must be used in order to make dist"
	false

endif

MAINTAINERCLEANFILES = $(man_MANS) $(BUILT_SOURCES)

EXTRA_DIST += $(man_MANS)

if ENABLE_DOC
TESTS_ENVIRONMENT = cd $(srcdir) && \
  DOC_MODULE=$(DOC_MODULE) DOC_MAIN_SGML_FILE=$(DOC_MAIN_SGML_FILE) \
  SRCDIR=$(abs_srcdir) BUILDDIR=$(abs_builddir)
endif

dist-hook-local: dist-local-check-mans-enabled all-local

sandboxutils-docs-clean: clean
	cd $(srcdir) && rm -rf xml html

-include $(top_srcdir)/git.mk
