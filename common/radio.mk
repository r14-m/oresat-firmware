# Required libraries for RADIO
include $(PROJ_SRC)/si41xx.mk
include $(PROJ_SRC)/ax5043.mk
include $(PROJ_SRC)/pdu.mk
include $(PROJ_SRC)/ax25.mk
include $(PROJ_SRC)/aprs.mk
include $(PROJ_SRC)/uslp.mk
include $(PROJ_SRC)/sdls.mk
include $(PROJ_SRC)/spp.mk

# List of all the RADIO device files.
RADIOSRC := $(PROJ_SRC)/radio.c

# Required include directories
RADIOINC := $(PROJ_SRC)/include

# Shared variables
ALLCSRC += $(RADIOSRC)
ALLINC  += $(RADIOINC)
