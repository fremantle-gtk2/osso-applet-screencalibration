#
# Makefile for touchscreen applet
#

DESTDIR = @DESTDIR@
INSTALL  = /usr/bin/install -c -m 644

all :

install :
#copy configuration
	$(INSTALL) tscalibrate.desktop $(DESTDIR)/usr/share/applications/hildon-control-panel

distclean :
	rm -f Makefile
.PHONY:
