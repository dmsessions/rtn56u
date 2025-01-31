/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 *
 * Copyright 2004, ASUSTeK Inc.
 * All Rights Reserved.
 * 
 * THIS SOFTWARE IS OFFERED "AS IS", AND ASUS GRANTS NO WARRANTIES OF ANY
 * KIND, EXPRESS OR IMPLIED, BY STATUTE, COMMUNICATION OR OTHERWISE. BROADCOM
 * SPECIFICALLY DISCLAIMS ANY IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A SPECIFIC PURPOSE OR NONINFRINGEMENT CONCERNING THIS SOFTWARE.
 *
 * $Id: common_ex.c,v 1.3 2007/03/29 06:02:23 shinjung Exp $
 */


#include<stdlib.h>
#include<stdio.h>
#include<time.h>
#include<signal.h>
#include<nvram/bcmnvram.h>
#include<shutils.h>
#include<netconf.h>
typedef unsigned char   bool;
#include<wlioctl.h>
#include<sys/time.h>
#include<syslog.h>
#include<stdarg.h>
#include <arpa/inet.h>	// oleg patch
#include <string.h>	// oleg patch
#include "rc.h"	// oleg patch

#include <ralink.h>
#include <flash_mtd.h>

//#if 0
#define XSTR(s) STR(s)
#define STR(s) #s

extern in_addr_t inet_addr_(const char *cp);    // oleg patch

void update_lan_status(int);
static char list[2048];
//#endif

// oleg patch ~
in_addr_t
inet_addr_(const char *cp)
{
       struct in_addr a;

       if (!inet_aton(cp, &a))
	       return INADDR_ANY;
       else
	       return a.s_addr;
}
// ~ oleg patch

/* remove space in the end of string */
char *trim_r(char *str)
{
	int i;

	i=strlen(str);

	while (i>=1)
	{
		if (*(str+i-1) == ' ' || *(str+i-1) == 0x0a || *(str+i-1) == 0x0d) *(str+i-1)=0x0;
		else break;
		i--;
	}
	return (str);
}

/* convert mac address format from XXXXXXXXXXXX to XX:XX:XX:XX:XX:XX */
char *mac_conv(char *mac_name, int idx, char *buf)
{
	char *mac, name[32];
	int i, j;

	if (idx!=-1)	
		sprintf(name, "%s%d", mac_name, idx);
	else sprintf(name, "%s", mac_name);

	mac = nvram_safe_get(name);

	if (strlen(mac)==0) 
	{
		buf[0] = 0;
	}
	else
	{
		j=0;	
		for (i=0; i<12; i++)
		{		
			if (i!=0&&i%2==0) buf[j++] = ':';
			buf[j++] = mac[i];
		}
		buf[j] = 0;	// oleg patch
	}
	//buf[j] = 0;

	dprintf("mac: %s\n", buf);

	return (buf);
}

// 2010.09 James. {
/* convert mac address format from XX:XX:XX:XX:XX:XX to XXXXXXXXXXXX */
char *mac_conv2(char *mac_name, int idx, char *buf)
{
	char *mac, name[32];
	int i, j;

	if(idx != -1)	
		sprintf(name, "%s%d", mac_name, idx);
	else
		sprintf(name, "%s", mac_name);

	mac = nvram_safe_get(name);

	if(strlen(mac) == 0 || strlen(mac) != 17)
		buf[0] = 0;
	else{
		for(i = 0, j = 0; i < 17; ++i){
			if(i%3 != 2){
				buf[j] = mac[i];
				++j;
			}

			buf[j] = 0;
		}
	}

	return(buf);
}
// 2010.09 James. }

int valid_subver(char subfs)
{
	printf("validate subfs: %c\n", subfs);	// tmp test
	if(((subfs >= 'a') && (subfs <= 'z' )) || ((subfs >= 'A') && (subfs <= 'Z' )))
		return 1;
	else
		return 0;
}

void getsyspara(void)
{
	unsigned char buffer[32];
	unsigned int *src;
	unsigned int *dst;
	unsigned int bytes;
	int i;
	char macaddr[]="00:11:22:33:44:55";
	char macaddr2[]="00:11:22:33:44:56";
	char macaddr3[]="001122334457";
	char macaddr4[]="001122334458";
	char country_code[3];
	char pin[9];
	char productid[13];
	char fwver[8], fwver_sub[8];
	char blver[20];
	unsigned char txbf_para[33];

	/* /dev/mtd/2, RF parameters, starts from 0x40000 */
	dst = (unsigned int *)buffer;
	bytes = 6;
	memset(buffer, 0, sizeof(buffer));
	memset(country_code, 0, sizeof(country_code));
	memset(pin, 0, sizeof(pin));
	memset(productid, 0, sizeof(productid));
	memset(fwver, 0, sizeof(fwver));
	memset(fwver_sub, 0, sizeof(fwver_sub));
	memset(txbf_para, 0, sizeof(txbf_para));

	if (FRead(dst, OFFSET_MAC_ADDR, bytes)<0)
	{
		dbg("READ MAC address: Out of scope\n");
	}
	else
	{
		if (buffer[0]!=0xff)
		{
			ether_etoa(buffer, macaddr);
			ether_etoa2(buffer, macaddr3);
		}
	}

	if (FRead(dst, OFFSET_MAC_ADDR_2G, bytes)<0)
	{
		dbg("READ MAC address 2G: Out of scope\n");
	}
	else
	{
		if (buffer[0]!=0xff)
		{
			ether_etoa(buffer, macaddr2);
			ether_etoa2(buffer, macaddr4);
		}
	}

	nvram_set("il0macaddr", macaddr);
	nvram_set("il1macaddr", macaddr2);
	nvram_set("et0macaddr", macaddr);
	nvram_set("br0hexaddr", macaddr3);
	nvram_set("wanhexaddr", macaddr4);

	/* reserved for Ralink. used as ASUS country code. */
	dst = (unsigned int *)country_code;
	bytes = 2;
	if (FRead(dst, OFFSET_COUNTRY_CODE, bytes)<0)
	{
		dbg("READ ASUS country code: Out of scope\n");
		nvram_set("wl_country_code", "");
	}
	else
	{
		if ((unsigned char)country_code[0]!=0xff)
			nvram_set("wl_country_code", country_code);
		else
			nvram_set("wl_country_code", "DB");

		if (!strcasecmp(nvram_safe_get("wl_country_code"), "BR"))
			nvram_set("wl_country_code", "UZ");

		if (nvram_match("wl_country_code", "HK") && nvram_match("preferred_lang", ""))
			nvram_set("preferred_lang", "TW");
	}

	/* reserved for Ralink. used as ASUS pin code. */
	dst = (unsigned int *)pin;
	bytes = 8;
	if (FRead(dst, OFFSET_PIN_CODE, bytes)<0)
	{
		dbg("READ ASUS pin code: Out of scope\n");
		nvram_set("wl_pin_code", "");
	}
	else
	{
		if ((unsigned char)pin[0]!=0xff)
			nvram_set("secret_code", pin);
		else
			nvram_set("secret_code", "12345670");
	}

	src = 0x50020;  /* /dev/mtd/3, firmware, starts from 0x50000 */
	dst = (unsigned int *)buffer;
	bytes = sizeof(buffer);
	printf("Fread %d bytes \n", bytes);	// tmp test
	if (FRead(dst, src, bytes)<0)
	{
		dbg("READ firmware header: Out of scope\n");
		nvram_set("productid", "unknown");
		nvram_set("firmver", "unknown");
	}
	else
	{
		strncpy(productid, buffer + 4, 12);
		productid[12] = 0;
		if(valid_subver(buffer[27]))
			sprintf(fwver_sub, "%d.%d.%d.%d%c", buffer[0], buffer[1], buffer[2], buffer[3], buffer[27]);
		else
			sprintf(fwver_sub, "%d.%d.%d.%d", buffer[0], buffer[1], buffer[2], buffer[3]);
		
		sprintf(fwver, "%d.%d.%d.%d", buffer[0], buffer[1], buffer[2], buffer[3]);
		nvram_set("productid", trim_r(productid));
		nvram_set("firmver", trim_r(fwver));
		nvram_set("firmver_sub", trim_r(fwver_sub));
	}

        memset(buffer, 0, sizeof(buffer));
        FRead(buffer, OFFSET_BOOT_VER, 4);
//	sprintf(blver, "%c.%c.%c.%c", buffer[0], buffer[1], buffer[2], buffer[3]);
	sprintf(blver, "%s-0%c-0%c-0%c-0%c", trim_r(productid), buffer[0], buffer[1], buffer[2], buffer[3]);
	nvram_set("blver", trim_r(blver));

	dbg("bootloader version: %s\n", nvram_safe_get("blver"));
	dbg("firmware version: %s\n", nvram_safe_get("firmver"));

	dst = (unsigned int *)txbf_para;
	int count_0xff = 0;
	if (FRead(dst, OFFSET_TXBF_PARA, 33) < 0)
	{
		dbg("READ TXBF PARA address: Out of scope\n");
	}
	else
	{
		for (i = 0; i < 33; i++)
		{
			if (txbf_para[i] == 0xff)
				count_0xff++;
/*
			if ((i % 16) == 0) dbg("\n");
			dbg("%02x ", (unsigned char) txbf_para[i]);
*/
		}
/*
		dbg("\n");

		dbg("TxBF parameter 0xFF count: %d\n", count_0xff);
*/
	}

	if (count_0xff == 33)
		nvram_set("wl_txbf_en", "0");
	else
		nvram_set("wl_txbf_en", "1");
}

//#if 0
void wan_netmask_check(void)
{
	unsigned int ip, gw, nm, lip, lnm;

	if (nvram_match("wan0_proto", "static") ||
	    //nvram_match("wan0_proto", "pptp"))
	    nvram_match("wan0_proto", "pptp") || nvram_match("wan0_proto", "l2tp"))	// oleg patch
	{
		ip = inet_addr(nvram_safe_get("wan_ipaddr"));
		gw = inet_addr(nvram_safe_get("wan_gateway"));
		nm = inet_addr(nvram_safe_get("wan_netmask"));

		lip = inet_addr(nvram_safe_get("lan_ipaddr"));
		lnm = inet_addr(nvram_safe_get("lan_netmask"));

		dprintf("ip : %x %x %x\n", ip, gw, nm);

		if (ip==0x0 && (nvram_match("wan0_proto", "pptp") || nvram_match("wan0_proto", "l2tp")))	// oleg patch
			return;

		if (ip==0x0 || (ip&lnm)==(lip&lnm))
		{
			nvram_set("wan_ipaddr", "1.1.1.1");
			nvram_set("wan_netmask", "255.0.0.0");	
			nvram_set("wan0_ipaddr", nvram_safe_get("wan_ipaddr"));
			nvram_set("wan0_netmask", nvram_safe_get("wan_netmask"));
		}

		// check netmask here
		if (gw==0 || gw==0xffffffff || (ip&nm)==(gw&nm))
		{
			nvram_set("wan0_netmask", nvram_safe_get("wan_netmask"));
		}
		else
		{		
			for (nm=0xffffffff;nm!=0;nm=(nm>>8))
			{
				if ((ip&nm)==(gw&nm)) break;
			}

			dprintf("nm: %x\n", nm);

			if (nm==0xffffffff) nvram_set("wan0_netmask", "255.255.255.255");
			else if (nm==0xffffff) nvram_set("wan0_netmask", "255.255.255.0");
			else if (nm==0xffff) nvram_set("wan0_netmask", "255.255.0.0");
			else if (nm==0xff) nvram_set("wan0_netmask", "255.0.0.0");
			else nvram_set("wan0_netmask", "0.0.0.0");
		}

		nvram_set("wanx_ipaddr", nvram_safe_get("wan0_ipaddr"));	// oleg patch, he suggests to mark the following 3 lines
		nvram_set("wanx_netmask", nvram_safe_get("wan0_netmask"));
		nvram_set("wanx_gateway", nvram_safe_get("wan0_gateway"));
	}
}

void init_switch_mode()
{
//	sw_mode_check();					// save switch mode into nvram name sw_mode
//	nvram_set("sw_mode_ex", nvram_safe_get("sw_mode"));	// save working switch mode into nvram name sw_mode_ex

	if (!nvram_get("sw_mode"))
		nvram_set("sw_mode", "1");
	nvram_set("sw_mode_ex", nvram_safe_get("sw_mode"));

	if (nvram_match("sw_mode_ex", "1"))			// Gateway mode
	{
		nvram_set("wan_nat_x", "1");
		nvram_set("wan_route_x", "IP_Routed");
		nvram_set("wl_mode_ex", "ap");
	}
	else if (nvram_match("sw_mode_ex", "4"))		// Router mode
	{
		nvram_set("wan_nat_x", "0");
		nvram_set("wan_route_x", "IP_Routed");
		nvram_set("wl_mode_ex", "ap");
	}
/*
	else if (nvram_match("sw_mode_ex", "2"))		// Repeater mode
	{
		nvram_set("wan_nat_x", "0");
		nvram_set("wan_route_x", "IP_Bridged");
		nvram_set("wl_mode_ex", "re");
	}
*/
	else if (nvram_match("sw_mode_ex", "3"))		// AP mode
	{
		nvram_set("wan_nat_x", "0");
		nvram_set("wan_route_x", "IP_Bridged");
		nvram_set("wl_mode_ex", "ap");
	}
	else
	{
		nvram_set("sw_mode", "1");
		nvram_set("sw_mode_ex", "1");
		nvram_set("wan_nat_x", "1");
		nvram_set("wan_route_x", "IP_Routed");
		nvram_set("wl_mode_ex", "ap");
	}
}

char usb_path1[16];
char usb_path1_old[16];
char usb_path2[16];
char usb_path2_old[16];

/* This function is used to map nvram value from asus to Broadcom */
void convert_asus_values(int skipflag)
{	
	char tmpstr[32], tmpstr1[32], macbuf[36];
	char servers[64];
	char ifnames[36];
	char sbuf[64];
	char nvram_name[32];
	int i, j, num;
	char *ptr;
	FILE *fp;
// 2008.09 magic  {
	
	nvram_set("success_start_service", "0");	// 2008.05 James. For judging if the system is ready.
	
	nvram_unset("manually_disconnect_wan");	// 2008.07 James.

	if (nvram_match("macfilter_enable_x", "disabled"))
		nvram_set("macfilter_enable_x", "0");
	
	if (nvram_match("wl_frameburst", "0"))
		nvram_set("wl_frameburst", "off");
	
	if (nvram_match("wl_preauth", "1"))
		nvram_set("wl_preauth", "enabled");
	else if (nvram_match("wl_preauth", "0"))
		nvram_set("wl_preauth", "disabled");
	// Fixed the wrong value of nvram. }
/*	
	char *ddns_hostname_x = nvram_safe_get("ddns_hostname_x");
	char *follow_info = strchr(ddns_hostname_x, '.');
	char ddns_account[64];
	int len = 0;

	if (nvram_match("ddns_enable_x", "1") && nvram_match("ddns_server_x", "WWW.ASUS.COM")
			&& follow_info != NULL && !strcmp(follow_info, ".asuscomm.com")) {
		len = strlen(ddns_hostname_x)-strlen(follow_info);
		
		memset(ddns_account, 0, 64);
		strncpy(ddns_account, ddns_hostname_x, len);
		
		nvram_set("computer_name", ddns_account);
		nvram_set("computer_nameb", ddns_account);
	}
*/
// 2008.09 magic }

#ifndef CDMA
	if (nvram_match("wan_proto", "cdma"))
		nvram_set("wan_proto", "dhcp");
#endif

#ifdef DLM
	if (nvram_match("ftp_lang","")) {
//		if ((nvram_match("regulation_domain","0x47TW"))||(nvram_match("regulation_domain","0X47TW")))
		if (nvram_match("wl_country_code", "TW"))
			nvram_set("ftp_lang","TW");
//		else if ((nvram_match("regulation_domain","0x44CN"))||(nvram_match("regulation_domain","0X44CN")))
		else if (nvram_match("wl_country_code", "CN"))
			nvram_set("ftp_lang", "CN");
//		else if ((nvram_match("regulation_domain","0x46KR"))||(nvram_match("regulation_domain","0X46KR")))
//			nvram_set("ftp_lang", "KR");
		else
			nvram_set("ftp_lang","EN");
	}
#endif

	/* Clean MFG test values when boot */
	nvram_set("asus_mfg", "0");
	nvram_set("btn_rst", "");
	nvram_set("btn_ez", "");

	cprintf("read from nvram\n");

	//2008.09 magic {
	nvram_set("wl0_bss_enabled", nvram_safe_get("wl_bss_enabled"));
	
	/* Country Code */
	nvram_set("wl0_country_code", nvram_safe_get("wl_country_code"));

	/* GMODE */
	nvram_set("wl0_gmode", nvram_safe_get("wl_gmode"));
	
		if (nvram_match("wl_gmode_protection", "auto"))
	{
		nvram_set("wl0_gmode_protection", "auto");
	}
	else
	{
		nvram_set("wl0_gmode_protection", "off");
	}
	//2009.01 magic}

	if (nvram_match("wl_wep_x", "0") || nvram_match("wl_auth_mode", "psk"))
		nvram_set("wl0_wep", "disabled");
	else nvram_set("wl0_wep", "enabled");

	if (nvram_match("wl_auth_mode", "shared"))
		nvram_set("wl0_auth", "1");
	else nvram_set("wl0_auth", "0");


#ifdef WPA2_WMM
	if (nvram_match("wl_auth_mode", "psk")) {
		if (nvram_match("wl_wpa_mode", "1")) {
			nvram_set("wl_akm", "psk");
			nvram_set("wl0_akm", "psk");
		}
		else if (nvram_match("wl_wpa_mode", "2")) {
			nvram_set("wl_akm", "psk2");
			nvram_set("wl0_akm", "psk2");
		}
		else{	// wl_wpa_mode == 0
			nvram_set("wl_akm", "psk"); // according to the official firmware.
			nvram_set("wl0_akm", "psk psk2");
		}
	}
	else if (nvram_match("wl_auth_mode", "wpa") || nvram_match("wl_auth_mode", "wpa2")) {
		if (nvram_match("wl_auth_mode", "wpa2")) {
			nvram_set("wl_akm", "wpa2");
			nvram_set("wl0_akm", "wpa2");
		}
		else if (nvram_match("wl_wpa_mode", "3")) {
			nvram_set("wl_akm", "wpa");
			nvram_set("wl0_akm", "wpa");
		}
		else{	// wl_wpa_mode == 4
			nvram_set("wl_akm", "psk");	// according to the official firmware.
			nvram_set("wl0_akm", "wpa wpa2");
		}
	}
	else{
		nvram_set("wl_akm", "");
		nvram_set("wl0_akm", "");
	}//*/
// 2008.06 James. }
	// thanks for Oleg
	nvram_set("wl0_auth_mode", nvram_match("wl_auth_mode", "radius") ? "radius" : "none");
	
	nvram_set("wl0_preauth", nvram_safe_get("wl_preauth"));
	nvram_set("wl0_net_reauth", nvram_safe_get("wl_net_reauth"));
	nvram_set("wl0_wme", nvram_safe_get("wl_wme"));
	nvram_set("wl0_wme_no_ack", nvram_safe_get("wl_wme_no_ack"));
	nvram_set("wl0_wme_sta_bk", nvram_safe_get("wl_wme_sta_bk"));
	nvram_set("wl0_wme_sta_be", nvram_safe_get("wl_wme_sta_be"));
	nvram_set("wl0_wme_sta_vi", nvram_safe_get("wl_wme_sta_vi"));
	nvram_set("wl0_wme_sta_vo", nvram_safe_get("wl_wme_sta_vo"));
	nvram_set("wl0_wme_ap_bk", nvram_safe_get("wl_wme_ap_bk"));
	nvram_set("wl0_wme_ap_be", nvram_safe_get("wl_wme_ap_be"));
	nvram_set("wl0_wme_ap_vi", nvram_safe_get("wl_wme_ap_vi"));
	nvram_set("wl0_wme_ap_vo", nvram_safe_get("wl_wme_ap_vo"));
// 2008.06 James. {
	nvram_set("wl0_wme_txp_bk", nvram_safe_get("wl_wme_txp_bk"));
	nvram_set("wl0_wme_txp_be", nvram_safe_get("wl_wme_txp_be"));
	nvram_set("wl0_wme_txp_vi", nvram_safe_get("wl_wme_txp_vi"));
	nvram_set("wl0_wme_txp_vo", nvram_safe_get("wl_wme_txp_vo"));
// 2008.06 James. }
#else	// WPA2_WMM
	nvram_set("wl0_auth_mode", nvram_safe_get("wl_auth_mode"));
	nvram_set("wl_akm", "");
	nvram_set("wl0_akm", "");
	nvram_set("wl0_wme", "off");
#endif	// WPA2_WMM

	nvram_set("wl0_ssid", nvram_safe_get("wl_ssid"));
	nvram_set("wl0_channel", nvram_safe_get("wl_channel"));
	nvram_set("wl0_country_code", nvram_safe_get("wl_country_code"));
	nvram_set("wl0_rate", nvram_safe_get("wl_rate"));
	nvram_set("wl0_mrate", nvram_safe_get("wl_mrate"));
	nvram_set("wl0_rateset", nvram_safe_get("wl_rateset"));
	nvram_set("wl0_frag", nvram_safe_get("wl_frag"));
	nvram_set("wl0_rts", nvram_safe_get("wl_rts"));
	nvram_set("wl0_dtim", nvram_safe_get("wl_dtim"));
	nvram_set("wl0_bcn", nvram_safe_get("wl_bcn"));
	nvram_set("wl0_plcphdr", nvram_safe_get("wl_plcphdr"));
	nvram_set("wl0_crypto", nvram_safe_get("wl_crypto"));
	nvram_set("wl0_wpa_psk", nvram_safe_get("wl_wpa_psk"));
	nvram_set("wl0_key", nvram_safe_get("wl_key"));
	nvram_set("wl0_key1", nvram_safe_get("wl_key1"));
	nvram_set("wl0_key2", nvram_safe_get("wl_key2"));
	nvram_set("wl0_key3", nvram_safe_get("wl_key3"));
	nvram_set("wl0_key4", nvram_safe_get("wl_key4"));
	nvram_set("wl0_closed", nvram_safe_get("wl_closed"));
	nvram_set("wl0_frameburst", nvram_safe_get("wl_frameburst"));
	nvram_set("wl0_afterburner", nvram_safe_get("wl_afterburner"));
	nvram_set("wl0_ap_isolate", nvram_safe_get("wl_ap_isolate"));
	nvram_set("wl0_radio", nvram_safe_get("wl_radio_x"));

	if (nvram_match("wl_wpa_mode", ""))
		nvram_set("wl_wpa_mode", "0");


	nvram_set("wl0_radius_ipaddr", nvram_safe_get("wl_radius_ipaddr"));
	nvram_set("wl0_radius_port", nvram_safe_get("wl_radius_port"));
	nvram_set("wl0_radius_key", nvram_safe_get("wl_radius_key"));
	nvram_set("wl0_wpa_gtk_rekey", nvram_safe_get("wl_wpa_gtk_rekey"));


	if (!nvram_match("wl_mode_ex", "ap"))
		nvram_set("wl_mode", nvram_safe_get("wl_mode_ex"));
	else
	{
		/* WDS control */
		if (nvram_match("wl_mode_x", "1"))
			nvram_set("wl_mode", "wds");
		else
			nvram_set("wl_mode", "ap");

		nvram_set("wl0_lazywds", nvram_safe_get("wl_lazywds"));
	}

	if (nvram_match("wl_wdsapply_x", "1"))
	{
		num = atoi(nvram_safe_get("wl_wdsnum_x"));
		list[0]=0;

		for (i=0;i<num;i++)
		{
			sprintf(list, "%s %s", list, mac_conv("wl_wdslist_x", i, macbuf));
		}

		dprintf("wds list %s %x\n", list, num);

		nvram_set("wl_wds", list);	// 2008.06 James.
		nvram_set("wl0_wds", list);
	}
	else{
		nvram_set("wl_wds", "");	// 2008.06 James.
		nvram_set("wl0_wds", "");
	}

	if (nvram_match("rt_wdsapply_x", "1"))
	{
		num = atoi(nvram_safe_get("rt_wdsnum_x"));
		list[0]=0;

		for (i=0;i<num;i++)
		{
			sprintf(list, "%s %s", list, mac_conv("rt_wdslist_x", i, macbuf));
		}
		dprintf("rt wds list %s %x\n", list, num);

		nvram_set("rt_wds", list);
		nvram_set("rt0_wds", list);
	}
	else{
		nvram_set("rt_wds", "");
		nvram_set("rt0_wds", "");
	}

	/* Mac filter */
	nvram_set("wl0_macmode", nvram_safe_get("wl_macmode"));

	if (!nvram_match("wl_macmode", "disabled"))
	{
		num = atoi(nvram_safe_get("wl_macnum_x"));
		list[0]=0;

		for (i=0;i<num;i++)
		{
			sprintf(list, "%s %s", list, mac_conv("wl_maclist_x", i, macbuf));
		}

		nvram_set("wl0_maclist", list);
	}

	if (!nvram_match("rt_macmode", "disabled"))
	{
		num = atoi(nvram_safe_get("rt_macnum_x"));
		list[0]=0;

		for (i=0;i<num;i++)
		{
			sprintf(list, "%s %s", list, mac_conv("rt_maclist_x", i, macbuf));
		}

		nvram_set("rt0_maclist", list);
	}

//2008.09 magic }

	if (!skipflag) {
#ifdef RTCONFIG_USB_MODEM
		nvram_unset("system_ready");	// for notifying wanduck.
		set_usb_modem_state(0);
#endif

	/* Direct copy value */
	/* LAN Section */
	if (nvram_match("dhcp_enable_x", "1"))
		nvram_set("lan_proto", "dhcp");
	else nvram_set("lan_proto", "static");

	nvram_set("wan0_proto", nvram_safe_get("wan_proto"));
	if (nvram_match("x_DHCPClient", "0")) {	// 2008.09 magic
		nvram_set("wan0_ipaddr", nvram_safe_get("wan_ipaddr"));
		nvram_set("wan0_netmask", nvram_safe_get("wan_netmask"));
		nvram_set("wan0_gateway", nvram_safe_get("wan_gateway"));
	}	// 2008.09 magic

// 2008.10 magic {
	else if (nvram_match("wan_proto", "pppoe")
			|| nvram_match("wan_proto", "pptp")
			|| nvram_match("wan_proto", "l2tp")) {
		if (nvram_match("x_DHCPClient", "1")) {
			nvram_set("wan_ipaddr", "0.0.0.0");
			nvram_set("wan_netmask", "0.0.0.0");
			nvram_set("wan_gateway", "0.0.0.0");
		}
	}
	else if (nvram_match("wan_proto", "dhcp")
			|| nvram_match("wan_proto", "static")) {
		if (nvram_match("x_DHCPClient", "1")) {
			nvram_set("wan_ipaddr", "");
			nvram_set("wan_netmask", "");
			nvram_set("wan_gateway", "");
		}
	}

	}
// 2008.10 magic }	

// 2008.09 magic for the procedure of no-reboot rc, and start_wan() will do this. {
	if (!skipflag) {
		//2008.10 magic{
//		nvram_unset("ntp_ready");	// for notifying detectWAN.
		nvram_unset("wan_ready");	// for notifying wanduck.
		nvram_unset("manually_disconnect_wan");	// 2008.07 James.
		//2008.10 magic}
		nvram_set("wan_ipaddr_t", "");
		nvram_set("wan_netmask_t", "");
		nvram_set("wan_gateway_t", "");
		nvram_set("wan_dns_t", "");
		nvram_set("wan_status_t", "Disconnected");
		nvram_set("wan_subnet_t", ""); // 2010.09 James.
		nvram_set("lan_subnet_t", ""); // 2010.09 James.
	}
// 2008.09 magic }

	wan_netmask_check();

	if (!skipflag)
	{
	//if (nvram_match("wan_proto", "pppoe") || nvram_match("wan_proto", "pptp"))
	if (nvram_match("wan_proto", "pppoe") || nvram_match("wan_proto", "pptp") || nvram_match("wan_proto", "l2tp"))	// oleg patch
	{
		printf(" set pppoe if as ppp0\n");
		nvram_set("wan0_pppoe_ifname", "ppp0");
		nvram_set("upnp_wan_proto", "pppoe");
		nvram_set("wan0_pppoe_username", nvram_safe_get("wan_pppoe_username"));
		nvram_set("wan0_pppoe_passwd", nvram_safe_get("wan_pppoe_passwd"));
		if (nvram_match("wan_proto", "pppoe"))
			nvram_set("wan0_pppoe_idletime", nvram_safe_get("wan_pppoe_idletime"));
		else
			nvram_set("wan0_pppoe_idletime", "0");
		nvram_set("wan0_pppoe_txonly_x", nvram_safe_get("wan_pppoe_txonly_x"));
		nvram_set("wan0_pppoe_mtu", nvram_safe_get("wan_pppoe_mtu"));
		nvram_set("wan0_pppoe_mru", nvram_safe_get("wan_pppoe_mru"));
		nvram_set("wan0_pppoe_service", nvram_safe_get("wan_pppoe_service"));
		nvram_set("wan0_pppoe_ac", nvram_safe_get("wan_pppoe_ac"));
		nvram_set("wan0_pppoe_options_x", nvram_safe_get("wan_pppoe_options_x"));	// oleg patch
		nvram_set("wan0_pptp_options_x", nvram_safe_get("wan_pptp_options_x"));		// oleg patch

#ifdef REMOVE
		nvram_set("wan0_pppoe_demand", "1");
		nvram_set("wan0_pppoe_keepalive", "1");
#endif
		nvram_set("wan0_pppoe_ipaddr", nvram_safe_get("wan_ipaddr"));
		//nvram_set("wan0_pppoe_netmask", nvram_safe_get("wan_netmask"));
		//nvram_set("wan0_pppoe_gateway", nvram_safe_get("wan_gateway"));

		nvram_set("wan0_pppoe_netmask", 	// oleg patch ~
			inet_addr_(nvram_safe_get("wan_ipaddr")) && 
			inet_addr_(nvram_safe_get("wan_netmask")) ? 
				nvram_safe_get("wan_netmask") : NULL);
		nvram_set("wan0_pppoe_gateway", nvram_safe_get("wan_gateway"));
		
		/* current interface address (dhcp + firewall) */
		nvram_set("wanx_ipaddr", nvram_safe_get("wan_ipaddr"));
							// ~ oleg patch
	}
	// 2008.09 magic {
	else
		nvram_unset("upnp_wan_proto");
	// 2008.09 magic }
	
	nvram_set("wan0_hostname", nvram_safe_get("wan_hostname"));

	nvram_set("lan_hwaddr", nvram_safe_get("il0macaddr"));
	mac_conv("wan_hwaddr_x", -1, macbuf);
	if (!nvram_match("wan_hwaddr_x", "") && strcasecmp(macbuf, "FF:FF:FF:FF:FF:FF"))
	{
		nvram_set("wan_hwaddr", macbuf);
		nvram_set("wan0_hwaddr", macbuf);
	}
	else
	{
		nvram_set("wan_hwaddr", nvram_safe_get("il1macaddr"));
		nvram_set("wan0_hwaddr", nvram_safe_get("il1macaddr"));
	}

	nvram_set("wan0_dnsenable_x", nvram_safe_get("wan_dnsenable_x"));
	nvram_unset("wan0_dns");	// oleg patch //2008.09 magic
	nvram_unset("wanx_dns");	// oleg patch //2008.09 magic

	convert_routes();

	}

	memset(servers, 0, sizeof(servers));

	if (!nvram_match("ntp_server0", ""))
		sprintf(servers, "%s%s ", servers, nvram_safe_get("ntp_server0"));
	if (!nvram_match("ntp_server1", ""))
		sprintf(servers, "%s%s ", servers, nvram_safe_get("ntp_server1"));

	nvram_set("ntp_servers", servers);

	if (!skipflag)
	{
	if (nvram_match("wan_nat_x", "0") && nvram_match("wan_route_x", "IP_Bridged"))
	{
		sprintf(ifnames, "%s", nvram_safe_get("lan_ifnames"));
#ifndef WL500GP
		sprintf(ifnames, "%s %s", ifnames, nvram_safe_get("wan_ifnames"));
#endif
		nvram_set("lan_ifnames_t", ifnames);
		nvram_set("br0_ifnames", ifnames);	// 2008.09 magic
		nvram_set("router_disable", "1");
	}
#ifdef WIRELESS_WAN
	else if (!nvram_match("wl_mode_ex", "ap") && !nvram_match("wl_mode_ex", "re")) // thanks for Oleg
	{
		char name[80], *next;
		
		char *wl_ifname=nvram_safe_get("wl0_ifname");

		/* remove wl_ifname from the ifnames */
		strcpy(ifnames, nvram_safe_get("wan_ifnames"));
		foreach(name, nvram_safe_get("lan_ifnames"), next) {
			if (strcmp(name, wl_ifname)) {
				sprintf(ifnames, "%s %s", ifnames, name);
			}
		}
		nvram_set("lan_ifnames_t", ifnames);
		nvram_set("br0_ifnames", ifnames);	// 2008.09 magic
		nvram_set("router_disable", "0");
	}
#endif
	else 
	{ 
// 2008.09 magic {
		//nvram_set("lan_ifnames_t", nvram_safe_get("lan_ifnames"));
		memset(ifnames, 0, sizeof(ifnames));
		strcpy(ifnames, nvram_safe_get("lan_ifnames"));
		nvram_set("lan_ifnames_t", ifnames);
		nvram_set("br0_ifnames", ifnames);
		nvram_set("router_disable", "0");
	}

	}

//	if (nvram_match("ddns_enable_x", "1") && nvram_match("wan_proto", "pppoe"))
//		system("start_ddns");
// 2008.09 magic }

#ifdef USB_SUPPORT
	// clean some temp variables
	if (!skipflag)
	{
/*
	nvram_set("usb_device", "");
	nvram_set("usb_ftp_device", "");
	nvram_set("usb_storage_device", "");
	nvram_set("usb_web_device", "");
	nvram_set("usb_audio_device", "");
	nvram_set("usb_webdriver_x", "");
	nvram_set("usb_web_flag", "");
	nvram_set("usb_disc0_path0", "");	
	nvram_set("usb_disc0_path1", "");
	nvram_set("usb_disc0_path2", "");
	nvram_set("usb_disc0_path3", "");
	nvram_set("usb_disc0_path4", "");
	nvram_set("usb_disc0_path5", "");
	nvram_set("usb_disc0_path6", "");
	nvram_set("usb_disc1_path0", "");
	nvram_set("usb_disc1_path1", "");
	nvram_set("usb_disc1_path2", "");
	nvram_set("usb_disc1_path3", "");
	nvram_set("usb_disc1_path4", "");
	nvram_set("usb_disc1_path5", "");
	nvram_set("usb_disc1_path6", "");
	nvram_set("usb_disc0_fs_path0", "");
	nvram_set("usb_disc0_fs_path1", "");
	nvram_set("usb_disc0_fs_path2", "");
	nvram_set("usb_disc0_fs_path3", "");
	nvram_set("usb_disc0_fs_path4", "");
	nvram_set("usb_disc0_fs_path5", "");
	nvram_set("usb_disc0_fs_path6", "");
	nvram_set("usb_disc1_fs_path0", "");
	nvram_set("usb_disc1_fs_path1", "");
	nvram_set("usb_disc1_fs_path2", "");
	nvram_set("usb_disc1_fs_path3", "");
	nvram_set("usb_disc1_fs_path4", "");
	nvram_set("usb_disc1_fs_path5", "");
	nvram_set("usb_disc1_fs_path6", "");

	nvram_set("usb_disc0_index", "0");
	nvram_set("usb_disc1_index", "0");
	nvram_set("usb_disc0_port", "0");
	nvram_set("usb_disc1_port", "0");
	nvram_set("usb_disc0_dev", "");
	nvram_set("usb_disc1_dev", "");
*/
	nvram_set("usb_dev_state", "none");
	nvram_set("usb_mnt_first_path", "");
	nvram_set("usb_mnt_first_path_port", "0");

	memset(usb_path1, 0x0, 16);
	memset(usb_path1_old, 0x0, 16);
	memset(usb_path2, 0x0, 16);
	memset(usb_path2_old, 0x0, 16);

	nvram_set("usb_path", "");
	nvram_set("usb_path1", "");
	nvram_set("usb_path2", "");
	for (i = 1; i < 3 ; i++)
	{
		for (j = 0; j < 16 ; j++)
		{
			sprintf(nvram_name, "usb_path%d_fs_path%d", i, j);
			nvram_unset(nvram_name);
		}
	}
	nvram_set("usb_path1_index", "0");
	nvram_set("usb_path1_add", "0");
	nvram_set("usb_path1_vid", "");
	nvram_set("usb_path1_pid", "");
	nvram_set("usb_path1_manufacturer", "");
	nvram_set("usb_path1_product", "");
	nvram_set("usb_path1_removed", "0");
	nvram_set("usb_path2_index", "0");
	nvram_set("usb_path2_add", "0");
	nvram_set("usb_path2_vid", "");
	nvram_set("usb_path2_pid", "");
	nvram_set("usb_path2_manufacturer", "");
	nvram_set("usb_path2_product", "");
	nvram_set("usb_path2_removed", "0");

#ifdef RTCONFIG_USB_MODEM
	nvram_set("usb_path1_act", "");
	nvram_set("usb_path2_act", "");
#endif
#ifdef DLM
	nvram_set("st_samba_mode_x", "-1");
	nvram_set("apps_dms_ex", "0");
	nvram_set("apps_itunes_ex", "0");
	nvram_set("apps_smb_ex", "0");
	nvram_set("apps_u2ec_ex", "0");
	nvram_set("apps_status_checked", "1");	// it means need to check
	nvram_set("apps_comp", "0");
	nvram_set("apps_disk_free", "0");
	nvram_set("dm_block", "0");
	nvram_set("no_usb_led", "0");
	if (!nvram_get("usb_vid_allow"))
		nvram_set("usb_vid_allow", "0");
#endif
	}
#endif

#if 0
	if (!nvram_match("sp_battle_ips", "0") && !skipflag)
	{
		system("insmod ip_nat_starcraft.o");
		system("insmod ipt_NETMAP.o");
	}
#endif

	time_zone_x_mapping();

	if (!skipflag)
	{
#ifdef CDMA
	nvram_set("support_cdma", "1");
	nvram_set("cdma_down", "99");
#else
	nvram_unset("support_cdma");
#endif
	nvram_set("reboot", "");
#ifdef WSC
	nvram_unset("wps_start_flag");
	nvram_unset("wps_oob_flag");
	nvram_set("wps_enable", "1");
	nvram_set("wps_mode", "1");	// PIN method
#endif

	if (nvram_match("wsc_config_state", "2"))
		nvram_match("wsc_config_state", "1");

	if (nvram_match("rt_wsc_config_state", "2"))
		nvram_match("rt_wsc_config_state", "1");

	if (!nvram_match("wsc_config_state", "0") || !nvram_match("rt_wsc_config_state", "0"))
		nvram_set("x_Setting", "1");      

	nvram_set("networkmap_fullscan", "0");	// 2008.07 James.
	nvram_set("mac_clone_en", "0");
#if (!defined(W7_LOGO) && !defined(WIFI_LOGO))
	nvram_set("link_internet", "2");
#else
	nvram_set("link_internet", "1");
#endif
	nvram_set("detect_timestamp", "0");	// 2010.10 James.
	nvram_set("fullscan_timestamp", "0");
	nvram_set("renew_timestamp", "0");
	nvram_set("no_internet_detect", "0");
	nvram_unset("wan_gateway_tmp");
	nvram_unset("wan_ipaddr_tmp");
	nvram_unset("wan_netmask_tmp");

	nvram_set("done_auto_mac", "0");	// 2010.09 James.
	nvram_set("upnp_started", "0");
	nvram_set("ntp_ready", "0");
	nvram_set("ntp_restart_upnp", "0");
	nvram_set("bak_ddns_enable_x", nvram_safe_get("ddns_enable_x"));
	nvram_set("bak_ddns_wildcard_x", nvram_safe_get("ddns_wildcard_x"));

	nvram_set("reload_svc_wl", "0");
	nvram_set("reload_svc_rt", "0");

	if (!nvram_get("hwnat"))
		nvram_set("hwnat", "1");

	nvram_unset("reboot_timestamp");

	nvram_unset("ddns_cache");
	nvram_unset("ddns_ipaddr");
	nvram_unset("ddns_status");
	nvram_unset("ddns_updated");

	nvram_set("qos_ubw", "");
	nvram_set("qos_ubw_status", "");
	nvram_set("qos_ubw_reason", "");

	}
}

/*
char *findpattern(char *target, char *pattern)
{
	char *find;
	int len;

	if ((find=strstr(target, pattern)))
	{
		len = strlen(pattern);
		if (find[len]==';' || find[len]==0)
		{
			return find;
		}
	}
	return NULL;
}
*/

/*
 * wanmessage
 *
 */
void wanmessage(char *fmt, ...)
{
  va_list args;
  char buf[512];

  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  nvram_set("wan_reason_t", buf);
  va_end(args);
}

#if 0
#ifdef RT2400_SUPPORT
void write_rt2400_conf(void)
{
	
	FILE *fp;
	char *tmpstr;

	// create hostapd.conf
	fp=fopen("/tmp/RT2400AP.dat","w");
	if (fp==NULL)
	{
		return;	
	}


	fprintf(fp, "[Default]\n");
	fprintf(fp, "SSID=%s\n", nvram_safe_get("wl_ssid"));
	if (nvram_match("wl_channel", "0")) 
		fprintf(fp, "Channel=6\n");
	else
		fprintf(fp, "Channel=%s\n", nvram_safe_get("wl_channel"));	

	fprintf(fp, "HIDESSID=%s\n", nvram_safe_get("wl_closed"));

	fprintf(fp, "BeaconPeriod=%s\n", nvram_safe_get("wl_bcn")); 	
	fprintf(fp, "RTSThreshold=%s\n", nvram_safe_get("wl_rts")); 		
	fprintf(fp, "FragThreshold=%s\n", nvram_safe_get("wl_frag")); 


	if (!nvram_match("wl_wep_x","0"))
	{	
		fprintf(fp, "DefaultKeyID=%s\n", nvram_safe_get("wl_key")); 	

		fprintf(fp, "Key1Type=0\n");		
		fprintf(fp, "Key1Str=%s\n", nvram_safe_get("wl_key1")); 	
		fprintf(fp, "Key2Type=0\n");		
		fprintf(fp, "Key2Str=%s\n", nvram_safe_get("wl_key2")); 	
		fprintf(fp, "Key3Type=0\n");		
		fprintf(fp, "Key3Str=%s\n", nvram_safe_get("wl_key3")); 	
		fprintf(fp, "Key4Type=0\n");		
		fprintf(fp, "Key4Str=%s\n", nvram_safe_get("wl_key4")); 	
	}


	if (nvram_match("wl_auth_mode","shared"))
	{
		fprintf(fp, "AuthMode=shared\n"); 
		fprintf(fp, "EncrypType=wep\n");
	}
	else if (nvram_match("wl_auth_mode","psk"))
	{
		fprintf(fp, "AuthMode=wpapsk\n"); 
		fprintf(fp, "EncrypType=tkip\n");
		fprintf(fp, "WPAPSK=%s\n", nvram_safe_get("wl_wpa_psk"));
		fprintf(fp, "RekeyInterval=%s\n", nvram_safe_get("wl_wpa_gtk_rekey"));
		fprintf(fp, "RekeyMethod=time\n");
	}
	else 
	{
		fprintf(fp, "AuthMode=open\n"); 
		if (!nvram_match("wl_wep_x","0"))	
			fprintf(fp, "EncrypType=wep\n");
		else fprintf(fp, "EncrypType=none\n");
	}

	if (nvram_match("wl_macmode", "allow"))
	{	
		fprintf(fp, "AclEnable=1\n");
	}
	else if (nvram_match("wl_macmode", "deny"))
	{	
		fprintf(fp, "AclEnable=2\n");
	}
	else fprintf(fp, "AclEnable=0\n");

	if (nvram_match("wl_mode_x", "0"))
		fprintf(fp, "WdsEnable=0\n");
	else fprintf(fp, "WdsEnable=1\n");
	fclose(fp);
}
#endif

#endif

void update_lan_status(int isup)
{
	if (isup)
	{       
		nvram_set("lan_ipaddr_t", nvram_safe_get("lan_ipaddr"));
		nvram_set("lan_netmask_t", nvram_safe_get("lan_netmask"));

		if (nvram_match("wan_route_x", "IP_Routed"))
		{
			if (nvram_match("lan_proto", "dhcp"))
			{
				if (!nvram_match("dhcp_gateway_x", ""))
					nvram_set("lan_gateway_t", nvram_safe_get("dhcp_gateway_x"));
				else nvram_set("lan_gateway_t", nvram_safe_get("lan_ipaddr"));
			}
			else nvram_set("lan_gateway_t", nvram_safe_get("lan_ipaddr"));
		}
		else nvram_set("lan_gateway_t", nvram_safe_get("lan_gateway"));

// 2010.09 James. {
		char lan_gateway[16], lan_ipaddr[16], lan_netmask[16], lan_subnet[11];
		
		memset(lan_gateway, 0, 16);
		strcpy(lan_gateway, nvram_safe_get("lan_gateway_t"));
		memset(lan_ipaddr, 0, 16);
		strcpy(lan_ipaddr, nvram_safe_get("lan_ipaddr_t"));
		memset(lan_netmask, 0, 16);
		strcpy(lan_netmask, nvram_safe_get("lan_netmask_t"));
		memset(lan_subnet, 0, 11);
		sprintf(lan_subnet, "0x%x", inet_network(lan_ipaddr)&inet_network(lan_netmask));
		nvram_set("lan_subnet_t", lan_subnet);
// 2010.09 James. }
	}
	else
	{
/*
		if (nvram_match("sw_mode_ex", "2"))     // for RT-N13 repeater mode
		{
			nvram_set("lan_ipaddr_old", nvram_safe_get("lan_ipaddr"));
			nvram_set("lan_netmask_old", nvram_safe_get("lan_netmask"));
			nvram_set("lan_gateway_old", nvram_safe_get("lan_gateway"));
			nvram_set("lan_dns_old", nvram_safe_get("lan_dns_t"));
			nvram_set("lan_wins_old", nvram_safe_get("lan_wins_t"));
			nvram_set("lan_domain_old", nvram_safe_get("lan_domain_t"));
			nvram_set("lan_lease_old", nvram_safe_get("lan_lease_t"));
		}
*/
		nvram_set("lan_ipaddr_t", "");
		nvram_set("lan_netmask_t", "");
		nvram_set("lan_gateway_t", "");
		nvram_set("lan_subnet_t", ""); // 2010.09 James.
	}
}

