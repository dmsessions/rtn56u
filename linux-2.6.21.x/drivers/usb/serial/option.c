/*
  USB Driver for GSM modems

  Copyright (C) 2005  Matthias Urlichs <smurf@smurf.noris.de>

  This driver is free software; you can redistribute it and/or modify
  it under the terms of Version 2 of the GNU General Public License as
  published by the Free Software Foundation.

  Portions copied from the Keyspan driver by Hugh Blemings <hugh@blemings.org>

  History: see the git log.

  Work sponsored by: Sigos GmbH, Germany <info@sigos.de>

  This driver exists because the "normal" serial driver doesn't work too well
  with GSM modems. Issues:
  - data loss -- one single Receive URB is not nearly enough
  - nonstandard flow (Option devices) control
  - controlling the baud rate doesn't make sense

  This driver is named "option" because the most common device it's
  used for is a PC-Card (with an internal OHCI-USB interface, behind
  which the GSM interface sits), made by Option Inc.

  Some of the "one port" devices actually exhibit multiple USB instances
  on the USB bus. This is not a bug, these ports are used for different
  device features.
*/

#define DRIVER_VERSION "v0.7.1"
#define DRIVER_AUTHOR "Matthias Urlichs <smurf@smurf.noris.de>"
#define DRIVER_DESC "USB Driver for GSM modems"

#include <linux/kernel.h>
#include <linux/jiffies.h>
#include <linux/errno.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/module.h>
#include <linux/usb.h>
#include <linux/usb/serial.h>

/* Function prototypes */
static int  option_open(struct usb_serial_port *port, struct file *filp);
static void option_close(struct usb_serial_port *port, struct file *filp);
static int  option_startup(struct usb_serial *serial);
static void option_shutdown(struct usb_serial *serial);
static void option_rx_throttle(struct usb_serial_port *port);
static void option_rx_unthrottle(struct usb_serial_port *port);
static int  option_write_room(struct usb_serial_port *port);

static void option_instat_callback(struct urb *urb);

static int option_write(struct usb_serial_port *port,
			const unsigned char *buf, int count);

static int  option_chars_in_buffer(struct usb_serial_port *port);
static int  option_ioctl(struct usb_serial_port *port, struct file *file,
			unsigned int cmd, unsigned long arg);
static void option_set_termios(struct usb_serial_port *port,
				struct ktermios *old);
static void option_break_ctl(struct usb_serial_port *port, int break_state);
static int  option_tiocmget(struct usb_serial_port *port, struct file *file);
static int  option_tiocmset(struct usb_serial_port *port, struct file *file,
				unsigned int set, unsigned int clear);
static int  option_send_setup(struct usb_serial_port *port);

/* Vendor and product IDs */
#define OPTION_VENDOR_ID			0x0AF0
#define OPTION_PRODUCT_COLT			0x5000
#define OPTION_PRODUCT_RICOLA			0x6000
#define OPTION_PRODUCT_RICOLA_LIGHT		0x6100
#define OPTION_PRODUCT_RICOLA_QUAD		0x6200
#define OPTION_PRODUCT_RICOLA_QUAD_LIGHT	0x6300
#define OPTION_PRODUCT_RICOLA_NDIS		0x6050
#define OPTION_PRODUCT_RICOLA_NDIS_LIGHT	0x6150
#define OPTION_PRODUCT_RICOLA_NDIS_QUAD		0x6250
#define OPTION_PRODUCT_RICOLA_NDIS_QUAD_LIGHT	0x6350
#define OPTION_PRODUCT_COBRA			0x6500
#define OPTION_PRODUCT_COBRA_BUS		0x6501
#define OPTION_PRODUCT_VIPER			0x6600
#define OPTION_PRODUCT_VIPER_BUS		0x6601
#define OPTION_PRODUCT_GT_MAX_READY		0x6701
#define OPTION_PRODUCT_GT_MAX			0x6711
#define OPTION_PRODUCT_FUJI_MODEM_LIGHT		0x6721
#define OPTION_PRODUCT_FUJI_MODEM_GT		0x6741
#define OPTION_PRODUCT_FUJI_MODEM_EX		0x6761
#define OPTION_PRODUCT_FUJI_NETWORK_LIGHT	0x6731
#define OPTION_PRODUCT_FUJI_NETWORK_GT		0x6751
#define OPTION_PRODUCT_FUJI_NETWORK_EX		0x6771
#define OPTION_PRODUCT_KOI_MODEM		0x6800
#define OPTION_PRODUCT_KOI_NETWORK		0x6811
#define OPTION_PRODUCT_SCORPION_MODEM		0x6901
#define OPTION_PRODUCT_SCORPION_NETWORK		0x6911
#define OPTION_PRODUCT_ETNA_MODEM		0x7001
#define OPTION_PRODUCT_ETNA_NETWORK		0x7011
#define OPTION_PRODUCT_ETNA_MODEM_LITE		0x7021
#define OPTION_PRODUCT_ETNA_MODEM_GT		0x7041
#define OPTION_PRODUCT_ETNA_MODEM_EX		0x7061
#define OPTION_PRODUCT_ETNA_NETWORK_LITE	0x7031
#define OPTION_PRODUCT_ETNA_NETWORK_GT		0x7051
#define OPTION_PRODUCT_ETNA_NETWORK_EX		0x7071
#define OPTION_PRODUCT_ETNA_KOI_MODEM		0x7100
#define OPTION_PRODUCT_ETNA_KOI_NETWORK		0x7111

#define HUAWEI_VENDOR_ID			0x12D1
#define HUAWEI_PRODUCT_E600			0x1001
#define HUAWEI_PRODUCT_E220			0x1003
#define HUAWEI_PRODUCT_E220BIS			0x1004
#define HUAWEI_PRODUCT_E1401			0x1401
#define HUAWEI_PRODUCT_E1403			0x1403
#define HUAWEI_PRODUCT_E1405			0x1405
#define HUAWEI_PRODUCT_E1406			0x1406
#define HUAWEI_PRODUCT_E1408			0x1408
#define HUAWEI_PRODUCT_E1409			0x1409
#define HUAWEI_PRODUCT_E1410			0x1410
#define HUAWEI_PRODUCT_E1411			0x1411
#define HUAWEI_PRODUCT_E1412			0x1412
#define HUAWEI_PRODUCT_E1413			0x1413
#define HUAWEI_PRODUCT_E1414			0x1414
#define HUAWEI_PRODUCT_E1415			0x1415
#define HUAWEI_PRODUCT_E1416			0x1416
#define HUAWEI_PRODUCT_E1417			0x1417
#define HUAWEI_PRODUCT_E1418			0x1418
#define HUAWEI_PRODUCT_E1419			0x1419
#define HUAWEI_PRODUCT_E140B                    0x140b          // 0701 ASUS    /* if target id, then use it */
#define HUAWEI_PRODUCT_EC122                    0x140C          // 0818 ASUS
#define HUAWEI_PRODUCT_E1820                    0x14AC          // 0818 ASUS
#define HUAWEI_PRODUCT_EC306                    0x1506          // 0818 ASUS


#define DATANG_VENDOR_ID			0x1ab7
#define DATANG_PRODUCT_M5731			0x5731

#define BANDLUXE_VENDOR_ID			0x1a8d
#define BANDLUXE_PRODUCT_C270			0x1009

#define ALCATEL_VENDOR_ID			0x05c6
#define ALCATEL_PRODUCT_C820			0x00a0

#define DLINK_VENDOR_ID				0x1186
#define DLINK_PRODUCT_DWM652			0x3e04

#define MU_VENDOR_ID				0x0408
#define MU_PRODUCT_Q101_MODEM			0xea02
#define MU_PRODUCT_Q101				0x1000

#define NOVATELWIRELESS_VENDOR_ID		0x1410

/* MERLIN EVDO PRODUCTS */
#define NOVATELWIRELESS_PRODUCT_V640		0x1100
#define NOVATELWIRELESS_PRODUCT_V620		0x1110
#define NOVATELWIRELESS_PRODUCT_V740		0x1120
#define NOVATELWIRELESS_PRODUCT_V720		0x1130

/* MERLIN HSDPA/HSPA PRODUCTS */
#define NOVATELWIRELESS_PRODUCT_U730		0x1400
#define NOVATELWIRELESS_PRODUCT_U740		0x1410
#define NOVATELWIRELESS_PRODUCT_U870		0x1420
#define NOVATELWIRELESS_PRODUCT_XU870		0x1430
#define NOVATELWIRELESS_PRODUCT_X950D		0x1450

/* EXPEDITE PRODUCTS */
#define NOVATELWIRELESS_PRODUCT_EV620		0x2100
#define NOVATELWIRELESS_PRODUCT_ES720		0x2110
#define NOVATELWIRELESS_PRODUCT_E725		0x2120
#define NOVATELWIRELESS_PRODUCT_ES620		0x2130
#define NOVATELWIRELESS_PRODUCT_EU730		0x2400
#define NOVATELWIRELESS_PRODUCT_EU740		0x2410
#define NOVATELWIRELESS_PRODUCT_EU870D		0x2420

/* OVATION PRODUCTS */
#define NOVATELWIRELESS_PRODUCT_MC727		0x4100
#define NOVATELWIRELESS_PRODUCT_MC950D		0x4400

/* FUTURE NOVATEL PRODUCTS */
#define NOVATELWIRELESS_PRODUCT_EVDO_1		0x6000
#define NOVATELWIRELESS_PRODUCT_HSPA_1		0x7000
#define NOVATELWIRELESS_PRODUCT_EMBEDDED_1	0x8000
#define NOVATELWIRELESS_PRODUCT_GLOBAL_1	0x9000
#define NOVATELWIRELESS_PRODUCT_EVDO_2		0x6001
#define NOVATELWIRELESS_PRODUCT_HSPA_2		0x7001
#define NOVATELWIRELESS_PRODUCT_EMBEDDED_2	0x8001
#define NOVATELWIRELESS_PRODUCT_GLOBAL_2	0x9001

/* AMOI PRODUCTS */
#define AMOI_VENDOR_ID				0x1614
#define AMOI_PRODUCT_H01			0x0800
#define AMOI_PRODUCT_H01A			0x7002
#define AMOI_PRODUCT_H02			0x0802

#define DELL_VENDOR_ID				0x413C

#define KYOCERA_VENDOR_ID			0x0c88
#define KYOCERA_PRODUCT_KPC650			0x17da
#define KYOCERA_PRODUCT_KPC680			0x180a

#define ANYDATA_VENDOR_ID			0x16d5
#define ANYDATA_PRODUCT_ADU_620UW		0x6202
#define ANYDATA_PRODUCT_ADU_E100A		0x6501
#define ANYDATA_PRODUCT_ADU_500A		0x6502

#define AXESSTEL_VENDOR_ID			0x1726
#define AXESSTEL_PRODUCT_MV110H			0x1000

#define ONDA_VENDOR_ID				0x19d2
#define ONDA_PRODUCT_MSA501HS			0x0001
#define ONDA_PRODUCT_ET502HS			0x0002

#define BANDRICH_VENDOR_ID			0x1A8D
#define BANDRICH_PRODUCT_C100_1			0x1002
#define BANDRICH_PRODUCT_C100_2			0x1003
#define BANDRICH_PRODUCT_1004			0x1004
#define BANDRICH_PRODUCT_1005			0x1005
#define BANDRICH_PRODUCT_1006			0x1006
#define BANDRICH_PRODUCT_1007			0x1007
#define BANDRICH_PRODUCT_1008			0x1008
#define BANDRICH_PRODUCT_1009			0x1009
#define BANDRICH_PRODUCT_100A			0x100a

#define BANDRICH_PRODUCT_100B			0x100b
#define BANDRICH_PRODUCT_100C			0x100c
#define BANDRICH_PRODUCT_100D			0x100d
#define BANDRICH_PRODUCT_100E			0x100e

#define BANDRICH_PRODUCT_100F			0x100f
#define BANDRICH_PRODUCT_1010			0x1010
#define BANDRICH_PRODUCT_1011			0x1011
#define BANDRICH_PRODUCT_1012			0x1012

#define AMOI_VENDOR_ID			0x1614
#define AMOI_PRODUCT_9508			0x0800

#define QUALCOMM_VENDOR_ID			0x05C6

#define MAXON_VENDOR_ID				0x16d8

#define TELIT_VENDOR_ID				0x1bc7
#define TELIT_PRODUCT_UC864E			0x1003

/* ZTE PRODUCTS */
#define ZTE_VENDOR_ID				0x19d2
#define ZTE_PRODUCT_MF628			0x0015
#define ZTE_PRODUCT_AC2746                      0xfff1
#define ZTE_PRODUCT_CDMA_TECH			0xfffe
#define ZTE_PRODUCT_CDMA_TECH2			0x0003
#define ZTE_PRODUCT_CDMA_TECH3                  0x0034
#define ZTE_PRODUCT_CDMA_TECH4                  0x0090

#define ANYDATA_VENDOR_ID			0x16d5
#define ANYDATA_PRODUCT_ID			0x6501

/* QISDA PRODUCTS */
#define QISDA_VENDOR_ID                         0x1da5
#define QISDA_PRODUCT_H21_4512                  0x4512
#define QISDA_PRODUCT_H21_4523                  0x4523
#define QISDA_PRODUCT_H20_4515                  0x4515
#define QISDA_PRODUCT_H20_4519                  0x4519

/* PANTECH PRODUCTS */
#define PANTECH_VENDOR_ID                       0x106c          // 0818 ASUS
#define PANTECH_PRODUCT_UM150                   0x3711
#define PANTECH_PRODUCT_UM175                   0x3714

/* SIERRA PRODUCTS */
#define SIERRA_VENDOR_ID                        0x1199          // 0818 ASUS
#define SIERRA_PRODUCT_U595                     0x0120          // 0818 ASUS
#define SIERRA_PRODUCT_U597                     0x0023          // 0818 ASUS
#define SIERRA_PRODUCT_U598                     0x0025          // 0818 ASUS

/* HAIER PRODUCTS */
#define HAIER_VENDOR_ID                         0x201e          // 0824 ASUS
#define HAIER_PRODUCT_CE100                     0x2009          // 0824 ASUS

static struct usb_device_id option_ids[] = {
	{ USB_DEVICE(OPTION_VENDOR_ID, OPTION_PRODUCT_COLT) },
	{ USB_DEVICE(OPTION_VENDOR_ID, OPTION_PRODUCT_RICOLA) },
	{ USB_DEVICE(OPTION_VENDOR_ID, OPTION_PRODUCT_RICOLA_LIGHT) },
	{ USB_DEVICE(OPTION_VENDOR_ID, OPTION_PRODUCT_RICOLA_QUAD) },
	{ USB_DEVICE(OPTION_VENDOR_ID, OPTION_PRODUCT_RICOLA_QUAD_LIGHT) },
	{ USB_DEVICE(OPTION_VENDOR_ID, OPTION_PRODUCT_RICOLA_NDIS) },
	{ USB_DEVICE(OPTION_VENDOR_ID, OPTION_PRODUCT_RICOLA_NDIS_LIGHT) },
	{ USB_DEVICE(OPTION_VENDOR_ID, OPTION_PRODUCT_RICOLA_NDIS_QUAD) },
	{ USB_DEVICE(OPTION_VENDOR_ID, OPTION_PRODUCT_RICOLA_NDIS_QUAD_LIGHT) },
	{ USB_DEVICE(OPTION_VENDOR_ID, OPTION_PRODUCT_COBRA) },
	{ USB_DEVICE(OPTION_VENDOR_ID, OPTION_PRODUCT_COBRA_BUS) },
	{ USB_DEVICE(OPTION_VENDOR_ID, OPTION_PRODUCT_VIPER) },
	{ USB_DEVICE(OPTION_VENDOR_ID, OPTION_PRODUCT_VIPER_BUS) },
	{ USB_DEVICE(OPTION_VENDOR_ID, OPTION_PRODUCT_GT_MAX_READY) },
	{ USB_DEVICE(OPTION_VENDOR_ID, OPTION_PRODUCT_GT_MAX) },
	{ USB_DEVICE(OPTION_VENDOR_ID, OPTION_PRODUCT_FUJI_MODEM_LIGHT) },
	{ USB_DEVICE(OPTION_VENDOR_ID, OPTION_PRODUCT_FUJI_MODEM_GT) },
	{ USB_DEVICE(OPTION_VENDOR_ID, OPTION_PRODUCT_FUJI_MODEM_EX) },
	{ USB_DEVICE(OPTION_VENDOR_ID, OPTION_PRODUCT_FUJI_NETWORK_LIGHT) },
	{ USB_DEVICE(OPTION_VENDOR_ID, OPTION_PRODUCT_FUJI_NETWORK_GT) },
	{ USB_DEVICE(OPTION_VENDOR_ID, OPTION_PRODUCT_FUJI_NETWORK_EX) },
	{ USB_DEVICE(OPTION_VENDOR_ID, OPTION_PRODUCT_KOI_MODEM) },
	{ USB_DEVICE(OPTION_VENDOR_ID, OPTION_PRODUCT_KOI_NETWORK) },
	{ USB_DEVICE(OPTION_VENDOR_ID, OPTION_PRODUCT_SCORPION_MODEM) },
	{ USB_DEVICE(OPTION_VENDOR_ID, OPTION_PRODUCT_SCORPION_NETWORK) },
	{ USB_DEVICE(OPTION_VENDOR_ID, OPTION_PRODUCT_ETNA_MODEM) },
	{ USB_DEVICE(OPTION_VENDOR_ID, OPTION_PRODUCT_ETNA_NETWORK) },
	{ USB_DEVICE(OPTION_VENDOR_ID, OPTION_PRODUCT_ETNA_MODEM_LITE) },
	{ USB_DEVICE(OPTION_VENDOR_ID, OPTION_PRODUCT_ETNA_MODEM_GT) },
	{ USB_DEVICE(OPTION_VENDOR_ID, OPTION_PRODUCT_ETNA_MODEM_EX) },
	{ USB_DEVICE(OPTION_VENDOR_ID, OPTION_PRODUCT_ETNA_NETWORK_LITE) },
	{ USB_DEVICE(OPTION_VENDOR_ID, OPTION_PRODUCT_ETNA_NETWORK_GT) },
	{ USB_DEVICE(OPTION_VENDOR_ID, OPTION_PRODUCT_ETNA_NETWORK_EX) },
	{ USB_DEVICE(OPTION_VENDOR_ID, OPTION_PRODUCT_ETNA_KOI_MODEM) },
	{ USB_DEVICE(OPTION_VENDOR_ID, OPTION_PRODUCT_ETNA_KOI_NETWORK) },
	{ USB_DEVICE(HUAWEI_VENDOR_ID, HUAWEI_PRODUCT_E600) },
	{ USB_DEVICE(HUAWEI_VENDOR_ID, HUAWEI_PRODUCT_E220) },
	{ USB_DEVICE(HUAWEI_VENDOR_ID, HUAWEI_PRODUCT_E220BIS) },
	{ USB_DEVICE(HUAWEI_VENDOR_ID, HUAWEI_PRODUCT_E1401) },
	{ USB_DEVICE(HUAWEI_VENDOR_ID, HUAWEI_PRODUCT_E1403) },
	{ USB_DEVICE(HUAWEI_VENDOR_ID, HUAWEI_PRODUCT_E1405) },
	{ USB_DEVICE(HUAWEI_VENDOR_ID, HUAWEI_PRODUCT_E1406) },
	{ USB_DEVICE(HUAWEI_VENDOR_ID, HUAWEI_PRODUCT_E1408) },
	{ USB_DEVICE(HUAWEI_VENDOR_ID, HUAWEI_PRODUCT_E1409) },
	{ USB_DEVICE(HUAWEI_VENDOR_ID, HUAWEI_PRODUCT_E1410) },
	{ USB_DEVICE(HUAWEI_VENDOR_ID, HUAWEI_PRODUCT_E1411) },
	{ USB_DEVICE(HUAWEI_VENDOR_ID, HUAWEI_PRODUCT_E1412) },
	{ USB_DEVICE(HUAWEI_VENDOR_ID, HUAWEI_PRODUCT_E1413) },
	{ USB_DEVICE(HUAWEI_VENDOR_ID, HUAWEI_PRODUCT_E1414) },
	{ USB_DEVICE(HUAWEI_VENDOR_ID, HUAWEI_PRODUCT_E1415) },
	{ USB_DEVICE(HUAWEI_VENDOR_ID, HUAWEI_PRODUCT_E1416) },
	{ USB_DEVICE(HUAWEI_VENDOR_ID, HUAWEI_PRODUCT_E1417) },
	{ USB_DEVICE(HUAWEI_VENDOR_ID, HUAWEI_PRODUCT_E1418) },
	{ USB_DEVICE(HUAWEI_VENDOR_ID, HUAWEI_PRODUCT_E1419) },
        { USB_DEVICE(HUAWEI_VENDOR_ID, HUAWEI_PRODUCT_E140B) },         /* 0701 ASUS */
        { USB_DEVICE(HUAWEI_VENDOR_ID, HUAWEI_PRODUCT_EC122) },         /* 0818 ASUS */
        { USB_DEVICE(HUAWEI_VENDOR_ID, HUAWEI_PRODUCT_E1820) },         /* 0818 ASUS */
        { USB_DEVICE(HUAWEI_VENDOR_ID, HUAWEI_PRODUCT_EC306) },         /* 0818 ASUS */
	{ USB_DEVICE(BANDLUXE_VENDOR_ID, BANDLUXE_PRODUCT_C270) },
	{ USB_DEVICE(DATANG_VENDOR_ID, DATANG_PRODUCT_M5731) },
	{ USB_DEVICE(ALCATEL_VENDOR_ID, ALCATEL_PRODUCT_C820) },
	{ USB_DEVICE(MU_VENDOR_ID, MU_PRODUCT_Q101) },
	{ USB_DEVICE(DLINK_VENDOR_ID, DLINK_PRODUCT_DWM652) },
	{ USB_DEVICE(MU_VENDOR_ID, MU_PRODUCT_Q101_MODEM) },
	{ USB_DEVICE(AMOI_VENDOR_ID, AMOI_PRODUCT_9508) },
	{ USB_DEVICE(NOVATELWIRELESS_VENDOR_ID, NOVATELWIRELESS_PRODUCT_V640) }, /* Novatel Merlin V640/XV620 */
	{ USB_DEVICE(NOVATELWIRELESS_VENDOR_ID, NOVATELWIRELESS_PRODUCT_V620) }, /* Novatel Merlin V620/S620 */
	{ USB_DEVICE(NOVATELWIRELESS_VENDOR_ID, NOVATELWIRELESS_PRODUCT_V740) }, /* Novatel Merlin EX720/V740/X720 */
	{ USB_DEVICE(NOVATELWIRELESS_VENDOR_ID, NOVATELWIRELESS_PRODUCT_V720) }, /* Novatel Merlin V720/S720/PC720 */
	{ USB_DEVICE(NOVATELWIRELESS_VENDOR_ID, NOVATELWIRELESS_PRODUCT_U730) }, /* Novatel U730/U740 (VF version) */
	{ USB_DEVICE(NOVATELWIRELESS_VENDOR_ID, NOVATELWIRELESS_PRODUCT_U740) }, /* Novatel U740 */
	{ USB_DEVICE(NOVATELWIRELESS_VENDOR_ID, NOVATELWIRELESS_PRODUCT_U870) }, /* Novatel U870 */
	{ USB_DEVICE(NOVATELWIRELESS_VENDOR_ID, NOVATELWIRELESS_PRODUCT_XU870) }, /* Novatel Merlin XU870 HSDPA/3G */
	{ USB_DEVICE(NOVATELWIRELESS_VENDOR_ID, NOVATELWIRELESS_PRODUCT_X950D) }, /* Novatel X950D */
	{ USB_DEVICE(NOVATELWIRELESS_VENDOR_ID, NOVATELWIRELESS_PRODUCT_EV620) }, /* Novatel EV620/ES620 CDMA/EV-DO */
	{ USB_DEVICE(NOVATELWIRELESS_VENDOR_ID, NOVATELWIRELESS_PRODUCT_ES720) }, /* Novatel ES620/ES720/U720/USB720 */
	{ USB_DEVICE(NOVATELWIRELESS_VENDOR_ID, NOVATELWIRELESS_PRODUCT_E725) }, /* Novatel E725/E726 */
	{ USB_DEVICE(NOVATELWIRELESS_VENDOR_ID, NOVATELWIRELESS_PRODUCT_ES620) }, /* Novatel Merlin ES620 SM Bus */
	{ USB_DEVICE(NOVATELWIRELESS_VENDOR_ID, NOVATELWIRELESS_PRODUCT_EU730) }, /* Novatel EU730 and Vodafone EU740 */
	{ USB_DEVICE(NOVATELWIRELESS_VENDOR_ID, NOVATELWIRELESS_PRODUCT_EU740) }, /* Novatel non-Vodafone EU740 */
	{ USB_DEVICE(NOVATELWIRELESS_VENDOR_ID, NOVATELWIRELESS_PRODUCT_EU870D) }, /* Novatel EU850D/EU860D/EU870D */
	{ USB_DEVICE(NOVATELWIRELESS_VENDOR_ID, NOVATELWIRELESS_PRODUCT_MC950D) }, /* Novatel MC930D/MC950D */
	{ USB_DEVICE(NOVATELWIRELESS_VENDOR_ID, NOVATELWIRELESS_PRODUCT_MC727) }, /* Novatel MC727/U727/USB727 */
	{ USB_DEVICE(NOVATELWIRELESS_VENDOR_ID, NOVATELWIRELESS_PRODUCT_EVDO_1) }, /* Novatel EVDO product */
	{ USB_DEVICE(NOVATELWIRELESS_VENDOR_ID, NOVATELWIRELESS_PRODUCT_HSPA_1) }, /* Novatel HSPA product */
	{ USB_DEVICE(NOVATELWIRELESS_VENDOR_ID, NOVATELWIRELESS_PRODUCT_EMBEDDED_1) }, /* Novatel Embedded product */
	{ USB_DEVICE(NOVATELWIRELESS_VENDOR_ID, NOVATELWIRELESS_PRODUCT_GLOBAL_1) }, /* Novatel Global product */
	{ USB_DEVICE(NOVATELWIRELESS_VENDOR_ID, NOVATELWIRELESS_PRODUCT_EVDO_2) }, /* Novatel EVDO product */
	{ USB_DEVICE(NOVATELWIRELESS_VENDOR_ID, NOVATELWIRELESS_PRODUCT_HSPA_2) }, /* Novatel HSPA product */
	{ USB_DEVICE(NOVATELWIRELESS_VENDOR_ID, NOVATELWIRELESS_PRODUCT_EMBEDDED_2) }, /* Novatel Embedded product */
	{ USB_DEVICE(NOVATELWIRELESS_VENDOR_ID, NOVATELWIRELESS_PRODUCT_GLOBAL_2) }, /* Novatel Global product */

	{ USB_DEVICE(AMOI_VENDOR_ID, AMOI_PRODUCT_H01) },
	{ USB_DEVICE(AMOI_VENDOR_ID, AMOI_PRODUCT_H01A) },
	{ USB_DEVICE(AMOI_VENDOR_ID, AMOI_PRODUCT_H02) },

	{ USB_DEVICE(DELL_VENDOR_ID, 0x8114) },	/* Dell Wireless 5700 Mobile Broadband CDMA/EVDO Mini-Card == Novatel Expedite EV620 CDMA/EV-DO */
	{ USB_DEVICE(DELL_VENDOR_ID, 0x8115) },	/* Dell Wireless 5500 Mobile Broadband HSDPA Mini-Card == Novatel Expedite EU740 HSDPA/3G */
	{ USB_DEVICE(DELL_VENDOR_ID, 0x8116) },	/* Dell Wireless 5505 Mobile Broadband HSDPA Mini-Card == Novatel Expedite EU740 HSDPA/3G */
	{ USB_DEVICE(DELL_VENDOR_ID, 0x8117) },	/* Dell Wireless 5700 Mobile Broadband CDMA/EVDO ExpressCard == Novatel Merlin XV620 CDMA/EV-DO */
	{ USB_DEVICE(DELL_VENDOR_ID, 0x8118) },	/* Dell Wireless 5510 Mobile Broadband HSDPA ExpressCard == Novatel Merlin XU870 HSDPA/3G */
	{ USB_DEVICE(DELL_VENDOR_ID, 0x8128) },	/* Dell Wireless 5700 Mobile Broadband CDMA/EVDO Mini-Card == Novatel Expedite E720 CDMA/EV-DO */
	{ USB_DEVICE(DELL_VENDOR_ID, 0x8129) },	/* Dell Wireless 5700 Mobile Broadband CDMA/EVDO Mini-Card == Novatel Expedite ET620 CDMA/EV-DO */
	{ USB_DEVICE(DELL_VENDOR_ID, 0x8133) }, /* Dell Wireless 5720 == Novatel EV620 CDMA/EV-DO */
	{ USB_DEVICE(DELL_VENDOR_ID, 0x8136) },	/* Dell Wireless HSDPA 5520 == Novatel Expedite EU860D */
	{ USB_DEVICE(DELL_VENDOR_ID, 0x8137) },	/* Dell Wireless HSDPA 5520 */
	{ USB_DEVICE(DELL_VENDOR_ID, 0x8138) },	/* Dell Wireless 5520 Voda I Mobile Broadband (3G HSDPA) Minicard */
	{ USB_DEVICE(ANYDATA_VENDOR_ID, ANYDATA_PRODUCT_ADU_E100A) },
	{ USB_DEVICE(ANYDATA_VENDOR_ID, ANYDATA_PRODUCT_ADU_500A) },
	{ USB_DEVICE(ANYDATA_VENDOR_ID, ANYDATA_PRODUCT_ADU_620UW) },
	{ USB_DEVICE(AXESSTEL_VENDOR_ID, AXESSTEL_PRODUCT_MV110H) },
	{ USB_DEVICE(ONDA_VENDOR_ID, ONDA_PRODUCT_MSA501HS) },
	{ USB_DEVICE(ONDA_VENDOR_ID, ONDA_PRODUCT_ET502HS) },
	{ USB_DEVICE(BANDRICH_VENDOR_ID, BANDRICH_PRODUCT_C100_1) },
	{ USB_DEVICE(BANDRICH_VENDOR_ID, BANDRICH_PRODUCT_C100_2) },
	{ USB_DEVICE(BANDRICH_VENDOR_ID, BANDRICH_PRODUCT_1004) },
	{ USB_DEVICE(BANDRICH_VENDOR_ID, BANDRICH_PRODUCT_1005) },
	{ USB_DEVICE(BANDRICH_VENDOR_ID, BANDRICH_PRODUCT_1006) },
	{ USB_DEVICE(BANDRICH_VENDOR_ID, BANDRICH_PRODUCT_1007) },
	{ USB_DEVICE(BANDRICH_VENDOR_ID, BANDRICH_PRODUCT_1008) },
	{ USB_DEVICE(BANDRICH_VENDOR_ID, BANDRICH_PRODUCT_1009) },
	{ USB_DEVICE(BANDRICH_VENDOR_ID, BANDRICH_PRODUCT_100A) },
	{ USB_DEVICE(BANDRICH_VENDOR_ID, BANDRICH_PRODUCT_100B) },
	{ USB_DEVICE(BANDRICH_VENDOR_ID, BANDRICH_PRODUCT_100C) },
	{ USB_DEVICE(BANDRICH_VENDOR_ID, BANDRICH_PRODUCT_100D) },
	{ USB_DEVICE(BANDRICH_VENDOR_ID, BANDRICH_PRODUCT_100E) },
	{ USB_DEVICE(BANDRICH_VENDOR_ID, BANDRICH_PRODUCT_100F) },
	{ USB_DEVICE(BANDRICH_VENDOR_ID, BANDRICH_PRODUCT_1010) },
	{ USB_DEVICE(BANDRICH_VENDOR_ID, BANDRICH_PRODUCT_1011) },
	{ USB_DEVICE(BANDRICH_VENDOR_ID, BANDRICH_PRODUCT_1012) },
	{ USB_DEVICE(KYOCERA_VENDOR_ID, KYOCERA_PRODUCT_KPC650) },
	{ USB_DEVICE(KYOCERA_VENDOR_ID, KYOCERA_PRODUCT_KPC680) },
	{ USB_DEVICE(QUALCOMM_VENDOR_ID, 0x6000)}, /* ZTE AC8700 */
	{ USB_DEVICE(QUALCOMM_VENDOR_ID, 0x6613)}, /* Onda H600/ZTE MF330 */
	{ USB_DEVICE(MAXON_VENDOR_ID, 0x6280) }, /* BP3-USB & BP3-EXT HSDPA */
	{ USB_DEVICE(TELIT_VENDOR_ID, TELIT_PRODUCT_UC864E) },
        { USB_DEVICE(ZTE_VENDOR_ID, ZTE_PRODUCT_MF628) },
        { USB_DEVICE(ZTE_VENDOR_ID, ZTE_PRODUCT_AC2746) },
        { USB_DEVICE(ZTE_VENDOR_ID, ZTE_PRODUCT_CDMA_TECH) },
        { USB_DEVICE(ZTE_VENDOR_ID, ZTE_PRODUCT_CDMA_TECH2) },
        { USB_DEVICE(ZTE_VENDOR_ID, ZTE_PRODUCT_CDMA_TECH3) },
        { USB_DEVICE(ZTE_VENDOR_ID, ZTE_PRODUCT_CDMA_TECH4) },
        { USB_DEVICE(QISDA_VENDOR_ID, QISDA_PRODUCT_H21_4512) },
        { USB_DEVICE(QISDA_VENDOR_ID, QISDA_PRODUCT_H21_4523) },
        { USB_DEVICE(QISDA_VENDOR_ID, QISDA_PRODUCT_H20_4515) },
        { USB_DEVICE(QISDA_VENDOR_ID, QISDA_PRODUCT_H20_4519) },
        { USB_DEVICE(PANTECH_VENDOR_ID, PANTECH_PRODUCT_UM150) },       /* 0818 ASUS */
        { USB_DEVICE(PANTECH_VENDOR_ID, PANTECH_PRODUCT_UM175) },       /* 0818 ASUS */
        { USB_DEVICE(SIERRA_VENDOR_ID, SIERRA_PRODUCT_U595) },          /* 0818 ASUS */
        { USB_DEVICE(SIERRA_VENDOR_ID, SIERRA_PRODUCT_U597) },          /* 0818 ASUS */
        { USB_DEVICE(SIERRA_VENDOR_ID, SIERRA_PRODUCT_U598) },          /* 0818 ASUS */
        { USB_DEVICE(HAIER_VENDOR_ID, HAIER_PRODUCT_CE100) },           /* 0824 ASUS */
	{ } /* Terminating entry */
};
MODULE_DEVICE_TABLE(usb, option_ids);

static struct usb_driver option_driver = {
	.name       = "option",
	.probe      = usb_serial_probe,
	.disconnect = usb_serial_disconnect,
	.id_table   = option_ids,
	.no_dynamic_id = 	1,
};

/* The card has three separate interfaces, which the serial driver
 * recognizes separately, thus num_port=1.
 */

static struct usb_serial_driver option_1port_device = {
	.driver = {
		.owner =	THIS_MODULE,
		.name =		"option1",
	},
	.description       = "GSM modem (1-port)",
	.usb_driver        = &option_driver,
	.id_table          = option_ids,
	.num_interrupt_in  = NUM_DONT_CARE,
	.num_bulk_in       = NUM_DONT_CARE,
	.num_bulk_out      = NUM_DONT_CARE,
	.num_ports         = 1,
	.open              = option_open,
	.close             = option_close,
	.write             = option_write,
	.write_room        = option_write_room,
	.chars_in_buffer   = option_chars_in_buffer,
	.throttle          = option_rx_throttle,
	.unthrottle        = option_rx_unthrottle,
	.ioctl             = option_ioctl,
	.set_termios       = option_set_termios,
	.break_ctl         = option_break_ctl,
	.tiocmget          = option_tiocmget,
	.tiocmset          = option_tiocmset,
	.attach            = option_startup,
	.shutdown          = option_shutdown,
	.read_int_callback = option_instat_callback,
};

#ifdef CONFIG_USB_DEBUG
static int debug;
#else
#define debug 0
#endif

/* per port private data */

#define N_IN_URB 4
#define N_OUT_URB 1
#define IN_BUFLEN 4096
#define OUT_BUFLEN 128

struct option_port_private {
	/* Input endpoints and buffer for this port */
	struct urb *in_urbs[N_IN_URB];
	char in_buffer[N_IN_URB][IN_BUFLEN];
	/* Output endpoints and buffer for this port */
	struct urb *out_urbs[N_OUT_URB];
	char out_buffer[N_OUT_URB][OUT_BUFLEN];

	/* Settings for the port */
	int rts_state;	/* Handshaking pins (outputs) */
	int dtr_state;
	int cts_state;	/* Handshaking pins (inputs) */
	int dsr_state;
	int dcd_state;
	int ri_state;

	unsigned long tx_start_time[N_OUT_URB];
};

/* Functions used by new usb-serial code. */
static int __init option_init(void)
{
	int retval;
	retval = usb_serial_register(&option_1port_device);
	if (retval)
		goto failed_1port_device_register;
	retval = usb_register(&option_driver);
	if (retval)
		goto failed_driver_register;

	info(DRIVER_DESC ": " DRIVER_VERSION);

	return 0;

failed_driver_register:
	usb_serial_deregister (&option_1port_device);
failed_1port_device_register:
	return retval;
}

static void __exit option_exit(void)
{
	usb_deregister (&option_driver);
	usb_serial_deregister (&option_1port_device);
}

module_init(option_init);
module_exit(option_exit);

static void option_rx_throttle(struct usb_serial_port *port)
{
	dbg("%s", __FUNCTION__);
}

static void option_rx_unthrottle(struct usb_serial_port *port)
{
	dbg("%s", __FUNCTION__);
}

static void option_break_ctl(struct usb_serial_port *port, int break_state)
{
	/* Unfortunately, I don't know how to send a break */
	dbg("%s", __FUNCTION__);
}

static void option_set_termios(struct usb_serial_port *port,
			struct ktermios *old_termios)
{
	dbg("%s", __FUNCTION__);

	option_send_setup(port);
}

static int option_tiocmget(struct usb_serial_port *port, struct file *file)
{
	unsigned int value;
	struct option_port_private *portdata;

	portdata = usb_get_serial_port_data(port);

	value = ((portdata->rts_state) ? TIOCM_RTS : 0) |
		((portdata->dtr_state) ? TIOCM_DTR : 0) |
		((portdata->cts_state) ? TIOCM_CTS : 0) |
		((portdata->dsr_state) ? TIOCM_DSR : 0) |
		((portdata->dcd_state) ? TIOCM_CAR : 0) |
		((portdata->ri_state) ? TIOCM_RNG : 0);

	return value;
}

static int option_tiocmset(struct usb_serial_port *port, struct file *file,
			unsigned int set, unsigned int clear)
{
	struct option_port_private *portdata;

	portdata = usb_get_serial_port_data(port);

	if (set & TIOCM_RTS)
		portdata->rts_state = 1;
	if (set & TIOCM_DTR)
		portdata->dtr_state = 1;

	if (clear & TIOCM_RTS)
		portdata->rts_state = 0;
	if (clear & TIOCM_DTR)
		portdata->dtr_state = 0;
	return option_send_setup(port);
}

static int option_ioctl(struct usb_serial_port *port, struct file *file,
			unsigned int cmd, unsigned long arg)
{
	return -ENOIOCTLCMD;
}

/* Write */
static int option_write(struct usb_serial_port *port,
			const unsigned char *buf, int count)
{
	struct option_port_private *portdata;
	int i;
	int left, todo;
	struct urb *this_urb = NULL; /* spurious */
	int err;

	portdata = usb_get_serial_port_data(port);

	dbg("%s: write (%d chars)", __FUNCTION__, count);

	i = 0;
	left = count;
	for (i=0; left > 0 && i < N_OUT_URB; i++) {
		todo = left;
		if (todo > OUT_BUFLEN)
			todo = OUT_BUFLEN;

		this_urb = portdata->out_urbs[i];
		if (this_urb->status == -EINPROGRESS) {
			if (time_before(jiffies,
					portdata->tx_start_time[i] + 10 * HZ))
				continue;
			usb_unlink_urb(this_urb);
			continue;
		}
		if (this_urb->status != 0)
			dbg("usb_write %p failed (err=%d)",
				this_urb, this_urb->status);

		dbg("%s: endpoint %d buf %d", __FUNCTION__,
			usb_pipeendpoint(this_urb->pipe), i);

		/* send the data */
		memcpy (this_urb->transfer_buffer, buf, todo);
		this_urb->transfer_buffer_length = todo;

		this_urb->dev = port->serial->dev;
		err = usb_submit_urb(this_urb, GFP_ATOMIC);
		if (err) {
			dbg("usb_submit_urb %p (write bulk) failed "
				"(%d, has %d)", this_urb,
				err, this_urb->status);
			continue;
		}
		portdata->tx_start_time[i] = jiffies;
		buf += todo;
		left -= todo;
	}

	count -= left;
	dbg("%s: wrote (did %d)", __FUNCTION__, count);
	return count;
}

static void option_indat_callback(struct urb *urb)
{
	int err;
	int endpoint;
	struct usb_serial_port *port;
	struct tty_struct *tty;
	unsigned char *data = urb->transfer_buffer;

	dbg("%s: %p", __FUNCTION__, urb);

	endpoint = usb_pipeendpoint(urb->pipe);
	port = (struct usb_serial_port *) urb->context;

	if (urb->status) {
		dbg("%s: nonzero status: %d on endpoint %02x.",
		    __FUNCTION__, urb->status, endpoint);
	} else {
		tty = port->tty;
		if (urb->actual_length) {
			tty_buffer_request_room(tty, urb->actual_length);
			tty_insert_flip_string(tty, data, urb->actual_length);
			tty_flip_buffer_push(tty);
		} else {
			dbg("%s: empty read urb received", __FUNCTION__);
		}

		/* Resubmit urb so we continue receiving */
		if (port->open_count && urb->status != -ESHUTDOWN) {
			err = usb_submit_urb(urb, GFP_ATOMIC);
			if (err)
				printk(KERN_ERR "%s: resubmit read urb failed. "
					"(%d)", __FUNCTION__, err);
		}
	}
	return;
}

static void option_outdat_callback(struct urb *urb)
{
	struct usb_serial_port *port;

	dbg("%s", __FUNCTION__);

	port = (struct usb_serial_port *) urb->context;

	usb_serial_port_softint(port);
}

static void option_instat_callback(struct urb *urb)
{
	int err;
	struct usb_serial_port *port = (struct usb_serial_port *) urb->context;
	struct option_port_private *portdata = usb_get_serial_port_data(port);
	struct usb_serial *serial = port->serial;

	dbg("%s", __FUNCTION__);
	dbg("%s: urb %p port %p has data %p", __FUNCTION__,urb,port,portdata);

	if (urb->status == 0) {
		struct usb_ctrlrequest *req_pkt =
				(struct usb_ctrlrequest *)urb->transfer_buffer;

		if (!req_pkt) {
			dbg("%s: NULL req_pkt\n", __FUNCTION__);
			return;
		}
		if ((req_pkt->bRequestType == 0xA1) &&
				(req_pkt->bRequest == 0x20)) {
			int old_dcd_state;
			unsigned char signals = *((unsigned char *)
					urb->transfer_buffer +
					sizeof(struct usb_ctrlrequest));

			dbg("%s: signal x%x", __FUNCTION__, signals);

			old_dcd_state = portdata->dcd_state;
			portdata->cts_state = 1;
			portdata->dcd_state = ((signals & 0x01) ? 1 : 0);
			portdata->dsr_state = ((signals & 0x02) ? 1 : 0);
			portdata->ri_state = ((signals & 0x08) ? 1 : 0);

			if (port->tty && !C_CLOCAL(port->tty) &&
					old_dcd_state && !portdata->dcd_state)
				tty_hangup(port->tty);
		} else {
			dbg("%s: type %x req %x", __FUNCTION__,
				req_pkt->bRequestType,req_pkt->bRequest);
		}
	} else
		dbg("%s: error %d", __FUNCTION__, urb->status);

	/* Resubmit urb so we continue receiving IRQ data */
	if (urb->status != -ESHUTDOWN) {
		urb->dev = serial->dev;
		err = usb_submit_urb(urb, GFP_ATOMIC);
		if (err)
			dbg("%s: resubmit intr urb failed. (%d)",
				__FUNCTION__, err);
	}
}

static int option_write_room(struct usb_serial_port *port)
{
	struct option_port_private *portdata;
	int i;
	int data_len = 0;
	struct urb *this_urb;

	portdata = usb_get_serial_port_data(port);

	for (i=0; i < N_OUT_URB; i++) {
		this_urb = portdata->out_urbs[i];
		if (this_urb && this_urb->status != -EINPROGRESS)
			data_len += OUT_BUFLEN;
	}

	dbg("%s: %d", __FUNCTION__, data_len);
	return data_len;
}

static int option_chars_in_buffer(struct usb_serial_port *port)
{
	struct option_port_private *portdata;
	int i;
	int data_len = 0;
	struct urb *this_urb;

	portdata = usb_get_serial_port_data(port);

	for (i=0; i < N_OUT_URB; i++) {
		this_urb = portdata->out_urbs[i];
		if (this_urb && this_urb->status == -EINPROGRESS)
			data_len += this_urb->transfer_buffer_length;
	}
	dbg("%s: %d", __FUNCTION__, data_len);
	return data_len;
}

static int option_open(struct usb_serial_port *port, struct file *filp)
{
	struct option_port_private *portdata;
	struct usb_serial *serial = port->serial;
	int i, err;
	struct urb *urb;

	portdata = usb_get_serial_port_data(port);

	dbg("%s", __FUNCTION__);

	/* Set some sane defaults */
	portdata->rts_state = 1;
	portdata->dtr_state = 1;

	/* Reset low level data toggle and start reading from endpoints */
	for (i = 0; i < N_IN_URB; i++) {
		urb = portdata->in_urbs[i];
		if (! urb)
			continue;
		if (urb->dev != serial->dev) {
			dbg("%s: dev %p != %p", __FUNCTION__,
				urb->dev, serial->dev);
			continue;
		}

		/*
		 * make sure endpoint data toggle is synchronized with the
		 * device
		 */
		usb_clear_halt(urb->dev, urb->pipe);

		err = usb_submit_urb(urb, GFP_KERNEL);
		if (err) {
			dbg("%s: submit urb %d failed (%d) %d",
				__FUNCTION__, i, err,
				urb->transfer_buffer_length);
		}
	}

	/* Reset low level data toggle on out endpoints */
	for (i = 0; i < N_OUT_URB; i++) {
		urb = portdata->out_urbs[i];
		if (! urb)
			continue;
		urb->dev = serial->dev;
		/* usb_settoggle(urb->dev, usb_pipeendpoint(urb->pipe),
				usb_pipeout(urb->pipe), 0); */
	}

	port->tty->low_latency = 1;
	option_send_setup(port);

	return (0);
}

static inline void stop_urb(struct urb *urb)
{
	if (urb && urb->status == -EINPROGRESS)
		usb_kill_urb(urb);
}

static void option_close(struct usb_serial_port *port, struct file *filp)
{
	int i;
	struct usb_serial *serial = port->serial;
	struct option_port_private *portdata;

	dbg("%s", __FUNCTION__);
	portdata = usb_get_serial_port_data(port);

	portdata->rts_state = 0;
	portdata->dtr_state = 0;

	if (serial->dev) {
		option_send_setup(port);

		/* Stop reading/writing urbs */
		for (i = 0; i < N_IN_URB; i++)
			stop_urb(portdata->in_urbs[i]);
		for (i = 0; i < N_OUT_URB; i++)
			stop_urb(portdata->out_urbs[i]);
	}
	port->tty = NULL;
}

/* Helper functions used by option_setup_urbs */
static struct urb *option_setup_urb(struct usb_serial *serial, int endpoint,
		int dir, void *ctx, char *buf, int len,
		void (*callback)(struct urb *))
{
	struct urb *urb;

	if (endpoint == -1)
		return NULL;		/* endpoint not needed */

	urb = usb_alloc_urb(0, GFP_KERNEL);		/* No ISO */
	if (urb == NULL) {
		dbg("%s: alloc for endpoint %d failed.", __FUNCTION__, endpoint);
		return NULL;
	}

		/* Fill URB using supplied data. */
	usb_fill_bulk_urb(urb, serial->dev,
		      usb_sndbulkpipe(serial->dev, endpoint) | dir,
		      buf, len, callback, ctx);

	return urb;
}

/* Setup urbs */
static void option_setup_urbs(struct usb_serial *serial)
{
	int i,j;
	struct usb_serial_port *port;
	struct option_port_private *portdata;

	dbg("%s", __FUNCTION__);

	for (i = 0; i < serial->num_ports; i++) {
		port = serial->port[i];
		portdata = usb_get_serial_port_data(port);

	/* Do indat endpoints first */
		for (j = 0; j < N_IN_URB; ++j) {
			portdata->in_urbs[j] = option_setup_urb (serial,
                  	port->bulk_in_endpointAddress, USB_DIR_IN, port,
                  	portdata->in_buffer[j], IN_BUFLEN, option_indat_callback);
		}

		/* outdat endpoints */
		for (j = 0; j < N_OUT_URB; ++j) {
			portdata->out_urbs[j] = option_setup_urb (serial,
                  	port->bulk_out_endpointAddress, USB_DIR_OUT, port,
                  	portdata->out_buffer[j], OUT_BUFLEN, option_outdat_callback);
		}
	}
}

static int option_send_setup(struct usb_serial_port *port)
{
	struct usb_serial *serial = port->serial;
	struct option_port_private *portdata;

	dbg("%s", __FUNCTION__);

	if (port->number != 0)
		return 0;

	portdata = usb_get_serial_port_data(port);

	if (port->tty) {
		int val = 0;
		if (portdata->dtr_state)
			val |= 0x01;
		if (portdata->rts_state)
			val |= 0x02;

		return usb_control_msg(serial->dev,
				usb_rcvctrlpipe(serial->dev, 0),
				0x22,0x21,val,0,NULL,0,USB_CTRL_SET_TIMEOUT);
	}

	return 0;
}

static int option_startup(struct usb_serial *serial)
{
	int i, err;
	struct usb_serial_port *port;
	struct option_port_private *portdata;

	dbg("%s", __FUNCTION__);

	/* Now setup per port private data */
	for (i = 0; i < serial->num_ports; i++) {
		port = serial->port[i];
		portdata = kzalloc(sizeof(*portdata), GFP_KERNEL);
		if (!portdata) {
			dbg("%s: kmalloc for option_port_private (%d) failed!.",
					__FUNCTION__, i);
			return (1);
		}

		usb_set_serial_port_data(port, portdata);

		if (! port->interrupt_in_urb)
			continue;
		err = usb_submit_urb(port->interrupt_in_urb, GFP_KERNEL);
		if (err)
			dbg("%s: submit irq_in urb failed %d",
				__FUNCTION__, err);
	}

	option_setup_urbs(serial);

	return (0);
}

static void option_shutdown(struct usb_serial *serial)
{
	int i, j;
	struct usb_serial_port *port;
	struct option_port_private *portdata;

	dbg("%s", __FUNCTION__);

	/* Stop reading/writing urbs */
	for (i = 0; i < serial->num_ports; ++i) {
		port = serial->port[i];
		portdata = usb_get_serial_port_data(port);
		for (j = 0; j < N_IN_URB; j++)
			stop_urb(portdata->in_urbs[j]);
		for (j = 0; j < N_OUT_URB; j++)
			stop_urb(portdata->out_urbs[j]);
	}

	/* Now free them */
	for (i = 0; i < serial->num_ports; ++i) {
		port = serial->port[i];
		portdata = usb_get_serial_port_data(port);

		for (j = 0; j < N_IN_URB; j++) {
			if (portdata->in_urbs[j]) {
				usb_free_urb(portdata->in_urbs[j]);
				portdata->in_urbs[j] = NULL;
			}
		}
		for (j = 0; j < N_OUT_URB; j++) {
			if (portdata->out_urbs[j]) {
				usb_free_urb(portdata->out_urbs[j]);
				portdata->out_urbs[j] = NULL;
			}
		}
	}

	/* Now free per port private data */
	for (i = 0; i < serial->num_ports; i++) {
		port = serial->port[i];
		kfree(usb_get_serial_port_data(port));
	}
}

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_VERSION(DRIVER_VERSION);
MODULE_LICENSE("GPL");

#ifdef CONFIG_USB_DEBUG
module_param(debug, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(debug, "Debug messages");
#endif

