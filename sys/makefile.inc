# $Id: makefile.inc,v 1.2 2001/08/07 06:23:30 tekhedd Exp $

# Stuff added at the end of the makefile by build


# Target to build softice symbol table:
$(TARGETPATH)\i386\$(TARGETNAME).NMS: $(TARGETPATH)\i386\$(TARGETNAME).SYS
        d:\softice\softice\nmsym /trans:source,package,always $(TARGETPATH)\i386\$(TARGETNAME).SYS