char *pppstatus(char *buf)
{
   FILE *fp;
   char sline[128], *p;

   if ((fp=fopen("/tmp/wanstatus.log", "r")) && fgets(sline, sizeof(sline), fp))
   {
	p = strstr(sline, ",");
	strcpy(buf, p+1);
   }
   else
   {
	strcpy(buf, "unknown reason");
   }
   return buf;	// oleg patch
}

void logmessage(char *logheader, char *fmt, ...)
{
  va_list args;
  char buf[512];

  va_start(args, fmt);

  vsnprintf(buf, sizeof(buf), fmt, args);
  openlog(logheader, 0, 0);
  syslog(0, buf);
  closelog();
  va_end(args);
}

void update_wan_status(int isup)
{
	char *proto;
	char dns_str[36];

	memset(dns_str, 0, sizeof(dns_str));
	proto = nvram_safe_get("wan_proto");

#ifdef RTCONFIG_USB_MODEM
	if(get_usb_modem_state())
		nvram_set("wan_proto_t", "Modem");
	else
#endif
	if (!strcmp(proto, "static")) nvram_set("wan_proto_t", "Static");
	else if (!strcmp(proto, "dhcp")) nvram_set("wan_proto_t", "Automatic IP");
	else if (!strcmp(proto, "pppoe")) nvram_set("wan_proto_t", "PPPoE");
	else if (!strcmp(proto, "pptp")) nvram_set("wan_proto_t", "PPTP");
	else if (!strcmp(proto, "l2tp")) nvram_set("wan_proto_t", "L2TP");	// oleg patch
	//else if (!strcmp(proto, "bigpond")) nvram_set("wan_proto_t", "BigPond");
#ifdef CDMA
	else if (!strcmp(proto, "cdma")) nvram_set("wan_proto_t", "CDMA");
#endif
	if (!isup)
	{
		nvram_set("wan_ipaddr_t", "");
		nvram_set("wan_netmask_t", "");
		nvram_set("wan_gateway_t", "");
		nvram_set("wan_subnet_t", ""); // 2010.09 James.
		nvram_set("wan_dns_t", "");
	      //nvram_set("wan_ifname_t", ""); //2008.10 magic 
		nvram_set("wan_status_t", "Disconnected");
	}
	else
	{
		nvram_set("wan_ipaddr_t", nvram_safe_get("wan0_ipaddr"));
		nvram_set("wan_netmask_t", nvram_safe_get("wan0_netmask"));
		nvram_set("wan_gateway_t", nvram_safe_get("wan0_gateway"));

// 2010.09 James. {
		char wan_gateway[16], wan_ipaddr[16], wan_netmask[16], wan_subnet[11];
		
		memset(wan_gateway, 0, 16);
		strcpy(wan_gateway, nvram_safe_get("wan0_gateway"));
		memset(wan_ipaddr, 0, 16);
		strcpy(wan_ipaddr, nvram_safe_get("wan0_ipaddr"));
		memset(wan_netmask, 0, 16);
		strcpy(wan_netmask, nvram_safe_get("wan0_netmask"));
		memset(wan_subnet, 0, 11);
		sprintf(wan_subnet, "0x%x", inet_network(wan_ipaddr)&inet_network(wan_netmask));
		nvram_set("wan_subnet_t", wan_subnet);
// 2010.09 James. }

		if (!nvram_match("wan_dnsenable_x", "1"))
		{
			if (!nvram_match("wan_dns1_x",""))
				sprintf(dns_str, "%s", nvram_safe_get("wan_dns1_x"));

								
			if (!nvram_match("wan_dns2_x",""))
				sprintf(dns_str, " %s", nvram_safe_get("wan_dns2_x"));
												
			nvram_set("wan_dns_t", dns_str);
		}
		else 
			nvram_set("wan_dns_t", nvram_safe_get("wan0_dns"));

		nvram_set("wan_status_t", "Connected");
	}
}

void char_to_ascii(char *output, char *input)/* Transfer Char to ASCII */
{						   /* Cherry_Cho added in 2006/9/29. */
	int i;
	char tmp[10];
	char *ptr;

	ptr = output;

	for ( i=0; i<strlen(input); i++ )
	{
		if ((input[i]>='0' && input[i] <='9')
		   ||(input[i]>='A' && input[i]<='Z')
		   ||(input[i] >='a' && input[i]<='z')
		   || input[i] == '!' || input[i] == '*'
		   || input[i] == '(' || input[i] == ')'
		   || input[i] == '_' || input[i] == '-'
		   || input[i] == '\'' || input[i] == '.')
		{
			*ptr = input[i];
			ptr ++;
		}
		else
		{
			sprintf(tmp, "%%%.02X", input[i]);
			strcpy(ptr, tmp);
			ptr += 3;
		}
	}
	*(ptr) = '\0';
}

