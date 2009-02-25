#
# Makefile for touchscreen applet
#

DESTDIR = @DESTDIR@
INSTALL  = /usr/bin/install -c -m 644

all :

install :
#copy configuration
	$(INSTALL) tscalibrate.desktop $(DESTDIR)/usr/share/applications/hildon-control-panel
	$(INSTALL) tscalibrate.sudoers $(DESTDIR)/etc/sudoers.d
	
distclean :
	rm -f Makefile
.PHONY:
