obj-$(CONFIG_NET_DSA_RALINK_ESW) += ralink-esw.o

ralink-esw-y := ralink_esw.o

ifeq ($(CONFIG_LEDS_CLASS),y)
ralink-esw-y += ralink_esw_leds.o
endif

obj-$(CONFIG_NET_RALINK_MT7620_GSW) += mt7620-gsw.o

mt7620-gsw-y := mt7620_gsw.o
mt7620-gsw-$(CONFIG_NET_DSA_MT7620_GSW) += mt7620_gsw_dsa.o
