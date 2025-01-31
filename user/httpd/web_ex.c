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
 */
/*
 * ASUS Home Gateway Reference Design
 * Web Page Configuration Support Routines
 *
 * Copyright 2001, ASUSTeK Inc.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of ASUSTeK Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of ASUSTeK Inc..
 *
 * $Id: web_ex.c,v 1.4 2007/04/09 12:01:50 shinjung Exp $
 */

typedef unsigned char   bool;

#ifdef WEBS
#include <webs.h>
#include <uemf.h>
#include <ej.h>
#else /* !WEBS */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <httpd.h>
#endif /* WEBS */

#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <sys/klog.h>
#include <stdarg.h>
#include <sys/wait.h>
#include <dirent.h>
//#include <wlutils.h>  Viz banned 2010.08
#include <proto/ethernet.h>   //add by Viz 2010.08

#ifdef WRITE2FLASH
#include <linux/mtd/mtd.h>
#include <trxhdr.h>
#endif

#include <nvram/typedefs.h>
#include <nvram/bcmutils.h>
#include <nvram/bcmnvram.h>
#include <shutils.h>
#include <bcmnvram_f.h>
#include <common.h>

// 2008.08 magic {
#include "dp.h"	//for discover_all()
#include "wireless.h"    /* --Alicia, 08.09.23 */
//#include <detectWAN.h>	//for detectWAN

//#ifdef DLM
#include <disk_io_tools.h>
#include <disk_initial.h>
#include <disk_share.h>
//#include <sys/vfs.h>	// tmp test
//#include <disk_swap.h>
#include "initial_web_hook.h"
//#endif

#include <net/if.h>
#include <linux/sockios.h>

#include <ralink.h>
#include <semaphore_mfp.h>

#define wan_prefix(unit, prefix)	snprintf(prefix, sizeof(prefix), "wan%d_", unit)

#include <sys/mman.h>
//typedef uint32_t __u32; //2008.08 magic
#ifndef	O_BINARY		/* should be define'd on __WIN32__ */
#define O_BINARY	0
#endif
#include <image.h>
#ifndef MAP_FAILED
#define MAP_FAILED (-1)
#endif

#include <syslog.h>
#define logs(s) syslog(LOG_NOTICE, s)

#ifdef WEBS
#define init_cgi(query)
#define do_file(webs, file)
#endif

static void nvram_commit_safe();

static void nvram_commit_safe()
{
        spinlock_lock(SPINLOCK_NVRAMCommit);
        nvram_commit();
        spinlock_unlock(SPINLOCK_NVRAMCommit);
}

//#define sys_upgrade(image ) eval("/bin/mtd_write", "-r", "write", image, "Kernel");
#define sys_upgrade(image ) eval("/bin/mtd_write", "write", image, "Kernel");
#define sys_upload(image) eval("nvram", "restore", image)
#define sys_download(file) eval("nvram", "save", file)
#define sys_stats(url) eval("stats", (url))
#define sys_restore(sid) eval("nvram_x","get",(sid))
#define sys_commit(sid) nvram_commit_safe()
#define sys_default()   eval("erase", "/dev/mtd1")
#define sys_nvram_set(param) eval("nvram", "set", param)

#define UPNP_E_SUCCESS 0
#define UPNP_E_INVALID_ARGUMENT -1

#define GROUP_FLAG_REFRESH 	0
#define GROUP_FLAG_DELETE 	1
#define GROUP_FLAG_ADD 		2
#define GROUP_FLAG_REMOVE 	3

#define IMAGE_HEADER 	"HDR0"
#define PROFILE_HEADER 	"HDR1"
#define PROFILE_HEADER_NEW      "HDR2"
#define IH_MAGIC	0x27051956	/* Image Magic Number		*/

//#define wl_ioctl(name, cmd, buf, len) 1 //2008.09 magic

static int apply_cgi_group(webs_t wp, int sid, struct variable *var, char *groupName, int flag);
static int nvram_generate_table(webs_t wp, char *serviceId, char *groupName);

int count_sddev_mountpoint();

// 2008.08 magic {
//typedef u_int64_t u64;
//typedef u_int32_t u32;
//static u64 restart_needed_bits = 0;
static unsigned long restart_needed_bits = 0;
static unsigned int restart_tatal_time = 0; 
//static u32 last_arp_info_len = 0, last_disk_info_len = 0;
// 2008.08 magic }

#define Ralink_WPS	1 //2009.01 magic

char ibuf2[8192];

//static int ezc_error = 0;

#define ACTION_UPGRADE_OK   0
#define ACTION_UPGRADE_FAIL 1

int action;

char *serviceId;
#define MAX_GROUP_ITEM 10
#define MAX_GROUP_COUNT 300
#define MAX_LINE_SIZE 512
char *groupItem[MAX_GROUP_ITEM];
char urlcache[128];
char *next_host;
int delMap[MAX_GROUP_COUNT];
char SystemCmd[64];
char UserID[32]="";
char UserPass[32]="";
char ProductID[32]="";
extern int redirect;
extern int change_passwd;	// 2008.08 magic
extern int reget_passwd;	// 2008.08 magic

#ifdef WRITE2FLASH
char *flashblock=NULL;
int mtd_fd = -1;
int mtd_count;
mtd_info_t mtd_info;
erase_info_t erase_info;

int flog(char *log)
{
	FILE *fp;

	fp=fopen("/tmp/log", "a+");

	if (fp)
	{	
		fprintf(fp, "log : %d %d %s", erase_info.start, erase_info.length, log);
		fclose(fp);
	}	
}

int flashwrite(char *buf, int count, int filelen)
{
	int len;
	int offset;
	int size;

	// first time, the trx header
	if (mtd_fd==-1)
	{

		/* Open MTD device and get sector size */
		if ((mtd_fd = open("/dev/mtd3", O_RDWR)) < 0 ||
		    ioctl(mtd_fd, MEMGETINFO, &mtd_info) != 0 ||
		    mtd_info.erasesize < sizeof(struct trx_header)) {
			flog("1\r\n");
			return 0;
		}

		erase_info.start=0;
		erase_info.length=mtd_info.erasesize;
		mtd_count = 0;

		/* Allocate temporary buffer */
		if (!(flashblock = malloc(erase_info.length))) {
			flog("2\r\n");
			goto fail;
		}
	}
	
	offset=0;
	do {
		if ((mtd_count+(count-offset))>=erase_info.length)
		{
			memcpy(flashblock+mtd_count, buf+offset, erase_info.length-mtd_count);
			offset+=(erase_info.length-mtd_count);
			mtd_count=erase_info.length;
		}
		else 
		{
			memcpy(flashblock+mtd_count, buf+offset, count-offset);
			mtd_count+=(count-offset);
			if (filelen!=0) return 1;
		}

		(void) ioctl(mtd_fd, MEMUNLOCK, &erase_info);
		if (ioctl(mtd_fd, MEMERASE, &erase_info) != 0 ||
	    		write(mtd_fd, flashblock, mtd_count) != mtd_count) {
			flog("3\r\n");
			goto fail;
		}
		erase_info.start+=mtd_count;
		mtd_count=0;
		flog("4\r\n");
		if (filelen==0&&(count-offset)==0) break;
	} while (1);

fail:
	/* if end of file */
	if (flashblock) {
		/* Dummy read to ensure chip(s) are out of lock/suspend state */
		(void) read(mtd_fd, flashblock, 2);
		free(flashblock);
	}

	if (mtd_fd >= 0)
		close(mtd_fd);
	return 1;
}
#endif

int
stop_3g()
{
        char disconn_scr[100];
        char *modem_f = (*nvram_safe_get("modf") ? nvram_safe_get("modf") : "/ttyUSB0");
        int modem_mode = atoi(nvram_safe_get("modem_enable"));

	printf("stop 3g[%d]\n", modem_mode);	// tmp test

	if(!modem_mode)
		return -1;

        system("killall usb_modeswitch");	// why this still exist?
        system("killall pppd");
        sleep(1);

        memset(disconn_scr, 0, sizeof(disconn_scr));
        sprintf(disconn_scr, "/bin/comgt -d /dev/%s -s /etc_ro/ppp/3g/%s", modem_f, modem_mode==2?"EVDO_disconn.scr":"Generic_disconn.scr");
        system(disconn_scr);

	if(*nvram_safe_get("wan_3g_pin"))
	{
		printf("wait more time\n");
		sleep(8);
	}
	else
		sleep(2);

	printf("stop 3g end\n");	// tmp test

        return 0;
}

void
stop_apps()
{
	system("killall wanduck");
	system("killall detectWan");
}

void
sys_reboot()
{
	dbG("[httpd] reboot...\n");

	stop_apps();
	stop_3g();

	dbG("[httpd] Reboot\n");

	nvram_set("reboot", "1");
	kill(1, SIGTERM);
}

char *
rfctime(const time_t *timep)
{
	static char s[201];
	struct tm tm;

	time_zone_x_mapping();
	setenv("TZ", nvram_safe_get_x("", "time_zone_x"), 1);
	memcpy(&tm, localtime(timep), sizeof(struct tm));
	strftime(s, 200, "%a, %d %b %Y %H:%M:%S %z", &tm);
	return s;
}

void
reltime(unsigned long seconds, char *cs)
{
#ifdef SHOWALL
	unsigned int days=0, hours=0, minutes=0;

	if (seconds > 60*60*24) {
		days = seconds / (60*60*24);
		seconds %= 60*60*24;
	}
	if (seconds > 60*60) {
		hours = seconds / (60*60);
		seconds %= 60*60;
	}
	if (seconds > 60) {
		minutes = seconds / 60;
		seconds %= 60;
	}
	sprintf(cs, "%d days, %d hours, %d minutes, %d seconds", days, hours, minutes, seconds);
#else
	sprintf(cs, "%d secs", seconds);
#endif
}

#ifndef WEBS
/******************************************************************************/
/*
 *	Redirect the user to another webs page
 */
 
char *getip(FILE *fp)
{     
//printf("getip!!!"); 
    //getpeername(fd, &addr, &addrlen);
    if (next_host==NULL || strcmp(next_host, "")==0)    
    {
	
       return (nvram_get_x("BRIPAddress","lan_ipaddr"));	
    }
    else
{
	
       return (next_host);
}
} 

//2008.08 magic{
void websRedirect(webs_t wp, char_t *url)
{	
	//printf("Redirect to : %s\n", url);	
	websWrite(wp, T("<html><head>\r\n"));
	websWrite(wp, T("<meta http-equiv=\"refresh\" content=\"0; url=http://%s/%s\">\r\n"), getip((FILE *)wp), url);
	websWrite(wp, T("<meta http-equiv=\"Content-Type\" content=\"text/html\">\r\n"));
	websWrite(wp, T("</head></html>\r\n"));      
	
	websDone(wp, 200);	
}
#endif
//2008.08 magic}

void sys_script(char *name)
{

     char scmd[64];
	
     sprintf(scmd, "/tmp/%s", name);
     printf("run %s %d %s\n", name, strlen(name), scmd);	// tmp test
     
     //handle special scirpt first

     if (strcmp(name,"syscmd.sh")==0)
     {
	   if (strcmp(SystemCmd, "")!=0)
	   {
	   	//sprintf(SystemCmd, "%s > /tmp/syscmd.log\n", SystemCmd);
		sprintf(SystemCmd, "%s > /tmp/syscmd.log 2>&1\n", SystemCmd);	// oleg patch
	   	system(SystemCmd);
	   }	
	   else
	   {
	   	system("echo None > /tmp/syscmd.log\n");
	   }
     }
     else if (strcmp(name, "syslog.sh")==0)
     {
	   // to nothing
     }	
     else if (strcmp(name, "wan.sh")==0)
     {
	   kill_pidfile_s("/var/run/infosvr.pid", SIGUSR1);
     }
     else if (strcmp(name, "printer.sh")==0)
     {	
	   // update status of printer
	   kill_pidfile_s("/var/run/infosvr.pid", SIGUSR1);
     }
     else if (strcmp(name, "lpr_remove")==0)
     {
	   kill_pidfile_s("/var/run/lpdparent.pid", SIGUSR2);
     }
//#ifdef U2EC
	else if (!strcmp(name, "mfp_requeue")) {
		unsigned int login_ip = (unsigned int)atoll(nvram_safe_get("login_ip"));

		if (login_ip == 0x100007f || login_ip == 0x0)
			nvram_set("mfp_ip_requeue", "");
		else {
			struct in_addr addr;

			addr.s_addr = login_ip;
			nvram_set("mfp_ip_requeue", inet_ntoa(addr));
		}

		int u2ec_fifo = open("/var/u2ec_fifo", O_WRONLY|O_NONBLOCK);

		write(u2ec_fifo, "q", 1);
		close(u2ec_fifo);
	}
	else if (!strcmp(name, "mfp_monopolize")) {
		unsigned int login_ip = (unsigned int)atoll(nvram_safe_get("login_ip"));

		//printf("[httpd] run mfp monopolize\n");	// tmp test
		if (login_ip==0x100007f || login_ip==0x0)
			nvram_set("mfp_ip_monopoly", "");
		else
		{
			struct in_addr addr;
			addr.s_addr=login_ip;
			nvram_set("mfp_ip_monopoly", inet_ntoa(addr));
		}
		//int u2ec_fifo = open("/tmp/u2ec_fifo", O_WRONLY|O_NONBLOCK);
		int u2ec_fifo = open("/var/u2ec_fifo", O_WRONLY|O_NONBLOCK);
		write(u2ec_fifo, "m", 1);
		close(u2ec_fifo);
		//printf("[httpd] end run mfp monopolize ---\n");	// tmp test
	}
//#endif
     else if (strcmp(name, "wlan11a.sh")==0 || strcmp(name,"wlan11b.sh")==0)
     {
	  // do nothing	
     }
     else if (strcmp(name,"leases.sh")==0 || strcmp(name,"dleases.sh")==0) /* check here*/
     {		
		//csprintf("run sys_script\n");	// lan lease
		sys_refresh_lease();	
     }
     else if (strcmp(name,"iptable.sh")==0) 
     {
		// TODO	
     }
     else if (strcmp(name,"route.sh")==0)
     {
		// TODO
     }
     else if (strcmp(name,"dhcpc_renew")==0)
     {
		//printf("dhcpc renew\n");	// tmp test
		sys_renew();
		sleep(1);
     }
     else if (strcmp(name,"dhcpc_release")==0)
     {
		//printf("dhcpc release\n");	// tmp test
		sys_release();
		sleep(1);
     }
     else if (strcmp(name,"eject-usb.sh")==0)
     {
		eval("rmstorage");
     }
     else if (strcmp(name,"ddnsclient")==0)
     {
		eval("start_ddns");
     }
#ifdef ASUS_DDNS //2007.03.22 Yau add
     else if (strcmp(name,"hostname_check") == 0)
     {

//	fprintf(stderr, "bak_ddns_enable_x: %s\n", nvram_safe_get("bak_ddns_enable_x"));
//	fprintf(stderr, "bak_ddns_wildcard_x: %s\n", nvram_safe_get("bak_ddns_wildcard_x"));

//	fprintf(stderr, "ddns_enable_x: %s\n", nvram_safe_get("ddns_enable_x"));
//	fprintf(stderr, "ddns_wildcard_x: %s\n", nvram_safe_get("ddns_wildcard_x"));

	int i = 0;
	char return_buf[32];

	//Clear asus ddns nvram variables.
//	nvram_set("httpd_check_ddns","1");	// J++
	nvram_set("ddns_timeout", "0");
	nvram_set("rc_service", "ddns_hostname_check");

//	kill(1, SIGUSR1);
	dbG("[httpd] send SIGUSR1 to watchdog for rc_service: %s\n", nvram_safe_get("rc_service"));
	doSystem("killall -%d watchdog", SIGUSR1);
	sleep(1);

	while (1)
	{
		strcpy(return_buf, nvram_safe_get("ddns_return_code"));

		if (strcmp(return_buf, ""))
			break;
		else if (i == 3)
		{
			nvram_set("ddns_timeout", "1");
			break;
		}

		sleep(1);
		i++;
	}
     }
#endif
     else if (strstr(scmd, " ") == 0) // no parameter, run script with eval
     {
		eval(scmd);
     }
     else
	system(scmd);  
}

void websScan(char_t *str)
{
	unsigned int i, flag;
	char_t *v1, *v2, *v3, *sp;
	char_t groupid[64];
	char_t value[MAX_LINE_SIZE];
	char_t name[MAX_LINE_SIZE];
	
	v1 = strchr(str, '?');
			
	i = 0;
	flag = 0;
				     		
	while (v1!=NULL)
	{	   	    	
	    v2 = strchr(v1+1, '=');
	    v3 = strchr(v1+1, '&');

// 2008.08 magic {
		if (v2 == NULL)
			break;
// 2008.08 magic }
	    
	    if (v3!=NULL)
	    {
	       strncpy(value, v2+1, v3-v2-1);
	       value[v3-v2-1] = 0;  
	    }  
	    else
	    {
	       strcpy(value, v2+1);
	    }
	    
	    strncpy(name, v1+1, v2-v1-1);
	    name[v2-v1-1] = 0;
	    /*printf("Value: %s %s\n", name, value);*/
	    
	    if (v2 != NULL && ((sp = strchr(v1+1, ' ')) == NULL || (sp > v2))) 
	    {	    	
	       if (flag && strncmp(v1+1, groupid, strlen(groupid))==0)
	       {	    		    	   
		   delMap[i] = atoi(value);
		   /*printf("Del Scan : %x\n", delMap[i]);*/
		   if (delMap[i]==-1)  break;		   		   
		   i++;
	       }	
	       else if (strncmp(v1+1,"group_id", 8)==0)
	       {	    				       
		   sprintf(groupid, "%s_s", value);
		   flag = 1;
	       }   
	    }
	    v1 = strchr(v1+1, '&');
	} 
	delMap[i] = -1;
	return;
}


void websApply(webs_t wp, char_t *url)
{
#ifdef TRANSLATE_ON_FLY
	do_ej (url, wp);
	websDone (wp, 200);
#else   // define TRANSLATE_ON_FLY

     FILE *fp;
     char buf[MAX_LINE_SIZE];

     fp = fopen(url, "r");
     
     if (fp==NULL) return;
     
     while (fgets(buf, sizeof(buf), fp))
     {
	websWrite(wp, buf);
     } 
     
     websDone(wp, 200);	
     fclose(fp);
#endif
}


/*
 * Example: 
 * lan_ipaddr=192.168.1.1
 * <% nvram_get_x("lan_ipaddr"); %> produces "192.168.1.1"
 * <% nvram_get_x("undefined"); %> produces ""
 */
static int
ej_nvram_get_x(int eid, webs_t wp, int argc, char_t **argv)
{
	char *sid, *name, *c;
	int ret = 0;

	if (ejArgs(argc, argv, "%s %s", &sid, &name) < 2) {
		websError(wp, 400, "Insufficient args\n");
		return -1;
	}

	for (c = nvram_safe_get_x(sid, name); *c; c++) {
		if (isprint(*c) &&
		    *c != '"' && *c != '&' && *c != '<' && *c != '>')
			ret += websWrite(wp, "%c", *c);
		else
			ret += websWrite(wp, "&#%d", *c);
	}

	return ret;
}

#ifdef ASUS_DDNS
static int
ej_nvram_get_ddns(int eid, webs_t wp, int argc, char_t **argv)
{
	char *sid, *name, *c;
	int ret = 0;
																	      
	if (ejArgs(argc, argv, "%s %s", &sid, &name) < 2) {
		websError(wp, 400, "Insufficient args\n");
		return -1;
	}
																	      
	for (c = nvram_safe_get_x(sid, name); *c; c++) {
		if (isprint(*c) &&
		    *c != '"' && *c != '&' && *c != '<' && *c != '>')
			ret += websWrite(wp, "%c", *c);
		else
			ret += websWrite(wp, "&#%d", *c);
	}

	if (strcmp(name,"ddns_return_code")==0)
		nvram_set("ddns_return_code","");
	if (strcmp(name,"ddns_timeout")==0)
		nvram_set("ddns_timeout","0");

	return ret;
}
#endif
/*
 * Example: 
 * lan_ipaddr=192.168.1.1
 * <% nvram_get_x("lan_ipaddr"); %> produces "192.168.1.1"
 * <% nvram_get_x("undefined"); %> produces ""
 */
static int
ej_nvram_get_f(int eid, webs_t wp, int argc, char_t **argv)
{
	char *file, *field, *c, buf[64];
	int ret = 0;

	if (ejArgs(argc, argv, "%s %s", &file, &field) < 2) {
		websError(wp, 400, "Insufficient args\n");
		return -1;
	}
			
	strcpy(buf, nvram_safe_get_f(file, field));		
	for (c = buf; *c; c++) {
		if (isprint(*c) &&
		    *c != '"' && *c != '&' && *c != '<' && *c != '>')
			ret += websWrite(wp, "%c", *c);
		else
			ret += websWrite(wp, "&#%d", *c);
	}

	return ret;
}

/*
 * Example: 
 * wan_proto=dhcp
 * <% nvram_match("wan_proto", "dhcp", "selected"); %> produces "selected"
 * <% nvram_match("wan_proto", "static", "selected"); %> does not produce
 */
static int
ej_nvram_match_x(int eid, webs_t wp, int argc, char_t **argv)
{
	char *sid, *name, *match, *output;

	if (ejArgs(argc, argv, "%s %s %s %s", &sid, &name, &match, &output) < 4) {
		websError(wp, 400, "Insufficient args\n");
		return -1;
	}
	
	if (nvram_match_x(sid, name, match))
	{
		return websWrite(wp, output);
	}	

	return 0;
}	

static int
ej_nvram_double_match_x(int eid, webs_t wp, int argc, char_t **argv)
{
	char *sid, *name, *match, *output;
	char *sid2, *name2, *match2;

	if (ejArgs(argc, argv, "%s %s %s %s %s %s %s", &sid, &name, &match, &sid2, &name2, &match2, &output) < 7) {
		websError(wp, 400, "Insufficient args\n");
		return -1;
	}
	
	if (nvram_match_x(sid, name, match) && nvram_match_x(sid2, name2, match2))
	{
		return websWrite(wp, output);
	}	

	return 0;
}

/*
 * Example: 
 * wan_proto=dhcp
 * <% nvram_match("wan_proto", "dhcp", "selected"); %> produces "selected"
 * <% nvram_match("wan_proto", "static", "selected"); %> does not produce
 */
static int
ej_nvram_match_both_x(int eid, webs_t wp, int argc, char_t **argv)
{
	char *sid, *name, *match, *output, *output_not;

	if (ejArgs(argc, argv, "%s %s %s %s %s", &sid, &name, &match, &output, &output_not) < 5) 
	{
		websError(wp, 400, "Insufficient args\n");
		return -1;
	}
	
	if (nvram_match_x(sid, name, match))
	{
		return websWrite(wp, output);
	}
	else
	{
		return websWrite(wp, output_not);
	}	
}

/*
 * Example: 
 * lan_ipaddr=192.168.1.1 192.168.39.248
 * <% nvram_get_list("lan_ipaddr", 0); %> produces "192.168.1.1"
 * <% nvram_get_list("lan_ipaddr", 1); %> produces "192.168.39.248"
 */
static int
ej_nvram_get_list_x(int eid, webs_t wp, int argc, char_t **argv)
{
	char *sid, *name;
	int which;	
	int ret = 0;

	if (ejArgs(argc, argv, "%s %s %d", &sid, &name, &which) < 3) {
		websError(wp, 400, "Insufficient args\n");
		return -1;
	}

	ret += websWrite(wp, nvram_get_list_x(sid, name, which));
	return ret;
}

/*
 * Example: 
 * lan_ipaddr=192.168.1.1 192.168.39.248
 * <% nvram_get_list("lan_ipaddr", 0); %> produces "192.168.1.1"
 * <% nvram_get_list("lan_ipaddr", 1); %> produces "192.168.39.248"
 */
static int
ej_nvram_get_buf_x(int eid, webs_t wp, int argc, char_t **argv)
{
	char *sid, *name;
	int which;		

	if (ejArgs(argc, argv, "%s %s %d", &sid, &name, &which) < 3) {
		websError(wp, 400, "Insufficient args\n");
		return -1;
	}

	
	return 0;
}

/*
 * Example: 
 * lan_ipaddr=192.168.1.1 192.168.39.248
 * <% nvram_get_table_x("lan_ipaddr"); %> produces "192.168.1.1"
 * <% nvram_get_table_x("lan_ipaddr"); %> produces "192.168.39.248"
 */
static int
ej_nvram_get_table_x(int eid, webs_t wp, int argc, char_t **argv)
{
	char *sid, *name;
	int ret = 0;

	if (ejArgs(argc, argv, "%s %s", &sid, &name) < 2) {
		websError(wp, 400, "Insufficient args\n");
		return -1;
	}
			
	ret += nvram_generate_table(wp, sid, name);	
	return ret;
}

/*
 * Example: 
 * wan_proto=dhcp;dns
 * <% nvram_match_list("wan_proto", "dhcp", "selected", 0); %> produces "selected"
 * <% nvram_match_list("wan_proto", "static", "selected", 1); %> does not produce
 */
static int
ej_nvram_match_list_x(int eid, webs_t wp, int argc, char_t **argv)
{
	char *sid, *name, *match, *output;
	int which;

	if (ejArgs(argc, argv, "%s %s %s %s %d", &sid, &name, &match, &output, &which) < 5) {
		websError(wp, 400, "Insufficient args\n");
		return -1;
	}
	
	if (nvram_match_list_x(sid, name, match, which))
		return websWrite(wp, output);
	else
		return 0;		
}	

ej_select_channel(int eid, webs_t wp, int argc, char_t **argv)
{
	char *sid, chstr[32];
	int ret = 0;
	int idx = 0, channel;
	char *value = nvram_safe_get("wl_country_code");
	char *channel_s = nvram_safe_get("wl_channel");
	
	if (ejArgs(argc, argv, "%s", &sid) < 1) {
		websError(wp, 400, "Insufficient args\n");
		return -1;
	}

	channel = (channel_s == NULL)? 0 : atoi(channel_s);

	for (idx = 0; idx < 12; idx++)
	{
		if (idx == 0)
			strcpy(chstr, "Auto");
		else
			sprintf(chstr, "%d", idx);
		ret += websWrite(wp, "<option value=\"%d\" %s>%s</option>", idx, (idx == channel)? "selected" : "", chstr);
	}

	if (    strcasecmp(value, "CA") && strcasecmp(value, "CO") && strcasecmp(value, "DO") &&
		strcasecmp(value, "GT") && strcasecmp(value, "MX") && strcasecmp(value, "NO") &&
		strcasecmp(value, "PA") && strcasecmp(value, "PR") && strcasecmp(value, "TW") &&
		strcasecmp(value, "US") && strcasecmp(value, "UZ") )
	{
		for (idx = 12; idx < 14; idx++)
		{
			sprintf(chstr, "%d", idx);
			ret += websWrite(wp, "<option value=\"%d\" %s>%s</option>", idx, (idx == channel)? "selected" : "", chstr);
		}
	}

	if ((strcmp(value, "") == 0) || (strcasecmp(value, "DB") == 0)/* || (strcasecmp(value, "JP") == 0)*/)
		ret += websWrite(wp, "<option value=\"14\" %s>14</option>", (14 == channel)? "selected" : "");

	return ret;
}


static int
ej_nvram_char_to_ascii(int eid, webs_t wp, int argc, char_t **argv)
{
	char *sid, *name;
	int ret = 0;

	if (ejArgs(argc, argv, "%s %s", &sid, &name) < 2) {
		websError(wp, 400, "Insufficient args\n");
		return -1;
	}

	char tmpstr[256];
	memset(tmpstr, 0x0, sizeof(tmpstr));
	char_to_ascii(tmpstr, nvram_safe_get_x(sid, name));
	ret += websWrite(wp, "%s", tmpstr);

	return ret;
}

static int
ej_urlcache(int eid, webs_t wp, int argc, char_t **argv)
{
	int flag;
		
	if (strcmp(urlcache, "Main_Operation.asp")==0)
	   flag = 2;
	else if (strcmp(urlcache, "Main_Content.asp")==0)
	   flag = 1;		  		
	else
	   flag = 0;
	   
	cprintf("Url:%s %d\n", urlcache, flag);

	if (strcmp(nvram_safe_get_x("IPConnection","wan_route_x"), "IP_Routed")==0)
	{	
	   if (strcmp(nvram_safe_get_x("IPConnection", "wan_nat_x"), "1")==0)
	   {	   
	   	/* disable to redirect page */
	   	if (redirect)
	   	{	
			websWrite(wp, "Basic_GOperation_Content.asp");
			redirect=0;
	   	}
	   	else if (flag==2)
			websWrite(wp, "Basic_GOperation_Content.asp");
	   	else if (flag==1)
			websWrite(wp, "Basic_HomeGateway_SaveRestart.asp");
	   	else
	   		websWrite(wp, "Main_Index_HomeGateway.asp");
	    }
	    else
	    {	
	   	/* disable to redirect page */
	   	if (redirect)
	   	{	
			websWrite(wp, "Basic_ROperation_Content.asp");
			redirect=0;
	   	}
	   	else if (flag==2)
			websWrite(wp, "Basic_ROperation_Content.asp");
	   	else if (flag==1)
			websWrite(wp, "Basic_Router_SaveRestart.asp");
	   	else
	   		websWrite(wp, "Main_Index_Router.asp");
	    }	
	}      
	else
	{
	    if (flag==2)
		websWrite(wp, "Basic_AOperation_Content.asp");
	    else if (flag==1)	
		websWrite(wp, "Basic_AccessPoint_SaveRestart.asp");
	    else	
	    	websWrite(wp, "Main_Index_AccessPoint.asp");
	}		
	strcpy(urlcache,"");					     	
}


/* Report sys up time */
static int
ej_uptime(int eid, webs_t wp, int argc, char_t **argv)
{

//	FILE *fp;
	char buf[MAX_LINE_SIZE];
//	unsigned long uptime;
	int ret;
	char *str = file2str("/proc/uptime");
	time_t tm;

	time(&tm);
	sprintf(buf, rfctime(&tm));

	if (str) {
		unsigned long up = atol(str);
		free(str);
		char lease_buf[128];
		memset(lease_buf, 0, sizeof(lease_buf));
		reltime(up, lease_buf);
		sprintf(buf, "%s(%s since boot)", buf, lease_buf);
	}

	ret = websWrite(wp, buf);  
	return ret;	    
}

static int
ej_sysuptime(int eid, webs_t wp, int argc, char_t **argv)
{
	int ret=0;
	char *str = file2str("/proc/uptime");

	if (str) {
		unsigned long up = atoi(str);
		free(str);

		char lease_buf[128];
		memset(lease_buf, 0, sizeof(lease_buf));
		reltime(up, lease_buf);
		ret = websWrite(wp, "%s since boot", lease_buf);
	}

	return ret;	    
}

struct lease_t {
	unsigned char chaddr[16];
	u_int32_t yiaddr;
	u_int32_t expires;
	char hostname[64];
};

int
websWriteCh(webs_t wp, char *ch, int count)
{
   int i, ret;
   
   ret = 0;
   for (i=0; i<count; i++)
      ret+=websWrite(wp, "%s", ch);
   return (ret);   
} 

/* Dump leases in <tr><td>MAC</td><td>IP</td><td>expires</td></tr> format */
static int
ej_dumpleases(int eid, webs_t wp, char *lease_file)
{
	FILE *fp;
	struct lease_t lease;
	int i;
	struct in_addr addr;
	unsigned long expires;
	int ret = 0;

    	ret +=websWrite(wp,"Mac Address       IP Address      Lease Time\n");	
	
	/* Parse leases file */
	if (!(fp = fopen(lease_file, "r"))) 
	{
		//websWrite(wp, "No leases\n");
		return -1;
	}
					    		   	
	while (fread(&lease, sizeof(lease), 1, fp)) {
		//ret += websWrite(wp, "%s", format);
		for (i = 0; i < 6; i++) {
			ret += websWrite(wp, "%02x", lease.chaddr[i]);
			if (i != 5) ret += websWrite(wp, ":");
		}
		addr.s_addr = lease.yiaddr;
		ret += websWrite(wp, " %s", inet_ntoa(addr));
		ret += websWriteCh(wp," ", 16-strlen(inet_ntoa(addr)));
		expires = ntohl(lease.expires);
		
		if (expires==0xffffffff)	
			ret += websWrite(wp, "manual");
		else if (!expires)
			ret += websWrite(wp, "expired");
		else {
			if (expires > 60*60*24) {
				ret += websWrite(wp, "%ld days, ", expires / (60*60*24));
				expires %= 60*60*24;
			}
			if (expires > 60*60) {
				ret += websWrite(wp, "%ld hours, ", expires / (60*60));
				expires %= 60*60;
			}
			if (expires > 60) {
				ret += websWrite(wp, "%ld minutes, ", expires / 60);
				expires %= 60;
			}
			ret += websWrite(wp, "%ld seconds", expires);
		}
		ret += websWrite(wp, "\n");
	}
	fclose(fp);

	return ret;
}

static int dump_file(webs_t wp, char *filename)
{
	FILE *fp;
	char buf[MAX_LINE_SIZE];
	int ret;

	fp = fopen(filename, "r");
		
	if (fp==NULL) 
	{
		ret+=websWrite(wp, "");
		return (ret);
	}

	ret = 0;
		
	while (fgets(buf, MAX_LINE_SIZE, fp)!=NULL)
	{	 	
	    ret += websWrite(wp, buf);
	}		    				     		
	 
	fclose(fp);		
	
	return (ret);
}

static int
ej_dump(int eid, webs_t wp, int argc, char_t **argv)
{	
//	FILE *fp;
//	char buf[MAX_LINE_SIZE];
	char filename[32];
	char *file,*script;
	int ret;

	if (ejArgs(argc, argv, "%s %s", &file, &script) < 2) {
		websError(wp, 400, "Insufficient args\n");
		return -1;
	}
	
		
	//csprintf("Script : %s, File: %s\n", script, file);
	
	// run scrip first to update some status
	if (strcmp(script,"")!=0) sys_script(script); 
 
	if (strcmp(file, "wlan11b.log")==0)
		return (ej_wl_status(eid, wp, 0, NULL));
	else if (strcmp(file, "wlan11b_2g.log")==0)
		return (ej_wl_status_2g(eid, wp, 0, NULL));
	else if (strcmp(file, "leases.log")==0) 
		return (ej_lan_leases(eid, wp, 0, NULL));
	else if (strcmp(file, "iptable.log")==0) 
		return (ej_nat_table(eid, wp, 0, NULL));
	else if (strcmp(file, "route.log")==0)
		return (ej_route_table(eid, wp, 0, NULL));
	else if (strcmp(file, "client_list.log")==0)
		return (ej_getclientlist(eid, wp, 0, NULL));
	else if (strcmp(file, "wps_info.log")==0)
	{
		if (nvram_match("wps_band", "0"))
			return (ej_wps_info(eid, wp, 0, NULL));
		else
			return (ej_wps_info_2g(eid, wp, 0, NULL));
	}
#if 0
	else if (strcmp(file, "apselect.log")==0)
		return (ej_getSiteSurvey(eid, wp, 0, NULL));
	else if (strcmp(file, "apscan")==0)
		return (ej_SiteSurvey(eid, wp, 0, NULL));
	else if (strcmp(file, "urelease")==0)
		return (ej_urelease(eid, wp, 0, NULL));
#endif
	ret = 0;
			   
	if (strcmp(file, "syslog.log")==0)
	{
	   	sprintf(filename, "/tmp/%s-1", file);
	   	ret+=dump_file(wp, filename); 
	}
	   			   
	sprintf(filename, "/tmp/%s", file);
	ret+=dump_file(wp, filename);					
	   
	return ret;	    
}	

static int
ej_load(int eid, webs_t wp, int argc, char_t **argv)
{	
	char *script;
	
	if (ejArgs(argc, argv, "%s", &script) < 1) {
		websError(wp, 400, "Insufficient args\n");
		return -1;
	}
	 	  
	sys_script(script);
	return (websWrite(wp,""));						    
}	

static void
validate_cgi(webs_t wp, int sid, int groupFlag)
{
    struct variable *v;    
    char *value;
    const char name[64];
	
    /* Validate and set variables in table order */
    for (v = GetVariables(sid); v->name != NULL; v++) 
    {
//    	 printf("Name: %s %d\n", v->name, sid);
		memset(name, 0, 64); //2008.08 magic
    	 sprintf(name, "%s", v->name);    
    		 	 
    	 if ((value = websGetVar(wp, name, NULL)))
	 {   
			//if (strcmp(v->longname, "Group")==0) //2008.08 magic
    	 if (!strcmp(v->longname, "Group")) 
	     {
    	      	
    	      }
    	      else 
    	      {
//		 printf("set: %s %s\n", v->name, value);
    		 nvram_set_x(GetServiceId(sid), v->name, value);
    	      }   
	 }     
    }
}

enum {
	NOTHING,
	REBOOT,
	RESTART,
};

char *svc_pop_list(char *value, char key)
{    
    char *v, *buf;
    int i;
	       
    if (value==NULL || *value=='\0')
       return (NULL);      
	    
    buf = value;
    v = strchr(buf, key);

    i = 0;
    
    if (v!=NULL)
    {    	
	*v = '\0';  	
	return (buf);    	   
    }    
    return (NULL);
}

//2008.08 magic {
extern char *read_whole_file(const char *target) {
	FILE *fp = fopen(target, "r");
	char *buffer, *new_str;
	int i;
	unsigned int read_bytes = 0;
	unsigned int each_size = 1024;
	
	if (fp == NULL)
		return NULL;
	
	buffer = (char *)malloc(sizeof(char)*each_size+read_bytes);
	if (buffer == NULL) {
		csprintf("No memory \"buffer\".\n");
		fclose(fp);
		return NULL;
	}
	memset(buffer, 0, sizeof(char)*each_size+read_bytes);
	
	while ((i = fread(buffer+read_bytes, sizeof(char), each_size, fp)) == each_size) {
		read_bytes += each_size;
		new_str = (char *)malloc(sizeof(char)*each_size+read_bytes);
		if (new_str == NULL) {
			csprintf("No memory \"new_str\".\n");
			free(buffer);
			fclose(fp);
			return NULL;
		}
		memset(new_str, 0, sizeof(char)*each_size+read_bytes);
		memcpy(new_str, buffer, read_bytes);
		
		free(buffer);
		buffer = new_str;
	}
	
	fclose(fp);
	return buffer;
}

static char post_buf[10000] = { 0 };
static char post_buf_backup[10000] = { 0 };

static void do_html_post_and_get(char *url, FILE *stream, int len, char *boundary) {
	char *query = NULL;
	
	init_cgi(NULL);
	
	memset(post_buf, 0, 10000);
	memset(post_buf_backup, 0, 10000);
	
	if (fgets(post_buf, MIN(len+1, sizeof(post_buf)), stream)) {
		len -= strlen(post_buf);
		
		while (len--)
			(void)fgetc(stream);
	}
	
	query = url;
	strsep(&query, "?");
	
	if (query && strlen(query) > 0) {
		if (strlen(post_buf) > 0)
			sprintf(post_buf_backup, "?%s&%s", post_buf, query);
		else
			sprintf(post_buf_backup, "?%s", query);
		
		sprintf(post_buf, "%s", post_buf_backup+1);
	}
	else if (strlen(post_buf) > 0)
		sprintf(post_buf_backup, "?%s", post_buf);
	
	websScan(post_buf_backup);
	init_cgi(post_buf);
}

static char *error_msg_console = NULL, *error_msg = NULL;

static char *get_msg_from_dict(char *lang, const char *const msg_name) {
#define MAX_FILE_LENGTH 512
	char current_dir[MAX_FILE_LENGTH];
	char dict_file[MAX_FILE_LENGTH], *dict_info;
	char *follow_info, *follow_info_end, *target;
	int len;
	
	if (lang == NULL || strlen(lang) == 0)
		lang = "EN";
	
	memset(dict_file, 0, MAX_FILE_LENGTH);
	memset(current_dir, 0, MAX_FILE_LENGTH);
	getcwd(current_dir, MAX_FILE_LENGTH);
	sprintf(dict_file, "%s/%s.dict", current_dir, lang);
	
	dict_info = read_whole_file(dict_file);
	if (dict_info == NULL) {
		csprintf("No dictionary file, \"%s\".\n", dict_file);
		return NULL;
	}
	
	follow_info = strstr(dict_info, msg_name);
	if (follow_info == NULL) {
		csprintf("No \"%s\" in the dictionary file.\n", msg_name);
		free(dict_info);
		return NULL;
	}
	
	follow_info += strlen(msg_name)+strlen("=");
	follow_info_end = follow_info;
	while (*follow_info_end != 0 && *follow_info_end != '\n')
		++follow_info_end;
	
	len = follow_info_end-follow_info;
	
	target = (char *)malloc(sizeof(char)*(len+1));
	if (target == NULL) {
		csprintf("No memory \"target\".\n");
		free(dict_info);
		return NULL;
	}
	if (len > 0)
		strncpy(target, follow_info, len);
	target[len] = 0;
	
	free(dict_info);
	return target;
}

static void show_error_msg(const char *const msg_num) {
	char msg_name[32];
	
	memset(msg_name, 0, 32);
	sprintf(msg_name, "ALERT_OF_ERROR_%s", msg_num);
	
	error_msg_console = get_msg_from_dict("", msg_name);
	error_msg = get_msg_from_dict(nvram_safe_get("preferred_lang"), msg_name);
	
	return;
}

static void clean_error_msg() {
	if (error_msg_console != NULL)
		free(error_msg_console);
	
	if (error_msg != NULL)
		free(error_msg);
	
	return;
}

int nvram_modified = 0;
int wl_modified = 0;
int rt_modified = 0;

static int validate_asp_apply(webs_t wp, int sid, int groupFlag) {
	struct variable *v;
	char *value;
	char name[64];
	char buff[100];
	
	/* Validate and set variables in table order */
	for (v = GetVariables(sid); v->name != NULL; ++v) {
		memset(name, 0, 64);
		sprintf(name, "%s", v->name);

		if ((value = websGetVar(wp, name, NULL))) {

			if (!strcmp(v->longname, "Group")) {
//printf("set sid: %s, name: %s, value: %s.\n", GetServiceId(sid), name, value);	// tmp test
				;
			}
			else if (strcmp(nvram_safe_get(name), value) && strncmp(v->name, "wsc", 3) && strncmp(v->name, "wps", 3)) {
//csprintf("set sid: %s, name: %s, value: %s.\n", GetServiceId(sid), v->name, value);	// tmp test

//				if (!strncmp(v->name, "wl_", 3))
//					dbg("$$$ name: %s, org value: %s, new value: %s, restart_needed_bits: %x\n", name, nvram_safe_get(name), value, v->event);
//				else if (!strncmp(v->name, "rt_", 3))
//					dbg("### name: %s, org value: %s, new value: %s, restart_needed_bits: %x\n", name, nvram_safe_get(name), value, v->event);
//				else
//					dbg("@@@ name: %s, org value: %s, new value: %s, restart_needed_bits: %x\n", name, nvram_safe_get(name), value, v->event);

				nvram_set_x(GetServiceId(sid), v->name, value);
				
// 2008.03 James. {
				if ((/*!strcmp(GetServiceId(sid), "General") && */!strcmp(v->name, "http_passwd")) || !strcmp(v->name, "telnetd"))
				{
					change_passwd = 1;
					eval("/sbin/start_telnetd");
				}
// 2008.03 James. }
				
// 2007.11 James {
				nvram_modified = 1;

				if (!wl_modified && !strncmp(v->name, "wl_", 3))
				{
					if (!strcmp(v->name, "wl_radio_x"))
					{
						if (!strncmp(value, "0", 2))
						{
							dbg("5G radio off!\n");
							doSystem("iwpriv ra0 set RadioOn=0");
						}
						else if (!strncmp(value, "1", 2))
						{
							dbg("5G radio on!\n");
							doSystem("iwpriv ra0 set RadioOn=1");
						}
					}
					else if (!strcmp(v->name, "wl_ssid2"))
					{
					}
					else
					{
						dbg("5G setting changed!\n");
						wl_modified = 1;
					}
				}

				if (!wl_modified)
				{
					if (	!strcmp(v->name, "TxPower") ||
						!strcmp(v->name, "TxBurst") ||
						!strcmp(v->name, "PktAggregate") ||
						!strcmp(v->name, "HT_OpMode") ||
						!strcmp(v->name, "HT_BW") ||
						!strcmp(v->name, "HT_GI") ||
						!strcmp(v->name, "HT_MCS") ||
						!strcmp(v->name, "HT_RDG") ||
						!strcmp(v->name, "HT_AMSDU") ||
						!strcmp(v->name, "HT_EXTCHA") ||
						!strcmp(v->name, "APSDCapable") ||
						!strcmp(v->name, "DLSCapable")	)
					{
						dbg("5G setting changed!!!\n");
						wl_modified = 1;
					}
				}

				if (!rt_modified && !strncmp(v->name, "rt_", 3) && strcmp(v->name, "rt_wsc_config_state"))
				{
					if (!strcmp(v->name, "rt_radio_x"))
					{
						if (!strncmp(value, "0", 2))
						{
							dbg("2.4G radio off!\n");
							doSystem("iwpriv rai0 set RadioOn=0");
						}
						else if (!strncmp(value, "1", 2))
						{
							dbg("2.4G radio on!\n");
							doSystem("iwpriv rai0 set RadioOn=1");
						}
					}
					else if (!strcmp(v->name, "rt_ssid2"))
					{
					}
					else
					{
						dbg("2.4G setting changed!\n");
						rt_modified = 1;
					}
				}

				if (!strcmp(v->name, "wl_ssid")) {
					memset(buff, 0, 100);
					char_to_ascii(buff, value);
					nvram_set("wl_ssid2", buff);
				}

				if (!strcmp(v->name, "rt_ssid")) {
					memset(buff, 0, 100);
					char_to_ascii(buff, value);
					nvram_set("rt_ssid2", buff);
				}

// 2007.11 James }
				
				if (v->event)
				{
					//printf("add restart needed bits\n");	// tmp test
					dbG("debug restart_needed_bits before: %ld 0x%lx\n", restart_needed_bits, restart_needed_bits);
					dbG("debug v->event: %ld 0x%lx\n", v->event, v->event);
					restart_needed_bits |= v->event;
					dbG("debug restart_needed_bits after: %ld 0x%lx\n", restart_needed_bits, restart_needed_bits);
				}
			}

			if (!strcmp(v->name, "sta_ssid"))
			{
				fprintf(stderr, "name: %s, value: %s.\n", name, websGetVar(wp, name, NULL));
				nvram_set("wl_ssid", value);
				memset(buff, 0, 100);
				char_to_ascii(buff, value);
				nvram_set("wl_ssid2", buff);
				nvram_set("r_Setting", "1");	// cancel redirect rules
			}
			else if (!strcmp(v->name, "sta_auth_mode"))
			{
				fprintf(stderr, "name: %s, value: %s.\n", name, websGetVar(wp, name, NULL));
				nvram_set("wl_auth_mode", value);
			}
			else if (!strcmp(v->name, "sta_wep_x"))
			{
				fprintf(stderr, "name: %s, value: %s.\n", name, websGetVar(wp, name, NULL));
				nvram_set("wl_wep_x", value);
			}
			else if (!strcmp(v->name, "sta_crypto"))
			{
				fprintf(stderr, "name: %s, value: %s.\n", name, websGetVar(wp, name, NULL));
				nvram_set("wl_crypto", value);
			}
			else if (!strcmp(v->name, "sta_wpa_mode"))
			{
				fprintf(stderr, "name: %s, value: %s.\n", name, websGetVar(wp, name, NULL));
				nvram_set("wl_wpa_mode", value);
			}
			else if (!strcmp(v->name, "sta_wpa_psk"))
			{
				fprintf(stderr, "name: %s, value: %s.\n", name, websGetVar(wp, name, NULL));
				nvram_set("wl_wpa_psk", value);
			}
			else if (!strcmp(v->name, "sta_key"))
			{
				fprintf(stderr, "name: %s, value: %s.\n", name, websGetVar(wp, name, NULL));
				nvram_set("wl_key", value);
			}
			else if (!strcmp(v->name, "sta_key_type"))
			{
				fprintf(stderr, "name: %s, value: %s.\n", name, websGetVar(wp, name, NULL));
				nvram_set("wl_key_type", value);
			}
			else if (!strcmp(v->name, "sta_key1"))
			{
				fprintf(stderr, "name: %s, value: %s.\n", name, websGetVar(wp, name, NULL));
				nvram_set("wl_key1", value);
			}
			else if (!strcmp(v->name, "sta_key2"))
			{
				fprintf(stderr, "name: %s, value: %s.\n", name, websGetVar(wp, name, NULL));
				nvram_set("wl_key2", value);
			}
			else if (!strcmp(v->name, "sta_key3"))
			{
				fprintf(stderr, "name: %s, value: %s.\n", name, websGetVar(wp, name, NULL));
				nvram_set("wl_key3", value);
			}
			else if (!strcmp(v->name, "sta_key4"))
			{
				fprintf(stderr, "name: %s, value: %s.\n", name, websGetVar(wp, name, NULL));
				nvram_set("wl_key4", value);
			}
		}
	}
    
	return nvram_modified;
}

int
pinvalidate(char *pin_string)
{
	unsigned long PIN = strtoul(pin_string, NULL, 10);
	unsigned long int accum = 0;
	unsigned int len = strlen(pin_string);

	if (len != 4 && len != 8)
		return  -1;

	if (len == 8) {
		accum += 3 * ((PIN / 10000000) % 10);
		accum += 1 * ((PIN / 1000000) % 10);
		accum += 3 * ((PIN / 100000) % 10);
		accum += 1 * ((PIN / 10000) % 10);
		accum += 3 * ((PIN / 1000) % 10);
		accum += 1 * ((PIN / 100) % 10);
		accum += 3 * ((PIN / 10) % 10);
		accum += 1 * ((PIN / 1) % 10);

		if (0 == (accum % 10))
			return 0;
	}
	else if (len == 4)
		return 0;

	return -1;
}

static int update_variables_ex(int eid, webs_t wp, int argc, char_t **argv) {
	int sid;
//	char *value;
	char *action_mode;
//	char *current_url;
//	char *next_url;
	char *sid_list;
//	char *value1;
	char *script;
	char groupId[64];
	char *groupName;
//	char *next_host;
//	char *removelist;
	int result;

	restart_needed_bits = 0;
	
	// assign control variables
	action_mode = websGetVar(wp, "action_mode", "");
//	next_host = websGetVar(wp, "next_host", "");
//	current_url = websGetVar(wp, "current_page", "");
//	next_url = websGetVar(wp, "next_page", "");
	script = websGetVar(wp, "action_script", "");
	sid_list = websGetVar(wp, "sid_list", "");
	
//	websWrite(wp, " ");	// for strange web write, must stay
	csprintf("Apply: [%s] [%s]\n", action_mode, script); //2009.01 magic for debug

	while ((serviceId = svc_pop_list(sid_list, ';'))) {
		sid = 0;
		while (GetServiceId(sid) != NULL) {
			if (!strcmp(GetServiceId(sid), serviceId))
				break;
			
			sid++;
		}
		
		if (serviceId != NULL) {
			if (!strcmp(action_mode, "  Save  ") || !strcmp(action_mode, " Apply ")) {
				if (!validate_asp_apply(wp, sid, TRUE)) {
					websWrite(wp, "<script>no_changes_and_no_committing();</script>\n");
				}
				else {
					nvram_set("x_Setting", "1");
					nvram_set("w_Setting", "1");	// J++
					websWrite(wp, "<script>done_committing();</script>\n");
				}
			}
			else if (!strcmp(action_mode, "Update")) {
				validate_asp_apply(wp, sid, TRUE);
			}
			else {
				strcpy(groupId, websGetVar(wp, "group_id", ""));
				
				if (strlen(action_mode) > 0) {
					groupName = groupId;
//printf("--- groupName: %s. ---\n", groupName);
//dbg("action mode: %s, group_id: %s\n", action_mode, groupName);
					if (!strcmp(action_mode, " Add ")) {
						result = apply_cgi_group(wp, sid, NULL, groupName, GROUP_FLAG_ADD);
						
						if (result == 1)
							nvram_set("page_modified", "1");
						
						websWrite(wp, "<script>done_validating(\"%s\");</script>\n", action_mode);
					}
					else if (!strcmp(action_mode, " Del ")) {
						result = apply_cgi_group(wp, sid, NULL, groupName, GROUP_FLAG_REMOVE);
						
						if (result == 1)
							nvram_set("page_modified", "1");
						
						websWrite(wp, "<script>done_validating(\"%s\");</script>\n", action_mode);
					}
					else if (!strcmp(action_mode, " Restart ")) {
						struct variable *v;
						
						for (v = GetVariables(sid); v->name != NULL; ++v) {
       	   					if (!strcmp(v->name, groupName))
       	   						break;
       					}

       					printf("--- Restart group %s. ---\n", groupName);
					dbG("debug restart_needed_bits before: %ld 0x%lx\n", restart_needed_bits, restart_needed_bits);
					dbG("debug v->event: %ld 0x%lx\n", v->event, v->event);	
					restart_needed_bits |= v->event;
					dbG("debug restart_needed_bits after: %ld 0x%lx\n", restart_needed_bits, restart_needed_bits);

					if (!strcmp(groupName, "RBRList") || !strcmp(groupName, "ACLList"))
						wl_modified = 1;
					if (!strcmp(groupName, "rt_RBRList") || !strcmp(groupName, "rt_ACLList"))
						rt_modified = 1;
						
						validate_asp_apply(wp, sid, FALSE);	// for some nvram with this group
						
						nvram_set("page_modified", "0");
						nvram_set("x_Setting", "1");
						nvram_set("w_Setting", "1");	// J++
						
						if (!strcmp(script, "goonsetting")) {
							websWrite(wp, "<script>done_validating(\"%s\");</script>\n", action_mode);
							script = "";
						}
						else {
							websWrite(wp, "<script>done_committing();</script>\n");
						}
					}

					validate_cgi(wp, sid, FALSE);	// for some nvram with this group group
				}
			}
		}

		sid_list = sid_list+strlen(serviceId)+1;
	}

	if (strlen(script) > 0) {
		printf("There is a script!: %s\n", script);	// tmp test
#ifdef DLM
		if (!strcmp(script, "usbfs_check")) {
			nvram_set("st_tool_t", "/bin/fscheck");
			nvram_set("st_toolstatus_t", "USBstarting");
			nvram_set("st_time_t", "3");

			websRedirect(wp, "Checking.asp");
			deliver_signal("/var/run/infosvr.pid", SIGUSR1);
			return 0;
		}
		else if (!strcmp(script, "usbfs_eject1"))
			umount_disc_parts(1);
		else if (!strcmp(script, "usbfs_eject2"))
			umount_disc_parts(2);
		else
#endif
#ifdef Ralink_WPS
		if (!strcmp(script, "WPS_apply"))
		{
			if (	nvram_match("wl_auth_mode", "shared") ||
				nvram_match("wl_auth_mode", "wpa") ||
				nvram_match("wl_auth_mode", "wpa2") ||
				nvram_match("wl_auth_mode", "radius") ||
				!nvram_match("wan_route_x", "IP_Routed"))
			{
				goto WPS_refresh;
			}

			char wps_enable_old[3];
			strcpy(wps_enable_old, nvram_safe_get("wps_enable"));
			
			nvram_set("wps_enable", websGetVar(wp, "wps_enable",""));
			if (websGetVar(wp, "wps_mode", NULL))
				nvram_set("wps_mode", websGetVar(wp, "wps_mode", NULL));

			if (nvram_match("wps_enable", "0"))
			{
				nvram_set("wps_start_flag", "0");
				csprintf("Stopping wscd...\n");
				doSystem("killall -%d watchdog", SIGTSTP);
				//kill_pidfile_s("/var/run/watchdog.pid", SIGTSTP);
			}
			else if (strcmp(wps_enable_old, websGetVar(wp, "wps_enable","")))
			{
				nvram_set("wps_start_flag", "1");
			csprintf("Starting wscd...\n");
			csprintf("wps_enable:%s\n", nvram_safe_get("wps_enable"));
				doSystem("killall -%d watchdog", SIGTSTP);
				//kill_pidfile_s("/var/run/watchdog.pid", SIGTSTP);
				sleep(1);
			}
			else
			{
				char *wps_pin = websGetVar(wp, "wps_pin", NULL);
				nvram_set("wps_start_flag", "2");
				if (nvram_match("wps_mode", "1"))
				{
					csprintf("WPS: PIN\n");
					if (wps_pin /*&& (strlen(wps_pin) == 8)*/)
					{
						if (!pinvalidate(wps_pin))
						{
							csprintf("Pin: %s\n", wps_pin);
							nvram_set("wps_pin_web", wps_pin);
						}
						else
						{
							csprintf("Invalid pincode: %s\n", wps_pin ? wps_pin : "NULL");
							goto WPS_refresh;
						}
					}
					else
						nvram_set("wps_pin_web", "");
				}
				else
					csprintf("WPS: PBC\n");

				doSystem("killall -%d watchdog", SIGTSTP);
				//kill_pidfile_s("/var/run/watchdog.pid", SIGTSTP);
				sleep(3);
			}

			goto WPS_refresh;
			//return 0;
		}
		else if (!strcmp(script, "Switch_band")) { // jerry5chang added to switch wps working band for n56u
			if (nvram_match("wps_band", "1"))
				 nvram_set("wps_band", "0");
			else
				 nvram_set("wps_band", "1");
		}// end jerry5chang
		else if (!strcmp(script, "Reset_OOB"))
		{
			if (	nvram_match("wl_auth_mode", "shared") ||
				nvram_match("wl_auth_mode", "wpa") ||
				nvram_match("wl_auth_mode", "wpa2") ||
				nvram_match("wl_auth_mode", "radius") ||
				!nvram_match("wan_route_x", "IP_Routed"))
				return 0;

		//2009.01 magic{
			fprintf(stderr, "\nReset OOB\n\n");
			if (websGetVar(wp, "wps_mode", NULL))
			nvram_set("wps_mode", websGetVar(wp, "wps_mode", NULL));
    			nvram_set("wps_oob_flag", "1");
    			doSystem("killall -%d watchdog", SIGTSTP);
    			sleep(2);
			return 0;
		//2009.01 magic}
		}
		else if (!strcmp(script, "WPS_push_button"))
		{
			if (	nvram_match("wl_auth_mode", "shared") ||
				nvram_match("wl_auth_mode", "wpa") ||
				nvram_match("wl_auth_mode", "wpa2") ||
				nvram_match("wl_auth_mode", "radius") ||
				!nvram_match("wan_route_x", "IP_Routed"))
				return 0;

			csprintf("WPS: NM PBC 2.4G\n");
			nvram_set("wps_start_flag", "3");
			
			doSystem("killall -%d watchdog", SIGTSTP);
			//kill_pidfile_s("/var/run/watchdog.pid", SIGTSTP);
			return 0;
		}
		else if (!strcmp(script, "WPS_push_button_5g"))
		{
			if (	nvram_match("wl_auth_mode", "shared") ||
				nvram_match("wl_auth_mode", "wpa") ||
				nvram_match("wl_auth_mode", "wpa2") ||
				nvram_match("wl_auth_mode", "radius") ||
				!nvram_match("wan_route_x", "IP_Routed"))
				return 0;

			csprintf("WPS: NM PBC 5G\n");
			nvram_set("wps_start_flag", "4");
			
			doSystem("killall -%d watchdog", SIGTSTP);
			//kill_pidfile_s("/var/run/watchdog.pid", SIGTSTP);
			return 0;
		}
		else
#endif
		//2008.07.24 Yau add
		if (!strcmp(script, "networkmap_refresh")) {
			printf("@@@ restarting networkmap!!!\n");
			eval("restart_networkmap");
			sleep(1);
			websWrite(wp, "<script>restart_needed_time(%d);</script>\n", 1);
/*
			//signal to networkmap 
			printf("@@@ Signal to networkmap!!!\n");	// tmp test
			doSystem("killall -%d networkmap", SIGUSR1);

			FILE *fp = fopen("/tmp/static_ip.inf", "r");
			char buf[256];
			int row = 10;
			
			if (fp != NULL)
				while (fgets(buf, 256, fp) != NULL)
					++row;

			//printf("needed time(2) = %d\n", row);	// tmp test
			//websWrite(wp, "<script>restart_needed_time(%d);</script>\n", row);
			websWrite(wp, "<script>restart_needed_time(%d);</script>\n", row*15);	// ASUS
*/
		}
		else if (!strcmp(script, "mfp_monopolize"))
		{
			//printf("[httpd] handle monolopolize here\n");	// tmp test
			sys_script(script);
			websWrite(wp, "<script>restart_needed_time(3);</script>\n");
			return 0;
		}
		else if (!strcmp(script, "update_UPnP")) // jerry5chang
		{
/*
			printf("### Media Server: refresh media content ###\n"); // tmp test
			if (pids("ushare"))
				kill_pidfile_s("/var/run/ushare.pid", SIGHUP);
*/
		}
		else //End of Yau add
		{
			sys_script(script);
			//goto WPS_refresh;
		}

		websWrite(wp, "<script>done_committing();</script>\n");
		
		return 0;
	}
	
WPS_refresh:

	printf("chk restart bits=%d 0x%x\n", restart_needed_bits, restart_needed_bits);	// tmp test
	if (restart_needed_bits != 0 && (!strcmp(action_mode, " Apply ") || !strcmp(action_mode, " Restart ") || !strcmp(action_mode, " WPS_Apply "))) {
		if ((restart_needed_bits & RESTART_REBOOT) != 0) {
csprintf("*** set ITVL_RESTART_REBOOT(%d).\n", ITVL_RESTART_REBOOT);
			restart_tatal_time = ITVL_RESTART_REBOOT;
		}
		else if ((restart_needed_bits & RESTART_NETWORKING) != 0) {
csprintf("*** set ITVL_RESTART_NETWORKING(%d).\n", ITVL_RESTART_NETWORKING);
			restart_tatal_time = ITVL_RESTART_NETWORKING;
		}
		else {
			if ((restart_needed_bits & RESTART_FTPSAMBA) != 0) {
csprintf("*** add ITVL_RESTART_FTPSAMBA(%d).\n", ITVL_RESTART_FTPSAMBA);
				restart_tatal_time += ITVL_RESTART_FTPSAMBA;
			}
			if ((restart_needed_bits & RESTART_DDNS) != 0) {
csprintf("*** add ITVL_RESTART_DDNS(%d).\n", ITVL_RESTART_DDNS);
				restart_tatal_time += ITVL_RESTART_DDNS;
			}
			if ((restart_needed_bits & RESTART_HTTPD) != 0) {
csprintf("*** add ITVL_RESTART_HTTPD(%d).\n", ITVL_RESTART_HTTPD);
				restart_tatal_time += ITVL_RESTART_HTTPD;
			}
			if ((restart_needed_bits & RESTART_DNS) != 0) {
csprintf("*** add ITVL_RESTART_DNS(%d).\n", ITVL_RESTART_DNS);
				restart_tatal_time += ITVL_RESTART_DNS;
			}
			if ((restart_needed_bits & RESTART_DHCPD) != 0) {
csprintf("*** add ITVL_RESTART_DHCPD(%d).\n", ITVL_RESTART_DHCPD);
				restart_tatal_time += ITVL_RESTART_DHCPD;
			}
			if ((restart_needed_bits & RESTART_UPNP) != 0) {
csprintf("*** add ITVL_RESTART_UPNP(%d).\n", ITVL_RESTART_UPNP);
				restart_tatal_time += ITVL_RESTART_UPNP;
			}
                        if ((restart_needed_bits & RESTART_DMS) != 0) {
csprintf("*** add ITVL_RESTART_DMS(%d).\n", ITVL_RESTART_DMS);
                                restart_tatal_time += ITVL_RESTART_DMS;
                        }
                        if ((restart_needed_bits & RESTART_ITUNES) != 0) {
csprintf("*** add ITVL_RESTART_ITUNES(%d).\n", ITVL_RESTART_ITUNES);
                                restart_tatal_time += ITVL_RESTART_ITUNES;
			}
			if ((restart_needed_bits & RESTART_QOS) != 0) {
csprintf("*** add ITVL_RESTART_QOS(%d).\n", ITVL_RESTART_QOS);
				restart_tatal_time += ITVL_RESTART_QOS;
			}
			if ((restart_needed_bits & RESTART_VPN) != 0) {
csprintf("*** add ITVL_RESTART_VPN(%d).\n", ITVL_RESTART_VPN);
				restart_tatal_time += ITVL_RESTART_VPN;
			}
                        if ((restart_needed_bits & RESTART_SWITCH) != 0) {
csprintf("*** add ITVL_RESTART_SIWTCH(%d).\n", ITVL_RESTART_SWITCH);
                                restart_tatal_time += ITVL_RESTART_SWITCH;
                        }
			if ((restart_needed_bits & RESTART_SYSLOG) != 0) {
csprintf("*** add ITVL_RESTART_SYSLOG(%d).\n", ITVL_RESTART_SYSLOG);
				restart_tatal_time += ITVL_RESTART_SYSLOG;
			}
			if ((restart_needed_bits & RESTART_FIREWALL) != 0) {
csprintf("*** add ITVL_RESTART_FIREWALL(%d).\n", ITVL_RESTART_FIREWALL);
				restart_tatal_time += ITVL_RESTART_FIREWALL;
			}
			if ((restart_needed_bits & RESTART_NTPC) != 0) {
csprintf("*** add ITVL_RESTART_NTPC(%d).\n", ITVL_RESTART_NTPC);
				restart_tatal_time += ITVL_RESTART_NTPC;
			}
			if ((restart_needed_bits & RESTART_NAS) != 0) {
csprintf("*** add ITVL_RESTART_NAS(%d).\n", ITVL_RESTART_NAS);
				restart_tatal_time += ITVL_RESTART_NAS;
			}
			if ((restart_needed_bits & RESTART_NASDOCK) != 0) {
csprintf("*** add ITVL_RESTART_NASDOCK(%d).\n", ITVL_RESTART_NASDOCK);
				restart_tatal_time += ITVL_RESTART_NASDOCK;
			}
// 2008.01 James. {
			if ((restart_needed_bits & RESTART_TIME) != 0) {
csprintf("*** add ITVL_RESTART_TIME(%d).\n", ITVL_RESTART_TIME);
				restart_tatal_time += ITVL_RESTART_TIME;
			}
/*
                        if ((restart_needed_bits & RESTART_RSTATS) != 0) {
                                notify_rc("restart_rstats");
                                restart_needed_bits &= ~(u32)RESTART_RSTATS;
                        }
*/
			if ((restart_needed_bits & RESTART_WPS) != 0) {
csprintf("*** add ITVL_RESTART_WPS(%d).\n", ITVL_RESTART_WPS);
				/*char *wsc_mode = nvram_safe_get("wsc_mode");
				char *old_wsc_mode = nvram_safe_get("old_wsc_mode");
				
				if (nvram_match("wsc_config_state", "0") && nvram_match("wsc_proc_status", "0"))
					restart_tatal_time += ITVL_RESTART_WPS*3;
				else if (strcmp(wsc_mode, old_wsc_mode) != 0 && !strcmp(wsc_mode, "enabled"))
					restart_tatal_time += ITVL_RESTART_WPS*3;
				else*/
					restart_tatal_time += ITVL_RESTART_WPS;
			}
			if ((restart_needed_bits & RESTART_APCLI) != 0) {
csprintf("*** add ITVL_RESTART_APCLI(%d).\n", ITVL_RESTART_APCLI);
				restart_tatal_time += ITVL_RESTART_APCLI;
			}
			if ((restart_needed_bits & RESTART_WIFI) != 0) {
				if (wl_modified == 1)
				{
csprintf("*** add ITVL_RESTART_WIFI(%d).\n", ITVL_RESTART_WIFI);
					restart_tatal_time += ITVL_RESTART_WIFI;
				}
				else if (rt_modified == 1)
				{
csprintf("*** add ITVL_RESTART_WIFI(%d).\n", ITVL_RESTART_WIFI);
					restart_tatal_time += ITVL_RESTART_WIFI;
				}
			}
// 2008.01 James. }
		}
		
csprintf("*** restart_time needs %d seconds.\n", restart_tatal_time);
		printf("needed time(3) = %d\n", restart_tatal_time);	// tmp test
		websWrite(wp, "<script>restart_needed_time(%d);</script>\n", restart_tatal_time);
	}
	
	return 0;
}

static int convert_asus_variables(int eid, webs_t wp, int argc, char_t **argv) {
	if (restart_needed_bits != 0) {
csprintf("*** run convert_asus_variables.\n");
			eval("/sbin/convert_asus_values");
			
			return 0;
	}
	
csprintf("*** Don't run convert_asus_variables.\n");
	
	return 0;
}

static int asus_nvram_commit(int eid, webs_t wp, int argc, char_t **argv) {
	if (restart_needed_bits != 0 || nvram_modified == 1) {
csprintf("*** run asus_nvram_commit!\n");
		nvram_modified = 0;

		nvram_commit_safe();
	}
	else
csprintf("*** Don't run asus_nvram_commit!\n");
	
	return 0;
}

extern long uptime(void);

static void
set_reboot_timestamp()
{
	time_t reboot_timestamp;
	char reboot_timestampstr[32];

	reboot_timestamp = uptime();
	memset(reboot_timestampstr, 0, 32);
	sprintf(reboot_timestampstr, "%lu", reboot_timestamp);
	nvram_set("reboot_timestamp", reboot_timestampstr);
}

static int ej_notify_services(int eid, webs_t wp, int argc, char_t **argv) {
	restart_tatal_time = 0;
	int no_run_str = 0;

	dbG("debug restart_needed_bits: %ld 0x%lx\n", restart_needed_bits, restart_needed_bits);

	//printf("notify service: %d\n", restart_needed_bits);	// tmp test
	if (restart_needed_bits != 0) {
		no_run_str = 1;
		if ((restart_needed_bits & RESTART_REBOOT) != 0) {
			printf("*** Run notify_rc restart_reboot! \n");

			set_reboot_timestamp();
			sys_reboot();
		}
		else if ((restart_needed_bits & RESTART_NETWORKING) != 0) {
			csprintf("*** run notify_rc restart_networking! \n");
			notify_rc("restart_networking");
		}
		else {
			dbG("debug restart_needed_bits before: %ld 0x%lx\n", restart_needed_bits, restart_needed_bits);

			if ((restart_needed_bits & RESTART_FTPSAMBA) != 0) {
				notify_rc("restart_cifs");
				restart_needed_bits &= ~(u32)RESTART_FTPSAMBA;
			}
			if ((restart_needed_bits & RESTART_DDNS) != 0) {
				notify_rc("restart_ddns");
				restart_needed_bits &= ~(u32)RESTART_DDNS;
			}
			if ((restart_needed_bits & RESTART_HTTPD) != 0) {
				notify_rc("restart_httpd");
				restart_needed_bits &= ~(u32)RESTART_HTTPD;
			}
			if ((restart_needed_bits & RESTART_DNS) != 0) {
				csprintf("*** run notify_rc restart_dns! \n");
				notify_rc("restart_dns");
				restart_needed_bits &= ~(u32)RESTART_DNS;
			}
			if ((restart_needed_bits & RESTART_DHCPD) != 0) {
				csprintf("*** run notify_rc restart_dhcpd! \n");
				notify_rc("restart_dhcpd");
				restart_needed_bits &= ~(u32)RESTART_DHCPD;
			}
			if ((restart_needed_bits & RESTART_UPNP) != 0) {
				csprintf("*** run notify_rc restart_upnp! \n");
				notify_rc("restart_upnp");
				restart_needed_bits &= ~(u32)RESTART_UPNP;
			}
                        if ((restart_needed_bits & RESTART_DMS) != 0) {
                                csprintf("*** run notify_rc restart_dms! \n");
                                notify_rc("restart_dms");
                                restart_needed_bits &= ~(u32)RESTART_DMS;
                        }
                        if ((restart_needed_bits & RESTART_ITUNES) != 0) {
                                csprintf("*** run notify_rc restart_itunes! \n");
                                notify_rc("restart_itunes");
                                restart_needed_bits &= ~(u32)RESTART_ITUNES;
                        }
			if ((restart_needed_bits & RESTART_QOS) != 0) {
				csprintf("*** run notify_rc restart_qos! \n");
				notify_rc("restart_qos");
				restart_needed_bits &= ~(u32)RESTART_QOS;
			}
			if ((restart_needed_bits & RESTART_VPN) != 0) {
				csprintf("*** run notify_rc restart_vpn! \n");
				notify_rc("restart_vpn_pt");
				restart_needed_bits &= ~(u32)RESTART_VPN;
			}
                        if ((restart_needed_bits & RESTART_SWITCH) != 0) {
                                csprintf("*** run notify_rc restart_switch_config! \n");
                                notify_rc("restart_switch_config");
                                restart_needed_bits &= ~(u32)RESTART_SWITCH;
                        }
			if ((restart_needed_bits & RESTART_SYSLOG) != 0) {
				notify_rc("restart_syslog");
				restart_needed_bits &= ~(u32)RESTART_SYSLOG;
			}
			if ((restart_needed_bits & RESTART_FIREWALL) != 0) {
				notify_rc("restart_firewall");
				restart_needed_bits &= ~(u32)RESTART_FIREWALL;
			}
			if ((restart_needed_bits & RESTART_NTPC) != 0) {
				notify_rc("restart_ntpc");
				restart_needed_bits &= ~(u32)RESTART_NTPC;
			}
			if ((restart_needed_bits & RESTART_NAS) != 0) {
				notify_rc("restart_nas");
				restart_needed_bits &= ~(u32)RESTART_NAS;
			}
			if ((restart_needed_bits & RESTART_NASDOCK) != 0) {
				notify_rc("restart_nasdock");
				restart_needed_bits &= ~(u32)RESTART_NASDOCK;
			}
			if ((restart_needed_bits & RESTART_TIME) != 0) {
				notify_rc("restart_time");
				restart_needed_bits &= ~(u32)RESTART_TIME;
			}
                        if ((restart_needed_bits & RESTART_RSTATS) != 0) {
				printf("*** add ITVL_RESTART_RSTATS(%d).\n", ITVL_RESTART_RSTATS);
                                restart_tatal_time += ITVL_RESTART_RSTATS;
                        }

#ifdef WSC
			if ((restart_needed_bits & RESTART_WPS) != 0) {
				notify_rc("restart_wps");
				restart_needed_bits &= ~(u32)RESTART_WPS;
			}
#endif
			if ((restart_needed_bits & RESTART_APCLI) != 0) {
				notify_rc("restart_apcli");
				restart_needed_bits &= ~(u32)RESTART_APCLI;
			}

			if ((restart_needed_bits & RESTART_WIFI) != 0) {
				nvram_set("wps_enable", "0");
				nvram_set("wps_start_flag", "0");
				doSystem("killall -%d watchdog", SIGTSTP);

				sleep(1);

				if (wl_modified == 1)
				{
//dbg("notify_rc: restart_wifi\n");
					wl_modified = 0;
					notify_rc("restart_wifi");
				}

				if (rt_modified == 1)
				{
//dbg("notify_rc: restart_wifi_rt\n");
					rt_modified = 0;
					notify_rc("restart_wifi_rt");
				}

				restart_needed_bits &= ~(u32)RESTART_WIFI;
			}

			dbG("debug restart_needed_bits after: %ld 0x%lx\n", restart_needed_bits, restart_needed_bits);
		}
		
		restart_needed_bits = 0;
	}
	
	if (no_run_str == 0)
		printf("*** Don't run ej_notify_services!\n");
	
	return 0;
}

// for error_page.htm's detection
static int detect_if_wan(int eid, webs_t wp, int argc, char_t **argv) {
	int if_wan = is_phyconnected();
	
	websWrite(wp, "%d", if_wan);
	
	return 0;
}

char *Ch_conv(char *proto_name, int idx)
{
        char *proto;
        char qos_name_x[32];
        sprintf(qos_name_x, "%s%d", proto_name, idx);
        if (nvram_match(qos_name_x,""))
        {
                return NULL;
        }
        else
        {
                proto=nvram_get(qos_name_x);
                return proto;
        }
}

static int check_hwnat(int eid, webs_t wp, int argc, char_t **argv)
{
	DIR *dir_to_open = NULL;

	dir_to_open = opendir("/sys/module/hw_nat");
	if (dir_to_open)
	{
		closedir(dir_to_open);
		websWrite(wp, "1");
	}

	websWrite(wp, "0");
}

static int is_dm_running()
{
        if (    pids("rtorrent") ||
                pids("dmathined") ||
                pids("giftd") ||
                pids("snarf")   )
        {
                return 1;
        }
        else
                return 0;
}

static int dm_running(int eid, webs_t wp, int argc, char_t **argv) {
	if (is_dm_running())
		websWrite(wp, "1");
	else
		websWrite(wp, "0");
}

/*static int detect_wan_connection(int eid, webs_t wp, int argc, char_t **argv) {
	int ret =	detectWAN();
	
	if (ret < 0)
		websWrite(wp, "0");	// can not go out of LAN.
	else
		websWrite(wp, "1");	// can go out of LAN.
	
	return 0;
}*/

#if 0 // 2010.09 James.
static int detect_wan_connection(int eid, webs_t wp, int argc, char_t **argv) {
	int MAX_LOOKUP_NUM = 3, lookup_num;

	for (lookup_num = 0; lookup_num < MAX_LOOKUP_NUM; ++lookup_num) {
		if (nvram_match("ntp_ready", "1")) {
			websWrite(wp, "1");	// can go out of LAN.
			break;
		}

		if (lookup_num == MAX_LOOKUP_NUM-1) {
//			csprintf("Can't get the host from ntp!\n");
			websWrite(wp, "-1");      // can not go out of LAN.
			break;
		}

//		csprintf("Sleep 3 seconds...\n");
//		sleep(3);
	}

	return 0;
}
#else
// Define of return value: 1st bit is NTP, 2nd bit is WAN DNS, 3rd bit is more open DNS.
static int detect_wan_connection(int eid, webs_t wp, int argc, char_t **argv) {
	int MAX_LOOKUP_NUM = 1, lookup_num;
	//int got_ntp = 0, got_ping = 0;
	int result = 0;
	char target[16];
	FILE *fp;
	char buf[128], word[16], *next;
	char *ping_cmd[] = {"ping", word, "-c", "1", NULL};
	char *dns_list = NULL;
	int i;
	char *MORE_DNS = "8.8.8.8 208.67.220.220 8.8.4.4 208.67.222.222";
	
	memset(buf, 0, 128);
	
	for(lookup_num = 0; lookup_num < MAX_LOOKUP_NUM; ++lookup_num) {
		if (nvram_match("ntp_ready", "1"))
			//got_ntp = 1;
			result += 1;

		if (nvram_match("wan_proto", "static"))
			dns_list = nvram_safe_get("wan_dns_t");
		else
			dns_list = nvram_safe_get("wan0_dns");

		foreach(word, dns_list, next) {
			dbg("Try to ping dns: %s...\n", word);
			_eval(ping_cmd, ">/tmp/log.txt", 0, NULL);
			
			if ((fp = fopen("/tmp/log.txt", "r")) == NULL)
				continue;
			
			for(i = 0; i < 2 && fgets(buf, 128, fp) != NULL; ++i) {
				dbg("%d. Got the results: %s.\n", i+1, buf);
				if (strstr(buf, "alive") || strstr(buf, " ms"))
					//got_ping = 1;
					result += 2;
				
				//if (got_ping)
				if (result >= 2)
					break;
			}
			fclose(fp);
			
			//if (got_ping)
			if (result >= 2)
				break;
		}

//		dbg("Try to ping more dns: %s...\n", MORE_DNS);
		dbg("Try to check more dns: %s...\n", MORE_DNS);
		int dns_test = 0;
		foreach(word, MORE_DNS, next) {
			if (dns_test == 0 && !strcmp(nvram_safe_get("dns_test"), "1")) {
				dns_test = 1;
				continue;
			}
			dbg("Try to ping dns: %s...\n", word);
			
//			_eval(ping_cmd, ">/tmp/log.txt", 0, NULL);
			doSystem("/usr/sbin/tcpcheck 4 %s:53 >/tmp/log.txt", word);
			
			if ((fp = fopen("/tmp/log.txt", "r")) == NULL)
				continue;
			
			for(i = 0; i < 2 && fgets(buf, 128, fp) != NULL; ++i) {
				dbg("%d. Got the results: %s.\n", i+1, buf);
				if (strstr(buf, "alive")/* || strstr(buf, " ms")*/)
					//got_ping = 1;
					result += 4;
				
				//if (got_ping)
				if (result >= 4)
					break;
			}
			fclose(fp);
			
			//if (got_ping)
			if (result >= 4)
				break;
		}
		
		/*if (got_ntp && got_ping) {
			websWrite(wp, "3");
			break;
		}
		else if (got_ntp && !got_ping) {
			websWrite(wp, "2");
			break;
		}
		else if (!got_ntp && got_ping) {
			websWrite(wp, "1");
			break;
		}//*/
		if (result > 0) {
			websWrite(wp, "%d", result);
			break;
		}
		else if (lookup_num == MAX_LOOKUP_NUM-1) {
			dbg("Can't get the host from ntp or response from DNS!\n");
			websWrite(wp, "-1");
			break;
		}
	}
	
	return 0;
}
#endif

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

static int detect_dhcp_pppoe(int eid, webs_t wp, int argc, char_t **argv) {
	int ret;
//	printf("#### [hook] detect_dhcp_pppoe ####");
	eprintf("detect dhcp pppoe\n");	// tmp test
	if (nvram_match("wan_nat_x", "1"))       // Gateway mode
		;
	else if (nvram_match("wan_route_x", "IP_Routed"))	// Router mode
		;
	else {   // AP mode
		websWrite(wp, "AP mode");
		return 0;
	}

	/*if (nvram_match("wan_status_t", "Connected")) {
		if (!nvram_match("hsdpa_product", ""))
			websWrite(wp, "3.5G HSDPA");
		else
			websWrite(wp, "%s", nvram_safe_get("wan_proto"));
	}*/
	// jerry5 edited for n56u
#ifndef RTCONFIG_USB_MODEM
	if (nvram_match("wan_proto","3g") && nvram_match("sw_mode","1"))
#else
	//if(!nvram_match("modem_enable", "0") && (nvram_match("usb_path1", "modem") || nvram_match("usb_path2", "modem")))
	if(get_usb_modem_state())
#endif
		websWrite(wp, "modem");
	else {
		eprintf("discover all\n");			// tmp test
		ret = discover_all();

		eprintf("get result: %d\n", ret);
		if (ret == 0)
		{
			websWrite(wp, "no-respond");
		}
		else if (ret == 1)
		{
			websWrite(wp, "dhcp");
		}
		else if (ret == 2)
		{
			websWrite(wp, "pppoe");
		}
		else  // -1
		{
			websWrite(wp, "error");
		}
	}

	return 0;
}

static int get_wan_status_log(int eid, webs_t wp, int argc, char_t **argv) {
	FILE *fp = fopen("/tmp/wanstatus.log", "r");
	char log_info[64];
	int i;
	
	memset(log_info, 0, 64);
	
	if (fp != NULL) {
		fgets(log_info, 64, fp);
		
		i = 0;
		while (log_info[i] != 0) {
			if (log_info[i] == '\n') {
				log_info[i] = 0;
				break;
			}
			
			++i;
		}
		
		websWrite(wp, "%s", log_info);
		fclose(fp);
	}
	
	return 0;
}

int file_to_buf(char *path, char *buf, int len) {
	FILE *fp;
	memset(buf, 0 , len);
	
	if ((fp = fopen(path, "r")) != NULL) {
		fgets(buf, len, fp);
		fclose(fp);
		
		return 1;
	}
	
	return 0;
}

int get_ppp_pid(char *conntype) {
	int pid = -1;
	char tmp[80], tmp1[80];
	
	snprintf(tmp, sizeof(tmp), "/var/run/%s.pid", conntype);
	file_to_buf(tmp, tmp1, sizeof(tmp1));
	pid = atoi(tmp1);
	
	return pid;
}

/* Find process name by pid from /proc directory */
char *find_name_by_proc(int pid) {
	FILE *fp;
	char line[254];
	char filename[80];
	static char name[80];
	
	snprintf(filename, sizeof(filename), "/proc/%d/status", pid);
	
	if ((fp = fopen(filename, "r")) != NULL) {
		fgets(line, sizeof(line), fp);
		/* Buffer should contain a string like "Name:   binary_name" */
		sscanf(line, "%*s %s", name);
		fclose(fp);
		return name;
	}
	
	return "";
}

int check_ppp_exist() {
	DIR *dir;
	struct dirent *dent;
	char task_file[64], cmdline[64];
	int pid, fd;
	
	if ((dir = opendir("/proc")) == NULL) {
		perror("open proc");
		return -1;
	}
	
	while((dent = readdir(dir)) != NULL) {
		if ((pid = atoi(dent->d_name)) > 1) {
			memset(task_file, 0, 64);
			sprintf(task_file, "/proc/%d/cmdline", pid);
			if ((fd = open(task_file, O_RDONLY)) > 0) {
				memset(cmdline, 0, 64);
				read(fd, cmdline, 64);
				close(fd);
				
				if (strstr(cmdline, "pppoecd")
						|| strstr(cmdline, "pppd")
						) {
					closedir(dir);
					return 0;
				}
			}
			else
				printf("cannot open %s\n", task_file);
		}
	}
	closedir(dir);
	
	return -1;
}

#if 0
int check_subnet() {
	char lan_ipaddr_t[16];
	char lan_netmask_t[16];
//	static 
	unsigned int lan_subnet = 0;
	char wan_ipaddr_t[16];
	char wan_netmask_t[16];
//	static 
	unsigned int wan_subnet = 0;

//	if (lan_subnet == 0)
	{
		memset(lan_ipaddr_t, 0, 16);
		strcpy(lan_ipaddr_t, nvram_safe_get("lan_ipaddr_t"));
		memset(lan_netmask_t, 0, 16);
		strcpy(lan_netmask_t, nvram_safe_get("lan_netmask_t"));
		lan_subnet = inet_network(lan_ipaddr_t)&inet_network(lan_netmask_t);
//		printf("httpd get lan_subnet=%x!\n", lan_subnet);
	}
	
//	if (wan_subnet == 0)
	{
		memset(wan_ipaddr_t, 0, 16);
		memset(wan_netmask_t, 0, 16);

		if (nvram_match("wan0_proto", "dhcp"))
		{
			strcpy(wan_ipaddr_t, nvram_safe_get("wan_ipaddr_tmp"));
			strcpy(wan_netmask_t, nvram_safe_get("wan_netmask_tmp"));
		}	
		else
		{
			strcpy(wan_ipaddr_t, nvram_safe_get("wan_ipaddr_t"));
			strcpy(wan_netmask_t, nvram_safe_get("wan_netmask_t"));
		}
		wan_subnet = inet_network(wan_ipaddr_t)&inet_network(wan_netmask_t);
//		printf("httpd get wan_subnet=%x!\n", wan_subnet);
	}
	
	return (lan_subnet == wan_subnet);
}
#else
int check_subnet() {
	if (!strcmp(nvram_safe_get("wan_subnet_t"), nvram_safe_get("lan_subnet_t")))
		return 1;
	else
		return 0;
}
#endif

static int wanlink_hook(int eid, webs_t wp, int argc, char_t **argv) {
	FILE *fp;
	char type[32], ip[32], netmask[32], gateway[32], dns[128], statusstr[32];
	int status = 0, unit, s;
	char tmp[100], prefix[] = "wanXXXXXXXXXX_";
	char filename[80], conntype[10];
	struct ifreq ifr;
	struct sockaddr_in *our_ip;
	struct in_addr in;
	char *pwanip = NULL, *ppp_addr, *usb_device;
	
	/* current unit */
	if ((unit = atoi(nvram_safe_get("wan_unit"))) < 0)
		unit = 0;
	wan_prefix(unit, prefix);
	
#ifndef RTCONFIG_USB_MODEM
	if (nvram_match("wan_proto", "3g"))
	{
		ppp_addr = nvram_safe_get("wan0_ipaddr");
		if (ppp_addr && strlen(ppp_addr) > 0)
		{
			status = 1;
			strcpy(statusstr, "Connected");
		}
		else
		{
			status = 0;
			usb_device = nvram_safe_get("usb_path1");
			if (usb_device && strlen(usb_device) > 0)
			{
				if (strcmp(usb_device, "HSDPA") == 0)
					strcpy(statusstr, "hsdpa connect fail");
				else
					strcpy(statusstr, "error hsdpa device");
			} 
			else
				strcpy(statusstr, "usb dongle is not attached");

		}
	}
	else
#else
	if(get_usb_modem_state()){
		DIR *ppp_dir;
		int got_ppp_link;
		struct dirent *entry;

		if ((ppp_dir = opendir("/tmp/ppp")) == NULL) {
			status = 0;
			strcpy(statusstr, "Disconnected");
		}
		else {
			got_ppp_link = 0;
			while((entry = readdir(ppp_dir)) != NULL) {
				if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, ".."))
					continue;
				
				if (strstr(entry->d_name, "link") != NULL) {
					got_ppp_link = 1;
					
					break;
				}
			}
			closedir(ppp_dir);
			
			if (got_ppp_link == 0) {
				status = 0;
				strcpy(statusstr, "Disconnected");
			}
			else if (check_ppp_exist() == -1) {
				status = 0;
				strcpy(statusstr, "Disconnected");
			}
			else {
				status = 1;
				strcpy(statusstr, "Connected");
			}
		}
	}
	else
#endif
	if (!is_phyconnected()) {
		status = 0;
		strcpy(statusstr, "Cable is not attached");
	}
// 2008.07 James. {
	else if (!strcmp(nvram_safe_get("manually_disconnect_wan"), "1")) {
		status = 0;
		strcpy(statusstr, "Disconnected");
	}
// 2008.07 James. }
	else if (nvram_match(strcat_r(prefix, "proto", tmp), "pptp")
			|| nvram_match(strcat_r(prefix, "proto", tmp), "PPTP")
			|| nvram_match(strcat_r(prefix, "proto", tmp), "l2tp")
			|| nvram_match(strcat_r(prefix, "proto", tmp), "pppoe")
			|| nvram_match(strcat_r(prefix, "proto", tmp), "PPPoE")
			)
	{
		DIR *ppp_dir;
		int got_ppp_link;
		struct dirent *entry;

		if ((ppp_dir = opendir("/tmp/ppp")) == NULL) {
			status = 0;
			strcpy(statusstr, "Disconnected");
		}
		else {
			got_ppp_link = 0;
			while((entry = readdir(ppp_dir)) != NULL) {
				if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, ".."))
					continue;
				
				if (strstr(entry->d_name, "link") != NULL) {
					got_ppp_link = 1;
					
					break;
				}
			}
			closedir(ppp_dir);
			
			if (got_ppp_link == 0) {
				status = 0;
				strcpy(statusstr, "Disconnected");
			}
			else if (check_ppp_exist() == -1) {
				status = 0;
				strcpy(statusstr, "Disconnected");
			}
			else {
				status = 1;
				strcpy(statusstr, "Connected");
			}
		}
#if 0
		DIR *ppp_dir;	// 2008.01 James.
		struct dirent *entry;	// 2008.01 James.
		char *name;
		char* pos = NULL;
//		char* cpos = NULL;
		
		memset(filename, 0, 80);
		
		ppp_dir = opendir("/tmp/ppp");
		if (ppp_dir == NULL) {
			printf("wanlink_hook ppp_dir == NULL\n");	// tmp test
			status = 0;
			strcpy(statusstr, "Disconnected");
		}
		
		while ((entry = readdir(ppp_dir)) != NULL) {
			if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, ".."))
				continue;
			
			if ((pos = strstr(entry->d_name, "link")) != NULL) {
				sprintf(filename, "/tmp/ppp/%s", entry->d_name);
				strcpy(conntype, pos+5);
				
				break;
			}
		}
		closedir(ppp_dir);
		
    		if (strlen(filename) > 0 && (fp = fopen(filename, "r")) != NULL) {
	    		int pid = -1;
	    		fclose(fp);
			
			if (nvram_match(strcat_r(prefix, "proto", tmp), "heartbeat")) {
				char buf[20];
				
				file_to_buf(filename, buf, sizeof(buf));
				pid = atoi(buf);
			}
			else
				pid = get_ppp_pid(conntype);
												 
			name = find_name_by_proc(pid);  
			
			if (!strncmp(name, "pppoecd", 7) ||	// for PPPoE
				!strncmp(name, "pppd", 4)	// for PPTP
				/*!strncmp(name, "bpalogin", 8)	// for HearBeat*/
			) {
				if (!strcmp(nvram_safe_get("manually_disconnect_wan"), "1"))
				{
					status = 0;
					//printf("[status str] chk dicconn 0\n");	// tmp test
					strcpy(statusstr, "Disconnected");
				}
				else
				{
					status = 1;
					//printf("[status str] chk conn 1\n");	// tmp test
					strcpy(statusstr, "Connected");
				}
			}
			else {
				status = 0;
				strcpy(statusstr, "Disconnected");
				//printf("[status str] chk disconn 1\n");	// tmp test
				// For some reason, the pppoed had been died, by link file still exist.
				unlink(filename);
			}
		}
		else {
			//wendebug
			//printf("filename : open fail\n");	  
			//csprintf("wanlink_hook filename : open fail\n");	   
			status = 0;
			//printf("[status str] chk disconn 2\n");	// tmp test
			strcpy(statusstr, "Disconnected"); 
		}
#endif	// #if 0
	}
	else {
// 2009.05 James. {
		//if (!strcmp(nvram_safe_get("wan_gateway_t"), nvram_safe_get("lan_ipaddr_t"))) {
		if (check_subnet()) {
			status = 0;
			strcpy(statusstr, "Disconnected");
		}
		else
// 2009.05 James. }
		/* Open socket to kernel */
		if ((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
			//printf("wanlink_hook Open socket to kernel : open fail\n");	// tmp test
			status = 0;
			strcpy(statusstr, "Disconnected");
			//printf("[status str] chk disconn 3\n");	// tmp test
		}
		else {
			/* Check for valid IP address */
			strncpy(ifr.ifr_name, nvram_safe_get(strcat_r(prefix, "ifname", tmp)), IFNAMSIZ);
			
			if (!ioctl(s, SIOCGIFADDR, &ifr)) {
				our_ip = (struct sockaddr_in *) &ifr.ifr_addr;
				in.s_addr = our_ip->sin_addr.s_addr;
				pwanip = inet_ntoa(in);
				
				if (!strcmp(pwanip, "") || pwanip == NULL) {
				//	csprintf("wanlink_hook !strcmp(pwanip, "") || pwanip == NULL\n");
					status = 0;
					strcpy(statusstr, "Disconnected");
					//printf("[status str] chk disconn 4\n");	// tmp test
				}
// 2008.07 James. {
				else if (!strcmp(nvram_safe_get("manually_disconnect_wan"), "1")) {
					//printf("wanlink_hook manually_disconnect_wan=1\n");	// tmp test
					status = 0;
					strcpy(statusstr, "Disconnected");
					//printf("[status str] chk disconn 5\n");	// tmp test
				}
// 2008.07 James. }
				else {
					status = 1;
					strcpy(statusstr, "Connected");
					//printf("[status str] chk conn 2\n");	// tmp test
				}
			}
			else {
				status = 0;
				strcpy(statusstr, "Disconnected");
				//printf("[status str] chk disconn 6\n");	// tmp test
			}
			
			close(s);
		}
	}
	
	//printf("[httpd] wan status str is %s\n", statusstr);	// tmp test
#ifdef RTCONFIG_USB_MODEM
	if(get_usb_modem_state())
		strcpy(type, "Modem");
	else
#endif
	if (nvram_match(strcat_r(prefix, "proto", tmp), "pppoe"))
	{
		strcpy(type, "PPPoE");
	}
	else if (nvram_match(strcat_r(prefix, "proto", tmp), "pptp"))
	{
		strcpy(type, "PPTP");
	}
	//2008.10 magic add l2tp
		else if (nvram_match(strcat_r(prefix, "proto", tmp), "l2tp"))
	{
		strcpy(type, "L2TP");
	}
	else if (nvram_match(strcat_r(prefix, "proto", tmp), "bigpond"))
	{	       
		strcpy(type, "Big Pond");
	}
	else if (nvram_match(strcat_r(prefix, "proto", tmp), "static"))
	{
		strcpy(type, "Static IP");
	}
#ifndef RTCONFIG_USB_MODEM
	else if (nvram_match(strcat_r(prefix, "proto", tmp), "3g"))
	{
		strcpy(type, "Modem");
	}
#endif
	else // dhcp
	{
		strcpy(type, "Automatic IP");
	}
	
	memset(ip, 0, sizeof(ip));
	memset(netmask, 0, sizeof(netmask));
	memset(gateway, 0, sizeof(gateway));
	if (status == 0)
	{
		strcpy(ip, "0.0.0.0");
		strcpy(gateway, "0.0.0.0");	
	}
	else
	{
		strcpy(ip, nvram_safe_get(strcat_r(prefix, "ipaddr", tmp)));
		strcpy(netmask, nvram_safe_get(strcat_r(prefix, "netmask", tmp)));
		strcpy(gateway, nvram_safe_get(strcat_r(prefix, "gateway", tmp)));	
	}
	
// 2007.08 James {
	memset(dns, 0, 128);
	if (!nvram_match("wan_dnsenable_x", "1"))
	{
		sprintf(dns, "%s %s", nvram_safe_get("wan_dns1_x"), nvram_safe_get("wan_dns2_x"));
	}
	else
	{
		sprintf(dns, "%s", nvram_safe_get("wan_dns_t"));
	}
// 2007.08 James }
	
	websWrite(wp, "function wanlink_status() { return %d;}\n", status);
	websWrite(wp, "function wanlink_statusstr() { return '%s';}\n", statusstr);
	websWrite(wp, "function wanlink_type() { return '%s';}\n", type);
	websWrite(wp, "function wanlink_ipaddr() { return '%s';}\n", ip);
	websWrite(wp, "function wanlink_netmask() { return '%s';}\n", netmask);
	websWrite(wp, "function wanlink_gateway() { return '%s';}\n", gateway);
	websWrite(wp, "function wanlink_dns() { return '%s';}\n", dns);

	return 0;
}

static int wan_action_hook(int eid, webs_t wp, int argc, char_t **argv) {
	char *action;
	int needed_seconds = 0;
	
	// assign control variables
	action = websGetVar(wp, "wanaction", "");
	if (strlen(action) <= 0) {
		fprintf(stderr, "No connect action in wan_action_hook!\n");
		return -1;
	}
	
	fprintf(stderr, "wan action: %s\n", action);
	
	if (!strcmp(action, "Connect")) {
		nvram_set("link_internet", "2");
		nvram_unset("manually_disconnect_wan");	// 2008.01 James.
		nvram_set("rc_service", "wan_connect");
		needed_seconds = 5;	// WL500-series
csprintf("kill -USR1 1.\n");
//		kill(1, SIGUSR1);
		fprintf(stderr, "[httpd] send SIGUSR1 to watchdog for rc_service: %s\n", nvram_safe_get("rc_service"));
		doSystem("killall -%d watchdog", SIGUSR1);
csprintf("sleep 1 second.\n");
		sleep(1);
	}
	else if (!strcmp(action, "Disconnect")) {
		nvram_set("link_internet", "2");
		nvram_set("manually_disconnect_wan", "1");	// 2008.01 James.
		nvram_set("rc_service", "wan_disconnect");
		needed_seconds = 5;	// WL500-series
csprintf("kill -USR1 1.\n");
//		kill(1, SIGUSR1);
		fprintf(stderr, "[httpd] send SIGUSR1 to watchdog for rc_service: %s\n", nvram_safe_get("rc_service"));
		doSystem("killall -%d watchdog", SIGUSR1);
csprintf("sleep 1 second.\n");
		sleep(1);
	}
	
	//printf("needed time = %d\n", needed_seconds);	// tmp test
	websWrite(wp, "<script>restart_needed_time(%d);</script>\n", needed_seconds);
	
	return 0;
}

static int ej_get_parameter(int eid, webs_t wp, int argc, char_t **argv) {
//	char *c;
	bool last_was_escaped;
	int ret = 0;
	
	if (argc < 1) {
		websError(wp, 400,
				"get_parameter() used with no arguments, but at least one "
				"argument is required to specify the parameter name\n");
		return -1;
	}
	
	last_was_escaped = FALSE;
	
	char *value = websGetVar(wp, argv[0], "");
	websWrite(wp, "%s", value);//*/
	/*for (c = websGetVar(wp, argv[0], ""); *c; c++) {
		if (isprint((int)*c) &&
			*c != '"' && *c != '&' && *c != '<' && *c != '>' && *c != '\\' &&
			((!last_was_escaped) || !isdigit(*c)))
		{
			ret += websWrite(wp, "%c", *c);
			last_was_escaped = FALSE;
		}
		else if ((*c & 0x80) != 0)
		{
			ret += websWrite(wp, "%c", *c);
			last_was_escaped = FALSE;
		}
		else
		{
			ret += websWrite(wp, "&#%d", *c);
			last_was_escaped = TRUE;
		}
	}//*/
	
	return ret;
}

unsigned int getpeerip(webs_t wp) {
	int fd, ret;
	struct sockaddr peer;
	socklen_t peerlen = sizeof(struct sockaddr);
	struct sockaddr_in *sa;
	
	fd = fileno((FILE *)wp);
	ret = getpeername(fd, (struct sockaddr *)&peer, &peerlen);
	sa = (struct sockaddr_in *)&peer;
	
	if (!ret) {
//		csprintf("peer: %x\n", sa->sin_addr.s_addr);	// J++
		return (unsigned int)sa->sin_addr.s_addr;
	}
	else {
		csprintf("error: %d %d \n", ret, errno);
		return 0;
	}
}

static int login_state_hook(int eid, webs_t wp, int argc, char_t **argv) {
	unsigned int ip, login_ip;
	char ip_str[16], login_ip_str[16];
	time_t login_timestamp;
	struct in_addr now_ip_addr, login_ip_addr;
	time_t now;
	const int MAX = 80;
	const int VALUELEN = 18;
	char buffer[MAX], values[6][VALUELEN];
	
	ip = getpeerip(wp);
	//csprintf("ip = %u\n",ip);

	now_ip_addr.s_addr = ip;
	memset(ip_str, 0, 16);
	strcpy(ip_str, inet_ntoa(now_ip_addr));
//	time(&now);
	now = uptime();
	
	login_ip = (unsigned int)atoll(nvram_safe_get("login_ip"));
	login_ip_addr.s_addr = login_ip;
	memset(login_ip_str, 0, 16);
	strcpy(login_ip_str, inet_ntoa(login_ip_addr));
//	login_timestamp = (unsigned long)atol(nvram_safe_get("login_timestamp"));
	login_timestamp = strtoul(nvram_safe_get("login_timestamp"), NULL, 10);
	
	FILE *fp = fopen("/proc/net/arp", "r");
	if (fp) {
		memset(buffer, 0, MAX);
		memset(values, 0, 6*VALUELEN);
		
		while (fgets(buffer, MAX, fp)) {
			if (strstr(buffer, "br0") && !strstr(buffer, "00:00:00:00:00:00")) {
				if (sscanf(buffer, "%s%s%s%s%s%s", values[0], values[1], values[2], values[3], values[4], values[5]) == 6) {
					if (!strcmp(values[0], ip_str)) {
						break;
					}
				}
				
				memset(values, 0, 6*VALUELEN);
			}
			
			memset(buffer, 0, MAX);
		}
		
		fclose(fp);
	}
	
	if (ip != 0 && login_ip == ip) {
		websWrite(wp, "function is_logined() { return 1; }\n");
		websWrite(wp, "function login_ip_dec() { return '%u'; }\n", login_ip);
		websWrite(wp, "function login_ip_str() { return '%s'; }\n", login_ip_str);
		websWrite(wp, "function login_ip_str_now() { return '%s'; }\n", ip_str);
		
		if (values[3] != NULL)
			websWrite(wp, "function login_mac_str() { return '%s'; }\n", values[3]);
		else
			websWrite(wp, "function login_mac_str() { return ''; }\n");
//		time(&login_timestamp);
		login_timestamp = uptime();
	}
	else {
		websWrite(wp, "function is_logined() { return 0; }\n");
		websWrite(wp, "function login_ip_dec() { return '%u'; }\n", login_ip);
		
		if ((unsigned long)(now-login_timestamp) > 60)	//one minitues
			websWrite(wp, "function login_ip_str() { return '0.0.0.0'; }\n");
		else
			websWrite(wp, "function login_ip_str() { return '%s'; }\n", login_ip_str);
		
		websWrite(wp, "function login_ip_str_now() { return '%s'; }\n", ip_str);
		
		if (values[3] != NULL)
			websWrite(wp, "function login_mac_str() { return '%s'; }\n", values[3]);
		else
			websWrite(wp, "function login_mac_str() { return ''; }\n");
	}
	
	return 0;
}

static int ej_get_nvram_list(int eid, webs_t wp, int argc, char_t **argv) {
	struct variable *v, *gv;
	char buf[MAX_LINE_SIZE+MAX_LINE_SIZE];
	char *serviceId, *groupName;
	int i, groupCount, sid;
	int firstRow, firstItem;
	
	if (argc != 2)
		return 0;
	
	serviceId = argv[0];
	groupName = argv[1];
	
	sid = LookupServiceId(serviceId);
	if (sid == -1)
		return 0;
	
	/* Validate and set vairables in table order */
	for (v = GetVariables(sid); v->name != NULL; ++v)
		if (!strcmp(groupName, v->name))
			break;
	if (v->name == NULL)
		return 0;
	
	groupCount = atoi(nvram_safe_get_x(serviceId, v->argv[3]));
	
	firstRow = 1;
	for (i = 0; i < groupCount; ++i) {
		if (firstRow == 1)
			firstRow = 0;
		else
			websWrite(wp, ", ");
		websWrite(wp, "[");
		
		firstItem = 1;
		for (gv = (struct variable *)v->argv[0]; gv->name != NULL; ++gv) {
			if (firstItem == 1)
				firstItem = 0;
			else
				websWrite(wp, ", ");
			
			memset(buf, 0, sizeof(buf));
			sprintf(buf, "\"%s\"", nvram_get_list_x(serviceId, gv->name, i));
			websWrite(wp, "%s", buf);
		}
		
		websWrite(wp, "]");
	}
	
	return 0;
}

static int ej_dhcp_leases(int eid, webs_t wp, int argc, char_t **argv) {
	FILE *fp = NULL;
	struct lease_t lease;
	struct in_addr addr;
	unsigned long expires = 0;
	int i, firstRow;
	char str[80], buf[80];
	
	sys_script("leases.sh");
	
	/* Write out leases file */
	if (!(fp = fopen("/tmp/udhcpd-br0.leases", "r")))
		return 0;
	
	firstRow = 1;
	while (fread(&lease, sizeof(lease), 1, fp)) {
		/* Do not display reserved leases */
		if (ETHER_ISNULLADDR(lease.chaddr))
			continue;
		
		if (firstRow == 1)
			firstRow = 0;
		else
			websWrite(wp, ", ");
		websWrite(wp, "[");
		
		memset(str, 0, 80);
		memset(buf, 0, 80);
		sprintf(buf, "%-16s", lease.hostname);
		sscanf(buf, "%s", str);
		websWrite(wp, "\"%s\", ", str);
		
		websWrite(wp, "\"");
		for (i = 0; i < 6; ++i) {
			websWrite(wp, "%02X", lease.chaddr[i]);
			if (i != 5)
				websWrite(wp, ":");
		}
		websWrite(wp, "\", ");
		
		addr.s_addr = lease.yiaddr;
		memset(str, 0, 80);
		memset(buf, 0, 80);
		sprintf(buf, "%-15s", inet_ntoa(addr));
		sscanf(buf, "%s", str);
		websWrite(wp, "\"%s\", ", str);
		expires = ntohl(lease.expires);
		
		if (expires == 0xffffffff)
			websWrite(wp, "\"Manual\"");
		else if (!expires)
			websWrite(wp, "\"Expired\"");
		else {
			char lease_buf[128];
			memset(lease_buf, 0, sizeof(lease_buf));
			memset(str, 0, 80);
			memset(buf, 0, 80);
			reltime(expires, lease_buf);
			sprintf(buf, "%-15s", lease_buf);
			sscanf(buf, "%s", str);
			websWrite(wp, "\"%s\"", str);
		}
		
		websWrite(wp, "]");
	}
	
	fclose(fp);
	
	return 0;
}

static int ej_get_arp_table(int eid, webs_t wp, int argc, char_t **argv) {
	const int MAX = 80;
	const int FIELD_NUM = 6;
	const int VALUELEN = 18;
	char buffer[MAX], values[FIELD_NUM][VALUELEN];
	int num, firstRow;
	
	FILE *fp = fopen("/proc/net/arp", "r");
	if (fp) {
		memset(buffer, 0, MAX);
		memset(values, 0, FIELD_NUM*VALUELEN);
		
		firstRow = 1;
		while (fgets(buffer, MAX, fp)) {
			if (strstr(buffer, "br0") && !strstr(buffer, "00:00:00:00:00:00")) {
				if (firstRow == 1)
					firstRow = 0;
				else
					websWrite(wp, ", ");
				
				if ((num = sscanf(buffer, "%s%s%s%s%s%s", values[0], values[1], values[2], values[3], values[4], values[5])) == FIELD_NUM) {
					websWrite(wp, "[\"%s\", \"%s\", \"%s\", \"%s\", \"%s\", \"%s\"]", values[0], values[1], values[2], values[3], values[4], values[5]);
				}
				
				memset(values, 0, FIELD_NUM*VALUELEN);
			}
			
			memset(buffer, 0, MAX);
		}
		
		fclose(fp);
	}
	
	return 0;
}

// for detect static IP's client.
static int ej_get_static_client(int eid, webs_t wp, int argc, char_t **argv) {
	FILE *fp = fopen("/tmp/static_ip.inf", "r");
	char buf[1024], *head, *tail, field[1024];
	int len, i, first_client, first_field;
	
	if (fp == NULL) {
		csprintf("Don't detect static clients!\n");
		return 0;
	}
	
	memset(buf, 0, 1024);
	
	first_client = 1;
	while (fgets(buf, 1024, fp)) {
		if (first_client == 1)
			first_client = 0;
		else
			websWrite(wp, ", ");
		
		len = strlen(buf);
		buf[len-1] = ',';
		head = buf;
		first_field = 1;
		for (i = 0; i < 7; ++i) {
			tail = strchr(head, ',');
			if (tail != NULL) {
				memset(field, 0, 1024);
				strncpy(field, head, (tail-head));
			}
			
			if (first_field == 1) {
				first_field = 0;
				websWrite(wp, "[");
			}
			else
				websWrite(wp, ", ");
			
			if (strlen(field) > 0)
				websWrite(wp, "\"%s\"", field);
			else
				websWrite(wp, "null");
			
			//if (tail+1 != NULL)
				head = tail+1;
			
			if (i == 6)
				websWrite(wp, "]");
		}
		
		memset(buf, 0, 1024);
	}
	
	fclose(fp);
	return 0;
}


static int ej_get_changed_status(int eid, webs_t wp, int argc, char_t **argv) {
	char *arp_info = read_whole_file("/proc/net/arp");
	char *disk_info = read_whole_file(PARTITION_FILE); 
	char *mount_info = read_whole_file("/proc/mounts"); 
	u32 arp_info_len, disk_info_len, mount_info_len; 
//	u32 arp_change, disk_change;
	
	//printf("get changed status\n");	// tmp test

	if (arp_info != NULL) {
		arp_info_len = strlen(arp_info);
		free(arp_info);
	}
	else
		arp_info_len = 0;
	
	if (disk_info != NULL) {
		disk_info_len = strlen(disk_info);
		free(disk_info);
	}
	else
		disk_info_len = 0;
	
	if (mount_info != NULL) {
		mount_info_len = strlen(mount_info);
		free(mount_info);
	}
	else
		mount_info_len = 0;

	websWrite(wp, "function get_client_status_changed() {\n");
	websWrite(wp, "    return %u;\n", arp_info_len);
	websWrite(wp, "}\n\n");
	
	websWrite(wp, "function get_disk_status_changed() {\n");
	websWrite(wp, "    return %u;\n", disk_info_len);
	websWrite(wp, "}\n\n");
	
	websWrite(wp, "function get_mount_status_changed() {\n");
	websWrite(wp, "    return %u;\n", mount_info_len);
	websWrite(wp, "}\n\n");
	
	return 0;
}

// Wireless Client List		 /* Start --Alicia, 08.09.23 */
#define WIF     "ra0"
#define WIF2G	"rai0"
#define RTPRIV_IOCTL_GET_MAC_TABLE	SIOCIWFIRSTPRIV + 0x0F

static int ej_wl_auth_list(int eid, webs_t wp, int argc, char_t **argv)
{
	struct iwreq wrq;
	int i, firstRow;
	char data[4096];
	char mac[ETHER_ADDR_STR_LEN];	
	RT_802_11_MAC_TABLE *mp;
	RT_802_11_MAC_TABLE_2G *mp2;
	char *value;
	
	memset(mac, 0, sizeof(mac));
	
	/* query wl for authenticated sta list */
	memset(data, 0, 4096);
	wrq.u.data.pointer = data;
	wrq.u.data.length = 4096;
	wrq.u.data.flags = 0;	
	if (wl_ioctl(WIF, RTPRIV_IOCTL_GET_MAC_TABLE, &wrq) < 0)
		goto exit;

	/* build wireless sta list */
	firstRow = 1;
	mp = (RT_802_11_MAC_TABLE *)wrq.u.data.pointer;
	for (i=0; i<mp->Num; i++)
	{
		if (firstRow == 1)
			firstRow = 0;
		else
			websWrite(wp, ", ");
		websWrite(wp, "[");

		sprintf(mac, "%02X:%02X:%02X:%02X:%02X:%02X",
				mp->Entry[i].Addr[0], mp->Entry[i].Addr[1],
				mp->Entry[i].Addr[2], mp->Entry[i].Addr[3],
				mp->Entry[i].Addr[4], mp->Entry[i].Addr[5]);
		websWrite(wp, "\"%s\"", mac);
		value = "YES";
		websWrite(wp, ", \"%s\"", value);
		value = "";
		websWrite(wp, ", \"%s\"", value);
		websWrite(wp, "]");
	}

	/* query wl for authenticated sta list */
	memset(data, 0, 4096);
	wrq.u.data.pointer = data;
	wrq.u.data.length = 4096;
	wrq.u.data.flags = 0;	
	if (wl_ioctl(WIF2G, RTPRIV_IOCTL_GET_MAC_TABLE, &wrq) < 0)
		goto exit;

	/* build wireless sta list */
	mp2 = (RT_802_11_MAC_TABLE_2G *)wrq.u.data.pointer;
	for (i = 0; i<mp2->Num; i++)
	{
		if (firstRow == 1)
			firstRow = 0;
		else
			websWrite(wp, ", ");
		websWrite(wp, "[");
				
		sprintf(mac, "%02X:%02X:%02X:%02X:%02X:%02X",
				mp2->Entry[i].Addr[0], mp2->Entry[i].Addr[1],
				mp2->Entry[i].Addr[2], mp2->Entry[i].Addr[3],
				mp2->Entry[i].Addr[4], mp2->Entry[i].Addr[5]);
		websWrite(wp, "\"%s\"", mac);
		
		value = "YES";
		websWrite(wp, ", \"%s\"", value);
		
		value = "";
		websWrite(wp, ", \"%s\"", value);
		
		websWrite(wp, "]");
	}

	/* error/exit */
exit:
	return 0;
}						     /* End --Alicia, 08.09.23 */

/* remove space in the end of string */
char *trim_r(char *str)
{
	int i;

	i = strlen(str);

	while (i >= 1)
	{
		if (*(str+i-1) == ' ' || *(str+i-1) == 0x0a || *(str+i-1) == 0x0d)
			*(str+i-1)=0x0;
		else
			break;

		i--;
	}
	return (str);
}

static int ej_wl_scan_5g(int eid, webs_t wp, int argc, char_t **argv)
{
	int retval = 0, i = 0, apCount = 0;
	char data[8192];
	char ssid_str[256];
	char header[128];
	struct iwreq wrq;
	SSA *ssap;
	memset(data, 0x00, 255);
	strcpy(data, "SiteSurvey=1"); 
	wrq.u.data.length = strlen(data)+1; 
	wrq.u.data.pointer = data; 
	wrq.u.data.flags = 0; 
	spinlock_lock(SPINLOCK_SiteSurvey);
	if (wl_ioctl(WIF, RTPRIV_IOCTL_SET, &wrq) < 0)
	{
		spinlock_unlock(0);
		dbg("Site Survey fails\n");
		return 0;
	}
	spinlock_unlock(SPINLOCK_SiteSurvey);
	dbg("Please wait");
	sleep(1);
	dbg(".");
	sleep(1);
	dbg(".");
	sleep(1);
	dbg(".");
	sleep(1);
	dbg(".\n\n");
	memset(data, 0, 8192);
	strcpy(data, "");
	wrq.u.data.length = 8192;
	wrq.u.data.pointer = data;
	wrq.u.data.flags = 0;
	if (wl_ioctl(WIF, RTPRIV_IOCTL_GSITESURVEY, &wrq) < 0)
	{
		dbg("errors in getting site survey result\n");
		return 0;
	}
	memset(header, 0, sizeof(header));
	//sprintf(header, "%-3s%-33s%-18s%-8s%-15s%-9s%-8s%-2s\n", "Ch", "SSID", "BSSID", "Enc", "Auth", "Siganl(%)", "W-Mode", "NT");
	sprintf(header, "%-4s%-33s%-18s%-9s%-16s%-9s%-8s\n", "Ch", "SSID", "BSSID", "Enc", "Auth", "Siganl(%)", "W-Mode");
	dbg("\n%s", header);
	if (wrq.u.data.length > 0)
	{
		ssap=(SSA *)(wrq.u.data.pointer+strlen(header)+1);
		int len = strlen(wrq.u.data.pointer+strlen(header))-1;
		char *sp, *op;
 		op = sp = wrq.u.data.pointer+strlen(header)+1;
		while (*sp && ((len - (sp-op)) >= 0))
		{
			ssap->SiteSurvey[i].channel[3] = '\0';
			ssap->SiteSurvey[i].ssid[32] = '\0';
			ssap->SiteSurvey[i].bssid[17] = '\0';
			ssap->SiteSurvey[i].encryption[8] = '\0';
			ssap->SiteSurvey[i].authmode[15] = '\0';
			ssap->SiteSurvey[i].signal[8] = '\0';
			ssap->SiteSurvey[i].wmode[7] = '\0';
			sp+=strlen(header);
			apCount=++i;
		}
		if (apCount)
		{
			retval += websWrite(wp, "[");
			for (i = 0; i < apCount; i++)
			{
				dbg("%-4s%-33s%-18s%-9s%-16s%-9s%-8s\n",
					ssap->SiteSurvey[i].channel,
					(char*)ssap->SiteSurvey[i].ssid,
					ssap->SiteSurvey[i].bssid,
					ssap->SiteSurvey[i].encryption,
					ssap->SiteSurvey[i].authmode,
					ssap->SiteSurvey[i].signal,
					ssap->SiteSurvey[i].wmode
				);

				memset(ssid_str, 0, sizeof(ssid_str));
				char_to_ascii(ssid_str, trim_r(ssap->SiteSurvey[i].ssid));

				if (!i)
//					retval += websWrite(wp, "\"%s\"", ssap->SiteSurvey[i].bssid);
					retval += websWrite(wp, "[\"%s\", \"%s\"]", ssid_str, ssap->SiteSurvey[i].bssid);
				else
//					retval += websWrite(wp, ", \"%s\"", ssap->SiteSurvey[i].bssid);
					retval += websWrite(wp, ", [\"%s\", \"%s\"]", ssid_str, ssap->SiteSurvey[i].bssid);
			}
			retval += websWrite(wp, "]");
			dbg("\n");
		}
		else
			retval += websWrite(wp, "[]");
	}
	return retval;
}
static int ej_wl_scan_2g(int eid, webs_t wp, int argc, char_t **argv)
{
	int retval = 0, i = 0, apCount = 0;
	char data[8192];
	char ssid_str[256];
	char header[128];
	struct iwreq wrq;
	SSA *ssap;
	memset(data, 0x00, 255);
	strcpy(data, "SiteSurvey=1"); 
	wrq.u.data.length = strlen(data)+1; 
	wrq.u.data.pointer = data; 
	wrq.u.data.flags = 0; 
	spinlock_lock(SPINLOCK_SiteSurvey);
	if (wl_ioctl(WIF2G, RTPRIV_IOCTL_SET, &wrq) < 0)
	{
		spinlock_unlock(0);
		dbg("Site Survey fails\n");
		return 0;
	}
	spinlock_unlock(SPINLOCK_SiteSurvey);
	dbg("Please wait");
	sleep(1);
	dbg(".");
	sleep(1);
	dbg(".");
	sleep(1);
	dbg(".");
	sleep(1);
	dbg(".\n\n");
	memset(data, 0, 8192);
	strcpy(data, "");
	wrq.u.data.length = 8192;
	wrq.u.data.pointer = data;
	wrq.u.data.flags = 0;
	if (wl_ioctl(WIF2G, RTPRIV_IOCTL_GSITESURVEY, &wrq) < 0)
	{
		dbg("errors in getting site survey result\n");
		return 0;
	}
	memset(header, 0, sizeof(header));
	//sprintf(header, "%-3s%-33s%-18s%-8s%-15s%-9s%-8s%-2s\n", "Ch", "SSID", "BSSID", "Enc", "Auth", "Siganl(%)", "W-Mode", "NT");
	sprintf(header, "%-4s%-33s%-18s%-9s%-16s%-9s%-8s\n", "Ch", "SSID", "BSSID", "Enc", "Auth", "Siganl(%)", "W-Mode");
	dbg("\n%s", header);
	if (wrq.u.data.length > 0)
	{
		ssap=(SSA *)(wrq.u.data.pointer+strlen(header)+1);
		int len = strlen(wrq.u.data.pointer+strlen(header))-1;
		char *sp, *op;
 		op = sp = wrq.u.data.pointer+strlen(header)+1;
		while (*sp && ((len - (sp-op)) >= 0))
		{
			ssap->SiteSurvey[i].channel[3] = '\0';
			ssap->SiteSurvey[i].ssid[32] = '\0';
			ssap->SiteSurvey[i].bssid[17] = '\0';
			ssap->SiteSurvey[i].encryption[8] = '\0';
			ssap->SiteSurvey[i].authmode[15] = '\0';
			ssap->SiteSurvey[i].signal[8] = '\0';
			ssap->SiteSurvey[i].wmode[7] = '\0';
			sp+=strlen(header);
			apCount=++i;
		}
		if (apCount)
		{
			retval += websWrite(wp, "[");
			for (i = 0; i < apCount; i++)
			{
				dbg("%-4s%-33s%-18s%-9s%-16s%-9s%-8s\n",
					ssap->SiteSurvey[i].channel,
					(char*)ssap->SiteSurvey[i].ssid,
					ssap->SiteSurvey[i].bssid,
					ssap->SiteSurvey[i].encryption,
					ssap->SiteSurvey[i].authmode,
					ssap->SiteSurvey[i].signal,
					ssap->SiteSurvey[i].wmode
				);

				memset(ssid_str, 0, sizeof(ssid_str));
                                char_to_ascii(ssid_str, trim_r(ssap->SiteSurvey[i].ssid));

				if (!i)
					retval += websWrite(wp, "[\"%s\", \"%s\"]", ssid_str, ssap->SiteSurvey[i].bssid);
				else
					retval += websWrite(wp, ", [\"%s\", \"%s\"]", ssid_str, ssap->SiteSurvey[i].bssid);


			}
			retval += websWrite(wp, "]");
			dbg("\n");
		}
		else
			retval += websWrite(wp, "[]");
	}
	return retval;
}
static int ej_disk_pool_mapping_info(int eid, webs_t wp, int argc, char_t **argv) {
	disk_info_t *disks_info, *follow_disk;
	partition_info_t *follow_partition;
	int first;

	disks_info = read_disk_data();
	if (disks_info == NULL) {
		websWrite(wp, "%s", initial_disk_pool_mapping_info());
		return -1;
	}

	websWrite(wp, "function total_disk_sizes() {\n");
	websWrite(wp, "    return [");
	first = 1;
	for (follow_disk = disks_info; follow_disk != NULL; follow_disk = follow_disk->next) {
		if (first == 1)
			first = 0;
		else
			websWrite(wp, ", ");

		websWrite(wp, "\"%llu\"", follow_disk->size_in_kilobytes);
	}
	websWrite(wp, "];\n");
	websWrite(wp, "}\n\n");

	websWrite(wp, "function disk_interface_names() {\n");
	websWrite(wp, "    return [");
	first = 1;
	for (follow_disk = disks_info; follow_disk != NULL; follow_disk = follow_disk->next) {
		if (first == 1)
			first = 0;
		else
			websWrite(wp, ", ");

		websWrite(wp, "\"usb\"");
	}
	websWrite(wp, "];\n");
	websWrite(wp, "}\n\n");
	char *Ptr;

	websWrite(wp, "function pool_names() {\n");
	websWrite(wp, "    return [");

	first = 1;
	for (follow_disk = disks_info; follow_disk != NULL; follow_disk = follow_disk->next)
		for (follow_partition = follow_disk->partitions; follow_partition != NULL; follow_partition = follow_partition->next) {
			if (first == 1)
				first = 0;
			else
				websWrite(wp, ", ");

			if (follow_partition->mount_point == NULL) {
				websWrite(wp, "\"unknown\"");
				continue;
			}

			Ptr = rindex(follow_partition->mount_point, '/');
			if (Ptr == NULL) {
				websWrite(wp, "\"unknown\"");
				continue;
			}

			++Ptr;
			if (!strncmp(Ptr, "part", 4))
				websWrite(wp, "\"%s\"", Ptr);
			else if (!strncmp(Ptr, "AiDisk_", 7))
				websWrite(wp, "\"%s\"", Ptr);
			else
				websWrite(wp, "\"unknown\"");
		}
	websWrite(wp, "];\n");
	websWrite(wp, "}\n\n");

	websWrite(wp, "function pool_types() {\n");
	websWrite(wp, "    return [");
	first = 1;
	for (follow_disk = disks_info; follow_disk != NULL; follow_disk = follow_disk->next)
		for (follow_partition = follow_disk->partitions; follow_partition != NULL; follow_partition = follow_partition->next) {
			if (first == 1)
				first = 0;
			else
				websWrite(wp, ", ");

			if (follow_partition->mount_point == NULL) {
				websWrite(wp, "\"unknown\"");
				continue;
			}

			websWrite(wp, "\"%s\"", follow_partition->file_system);
		}
	websWrite(wp, "];\n");
	websWrite(wp, "}\n\n");

	websWrite(wp, "function pool_mirror_counts() {\n");
	websWrite(wp, "    return [");
	first = 1;
	for (follow_disk = disks_info; follow_disk != NULL; follow_disk = follow_disk->next)
		for (follow_partition = follow_disk->partitions; follow_partition != NULL; follow_partition = follow_partition->next) {
			if (first == 1)
				first = 0;
			else
				websWrite(wp, ", ");

			websWrite(wp, "0");
		}
	websWrite(wp, "];\n");
	websWrite(wp, "}\n\n");

	websWrite(wp, "function pool_status() {\n");
	websWrite(wp, "    return [");
	first = 1;
	for (follow_disk = disks_info; follow_disk != NULL; follow_disk = follow_disk->next)
		for (follow_partition = follow_disk->partitions; follow_partition != NULL; follow_partition = follow_partition->next) {
			if (first == 1)
				first = 0;
			else
				websWrite(wp, ", ");

			if (follow_partition->mount_point == NULL) {
				websWrite(wp, "\"unmounted\"");
				continue;
			}

			//if (strcmp(follow_partition->file_system, "ntfs") == 0)
			//	websWrite(wp, "\"ro\"");
			//else
			websWrite(wp, "\"rw\"");
		}
	websWrite(wp, "];\n");
	websWrite(wp, "}\n\n");

	websWrite(wp, "function pool_kilobytes() {\n");
	websWrite(wp, "    return [");
	first = 1;
	for (follow_disk = disks_info; follow_disk != NULL; follow_disk = follow_disk->next)
		for (follow_partition = follow_disk->partitions; follow_partition != NULL; follow_partition = follow_partition->next) {
			if (first == 1)
				first = 0;
			else
				websWrite(wp, ", ");

			websWrite(wp, "%llu", follow_partition->size_in_kilobytes);
		}
	websWrite(wp, "];\n");
	websWrite(wp, "}\n\n");

	websWrite(wp, "function pool_encryption_password_is_missing() {\n");
	websWrite(wp, "    return [");
	first = 1;
	for (follow_disk = disks_info; follow_disk != NULL; follow_disk = follow_disk->next)
		for (follow_partition = follow_disk->partitions; follow_partition != NULL; follow_partition = follow_partition->next) {
			if (first == 1)
				first = 0;
			else
				websWrite(wp, ", ");

			websWrite(wp, "\"no\"");
		}
	websWrite(wp, "];\n");
	websWrite(wp, "}\n\n");

	websWrite(wp, "function pool_kilobytes_in_use() {\n");
	websWrite(wp, "    return [");
	first = 1;
	for (follow_disk = disks_info; follow_disk != NULL; follow_disk = follow_disk->next)
		for (follow_partition = follow_disk->partitions; follow_partition != NULL; follow_partition = follow_partition->next) {
			if (first == 1)
				first = 0;
			else
				websWrite(wp, ", ");

			//printf("[%llu] ", follow_partition->used_kilobytes);	// tmp test
			websWrite(wp, "%llu", follow_partition->used_kilobytes);
		}

	websWrite(wp, "];\n");
	websWrite(wp, "}\n\n");

	u64 disk_used_kilobytes;

	websWrite(wp, "function disk_usage() {\n");
	websWrite(wp, "    return [");
	first = 1;
	for (follow_disk = disks_info; follow_disk != NULL; follow_disk = follow_disk->next) {
		if (first == 1)
			first = 0;
		else
			websWrite(wp, ", ");

		disk_used_kilobytes = 0;
		for (follow_partition = follow_disk->partitions; follow_partition != NULL; follow_partition = follow_partition->next)
			disk_used_kilobytes += follow_partition->size_in_kilobytes;

		websWrite(wp, "%llu", disk_used_kilobytes);
	}
	websWrite(wp, "];\n");
	websWrite(wp, "}\n\n");

	disk_info_t *follow_disk2;
	u32 disk_num, pool_num;
	websWrite(wp, "function per_pane_pool_usage_kilobytes(pool_num, disk_num) {\n");
	for (follow_disk = disks_info, pool_num = 0; follow_disk != NULL; follow_disk = follow_disk->next) {
		for (follow_partition = follow_disk->partitions; follow_partition != NULL; follow_partition = follow_partition->next, ++pool_num) {
			websWrite(wp, "    if (pool_num == %d) {\n", pool_num);
			if (follow_partition->mount_point != NULL)
				for (follow_disk2 = disks_info, disk_num = 0; follow_disk2 != NULL; follow_disk2 = follow_disk2->next, ++disk_num) {
					websWrite(wp, "	if (disk_num == %d) {\n", disk_num);

//					if (strcmp(follow_disk2->tag, follow_disk->tag) == 0)
					if (follow_disk2->major == follow_disk->major && follow_disk2->minor == follow_disk->minor)
						websWrite(wp, "	    return [%llu];\n", follow_partition->size_in_kilobytes);
					else
						websWrite(wp, "	    return [0];\n");

					websWrite(wp, "	}\n");
				}
			else
				websWrite(wp, "	return [0];\n");
			websWrite(wp, "    }\n");
		}
	}
	websWrite(wp, "}\n\n");
	free_disk_data(&disks_info);

	return 0;
}

static int ej_available_disk_names_and_sizes(int eid, webs_t wp, int argc, char_t **argv) {
	disk_info_t *disks_info, *follow_disk;
	int first;

	websWrite(wp, "function available_disks() { return [];}\n\n");
	websWrite(wp, "function available_disk_sizes() { return [];}\n\n");
	websWrite(wp, "function claimed_disks() { return [];}\n\n");
	websWrite(wp, "function claimed_disk_interface_names() { return [];}\n\n");
	websWrite(wp, "function claimed_disk_model_info() { return [];}\n\n");
	websWrite(wp, "function claimed_disk_total_size() { return [];}\n\n");
	websWrite(wp, "function claimed_disk_total_mounted_number() { return [];}\n\n");
	websWrite(wp, "function blank_disks() { return [];}\n\n");
	websWrite(wp, "function blank_disk_interface_names() { return [];}\n\n");
	websWrite(wp, "function blank_disk_model_info() { return [];}\n\n");
	websWrite(wp, "function blank_disk_total_size() { return [];}\n\n");
	websWrite(wp, "function blank_disk_total_mounted_number() { return [];}\n\n");

	disks_info = read_disk_data();
	if (disks_info == NULL) {
		websWrite(wp, "%s", initial_available_disk_names_and_sizes());
		return -1;
	}

	/* show name of the foreign disks */
	websWrite(wp, "function foreign_disks() {\n");
	websWrite(wp, "    return [");
	first = 1;
	for (follow_disk = disks_info; follow_disk != NULL; follow_disk = follow_disk->next) {
		if (first == 1)
			first = 0;
		else
			websWrite(wp, ", ");

		websWrite(wp, "\"%s\"", follow_disk->tag);
	}
	websWrite(wp, "];\n");
	websWrite(wp, "}\n\n");

	/* show interface of the foreign disks */
	websWrite(wp, "function foreign_disk_interface_names() {\n");
	websWrite(wp, "    return [");
	first = 1;
	for (follow_disk = disks_info; follow_disk != NULL; follow_disk = follow_disk->next) {
		if (first == 1)
			first = 0;
		else
			websWrite(wp, ", ");

//		websWrite(wp, "\"USB\"");
		websWrite(wp, "\"%s\"", follow_disk->port);
	}
	websWrite(wp, "];\n");
	websWrite(wp, "}\n\n");

	/* show model info of the foreign disks */
	websWrite(wp, "function foreign_disk_model_info() {\n");
	websWrite(wp, "    return [");
	first = 1;
	for (follow_disk = disks_info; follow_disk != NULL; follow_disk = follow_disk->next) {
		if (first == 1)
			first = 0;
		else
			websWrite(wp, ", ");

		websWrite(wp, "\"");

		if (follow_disk->vendor != NULL)
			websWrite(wp, "%s", follow_disk->vendor);
		if (follow_disk->model != NULL) {
			if (follow_disk->vendor != NULL)
				websWrite(wp, " ");

			websWrite(wp, "%s", follow_disk->model);
		}
		websWrite(wp, "\"");
	}
	websWrite(wp, "];\n");
	websWrite(wp, "}\n\n");

	/* show total_size of the foreign disks */
	websWrite(wp, "function foreign_disk_total_size() {\n");
	websWrite(wp, "    return [");
	first = 1;
	for (follow_disk = disks_info; follow_disk != NULL; follow_disk = follow_disk->next) {
		if (first == 1)
			first = 0;
		else
			websWrite(wp, ", ");

		websWrite(wp, "\"%llu\"", follow_disk->size_in_kilobytes);
	}
	websWrite(wp, "];\n");
	websWrite(wp, "}\n\n");
	/* show total number of the mounted partition in this foreign disk */
	websWrite(wp, "function foreign_disk_total_mounted_number() {\n");
	websWrite(wp, "    return [");
	first = 1;
	for (follow_disk = disks_info; follow_disk != NULL; follow_disk = follow_disk->next) {
		if (first == 1)
			first = 0;
		else
			websWrite(wp, ", ");

		websWrite(wp, "\"%u\"", follow_disk->mounted_number);
	}
	websWrite(wp, "];\n");
	websWrite(wp, "}\n\n");
	free_disk_data(&disks_info);

	return 0;
}

#if 0
static int ej_get_printer_info(int eid, webs_t wp, int argc, char_t **argv) {
	FILE *lpfp;
	char manufacturer[100], models[100], serialnos[100], pool[100];
	char buf[500];
	char *lpparam, *value, *v1 = NULL;
	
	if (!(lpfp = fopen("/proc/usblp/usblpid", "r"))) {
		strcpy(manufacturer, "");
		strcpy(models, "");
		strcpy(serialnos, "");
		strcpy(pool, "");
	}
	else {
		while (fgets(buf, 500, lpfp)) {
			value = &buf[0];
			lpparam = strsep(&value, "=")?:&buf[0];
			
			if (value) {
				v1 = strchr(value, '\n');
				*v1 = '\0';
				
				if (!strcmp(lpparam, "Manufacturer"))
					sprintf(manufacturer, "\"%s\"", value);
				else if (!strcmp(lpparam, "Model"))
					sprintf(models, "\"%s\"", value );
			}
		}
		
		/* compatible for WL700gE platform */
		strcpy(pool, "VirtualPool");
		fclose(lpfp);
	}
	
	websWrite(wp, "function printer_manufacturers() {\n return [%s];\n}\n", manufacturer);
	websWrite(wp, "function printer_models() {\n return [%s];\n}\n", models);
	websWrite(wp, "function printer_serialn() {\n return [%s];\n}\n", "");
	websWrite(wp, "function printer_pool() {\n return [\"%s\"];\n}\n", pool);
}
#else
#define MAX_PRINTER_NUM 2

static int ej_get_printer_info(int eid, webs_t wp, int argc, char_t **argv){
	int port_num = 0, first;
	char tmp[64], prefix[] = "usb_pathX";
	char printer_array[2][5][64];
	for(port_num = 1; port_num <= MAX_PRINTER_NUM; ++port_num){
		snprintf(prefix, sizeof(prefix), "usb_path%d", port_num);
		memset(printer_array[port_num-1][0], 0, 64);
		memset(printer_array[port_num-1][1], 0, 64);
		memset(printer_array[port_num-1][2], 0, 64);
		memset(printer_array[port_num-1][3], 0, 64);
		memset(printer_array[port_num-1][4], 0, 64);
		if(nvram_match(prefix, "printer")){
			strncpy(printer_array[port_num-1][0], "printer", 64);
			strncpy(printer_array[port_num-1][1], nvram_safe_get(strcat_r(prefix, "_manufacturer", tmp)), 64);
			strncpy(printer_array[port_num-1][2], nvram_safe_get(strcat_r(prefix, "_product", tmp)), 64);
			strncpy(printer_array[port_num-1][3], nvram_safe_get(strcat_r(prefix, "_serial", tmp)), 64);
			if(!strcmp(printer_array[port_num-1][3], nvram_safe_get("u2ec_serial")))
				strcpy(printer_array[port_num-1][4], "VirtualPool");
			else
				strcpy(printer_array[port_num-1][4], "");
		}
	}
	websWrite(wp, "function printer_manufacturers(){\n");
	websWrite(wp, "    return [");
	first = 1;
	for(port_num = 1; port_num <= MAX_PRINTER_NUM; ++port_num){
		if(strlen(printer_array[port_num-1][0]) > 0)
			websWrite(wp, "\"%s\"", printer_array[port_num-1][1]);
		else
			websWrite(wp, "\"\"");
		if(first){
			--first;
			websWrite(wp, ", ");
		}
	}
	websWrite(wp, "];\n");
	websWrite(wp, "}\n\n");
	websWrite(wp, "function printer_models(){\n");
	websWrite(wp, "    return [");
	first = 1;
	for(port_num = 1; port_num <= MAX_PRINTER_NUM; ++port_num){
		if(strlen(printer_array[port_num-1][0]) > 0)
			websWrite(wp, "\"%s\"", printer_array[port_num-1][2]);
		else
			websWrite(wp, "\"\"");
		if(first){
			--first;
			websWrite(wp, ", ");
		}
	}
	websWrite(wp, "];\n");
	websWrite(wp, "}\n\n");
	websWrite(wp, "function printer_serialn(){\n");
	websWrite(wp, "    return [");
	first = 1;
	for(port_num = 1; port_num <= MAX_PRINTER_NUM; ++port_num){
		if(strlen(printer_array[port_num-1][0]) > 0)
			websWrite(wp, "\"%s\"", printer_array[port_num-1][3]);
		else
			websWrite(wp, "\"\"");
		if(first){
			--first;
			websWrite(wp, ", ");
		}
	}
	websWrite(wp, "];\n");
	websWrite(wp, "}\n\n");
	websWrite(wp, "function printer_pool(){\n");
	websWrite(wp, "    return [");
	first = 1;
	for(port_num = 1; port_num <= MAX_PRINTER_NUM; ++port_num){
		if(strlen(printer_array[port_num-1][0]) > 0)
			websWrite(wp, "\"%s\"", printer_array[port_num-1][4]);
		else
			websWrite(wp, "\"\"");
		if(first){
			--first;
			websWrite(wp, ", ");
		}
	}
	websWrite(wp, "];\n");
	websWrite(wp, "}\n\n");
	return 0;
}
#endif
int ej_shown_time(int eid, webs_t wp, int argc, char **argv) {
	time_t t1;
	
	time(&t1);
	
	websWrite(wp, "%e", t1);
	
	return 0;
}

int ej_shown_language_option(int eid, webs_t wp, int argc, char **argv) {
	struct language_table *pLang = NULL;
	char lang[4];
	int i, len;
	FILE *fp = fopen("EN.dict", "r");
	char buffer[1024], key[16], target[16];
	char *follow_info, *follow_info_end;

	if (fp == NULL) {
		fprintf(stderr, "No English dictionary!\n");
		return 0;
	}

	memset(lang, 0, 4);
	strcpy(lang, nvram_safe_get("preferred_lang"));

	for (i = 0; i < 21; ++i) {
		memset(buffer, 0, sizeof(buffer));
		if ((follow_info = fgets(buffer, sizeof(buffer), fp)) != NULL) {
			if (strncmp(follow_info, "LANG_", 5))    // 5 = strlen("LANG_")
				continue;

			follow_info += 5;
			follow_info_end = strstr(follow_info, "=");
			len = follow_info_end-follow_info;
			memset(key, 0, sizeof(key));
			strncpy(key, follow_info, len);

			for (pLang = language_tables; pLang->Lang != NULL; ++pLang) {
				if (strcmp(key, pLang->Target_Lang))
					continue;
				follow_info = follow_info_end+1;
				follow_info_end = strstr(follow_info, "\n");
				len = follow_info_end-follow_info;
				memset(target, 0, sizeof(target));
				strncpy(target, follow_info, len);

				if (!strcmp(key, lang))
					websWrite(wp, "<option value=\"%s\" selected>%s</option>\\n", key, target);
				else
					websWrite(wp, "<option value=\"%s\">%s</option>\\n", key, target);
				break;
			}
		}
		else
			break;
	}
	fclose(fp);

	return 0;
}

static int
apply_cgi(webs_t wp, char_t *urlPrefix, char_t *webDir, int arg,
		char_t *url, char_t *path, char_t *query)
{
	int sid;
	char *value;
	char *current_url;
	char *next_url;
	char *sid_list;
	char *value1;
	char *script;
	char groupId[64];
	char urlStr[64];
	char *groupName;
//	char *host;

	//if (!query)
	//	goto footer;
	
	urlStr[0] = 0;
	
	value = websGetVar(wp, "action", "");
	
	//if (strcmp(value, "")==0)
	value = websGetVar(wp, "action_mode","");
	
	next_host = websGetVar(wp, "next_host", "");
	cprintf("host:%s\n", next_host);
	current_url = websGetVar(wp, "current_page", "");
	next_url = websGetVar(wp, "next_page", "");
	script = websGetVar(wp, "action_script","");
	
	//printf("Script: %s\n", script);
	cprintf("Apply: %s %s %s %s\n", value, current_url, next_url, websGetVar(wp, "group_id", ""));
	
	if (!strcmp(value," Refresh "))
	{
		strcpy(SystemCmd, websGetVar(wp,"SystemCmd",""));
		//csprintf("1. SystemCmd: %s.\n", SystemCmd);
		websRedirect(wp, current_url);
		return 0;
	}
	else if (!strcmp(value," Clear "))
	{
		// current only syslog implement this button
		unlink("/tmp/syslog.log-1");
		unlink("/tmp/syslog.log");
		websRedirect(wp, current_url);
		return 0;
	}
	else if (!strcmp(value,"NEXT"))
	{
		websRedirect(wp, next_url);
		return 0;
	}
	else if (!strcmp(value, "Save&Restart "))
	{
		dbG("[httpd] action mode: %s\n", value);

		websApply(wp, "Restarting.asp");
		nvram_set_f("General", "x_Setting", "1");
		nvram_set("w_Setting", "1");	// J++
		eval("early_convert");
		nvram_commit_safe();

		set_reboot_timestamp();
		sys_reboot();

		return (0);
	}
	else if (!strcmp(value, " Restart "))
	{
		dbG("[httpd] action mode: %s\n", value);

		websApply(wp, "Restarting.asp");

		set_reboot_timestamp();
		sys_reboot();

		return (0);
	}
	else if (!strcmp(value, "Restore"))
	{
		dbG("[httpd] action mode: %s\n", value);

		websApply(wp, "Restarting.asp");
		sys_default();

		set_reboot_timestamp();
		sys_reboot();

		return (0);
	}
	else
	{
		sid_list = websGetVar(wp, "sid_list", "");
		while ((serviceId = svc_pop_list(sid_list, ';')))
		{
			sid = 0;
			while (GetServiceId(sid) != 0)
			{
				if (!strcmp(GetServiceId(sid), serviceId))
					break;
				
				++sid;
			}
			
			if (serviceId != NULL)
			{
				if (!strcmp(value, " Restore "))
				{
					//sys_restore(serviceId);
				}
				else if (!strcmp(value, "  Save  ") || !strcmp(value, " Apply "))
				{
					validate_cgi(wp, sid, TRUE);
				}
				else if (!strcmp(value, "Set") || !strcmp(value, "Unset") || !strcmp(value, "Update") || !strcmp(value, "Eject")
#ifdef ASUS_DDNS
						|| !strcmp(value, "Check")
#endif
						)
				{
					validate_cgi(wp, sid, TRUE);
				}
				else if (!strcmp(value," Finish "))
				{
					validate_cgi(wp, sid, TRUE);
				}
				else
				{
					cprintf("group ?\n");
					strcpy(groupId,websGetVar(wp, "group_id", ""));
					
					if ((value1 = websGetVar(wp, "action_mode", NULL)) != NULL)
					{
						groupName = groupId;
						
						//if (!strcmp(GetServiceId(sid), groupId))
						{
							if (!strncmp(value1, " Delete ", 8))
							{
								apply_cgi_group(wp, sid, NULL, groupName, GROUP_FLAG_DELETE);
							}
							else if (!strncmp(value1, " Add ", 5))
							{
								apply_cgi_group(wp, sid, NULL, groupName, GROUP_FLAG_ADD);
							}
							else if (!strncmp(value1, " Del ", 5))
							{
#ifdef DLM	// 2005.12.16 Yau add for 500gP
								if (!strcmp(current_url, "Advanced_StorageRight_Content.asp") &&
										!strcmp(groupId, "Storage_SharedList"))
								{
									delSharedEntry(delMap);
								}
								else if (!strcmp(current_url, "Advanced_StorageUser_Content.asp") &&
										!strcmp(groupId, "Storage_UserRight_List"))
								{
									apply_right_group(wp, sid, NULL, groupName, GROUP_FLAG_REMOVE);
								}
								else
									apply_cgi_group(wp, sid, NULL, groupName, GROUP_FLAG_REMOVE);
#else
								apply_cgi_group(wp, sid, NULL, groupName, GROUP_FLAG_REMOVE);
#endif
							}
							else if (!strncmp(value1, " Refresh ", 9))
							{
								apply_cgi_group(wp, sid, NULL, groupName, GROUP_FLAG_REFRESH);
							}

							sprintf(urlStr, "%s#%s", current_url, groupName);
							validate_cgi(wp, sid, FALSE);
						}
					}
				}
			}
			
			sid_list = sid_list+strlen(serviceId)+1;
		}
		
		//printf("apply????\n");
		
		/* Add for Internet Access Quick Setup */
		//special_handler(wp, url);
		
		/* Add for NVRAM mapping if necessary */
		//nvMap(current_url);
		
		/* Add for EMI Test page */
		if (strcmp(script, ""))
		{
#ifdef DLM
			if (!strcmp(script, "usbfs_check"))
			{
				nvram_set("st_tool_t", "/bin/fscheck");
				nvram_set("st_toolstatus_t", "USBstarting");
				nvram_set("st_time_t", "3");
				//system("/bin/fscheck");
				websRedirect(wp, "Checking.asp");
				deliver_signal("/var/run/infosvr.pid", SIGUSR1);
				return 0;
			}
			else if (!strcmp(script, "usbfs_eject"))
				umount_disc_parts(1);
			else if (!strcmp(script, "usbfs_eject2"))
				umount_disc_parts(2);
			else
#endif
				sys_script(script);
		}
		
		if (!strcmp(value, "  Save  ") || !strcmp(value, " Apply "))
		{
			strcpy(urlcache, next_url);
			websRedirect(wp, next_url);
		}
		else if (!strcmp(value, " Finish "))
			websRedirect(wp, "SaveRestart.asp");
		else if (urlStr[0] == 0)
			websRedirect(wp, current_url);
		else
			websRedirect(wp, urlStr);
		
		cprintf("apply ok\n");
		return 0;
	}
	
	return 1;
}
//2008.08 magic}

void nvram_add_group_item(webs_t wp, struct variable *v, int sid)
{
    struct variable *gv;
    char *value;
    char name[64], cstr[5];    
    int count;
    
    if (v->argv[0]==NULL) 
    {
       return;
    }
		
//Yau
//printf("+++ add_group_item ++\n");		
    count = atoi(nvram_safe_get_x(serviceId, v->argv[3]));	
    cprintf("count: %d\n", count);
    
    for (gv = (struct variable *)v->argv[0]; gv->name!=NULL; gv++)
    {    	    		    	    	
    	sprintf(name, "%s_0", gv->name);    	
    	    	    	    	    	    	
	if ((value=websGetVar(wp, name, NULL)))
	{		    
	    /*SetGroupVariable(sid, v, gv->name, value, "Add");*/
	    nvram_add_lists_x(serviceId, gv->name, value, count);	    
	    cprintf("Add: %s %s\n", gv->name, value);
	}   
	else
	{		
	    /*SetGroupVariable(sid, v, gv->name, "", "Add");  */
	    nvram_add_lists_x(serviceId, gv->name, "", count);	     
	}		      
    }   
    
    count++;    
    sprintf(cstr, "%d", count);       
    nvram_set_x(serviceId, v->argv[3], cstr);
    return;
}


void nvram_remove_group_item(webs_t wp, struct variable *v, int sid, int *delMap)
{
    struct variable *gv;
//    char *value;
    char /*glist[MAX_LINE_SIZE], *itemPtr, *glistPtr, */cstr[5];
    int /*item, */i, count;
//Yau
//printf("+++ remove group item +++\n");
    if (v->argv[0]==NULL) 
    {
       return;
    }
    
    count = atoi(nvram_safe_get_x(serviceId, v->argv[3]));    
    for (gv = (struct variable *)v->argv[0]; gv->name!=NULL; gv++)
    {    	    		    	     	    	
//Yau
//printf("gv->name: %s, delMap[0]=%d\n",gv->name,delMap[0]);
    	nvram_del_lists_x(serviceId, gv->name, delMap);    		    	       
    }	   
    
    i=0;
    while (delMap[i]!=-1) i++;
    
    count-=i;
    sprintf(cstr, "%d", count);
//Yau
//printf("del cstr=%s\n",cstr);
    nvram_set_x(serviceId, v->argv[3], cstr);
    return;
}

/* Rule for table: 
 * 1. First field can not be NULL, or it is treat as empty
 * 2. 
 */

static int 
nvram_add_group_table(webs_t wp, char *serviceId, struct variable *v, int count)
{
    struct variable *gv;
    char buf[MAX_LINE_SIZE+MAX_LINE_SIZE];
    char bufs[MAX_LINE_SIZE+MAX_LINE_SIZE];    
    int i, j, fieldLen, rowLen, fieldCount, value;
//    unsigned short int wstr[33];
    int addlen=0;
//    int temp=0;
    
//Yau
//printf("+++ add group table +++\n");
    if (v->argv[0]==NULL) 
    {
       return 0;
    }
    
    bufs[0] = 0x0;
    rowLen = atoi(v->argv[2]);      
    
    if (count==-1)
    {    
       for (i=0;i<rowLen;i++)
       {     
       	   bufs[i] = ' ';
       }
       value = -1;	       	       		     	
       bufs[i] = 0x0;	     	
    	
       goto ToHTML;	
    }
	      
		    
    fieldCount = 0;
    
    value = count;
	    
    for (gv = (struct variable *)v->argv[0]; gv->name!=NULL; gv++)
    {    		    	    	    	
    	strcpy(buf, nvram_get_list_x(serviceId, gv->name, count));
//    	printf("bufs len:  %d\n", strlen(bufs));
//    	printf("name: %s\n", gv->name);
//    	printf("buf:  %s\n", buf);
    	addlen=0;
    	
    	fieldLen = atoi(gv->longname)+addlen;
    	rowLen+=addlen;
    	    	    	       			    
    	if (fieldLen!=0)
    	{    	
    	   if (strlen(buf)>fieldLen)
    	   {    	     
    	      buf[fieldLen] = 0x0; 	
    	   }   
    	   else
    	   {
    	      i = strlen(buf);	
    	      j = i;
    	      for (;i<fieldLen;i++)
    	      {
    		 buf[j++] = ' ';	
    	      }       	      
    	      buf[j] = 0x0;
    	   }      
    	}   
    	
    	if (fieldCount==0)
    	   sprintf(bufs,"%s", buf);
    	else
    	   sprintf(bufs,"%s%s",bufs, buf);
    	       	   
    	fieldCount++;
    }					      
    
    if (strlen(bufs)> rowLen)
       bufs[rowLen] = 0x0;
       
ToHTML:       
    /* Replace ' ' to &nbsp; */
    buf[0] = 0;
    j = 0;
    
    for (i=0; i<strlen(bufs);i++)
    {
    	if (bufs[i] == ' ')
    	{
    	   buf[j++] = '&';
    	   buf[j++] = 'n';
    	   buf[j++] = 'b';
    	   buf[j++] = 's';
    	   buf[j++] = 'p';
    	   buf[j++] = ';';    	   	
    	} 
    	else buf[j++] = bufs[i];
    }	  
    buf[j] = 0x0;
       
    return (websWrite(wp, "<option value=\"%d\">%s</option>", value, buf));
}

static int
apply_cgi_group(webs_t wp, int sid, struct variable *var, char *groupName, int flag)
{	
   struct variable *v;
//   char *value, *value1;
//   char item[64];
   int groupCount;
	 
   //Yau del c    
//   printf("==apply_cgi_group== This group limit is %d %d\n", flag, sid);

   if (var!=NULL) v = var;
   else
   {	 
       /* Validate and set vairables in table order */	      
       for (v = GetVariables(sid); v->name != NULL; v++) 
       {
       	   //printf("Find group : %s %s\n", v->name, groupName);
       	   if (!strcmp(groupName, v->name)) break;
       }			    
	//Yau
//	printf("Find group : %s %s\n", v->name, groupName);			    
   }    
   
   if (v->name == NULL) return 0;    
   
   groupCount = atoi(v->argv[1]);

   if (flag == GROUP_FLAG_ADD)/* if (!strcmp(value, " Refresh ")) || Save condition */
   {    
   	nvram_add_group_item(wp, v, sid);
		return 1;	// 2008.08 magic
   }	  
   else if (flag == GROUP_FLAG_REMOVE)/* if (!strcmp(value, " Refresh ")) || Save condition */
   {    
   	/*nvram_remove_group_item(wp, v, sid); 	*/
   	/*sprintf(item, "%s_s", websGetVar(wp, "group_id", ""));*/
	/*value1 = websGetVar(wp, item, NULL);*/
	/* TO DO : change format from 'x=1&x=2&...' to 'x=1 2 3 ..'*/
   	nvram_remove_group_item(wp, v, sid, delMap);  
		return 1; 	// 2008.08 magic
   }	     
  	return 0; // 2008.08 magic
}

static int
nvram_generate_table(webs_t wp, char *serviceId, char *groupName)
{	
   struct variable *v;
   int i, groupCount, ret, r, sid;
//Yau
//printf("=== generate_table === group:%s\n",groupName);     
   
   sid = LookupServiceId(serviceId);
	 
   if (sid==-1) return 0;
    
   /* Validate and set vairables in table order */	      
   for (v = GetVariables(sid); v->name != NULL; v++) 
   {
      /* printf("Find group : %s %s\n", v->name, groupName);*/
      if (!strcmp(groupName, v->name)) break;		       
   }    
   //Yau
//   printf("Find group :: %s %s\n", v->name, groupName);
   
   if (v->name == NULL) return 0;    
	    
   groupCount = atoi(nvram_safe_get_x(serviceId, v->argv[3]));	       
	    
//Yau
//printf("groupCount=%d\n",groupCount);	    

   if (groupCount==0) ret = nvram_add_group_table(wp, serviceId, v, -1);
   else
   {
   
      ret = 0;
   
      for (i=0; i<groupCount; i++)
      {       		    	
	r = nvram_add_group_table(wp, serviceId, v, i);
	if (r!=0)
	   ret += r;
	else break;
      }    
   }   
   
   return (ret);
}


#ifdef WEBS
mySecurityHandler(webs_t wp, char_t *urlPrefix, char_t *webDir, int arg, char_t *url, char_t *path, char_t *query)
{
	char_t *user, *passwd, *digestCalc;
	int flags, nRet;


	user = websGetRequestUserName(wp);
	passwd = websGetRequestPassword(wp);
	flags = websGetRequestFlags(wp);


	nRet = 0;

	if (user && *user)
	{
		if (strcmp(user, "admin")!=0)
		{
			websError(wp, 401, T("Wrong User Name"));
			nRet = 1;
		}
		else if (passwd && *passwd)
		{
			if (strcmp(passwd, websGetPassword())!=0)
			{
				websError(wp, 401, T("Wrong Password"));
				nRet = 1;
			}
		}
#ifdef DIGEST_ACCESS_SUPPORT		
		else if (flags & WEBS_AUTH_DIGEST)
		{
			wp->password = websGetPassword();

			a_assert(wp->digest);
			a_assert(wp->nonce);
			a_assert(wp->password);

			digestCalc = websCalcDigest(wp);
			a_assert(digestCalc);		

			if (gstrcmp(wp->digest, digestCalc)!=0)
			{
				websError(wp, 401, T("Wrong Password"));
				nRet = 1;
			}
			bfree(B_L, digestCalc);
		}
#endif	
	}
	else 
	{
#ifdef DIGEST_ACCESS_SUPPORT
		wp->flags |= WEBS_AUTH_DIGEST;
#endif
		websError(wp, 401, T(""));
		nRet = 1;
	}
	return nRet;

}
#else
static void
do_auth(char *userid, char *passwd, char *realm)
{	
//	time_t tm;
					
	if (strcmp(ProductID,"")==0)
	{
		strncpy(ProductID, nvram_safe_get_x("general.log", "productid"), 32);
	}
	if (strcmp(UserID,"")==0)
	{	   
	   	strncpy(UserID, nvram_safe_get_x("General","http_username"), 32);
	}
// 2008.08 magic {
	//if (strcmp(UserPass, "") == 0)
	if (strcmp(UserPass, "") == 0 || reget_passwd == 1)
	{
		reget_passwd = 0;
// 2008.08 magic }
		strncpy(UserPass, nvram_safe_get_x("General","http_passwd"), 32);
	}	
	//time(&tm);
	//printf("do_auth %s\n", rfctime(&tm));
	//printf("User: %s:%s%s\n", ProductID, UserID, UserPass);
	strncpy(userid, UserID, AUTH_MAX);

	//if (redirect || !is_auth()) //2008.08 magic
	if (!is_auth())
	{
		//printf("Disable password checking!!!\n");
		strcpy(passwd, "");
	}
	else
	{
		strncpy(passwd, UserPass, AUTH_MAX);
	}
	strncpy(realm, ProductID, AUTH_MAX);	
}
#endif


static void
do_apply_cgi(char *url, FILE *stream)
{
	/*char *path, *query;
	
	cprintf(" Before Apply : %s\n", url);
	
	websScan(url);	
	query = url;
	path = strsep(&query, "?") ? : url;	
#ifndef HANDLE_POST	
	init_cgi(query);
#endif	
	apply_cgi(stream, NULL, NULL, 0, url, path, query);
#ifndef HANDLE_POST	
	init_cgi(NULL);
#endif	*/
    apply_cgi(stream, NULL, NULL, 0, url, NULL, NULL);
}

#ifdef HANDLE_POST
static char post_buf[10000] = { 0 };
#endif

#if defined(linux)

//#if defined(ASUS_MIMO) && defined(TRANSLATE_ON_FLY)
#ifdef TRANSLATE_ON_FLY
static int refresh_title_asp = 0;

static void
do_lang_cgi(char *url, FILE *stream)
{
	if (refresh_title_asp)  {
		// Request refreshing pages from browser.
		websHeader(stream);
		websWrite(stream, "<head></head><title>REDIRECT TO INDEX.ASP</title>");

		// The text between <body> and </body> content may be rendered in Opera browser.
		websWrite(stream, "<body onLoad='if (navigator.appVersion.indexOf(\"Firefox\")!=-1||navigator.appName == \"Netscape\") {top.location=\"index.asp\";}else {top.location.reload(true);}'></body>");
		websFooter(stream);
		websDone(stream, 200);
	} else {
		// Send redirect-page if and only if refresh_title_asp is true.
		// If we do not send Title.asp, Firefox reload web-pages again and again.
		// This trick had been deprecated due to compatibility issue with Netscape and Mozilla browser.
		websRedirect(stream, "Title.asp");
	}
}


static void
do_lang_post(char *url, FILE *stream, int len, char *boundary)
{
	int c;
	char *p, *p1;
	char orig_lang[4], new_lang[4];
	char buf[1024];

	if (url == NULL)
		return;

	p = strstr (url, "preferred_lang_menu");
	if (p == NULL)
		return;
	memset (new_lang, 0, sizeof (new_lang));
	strncpy (new_lang, p + strlen ("preferred_lang_menu") + 1, 2);

	memset (orig_lang, 0, sizeof (orig_lang));
	p1 = nvram_safe_get_x ("", "preferred_lang");
	if (p1[0] != '\0')      {
		strncpy (orig_lang, p1, 2);
	} else {
		strncpy (orig_lang, "EN", 2);
	}

	// read remain data
#if 0
	if (feof (stream)) {
		while ((c = fgetc(stream) != EOF)) {
			;       // fall through
		}
	}
#else
	while ((c = fread(buf, 1, sizeof(buf), stream)) > 0)
		;
#endif

	cprintf ("lang: %s --> %s\n", orig_lang, new_lang);
	refresh_title_asp = 0;
	if (strcmp (orig_lang, new_lang) != 0 || is_firsttime ())       {
		// If language setting is different or first change language
		nvram_set_x ("", "preferred_lang", new_lang);
		if (is_firsttime ())    {
			cprintf ("set x_Setting --> 1\n");
			nvram_set_x ("", "x_Setting", "1");
			nvram_set("w_Setting", "1");	// J++
		}
		cprintf ("!!!!!!!!!Commit new language settings.\n");
		refresh_title_asp = 1;
		nvram_commit_safe();
	}
}
//#endif  // defined(ASUS_MIMO) && defined(TRANSLATE_ON_FLY)
#endif // TRANSLATE_ON_FLY


static void
do_webcam_cgi(char *url, FILE *stream)
{
	#define MAX_RECORD_IMG 12
	char *query, *path;
	char pic[32];
	int query_idx, last_idx;
		
	query = url;
	path = strsep(&query, "?") ? : url;
	cprintf("WebCam CGI\n");
	//ret = fcntl(fileno(stream), F_GETOWN, 0);
	query_idx = atoi(query);
	last_idx = atoi(nvram_get_f("webcam.log", "WebPic"));
	//pic = nvram_get_f("webcam.log","WebPic");
			
	if (query_idx==0) sprintf(pic, "../var/tmp/display.jpg");
	else sprintf(pic, "../var/tmp/record%d.jpg", (query_idx>last_idx+1) ? (last_idx+1+MAX_RECORD_IMG-query_idx):(last_idx+1-query_idx));
	
	cprintf("\nWebCam: %s\n", pic);
	do_file(pic, stream);
}

#define SWAP_LONG(x) \
	((__u32)( \
		(((__u32)(x) & (__u32)0x000000ffUL) << 24) | \
		(((__u32)(x) & (__u32)0x0000ff00UL) <<  8) | \
		(((__u32)(x) & (__u32)0x00ff0000UL) >>  8) | \
		(((__u32)(x) & (__u32)0xff000000UL) >> 24) ))

static int
checkcrc (const char *argv)
{
	int ifd;
	uint32_t checksum;
	struct stat sbuf;
	unsigned char *ptr;
	image_header_t header2;
	image_header_t *hdr, *hdr2=&header2;
	char *imagefile;
	int ret=0;

	imagefile = argv;
//	fprintf(stderr, "img file: %s\n", imagefile);

	ifd = open(imagefile, O_RDONLY|O_BINARY);

	if (ifd < 0) {
		fprintf (stderr, "Can't open %s: %s\n",
			imagefile, strerror(errno));
		ret=-1;
		goto checkcrc_end;
	}

	memset (hdr2, 0, sizeof(image_header_t));

	/* We're a bit of paranoid */
#if defined(_POSIX_SYNCHRONIZED_IO) && !defined(__sun__) && !defined(__FreeBSD__)
	(void) fdatasync (ifd);
#else
	(void) fsync (ifd);
#endif
	if (fstat(ifd, &sbuf) < 0) {
		fprintf (stderr, "Can't stat %s: %s\n",
			imagefile, strerror(errno));
		ret=-1;
		goto checkcrc_fail;
	}

	ptr = (unsigned char *)mmap(0, sbuf.st_size,
				    PROT_READ, MAP_SHARED, ifd, 0);
	if (ptr == (unsigned char *)MAP_FAILED) {
		fprintf (stderr, "Can't map %s: %s\n",
			imagefile, strerror(errno));
		ret=-1;
		goto checkcrc_fail;
	}
	hdr = (image_header_t *)ptr;
/*
	checksum = crc32_sp (0,
			  (const char *)(ptr + sizeof(image_header_t)),
			  sbuf.st_size - sizeof(image_header_t)
			 );
	fprintf(stderr,"data crc: %X\n", checksum);
	fprintf(stderr,"org data crc: %X\n", SWAP_LONG(hdr->ih_dcrc));
//	if (checksum!=SWAP_LONG(hdr->ih_dcrc))
//		return -1;
*/
	memcpy (hdr2, hdr, sizeof(image_header_t));
	memset(&hdr2->ih_hcrc, 0, sizeof(uint32_t));
	checksum = crc32_sp(0,(const char *)hdr2,sizeof(image_header_t));

	fprintf(stderr, "header crc: %X\n", checksum);
	fprintf(stderr, "org header crc: %X\n", SWAP_LONG(hdr->ih_hcrc));

	if (checksum!=SWAP_LONG(hdr->ih_hcrc))
	{
		ret=-1;
		goto checkcrc_fail;
	}

	(void) munmap((void *)ptr, sbuf.st_size);

	/* We're a bit of paranoid */
checkcrc_fail:
#if defined(_POSIX_SYNCHRONIZED_IO) && !defined(__sun__) && !defined(__FreeBSD__)
	(void) fdatasync (ifd);
#else
	(void) fsync (ifd);
#endif
	if (close(ifd)) {
		fprintf (stderr, "Read error on %s: %s\n",
			imagefile, strerror(errno));
		ret=-1;
	}
checkcrc_end:
	return ret;
}

int chk_image_err = 1;

static void
do_upgrade_post(char *url, FILE *stream, int len, char *boundary)
{
	#define MAX_VERSION_LEN 64
	char upload_fifo[] = "/tmp/linux.trx";
	FILE *fifo = NULL;
	/*char *write_argv[] = { "write", upload_fifo, "linux", NULL };*/
	char buf[4096];
	int count, ret = EINVAL, ch/*, ver_chk = 0*/;
	int cnt;
	long filelen, *filelenptr, tmp;
	char /*version[MAX_VERSION_LEN], */cmpHeader;
	
	eval("/sbin/stopservice", "99");

	/* Look for our part */
	while (len > 0) 
	{
		if (!fgets(buf, MIN(len + 1, sizeof(buf)), stream))
		{
			goto err;
		}			

		len -= strlen(buf);

		if (!strncasecmp(buf, "Content-Disposition:", 20) && strstr(buf, "name=\"file\""))
			break;
	}

	/* Skip boundary and headers */
	while (len > 0) {
		if (!fgets(buf, MIN(len + 1, sizeof(buf)), stream))
		{
			goto err;
		}
		len -= strlen(buf);
		if (!strcmp(buf, "\n") || !strcmp(buf, "\r\n"))
		{
			break;
		}
	}

	if (!(fifo = fopen(upload_fifo, "a+"))) goto err;

	filelen = len;
	cnt = 0;

	nvram_set("ignore_plug", "1");	// avoid usb event when flash write

	/* Pipe the rest to the FIFO */
	cmpHeader = 0;

	while (len>0 && filelen>0) 
	{
		if (waitfor (fileno(stream), 10) <= 0)
		{
			/*printf("Break while len=%x filelen=%x\n", len, filelen);*/
			break;
		}

		count = fread(buf, 1, MIN(len, sizeof(buf)), stream);

		if (cnt==0 && count>48)
		{
			if (!(	buf[0] == 0x27 &&	/* Image Magic Number: 0x27051956 */
				buf[1] == 0x05 &&
				buf[2] == 0x19 &&
				buf[3] == 0x56)
			)
			{
				fprintf(stderr, "Header %x %x %x %x\n", buf[0], buf[1], buf[2], buf[3]);// 520N debug
				len-=count;
				goto err;
			}
			
			//printf("chk ProductID is %s, compare w/ %s. end\n", ProductID, buf+36);	// tmp test

			//if (strncmp(ProductID, buf+36, strlen(ProductID))==0)
			//	cmpHeader=1;
			if (strncmp(buf+36, "RT-N56U", 7)==0)
				cmpHeader=1;
			else
			{
				fprintf(stderr, "Wrong PID!\n");
				len-=count;
				goto err;
			}

			filelenptr=(buf+12);
			tmp=*filelenptr;
			filelen=SWAP_LONG(tmp);
			filelen+=64;
			//fprintf(stderr, "file len:%d\n", filelen);// 520N debug
			/*printf("Filelen: %x %x %x %x %x %x\n", filelen, count, (unsigned long)(buf+4), (unsigned long)(buf+7), buf[5], buf[4]);*/
			cnt++;
		}
		filelen-=count;
		len-=count;
#ifndef WRITE2FLASH
		fwrite(buf, 1, count, fifo);
#else
		if (!flashwrite(buf, count, filelen)) goto err;
#endif
	}

	if (!cmpHeader) goto err;

	/* Slurp anything remaining in the request */
	while (len-- > 0)
	{
		ch = fgetc(stream);

		if (filelen>0)
		{
#ifndef WRITE2FLASH
			fwrite(&ch, 1, 1, fifo);
			filelen--;
#else
			flashwrite(buf, 1, --filelen);
#endif
		}
	}	
	
#ifndef	WRITE2FLASH
	ret=checkcrc(upload_fifo);
#else
	ret=0;
#endif	
	fseek(fifo, 0, SEEK_END);
	fclose(fifo);
	fifo = NULL;

	//printf("done\n");	// tmp test

 err:
	if (fifo)
		fclose(fifo);

	/* Slurp anything remaining in the request */
	while (len-- > 0)
		ch = fgetc(stream);
		
	//printf("crc ret is %d, ver comp is %d\n", ret, cmpHeader);	// tmp test
	if ((!ret) && (cmpHeader))
		chk_image_err = 0;

//	fcntl(fileno(stream), F_SETOWN, -ret);	// no use
}

static void
do_upgrade_cgi(char *url, FILE *stream)
{
//	int ret;
	
//	ret = fcntl(fileno(stream), F_GETOWN, 0);	// no use
	
	printf("## [httpd] do upgrade cgi\n");	// tmp test
	/* Reboot if successful */
	//if (ret == 0)
	if (chk_image_err == 0)
	{
#ifndef WRITE2FLASH
		system("killall watchdog");
		websApply(stream, "Updating.asp");
		/* Close the stream to force browser display the content,
		 * flushed by websApply, or it will be closed only on reboot
		 * AFTER flashing.
		 */
//		fclose(stream);
		sys_upgrade("/tmp/linux.trx");
#else
		websApply(stream, "Restarting.asp");
#endif
		set_reboot_timestamp();
		sys_reboot();
	}	
	else    
	{
	   	websApply(stream, "UpdateError.asp");
	   	unlink("/tmp/linux.trx");
	}   	
	  
}

static void
do_upload_post(char *url, FILE *stream, int len, char *boundary)
{
	#define MAX_VERSION_LEN 64
	char upload_fifo[] = "/tmp/settings_u.prf";
	FILE *fifo = NULL;
	/*char *write_argv[] = { "write", upload_fifo, "linux", NULL };*/
//	pid_t pid;
	char buf[1024];
	int count, ret = EINVAL, ch;
	int /*eno, */cnt;
	long filelen, *filelenptr;
//	int hwmajor = 0, hwminor = 0;
	char /*version[MAX_VERSION_LEN], */cmpHeader;
//	char *hwver;

	cprintf("Start upload\n");
	eval("/sbin/stopservice", "99");

	/* Look for our part */
	while (len > 0) {
		if (!fgets(buf, MIN(len + 1, sizeof(buf)), stream)) {
			goto err;
		}

		len -= strlen(buf);

		if (!strncasecmp(buf, "Content-Disposition:", 20)
				&& strstr(buf, "name=\"file\""))
			break;
	}

	/* Skip boundary and headers */
	while (len > 0) {
		if (!fgets(buf, MIN(len + 1, sizeof(buf)), stream)) {
			goto err;
		}

		len -= strlen(buf);
		if (!strcmp(buf, "\n") || !strcmp(buf, "\r\n")) {
			break;
		}
	}

	if (!(fifo = fopen(upload_fifo, "a+")))
		goto err;

	filelen = len;
	cnt = 0;

	/* Pipe the rest to the FIFO */
	cprintf("Upgrading %d\n", len);
	cmpHeader = 0;

	while (len > 0 && filelen > 0) {
		if (waitfor (fileno(stream), 10) <= 0) {
			break;
		}

		count = fread(buf, 1, MIN(len, sizeof(buf)), stream);

		if (cnt == 0 && count > 8) {
			if (!strncmp(buf, PROFILE_HEADER, 4))
			{
				filelenptr = (buf + 4);
				filelen = *filelenptr;

			}
			else if (!strncmp(buf, PROFILE_HEADER_NEW, 4))
			{
				filelenptr = (buf + 4);
				filelen = *filelenptr;
				filelen = filelen & 0xffffff;

			}
			else
			{
				len -= count;
				goto err;
			}

			cmpHeader = 1;
			++cnt;
		}

		filelen -= count;
		len -= count;

		fwrite(buf, 1, count, fifo);
	}

	if (!cmpHeader)
		goto err;

	/* Slurp anything remaining in the request */
	while (len-- > 0) {
		ch = fgetc(stream);

		if (filelen > 0) {
			fwrite(&ch, 1, 1, fifo);
			--filelen;
		}
	}

	ret = 0;

	fseek(fifo, 0, SEEK_END);
	fclose(fifo);
	fifo = NULL;
	/*printf("done\n");*/

err:
	if (fifo)
		fclose(fifo);

	/* Slurp anything remaining in the request */
	while (len-- > 0)
		ch = fgetc(stream);

	fcntl(fileno(stream), F_SETOWN, -ret);
}

static void
do_upload_cgi(char *url, FILE *stream)
{
	int ret;
	
	//printf("do upload CGI\n");	// tmp test

	ret = fcntl(fileno(stream), F_GETOWN, 0);
	
	/* Reboot if successful */
	if (ret == 0)
	{
		websApply(stream, "Uploading.asp"); 
		sys_upload("/tmp/settings_u.prf");
		nvram_commit_safe();

		set_reboot_timestamp();
		sys_reboot();
	}	
	else    
	{
	   	websApply(stream, "UploadError.asp");
	   	//unlink("/tmp/settings_u.prf");
	}   	
	  
}

// Viz 2010.08
static void
do_update_cgi(char *url, FILE *stream)
{
        struct ej_handler *handler;
        const char *pattern;
        int argc;
        char *argv[16];
        char s[32];

        if ((pattern = get_cgi("output")) != NULL) {
                for (handler = &ej_handlers[0]; handler->pattern; handler++) {
                        if (strcmp(handler->pattern, pattern) == 0) {
                                for (argc = 0; argc < 16; ++argc) {
                                        sprintf(s, "arg%d", argc);
                                        if ((argv[argc] = (char *)get_cgi(s)) == NULL) break;
                                }
                                handler->output(0, stream, argc, argv);
                                break;
                        }
                }
        }
}

//Traffic Monitor
void wo_bwmbackup(char *url, webs_t wp)
{
        const char mime_binary[] = "application/tomato-binary-file";  
        static const char *hfn = "/var/lib/misc/rstats-history.gz";
        struct mime_handler *handler;
        struct stat st;
        time_t t;
        int i;

        if (stat(hfn, &st) == 0) {
                t = st.st_mtime;
                sleep(1);
        }
        else {
                t = 0;
        }
        killall("rstats", SIGHUP);
        for (i = 10; i > 0; --i) {
                if ((stat(hfn, &st) == 0) && (st.st_mtime != t)) break;
                sleep(1);
        }
        if (i == 0) {
                //send_error(500, "Bad Request", (char*) 0, "Internal server error." );
                return;
        }
        //send_headers(200, NULL, mime_binary, 0);
        do_f((char *)hfn, wp);
}
// end Viz ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^











static void
do_prf_file(char *url, FILE *stream)
{
	nvram_commit_safe();
	sys_download("/tmp/settings");
	do_file(url, stream);
	//unlink("/tmp/settings");
}


#elif defined(vxworks)

static void
do_upgrade_post(char *url, FILE *stream, int len, char *boundary)
{
}

static void
do_upgrade_cgi(char *url, FILE *stream)
{
}

#endif

// 2010.09 James. {
static char no_cache_IE7[] =
"X-UA-Compatible: IE=EmulateIE7\r\n"
"Cache-Control: no-cache\r\n"
"Pragma: no-cache\r\n"
"Expires: 0"
;
// 2010.09 James. }

static char no_cache[] =
"Cache-Control: no-cache\r\n"
"Pragma: no-cache\r\n"
"Expires: 0"
;

static void 
do_log_cgi(char *path, FILE *stream)
{
	dump_file(stream, "/tmp/syslog.log-1");
	dump_file(stream, "/tmp/syslog.log");
	fputs("\r\n", stream); /* terminator */
	fputs("\r\n", stream); /* terminator */
}

#ifdef WEBS
void
initHandlers(void)
{		
	websAspDefine("nvram_get_x", ej_nvram_get_x);
	websAspDefine("nvram_get_f", ej_nvram_get_f);
	websAspDefine("nvram_get_list_x", ej_nvram_get_list_x);	
	websAspDefine("nvram_get_buf_x", ej_nvram_get_buf_x);	
	websAspDefine("nvram_get_table_x", ej_nvram_get_table_x);	
	websAspDefine("nvram_match_x", ej_nvram_match_x);
	websAspDefine("nvram_double_match_x", ej_nvram_double_match_x);
    	websAspDefine("nvram_match_both_x", ej_nvram_match_both_x);	
	websAspDefine("nvram_match_list_x", ej_nvram_match_list_x);
	websAspDefine("select_channel", ej_select_channel);
    	websAspDefine("urlcache", ej_urlcache);	
	websAspDefine("uptime", ej_uptime);
	websAspDefine("sysuptime", ej_sysuptime);
	websAspDefine("nvram_dump", ej_dump);	
    	websAspDefine("load_script", ej_load);	
//    add by Viz  2010.08
	websAspDefine("qrate", ej_qos_packet);
	websAspDefine{"cgi_get", ej_cgi_get};
	websAspDefine("ctdump", ej_ctdump);
        websAspDefine("netdev", ej_netdev);
        websAspDefine("bandwidth", ej_bandwidth);
        websAspDefine("nvram", ej_backup_nvram);
//  end Viz
	websSecurityDelete();
	websUrlHandlerDefine("", NULL, 0, mySecurityHandler, WEBS_HANDLER_FIRST);	
	websUrlHandlerDefine("/apply.cgi", NULL, 0, apply_cgi, 0);	
	websSetPassword(nvram_safe_get_x("General", "x_Password"));
	websSetRealm(nvram_safe_get_x("general.log", "ProductID"));
#ifdef ASUS_DDNS //2007.03.27 Yau add
	websAspDefine("nvram_get_ddns", ej_nvram_get_ddns);
	websAspDefine("nvram_char_to_ascii", ej_nvram_char_to_ascii);
#endif
}


#else

//2008.08 magic{
struct mime_handler mime_handlers[] = {
	{ "Nologin.asp", "text/html", no_cache_IE7, do_html_post_and_get, do_ej, NULL },
	{ "error_page.htm*", "text/html", no_cache_IE7, do_html_post_and_get, do_ej, NULL },
	{ "gotoHomePage.htm", "text/html", no_cache_IE7, do_html_post_and_get, do_ej, NULL },
	{ "ure_success.htm", "text/html", no_cache_IE7, do_html_post_and_get, do_ej, NULL },
	{ "ureip.asp", "text/html", no_cache_IE7, do_html_post_and_get, do_ej, NULL },
	{ "remote.asp", "text/html", no_cache_IE7, do_html_post_and_get, do_ej, NULL },
	{ "jquery.js", "text/javascript", no_cache_IE7, NULL, do_file, NULL }, // 2010.09 James.
	{ "httpd_check.htm", "text/html", no_cache_IE7, do_html_post_and_get, do_ej, NULL },
	{ "**.htm*", "text/html", no_cache_IE7, do_html_post_and_get, do_ej, do_auth },
	{ "**.asp*", "text/html", no_cache_IE7, do_html_post_and_get, do_ej, do_auth },
	
	{ "**.css", "text/css", NULL, NULL, do_file, NULL },
	{ "**.png", "image/png", NULL, NULL, do_file, NULL },
	{ "**.gif", "image/gif", NULL, NULL, do_file, NULL },
	{ "**.jpg", "image/jpeg", NULL, NULL, do_file, NULL },
	// Viz 2010.08
        { "**.svg", "image/svg+xml", NULL, NULL, do_file, NULL },
        { "**.swf", "application/x-shockwave-flash", NULL, NULL, do_file, NULL  },
        { "**.htc", "text/x-component", NULL, NULL, do_file, NULL  },
	// end Viz

//#if defined(ASUS_MIMO) && defined(TRANSLATE_ON_FLY)
#ifdef TRANSLATE_ON_FLY
	/* Only general.js and quick.js are need to translate. (reduce translation time) */
	{ "general.js|quick.js",  "text/javascript", no_cache_IE7, NULL, do_ej, do_auth },
//#endif  // defined(ASUS_MIMO) && defined(TRANSLATE_ON_FLY)
#endif //TRANSLATE_ON_FLY
	
	{ "**.js",  "text/javascript", no_cache_IE7, NULL, do_ej, do_auth },
	{ "**.cab", "text/txt", NULL, NULL, do_file, do_auth },
	{ "**.CFG", "text/txt", NULL, NULL, do_prf_file, do_auth },
	
	{ "apply.cgi*", "text/html", no_cache_IE7, do_html_post_and_get, do_apply_cgi, do_auth },
	{ "upgrade.cgi*", "text/html", no_cache_IE7, do_upgrade_post, do_upgrade_cgi, do_auth},
	{ "upload.cgi*", "text/html", no_cache_IE7, do_upload_post, do_upload_cgi, do_auth },
 	{ "syslog.cgi*", "text/txt", no_cache_IE7, do_html_post_and_get, do_log_cgi, do_auth },
        // Viz 2010.08 vvvvv  
        { "update.cgi*", "text/javascript", no_cache_IE7, do_html_post_and_get, do_update_cgi, do_auth }, // jerry5 
        { "bwm/*.gz", NULL, no_cache, do_html_post_and_get, wo_bwmbackup, do_auth }, // jerry5
        // end Viz  ^^^^^^^^ 
//#ifdef TRANSLATE_ON_FLY
#ifdef TRANSLATE_ON_FLY
	{ "change_lang.cgi*", "text/html", no_cache_IE7, do_lang_post, do_lang_cgi, do_auth },
//#endif // TRANSLATE_ON_FLY
#endif //TRANSLATE_ON_FLY
	{ "webcam.cgi*", "text/html", no_cache_IE7, NULL, do_webcam_cgi, do_auth },
	{ NULL, NULL, NULL, NULL, NULL, NULL }
};
//2008.08 magic}

int ej_get_AiDisk_status(int eid, webs_t wp, int argc, char **argv) {
	disk_info_t *disks_info, *follow_disk;
	partition_info_t *follow_partition;
	char *follow_info;
	int sh_num;
	char **folder_list = NULL;
	int first_pool, first_folder, result, i;

	websWrite(wp, "function get_cifs_status() {\n");
	//websWrite(wp, "    return %d;\n", atoi(nvram_safe_get("samba_running")));
	websWrite(wp, "    return %d;\n", atoi(nvram_safe_get("enable_samba")));
	websWrite(wp, "}\n\n");

	websWrite(wp, "function get_ftp_status() {\n");
	//websWrite(wp, "    return %d;\n", atoi(nvram_safe_get("ftp_running")));
	websWrite(wp, "    return %d;\n", atoi(nvram_safe_get("enable_ftp")));
	websWrite(wp, "}\n\n");

	websWrite(wp, "function get_dms_status() {\n");
	websWrite(wp, "    return %d;\n", pids("ushare"));
	websWrite(wp, "}\n\n");

	websWrite(wp, "function get_share_management_status(protocol) {\n");
	websWrite(wp, "    if (protocol == \"cifs\")\n");
	websWrite(wp, "	return %d;\n", atoi(nvram_safe_get("st_samba_mode")));
	websWrite(wp, "    else if (protocol == \"ftp\")\n");
	websWrite(wp, "    if (protocol == \"ftp\")\n");
	websWrite(wp, "	return %d;\n", atoi(nvram_safe_get("st_ftp_mode")));
	websWrite(wp, "    else\n");
	websWrite(wp, "	return -1;\n");
	websWrite(wp, "}\n\n");

	disks_info = read_disk_data();
	if (disks_info == NULL) {
		websWrite(wp, "function get_sharedfolder_in_pool(poolName) {}\n");
		return -1;
	}
	first_pool = 1;
	websWrite(wp, "function get_sharedfolder_in_pool(poolName) {\n");
	for (follow_disk = disks_info; follow_disk != NULL; follow_disk = follow_disk->next)
		for (follow_partition = follow_disk->partitions; follow_partition != NULL; follow_partition = follow_partition->next) {
			if (follow_partition->mount_point != NULL && strlen(follow_partition->mount_point) > 0) {
				websWrite(wp, "    ");

				if (first_pool == 1)
					first_pool = 0;
				else
					websWrite(wp, "else ");

				follow_info = rindex(follow_partition->mount_point, '/');
				websWrite(wp, "if (poolName == \"%s\") {\n", follow_info+1);
				websWrite(wp, "	return [");

				result = get_all_folder_in_mount_path(follow_partition->mount_point, &sh_num, &folder_list);
				if (result < 0) {
					websWrite(wp, "];\n");
					websWrite(wp, "    }\n");

					printf("get_AiDisk_status: Can't get the folder list in \"%s\".\n", follow_partition->mount_point);

					free_2_dimension_list(&sh_num, &folder_list);

					continue;
				}

				first_folder = 1;
				for (i = 0; i < sh_num; ++i) {
					if (first_folder == 1)
						first_folder = 0;
					else
						websWrite(wp, ", ");

					websWrite(wp, "\"%s\"", folder_list[i]);
#if 0
//	tmp test
					printf("[httpd] chk folder list[%s]:\n", folder_list[i]);
					for (j=0; j<strlen(folder_list[i]); ++j)
					{
						printf("[%x] ", folder_list[i][j]);
					}
					printf("\nlen:(%d)\n", strlen(folder_list[i]));
//	tmp test ~
#endif

				}

				websWrite(wp, "];\n");
				websWrite(wp, "    }\n");
			}
		}

	websWrite(wp, "}\n\n");

	if (disks_info != NULL) {
		free_2_dimension_list(&sh_num, &folder_list);
		free_disk_data(&disks_info);
	}

	return 0;
}

int ej_get_all_accounts(int eid, webs_t wp, int argc, char **argv) {
	int acc_num;
	char **account_list = NULL;
	int result, i, first;

	if ((result = get_account_list(&acc_num, &account_list)) < 0) {
		printf("Failed to get the account list!\n");
		return -1;
	}

	first = 1;
	for (i = 0; i < acc_num; ++i) {
		if (first == 1)
			first = 0;
		else
			websWrite(wp, ", ");

		websWrite(wp, "\"%s\"", account_list[i]);
	}

	free_2_dimension_list(&acc_num, &account_list);
	return 0;
}

int
count_sddev_mountpoint()
{
	FILE *procpt;
	char line[256], devname[32], mpname[32], system_type[10], mount_mode[96];
	int dummy1, dummy2, count = 0;
															       
	if (procpt = fopen("/proc/mounts", "r"))
	while (fgets(line, sizeof(line), procpt))
	{
		if (sscanf(line, "%s %s %s %s %d %d", devname, mpname, system_type, mount_mode, &dummy1, &dummy2) != 6)
			continue;
			
		if (strstr(devname, "/dev/sd"))
			count++;
	}

	if (procpt)
		fclose(procpt);

	return count;
}

int start_usbled()
{
        char *cmd[] = {"start_usbled", NULL};
        int pid;

        _eval(cmd, NULL, 0, &pid);
        return 0;
}

int stop_usbled()
{
        char *cmd[] = {"stop_usbled", NULL};
        int pid;

        _eval(cmd, NULL, 0, &pid);
        return 0;
}

static int ej_safely_remove_disk(int eid, webs_t wp, int argc, char_t **argv) {
	int result = -1;
        char *disk_port = websGetVar(wp, "disk", "");
	int part_num = 0;
	int modem_mode = atoi(nvram_safe_get("modem_enable"));

	csprintf("disk_port = %s\n", disk_port);

	start_usbled();

        if(modem_mode > 0)
	{
		nvram_set("manually_disconnect_wan", "1");	/* unless do it, wanduck won't stop restart-modem... */
                stop_3g();
	}
	printf("safely remove confirm:%d\n", modem_mode);	// tmp test

	if (!disk_port)
		;
	else if (atoi(disk_port) == 1)
		result = eval("/sbin/ejusb1");
	else if (atoi(disk_port) == 2)
		result = eval("/sbin/ejusb2");

	if (result != 0) {
		show_error_msg("Action9");

		websWrite(wp, "<script>\n");
		websWrite(wp, "safely_remove_disk_error(\'%s\');\n", error_msg);
		websWrite(wp, "</script>\n");
		clean_error_msg();
		stop_usbled();
		return -1;
	}
	websWrite(wp, "<script>\n");
	websWrite(wp, "safely_remove_disk_success(\'%s\');\n", error_msg);
	websWrite(wp, "</script>\n");

	stop_usbled();
	printf("chk 3\n");	// tmp test
	return 0;
}



int ej_get_permissions_of_account(int eid, webs_t wp, int argc, char **argv) {
	disk_info_t *disks_info, *follow_disk;
	partition_info_t *follow_partition;
	int acc_num = 0, sh_num = 0;
	char **account_list = NULL, **folder_list;
	int samba_right, ftp_right, dms_right;
	int result, i, j;
	int first_pool, first_account, first_folder;

	//printf("[httpd] get permission of account chk\n");	// tmp test

	disks_info = read_disk_data();
	if (disks_info == NULL) {
		websWrite(wp, "function get_account_permissions_in_pool(account, pool) {return [];}\n");
		websWrite(wp, "function get_dms_permissions_in_pool(pool) {return [];}\n");
		return -1;
	}

	//printf("chk a\n");	// tmp test
	result = get_account_list(&acc_num, &account_list);
	//printf("chk b\n");	// tmp test

	if (result < 0) {
		printf("1. Can't get the account list.\n");
		free_2_dimension_list(&acc_num, &account_list);
		free_disk_data(&disks_info);
	}

	websWrite(wp, "function get_account_permissions_in_pool(account, pool) {\n");

	if (acc_num <= 0)
		websWrite(wp, "    return [];\n");

	first_account = 1;
	for (i = 0; i < acc_num; ++i) {
		websWrite(wp, "    ");
		if (first_account == 1)
			first_account = 0;
		else
			websWrite(wp, "else ");

		websWrite(wp, "if (account == \"%s\") {\n", account_list[i]);

		first_pool = 1;
		for (follow_disk = disks_info; follow_disk != NULL; follow_disk = follow_disk->next) {
			for (follow_partition = follow_disk->partitions; follow_partition != NULL; follow_partition = follow_partition->next) {
				if (follow_partition->mount_point != NULL && strlen(follow_partition->mount_point) > 0) {
					websWrite(wp, "	");
					if (first_pool == 1)
						first_pool = 0;
					else
						websWrite(wp, "else ");

					websWrite(wp, "if (pool == \"%s\") {\n", rindex(follow_partition->mount_point, '/')+1);

					websWrite(wp, "	    return [");

					result = get_all_folder_in_mount_path(follow_partition->mount_point, &sh_num, &folder_list);
					if (result != 0) {
						websWrite(wp, "];\n");
						websWrite(wp, "	}\n");

						printf("get_permissions_of_account1: Can't get all folders in \"%s\".\n", follow_partition->mount_point);

						free_2_dimension_list(&sh_num, &folder_list);

						continue;
					}
					first_folder = 1;
					for (j = 0; j < sh_num; ++j) {
						samba_right = get_permission(account_list[i],
													 follow_partition->mount_point,
													 folder_list[j],
													 "cifs");
						ftp_right = get_permission(account_list[i],
												   follow_partition->mount_point,
												   folder_list[j],
												   "ftp");
						if (samba_right < 0 || samba_right > 3) {
							printf("Can't get the CIFS permission abount \"%s\"!\n", folder_list[j]);

//							samba_right = DEFAULT_SAMBA_RIGHT;	// J++
							samba_right = 0;	
						}

						if (ftp_right < 0 || ftp_right > 3) {
							printf("Can't get the FTP permission abount \"%s\"!\n", folder_list[j]);

//							ftp_right = DEFAULT_FTP_RIGHT;		// J++
							ftp_right = 0;
						}

						if (first_folder == 1) {
							first_folder = 0;
							websWrite(wp, "[\"%s\", %d, %d]", folder_list[j], samba_right, ftp_right);
						}
						else
							websWrite(wp, "		    [\"%s\", %d, %d]", folder_list[j], samba_right, ftp_right);

						if (j != sh_num-1)
							websWrite(wp, ",\n");
					}
					websWrite(wp, "];\n");
					websWrite(wp, "	}\n");
				}
			}
		}

		websWrite(wp, "    }\n");
	}

	websWrite(wp, "}\n\n");

	if (sh_num > 0)
		free_2_dimension_list(&sh_num, &folder_list);

	//printf("chk 1\n");	// tmp test
	websWrite(wp, "function get_dms_permissions_in_pool(pool) {\n");

	first_pool = 1;
	for (follow_disk = disks_info; follow_disk != NULL; follow_disk = follow_disk->next) {
		for (follow_partition = follow_disk->partitions; follow_partition != NULL; follow_partition = follow_partition->next) {
			if (follow_partition->mount_point != NULL && strlen(follow_partition->mount_point) > 0) {
				websWrite(wp, "    ");
				if (first_pool == 1)
					first_pool = 0;
				else
					websWrite(wp, "else ");
				websWrite(wp, "if (pool == \"%s\") {\n", rindex(follow_partition->mount_point, '/')+1);

				websWrite(wp, "	return [");

				//printf("chk p1\n");	// tmp test
				result = get_all_folder_in_mount_path(follow_partition->mount_point, &sh_num, &folder_list);
				//printf("chk p2\n");	// tmp test
				if (result != 0) {
					websWrite(wp, "];\n");
					websWrite(wp, "    }\n");

					printf("get_permissions_of_account2: Can't get all folders in \"%s\".\n", follow_partition->mount_point);

					free_2_dimension_list(&sh_num, &folder_list);

					continue;
				}

				//printf("chk p3\n");	// tmp test
				first_folder = 1;
				for (j = 0; j < sh_num; ++j) {
					dms_right = get_permission("MediaServer",
											   follow_partition->mount_point,
											   folder_list[j],
											   "dms");
					if (dms_right < 0 || dms_right > 1) {
						printf("Can't get the DMS permission abount \"%s\"!\n", folder_list[j]);

						dms_right = DEFAULT_DMS_RIGHT;
					}

					if (first_folder == 1) {
						first_folder = 0;
						websWrite(wp, "[\"%s\", %d]", folder_list[j], dms_right);
					}
					else
						websWrite(wp, "		[\"%s\", %d]", folder_list[j], dms_right);

					if (j != sh_num-1)
						websWrite(wp, ",\n");
				}
				//printf("chk p4\n");	// tmp test

				websWrite(wp, "];\n");
				websWrite(wp, "    }\n");
			}
		}
	}
	//printf("chk 2\n");	// tmp test

	websWrite(wp, "}\n\n");

	if (acc_num > 0)
		free_2_dimension_list(&acc_num, &account_list);

	if (sh_num > 0)
		free_2_dimension_list(&sh_num, &folder_list);

	if (disks_info != NULL)
		free_disk_data(&disks_info);

	return 0;
}


int ej_get_folder_tree(int eid, webs_t wp, int argc, char **argv) {
	char *layer_order = websGetVar(wp, "layer_order", ""), folder_code[1024];
	char *follow_info, *follow_info_end, backup;
	int layer = 0, first;
	int disk_count, partition_count, folder_count1, folder_count2;
	int disk_order = -1, partition_order = -1, folder_order = -1;
	disk_info_t *disks_info, *follow_disk;
	partition_info_t *follow_partition;
	char *pool_mount_dir;
	DIR *dir1, *dir2;
	struct dirent *dp1, *dp2;
	char dir1_Path[4096], dir2_Path[4096];

	if (strlen(layer_order) <= 0) {
		printf("No input \"layer_order\"!\n");
		return -1;
	}

	follow_info = index(layer_order, '_');
	while (follow_info != NULL && *follow_info != 0) {
		++layer;

		++follow_info;
		if (*follow_info == 0)
			break;
		follow_info_end = follow_info;
		while (*follow_info_end != 0 && isdigit(*follow_info_end))
			++follow_info_end;
		backup = *follow_info_end;
		*follow_info_end = 0;

		if (layer == 1)
			disk_order = atoi(follow_info);
		else if (layer == 2)
			partition_order = atoi(follow_info);
		else if (layer == 3)
			folder_order = atoi(follow_info);
		*follow_info_end = backup;

		if (layer == 3) {
			memset(folder_code, 0, 1024);
			strcpy(folder_code, follow_info);
		}

		follow_info = follow_info_end;
	}
	follow_info = folder_code;

	disks_info = read_disk_data();
	if (disks_info == NULL) {
		printf("Can't read the information of disks.\n");
		return -1;
	}

	first = 1;
	disk_count = 0;
	for (follow_disk = disks_info; follow_disk != NULL; follow_disk = follow_disk->next, ++disk_count) {
		if (layer == 0) { // get disks.
			partition_count = 0;
			for (follow_partition = follow_disk->partitions; follow_partition != NULL; follow_partition = follow_partition->next, ++partition_count)
				;

			if (first == 1)
				first = 0;
			else
				websWrite(wp, ", ");

			websWrite(wp, "\"%s#%u#%u\"", follow_disk->tag, disk_count, partition_count);

			continue;
		}
		if (disk_count != disk_order)
			continue;

		partition_count = 0;
		for (follow_partition = follow_disk->partitions; follow_partition != NULL; follow_partition = follow_partition->next, ++partition_count) {
			if (follow_partition->mount_point == NULL || strlen(follow_partition->mount_point) <= 0)
				continue;

			pool_mount_dir = rindex(follow_partition->mount_point, '/')+1;

			if (layer == 1) { // get pools.
				dir2 = opendir(follow_partition->mount_point);
				/* pool_mount_dir isn't file. */
				if (dir2 == NULL)
					continue;

				folder_count2 = 0;
				while ((dp2 = readdir(dir2)) != NULL) {
					if (dp2->d_name[0] == '.')
						continue;

					++folder_count2;
				}
				closedir(dir2);

				if (first == 1)
					first = 0;
				else
					websWrite(wp, ", ");

				websWrite(wp, "\"%s#%u#%u\"", pool_mount_dir, partition_count, folder_count2);

				continue;
			}
			if (partition_count != partition_order)
				continue;

			memset(dir1_Path, 0, 4096);
			sprintf(dir1_Path, "%s", follow_partition->mount_point);
			dir1 = opendir(dir1_Path);
			if (dir1 == NULL) {
				printf("Can't open the directory, %s.\n", dir1_Path);

				free_disk_data(&disks_info);

				return -1;
			}

			folder_count1 = -1;
			while ((dp1 = readdir(dir1)) != NULL) {
				if (dp1->d_name[0] == '.')
					continue;

				++folder_count1;

				if (layer == 2) { // get L1's folders.
					memset(dir2_Path, 0, 4096);
					sprintf(dir2_Path, "%s/%s", dir1_Path, dp1->d_name);
					dir2 = opendir(dir2_Path);

					folder_count2 = 0;
					if (dir2 != NULL) {
						while ((dp2 = readdir(dir2)) != NULL) {
							if (dp2->d_name[0] == '.')
								continue;

							++folder_count2;
						}
						closedir(dir2);
					}
					if (first == 1)
						first = 0;
					else
						websWrite(wp, ", ");

					websWrite(wp, "\"%s#%u#%u\"", dp1->d_name, folder_count1, folder_count2);

					continue;
				}

				if (folder_count1 == folder_order)
					sprintf(dir1_Path, "%s/%s", dir1_Path, dp1->d_name);
			}
			closedir(dir1);
		}
	}
	free_disk_data(&disks_info);
	layer -= 3;

	while (layer >= 0) {      // get Ln's folders.
		/* get the current folder_code and folder_order. */
		follow_info_end = index(follow_info, '_');
		if (follow_info_end != NULL)
			follow_info = follow_info_end+1;
		else
			backup = -1;
		folder_order = atoi(follow_info);

		dir1 = opendir(dir1_Path);
		if (dir1 == NULL) {
			printf("Can't open the directory, %s.\n", dir1_Path);

			return -1;
		}
		folder_count1 = -1;
		while ((dp1 = readdir(dir1)) != NULL) {
			if (dp1->d_name[0] == '.')
				continue;

			++folder_count1;

			if (layer == 0) {
				memset(dir2_Path, 0, 4096);
				sprintf(dir2_Path, "%s/%s", dir1_Path, dp1->d_name);
				dir2 = opendir(dir2_Path);
				folder_count2 = 0;
				if (dir2 != NULL) {
					while ((dp2 = readdir(dir2)) != NULL) {
						if (dp2->d_name[0] == '.')
							continue;

						++folder_count2;
					}
					closedir(dir2);
				}

				if (first == 1)
					first = 0;
				else
					websWrite(wp, ", ");

				websWrite(wp, "\"%s#%u#%u\"", dp1->d_name, folder_count1, folder_count2);

				continue;
			}

			if (folder_count1 == folder_order)
				sprintf(dir1_Path, "%s/%s", dir1_Path, dp1->d_name);
		}
		closedir(dir1);
		--layer;
	}

	return 0;
}

int ej_get_share_tree(int eid, webs_t wp, int argc, char **argv) {
	char *layer_order = websGetVar(wp, "layer_order", "");
	char *follow_info, *follow_info_end, backup;
	int layer = 0, first;
	int disk_count, partition_count, share_count;
	int disk_order = -1, partition_order = -1;
	disk_info_t *disks_info, *follow_disk;
	partition_info_t *follow_partition;

	if (strlen(layer_order) <= 0) {
		printf("No input \"layer_order\"!\n");
		return -1;
	}

	follow_info = index(layer_order, '_');
	while (follow_info != NULL && *follow_info != 0) {
		++layer;
		++follow_info;
		if (*follow_info == 0)
			break;
		follow_info_end = follow_info;
		while (*follow_info_end != 0 && isdigit(*follow_info_end))
			++follow_info_end;
		backup = *follow_info_end;
		*follow_info_end = 0;

		if (layer == 1)
			disk_order = atoi(follow_info);
		else if (layer == 2)
			partition_order = atoi(follow_info);
		else {
			*follow_info_end = backup;
			printf("Input \"%s\" is incorrect!\n", layer_order);
			return -1;
		}

		*follow_info_end = backup;
		follow_info = follow_info_end;
	}

	disks_info = read_disk_data();
	if (disks_info == NULL) {
		printf("Can't read the information of disks.\n");
		return -1;
	}

	first = 1;
	disk_count = 0;
	for (follow_disk = disks_info; follow_disk != NULL; follow_disk = follow_disk->next, ++disk_count) {
		partition_count = 0;
		for (follow_partition = follow_disk->partitions; follow_partition != NULL; follow_partition = follow_partition->next, ++partition_count) {
			if (layer != 0 && follow_partition->mount_point != NULL && strlen(follow_partition->mount_point) > 0) {
				int i;
				char **folder_list;
				int result;
				result = get_all_folder_in_mount_path(follow_partition->mount_point, &share_count, &folder_list);
				if (result < 0) {
					printf("get_disk_tree: Can't get the folder list in \"%s\".\n", follow_partition->mount_point);

					share_count = 0;
				}

				if (layer == 2 && partition_count == partition_order && disk_count == disk_order) {
					for (i = 0; i < share_count; ++i) {
						if (first == 1)
							first = 0;
						else
							websWrite(wp, ", ");

						websWrite(wp, "\"%s#%u#0\"", folder_list[i], i);
					}
				}
				else if (layer == 1 && disk_count == disk_order) {
					if (first == 1)
						first = 0;
					else
						websWrite(wp, ", ");

					follow_info = rindex(follow_partition->mount_point, '/');
					websWrite(wp, "\"%s#%u#%u\"", follow_info+1, partition_count, share_count);
				}

				free_2_dimension_list(&share_count, &folder_list);
			}
		}
		if (layer == 0) {
			if (first == 1)
				first = 0;
			else
				websWrite(wp, ", ");

			websWrite(wp, "\"%s#%u#%u\"", follow_disk->tag, disk_count, partition_count);
		}

		if (layer > 0 && disk_count == disk_order)
			break;
	}

	free_disk_data(&disks_info);

	return 0;
}

#ifndef RTCONFIG_USB_MODEM
int ej_start3g(int eid, webs_t wp, int argc, char **argv) {

	printf("[httpd start 3g process\n]");	// tmp test

	system("start3g");
	return 0;
}

int ej_stop3g(int eid, webs_t wp, int argc, char **argv) {

	printf("[httpd stop 3g process\n]");	// tmp test

	system("stop3g");
	return 0;
}
#endif

void not_ej_initial_folder_var_file()						// J++
{
	disk_info_t *disks_info, *follow_disk;
	partition_info_t *follow_partition;

	disks_info = read_disk_data();
	if (disks_info == NULL)
		return;

	for (follow_disk = disks_info; follow_disk != NULL; follow_disk = follow_disk->next)
		for (follow_partition = follow_disk->partitions; follow_partition != NULL; follow_partition = follow_partition->next)
			if (follow_partition->mount_point != NULL && strlen(follow_partition->mount_point) > 0) {
				initial_folder_list_in_mount_path(follow_partition->mount_point);
//				initial_all_var_file_in_mount_path(follow_partition->mount_point);
			}

	free_disk_data(&disks_info);
}

int ej_initial_folder_var_file(int eid, webs_t wp, int argc, char **argv)	// J++
{
//	not_ej_initial_folder_var_file();
	return 0;
}

int ej_set_share_mode(int eid, webs_t wp, int argc, char **argv) {
	int samba_mode = atoi(nvram_safe_get("st_samba_mode"));
	int ftp_mode = atoi(nvram_safe_get("st_ftp_mode"));
	char *dummyShareway = websGetVar(wp, "dummyShareway", "");
	char *protocol = websGetVar(wp, "protocol", "");
	char *mode = websGetVar(wp, "mode", "");
	int result;

	printf("[httpd] set share mode: prot = %s\n", protocol);	// tmp test
	if (strlen(dummyShareway) > 0)
		nvram_set("dummyShareway", dummyShareway);
	else
		nvram_set("dummyShareway", "0");

	if (strlen(protocol) <= 0) {
		show_error_msg("Input1");

		websWrite(wp, "<script>\n");
		websWrite(wp, "set_share_mode_error(\'%s\');\n", error_msg);
		websWrite(wp, "</script>\n");

		clean_error_msg();
		return -1;
	}
	if (strlen(mode) <= 0) {
		show_error_msg("Input3");

		websWrite(wp, "<script>\n");
		websWrite(wp, "set_share_mode_error(\'%s\');\n", error_msg);
		websWrite(wp, "</script>\n");

		clean_error_msg();
		return -1;
	}
	if (!strcmp(mode, "share")) {
		if (!strcmp(protocol, "cifs")) {
			if (samba_mode == 1 || samba_mode == 3)
				goto SET_SHARE_MODE_SUCCESS;

			nvram_set("st_samba_mode", "1");	// for test
		}
		else if (!strcmp(protocol, "ftp")) {
			if (ftp_mode == 1)
				goto SET_SHARE_MODE_SUCCESS;

			nvram_set("st_ftp_mode", "1");
		}
		else {
			show_error_msg("Input2");

			websWrite(wp, "<script>\n");
			websWrite(wp, "set_share_mode_error(\'%s\');\n", error_msg);
			websWrite(wp, "</script>\n");

			clean_error_msg();
			return -1;
		}
	}
	else if (!strcmp(mode, "account")) {
		if (!strcmp(protocol, "cifs")) {
			if (samba_mode == 2 || samba_mode == 4)
				goto SET_SHARE_MODE_SUCCESS;

			nvram_set("st_samba_mode", "4");
		}
		else if (!strcmp(protocol, "ftp")) {
			if (ftp_mode == 2)
				goto SET_SHARE_MODE_SUCCESS;

			nvram_set("st_ftp_mode", "2");
		}
		else {
			show_error_msg("Input2");

			websWrite(wp, "<script>\n");
			websWrite(wp, "set_share_mode_error(\'%s\');\n", error_msg);
			websWrite(wp, "</script>\n");

			clean_error_msg();
			return -1;
		}
	}
	else {
		show_error_msg("Input4");

		websWrite(wp, "<script>\n");
		websWrite(wp, "set_share_mode_error(\'%s\');\n", error_msg);
		websWrite(wp, "</script>\n");

		clean_error_msg();
		return -1;
	}

	nvram_commit_safe();

	not_ej_initial_folder_var_file();	// J++

	if (!strcmp(protocol, "cifs"))
	{
		system("nvram set chk=3");	// tmp test
		result = eval("/sbin/run_samba");
	}
	else if (!strcmp(protocol, "ftp"))
		result = eval("/sbin/run_ftp");
	else {
		show_error_msg("Input2");

		websWrite(wp, "<script>\n");
		websWrite(wp, "set_share_mode_error(\'%s\');\n", error_msg);
		websWrite(wp, "</script>\n");

		clean_error_msg();
		return -1;
	}

	if (result != 0) {
		show_error_msg("Action8");

		websWrite(wp, "<script>\n");
		websWrite(wp, "set_share_mode_error(\'%s\');\n", error_msg);
		websWrite(wp, "</script>\n");

		clean_error_msg();
		return -1;
	}

SET_SHARE_MODE_SUCCESS:
	websWrite(wp, "<script>\n");
	websWrite(wp, "set_share_mode_success();\n");
	websWrite(wp, "</script>\n");
	return 0;
}


int ej_modify_sharedfolder(int eid, webs_t wp, int argc, char **argv) {
	char *pool = websGetVar(wp, "pool", "");
	char *folder = websGetVar(wp, "folder", "");
	char *new_folder = websGetVar(wp, "new_folder", "");
	char *mount_path;

	//printf("[httpd] mod share folder\n");	// tmp test
	if (strlen(pool) <= 0) {
		show_error_msg("Input7");

		websWrite(wp, "<script>\n");
		websWrite(wp, "modify_sharedfolder_error(\'%s\');\n", error_msg);
		websWrite(wp, "</script>\n");

		clean_error_msg();
		return -1;
	}
	if (strlen(folder) <= 0) {
		show_error_msg("Input9");

		websWrite(wp, "<script>\n");
		websWrite(wp, "modify_sharedfolder_error(\'%s\');\n", error_msg);
		websWrite(wp, "</script>\n");

		clean_error_msg();
		return -1;
	}
	if (strlen(new_folder) <= 0) {
		show_error_msg("Input17");

		websWrite(wp, "<script>\n");
		websWrite(wp, "modify_sharedfolder_error(\'%s\');\n", error_msg);
		websWrite(wp, "</script>\n");

		clean_error_msg();
		return -1;
	}
	if (get_mount_path(pool, &mount_path) < 0) {
		show_error_msg("System1");

		websWrite(wp, "<script>\n");
		websWrite(wp, "modify_sharedfolder_error(\'%s\');\n", error_msg);
		websWrite(wp, "</script>\n");

		clean_error_msg();
		return -1;
	}

	if (mod_folder(mount_path, folder, new_folder) < 0) {
		show_error_msg("Action7");

		websWrite(wp, "<script>\n");
		websWrite(wp, "modify_sharedfolder_error(\'%s\');\n", error_msg);
		websWrite(wp, "</script>\n");
		free(mount_path);

		clean_error_msg();
		return -1;
	}
	free(mount_path);

	if (eval("/sbin/run_ftpsamba") != 0) {
		show_error_msg("Action7");

		websWrite(wp, "<script>\n");
		websWrite(wp, "modify_sharedfolder_error(\'%s\');\n", error_msg);
		websWrite(wp, "</script>\n");
		free(mount_path);

		clean_error_msg();
		return -1;
	}
	websWrite(wp, "<script>\n");
	websWrite(wp, "modify_sharedfolder_success();\n");
	websWrite(wp, "</script>\n");
	return 0;
}

int ej_delete_sharedfolder(int eid, webs_t wp, int argc, char **argv) {
	char *pool = websGetVar(wp, "pool", "");
	char *folder = websGetVar(wp, "folder", "");
	char *mount_path;

	if (strlen(pool) <= 0) {
		show_error_msg("Input7");

		websWrite(wp, "<script>\n");
		websWrite(wp, "delete_sharedfolder_error(\'%s\');\n", error_msg);
		websWrite(wp, "</script>\n");

		clean_error_msg();
		return -1;
	}
	if (strlen(folder) <= 0) {
		show_error_msg("Input9");

		websWrite(wp, "<script>\n");
		websWrite(wp, "delete_sharedfolder_error(\'%s\');\n", error_msg);
		websWrite(wp, "</script>\n");

		clean_error_msg();
		return -1;
	}

	if (get_mount_path(pool, &mount_path) < 0) {
		show_error_msg("System1");

		websWrite(wp, "<script>\n");
		websWrite(wp, "delete_sharedfolder_error(\'%s\');\n", error_msg);
		websWrite(wp, "</script>\n");

		clean_error_msg();
		return -1;
	}
	if (del_folder(mount_path, folder) < 0) {
		show_error_msg("Action6");

		websWrite(wp, "<script>\n");
		websWrite(wp, "delete_sharedfolder_error(\'%s\');\n", error_msg);
		websWrite(wp, "</script>\n");
		free(mount_path);

		clean_error_msg();
		return -1;
	}
	free(mount_path);

	if (eval("/sbin/run_ftpsamba") != 0) {
		show_error_msg("Action6");

		websWrite(wp, "<script>\n");
		websWrite(wp, "delete_sharedfolder_error(\'%s\');\n", error_msg);
		websWrite(wp, "</script>\n");
		free(mount_path);

		clean_error_msg();
		return -1;
	}

	websWrite(wp, "<script>\n");
	websWrite(wp, "delete_sharedfolder_success();\n");
	websWrite(wp, "</script>\n");
	return 0;
}

int ej_create_sharedfolder(int eid, webs_t wp, int argc, char **argv) {
	char *pool = websGetVar(wp, "pool", "");
	char *folder = websGetVar(wp, "folder", "");
	char *mount_path;

	printf("[httpd] create share folder\n");	// tmp test
	if (strlen(pool) <= 0) {
		show_error_msg("Input7");

		websWrite(wp, "<script>\n");
		websWrite(wp, "create_sharedfolder_error(\'%s\');\n", error_msg);
		websWrite(wp, "</script>\n");

		clean_error_msg();
		return -1;
	}
	if (strlen(folder) <= 0) {
		show_error_msg("Input9");

		websWrite(wp, "<script>\n");
		websWrite(wp, "create_sharedfolder_error(\'%s\');\n", error_msg);
		websWrite(wp, "</script>\n");

		clean_error_msg();
		return -1;
	}

	if (get_mount_path(pool, &mount_path) < 0) {
		fprintf(stderr, "Can't get the mount_path of %s.\n", pool);

		show_error_msg("System1");

		websWrite(wp, "<script>\n");
		websWrite(wp, "create_sharedfolder_error(\'%s\');\n", error_msg);
		websWrite(wp, "</script>\n");

		clean_error_msg();
		return -1;
	}
	if (add_folder(mount_path, folder) < 0) {
		show_error_msg("Action5");

		websWrite(wp, "<script>\n");
		websWrite(wp, "create_sharedfolder_error(\'%s\');\n", error_msg);
		websWrite(wp, "</script>\n");
		free(mount_path);

		clean_error_msg();
		return -1;
	}
	free(mount_path);

	system("nvram set chk=1");	// tmp test
	if (eval("/sbin/run_samba") != 0) {
		show_error_msg("Action5");

		websWrite(wp, "<script>\n");
		websWrite(wp, "create_sharedfolder_error(\'%s\');\n", error_msg);
		websWrite(wp, "</script>\n");
		free(mount_path);

		clean_error_msg();
		return -1;
	}
	websWrite(wp, "<script>\n");
	websWrite(wp, "create_sharedfolder_success();\n");
	websWrite(wp, "</script>\n");
	return 0;
}

int ej_set_AiDisk_status(int eid, webs_t wp, int argc, char **argv) {
	char *protocol = websGetVar(wp, "protocol", "");
	char *flag = websGetVar(wp, "flag", "");
	int result = 0;

	printf("[httpd] set aidisk status\n");	// tmp test
	if (strlen(protocol) <= 0) {
		show_error_msg("Input1");

		websWrite(wp, "<script>\n");
		websWrite(wp, "set_AiDisk_status_error(\'%s\');\n", error_msg);
		websWrite(wp, "</script>\n");

		clean_error_msg();
		return -1;
	}
	if (strlen(flag) <= 0) {
		show_error_msg("Input18");

		websWrite(wp, "<script>\n");
		websWrite(wp, "set_AiDisk_status_error(\'%s\');\n", error_msg);
		websWrite(wp, "</script>\n");

		clean_error_msg();
		return -1;
	}
	if (!strcmp(protocol, "cifs")) {
		if (!strcmp(flag, "on")) {
			nvram_set("enable_samba", "1");
			nvram_commit_safe();
			result = system("/sbin/run_samba");
		}
		else if (!strcmp(flag, "off")) {
			nvram_set("enable_samba", "0");
			nvram_commit_safe();
			if (!pids("smbd"))
				goto SET_AIDISK_STATUS_SUCCESS;

			result = system("/sbin/stop_samba");
		}
		else {
			show_error_msg("Input19");

			websWrite(wp, "<script>\n");
			websWrite(wp, "set_AiDisk_status_error(\'%s\');\n", error_msg);
			websWrite(wp, "</script>\n");

			clean_error_msg();
			return -1;
		}
	}
	else if (!strcmp(protocol, "ftp")) {
		if (!strcmp(flag, "on")) {
			nvram_set("enable_ftp", "1");
			nvram_commit_safe();
			result = system("run_ftp");
		}
		else if (!strcmp(flag, "off")) {
			nvram_set("enable_ftp", "0");
			nvram_commit_safe();
			if (!pids("vsftpd"))
				goto SET_AIDISK_STATUS_SUCCESS;

			result = system("/sbin/stop_ftp");
		}
		else {
			show_error_msg("Input19");

			websWrite(wp, "<script>\n");
			websWrite(wp, "set_AiDisk_status_error(\'%s\');\n", error_msg);
			websWrite(wp, "</script>\n");

			clean_error_msg();
			return -1;
		}
	}
	else {
		show_error_msg("Input2");

		websWrite(wp, "<script>\n");
		websWrite(wp, "set_AiDisk_status_error(\'%s\');\n", error_msg);
		websWrite(wp, "</script>\n");

		clean_error_msg();
		return -1;
	}

	if (result != 0) {
		show_error_msg("Action8");

		websWrite(wp, "<script>\n");
		websWrite(wp, "set_AiDisk_status_error(\'%s\');\n", error_msg);
		websWrite(wp, "</script>\n");

		clean_error_msg();
		return -1;
	}

SET_AIDISK_STATUS_SUCCESS:
	websWrite(wp, "<script>\n");
	//websWrite(wp, "set_AiDisk_status_success();\n");
	websWrite(wp, "parent.resultOfSwitchAppStatus();\n");
	websWrite(wp, "</script>\n");

	printf("set aidisk done\n");	// tmp test
	return 0;
}

int ej_modify_account(int eid, webs_t wp, int argc, char **argv) {
	char *account = websGetVar(wp, "account", "");
	char *new_account = websGetVar(wp, "new_account", "");
	char *new_password = websGetVar(wp, "new_password", "");

	if (strlen(account) <= 0) {
		show_error_msg("Input5");

		websWrite(wp, "<script>\n");
		websWrite(wp, "modify_account_error(\'%s\');\n", error_msg);
		websWrite(wp, "</script>\n");

		clean_error_msg();
		return -1;
	}
	if (strlen(new_account) <= 0 && strlen(new_password) <= 0) {
		show_error_msg("Input16");

		websWrite(wp, "<script>\n");
		websWrite(wp, "modify_account_error(\'%s\');\n", error_msg);
		websWrite(wp, "</script>\n");

		clean_error_msg();
		return -1;
	}

	if (mod_account(account, new_account, new_password) < 0) {
		show_error_msg("Action4");

		websWrite(wp, "<script>\n");
		websWrite(wp, "modify_account_error(\'%s\');\n", error_msg);
		websWrite(wp, "</script>\n");

		clean_error_msg();
		return -1;
	}
	websWrite(wp, "<script>\n");
	websWrite(wp, "modify_account_success();\n");
	websWrite(wp, "</script>\n");
	return 0;
}

int ej_delete_account(int eid, webs_t wp, int argc, char **argv) {
	char *account = websGetVar(wp, "account", "");

	printf("[httpd] delete account\n");	// tmp test
	if (strlen(account) <= 0) {
		show_error_msg("Input5");

		websWrite(wp, "<script>\n");
		websWrite(wp, "delete_account_error(\'%s\');\n", error_msg);
		websWrite(wp, "</script>\n");

		clean_error_msg();
		return -1;
	}

	not_ej_initial_folder_var_file();	// J++

	if (del_account(account) < 0) {
		show_error_msg("Action3");

		websWrite(wp, "<script>\n");
		websWrite(wp, "delete_account_error(\'%s\');\n", error_msg);
		websWrite(wp, "</script>\n");

		clean_error_msg();
		return -1;
	}

	websWrite(wp, "<script>\n");
	websWrite(wp, "delete_account_success();\n");
	websWrite(wp, "</script>\n");
	return 0;
}

int ej_initial_account(int eid, webs_t wp, int argc, char **argv) {
	disk_info_t *disks_info, *follow_disk;
	partition_info_t *follow_partition;
	char *command;
	int len, result;

	printf("[httpd] initial account\n");	// tmp test
	nvram_set("acc_num", "0");
	nvram_commit_safe();

	disks_info = read_disk_data();
	if (disks_info == NULL) {
		show_error_msg("System2");

		websWrite(wp, "<script>\n");
		websWrite(wp, "initial_account_error(\'%s\');\n", error_msg);
		websWrite(wp, "</script>\n");

		clean_error_msg();
		return -1;
	}

	for (follow_disk = disks_info; follow_disk != NULL; follow_disk = follow_disk->next)
		for (follow_partition = follow_disk->partitions; follow_partition != NULL; follow_partition = follow_partition->next)
			if (follow_partition->mount_point != NULL && strlen(follow_partition->mount_point) > 0) {
				len = strlen("rm -f ")+strlen(follow_partition->mount_point)+strlen("/.__*");
				command = (char *)malloc(sizeof(char)*(len+1));
				if (command == NULL) {
					show_error_msg("System1");

					websWrite(wp, "<script>\n");
					websWrite(wp, "initial_account_error(\'%s\');\n", error_msg);
					websWrite(wp, "</script>\n");

					clean_error_msg();
					return -1;
				}
				sprintf(command, "rm -f %s/.__*", follow_partition->mount_point);
				command[len] = 0;

				result = system(command);
				free(command);

				initial_folder_list_in_mount_path(follow_partition->mount_point);
				initial_all_var_file_in_mount_path(follow_partition->mount_point);
			}

	free_disk_data(&disks_info);

	if (eval("/sbin/run_ftpsamba") != 0) {
		show_error_msg("System1");

		websWrite(wp, "<script>\n");
		websWrite(wp, "initial_account_error(\'%s\');\n", error_msg);
		websWrite(wp, "</script>\n");

		clean_error_msg();
		return -1;
	}
	websWrite(wp, "<script>\n");
	websWrite(wp, "initial_account_success();\n");
	websWrite(wp, "</script>\n");

	return 0;
}

int ej_create_account(int eid, webs_t wp, int argc, char **argv) {
	char *account = websGetVar(wp, "account", "");
	char *password = websGetVar(wp, "password", "");

	printf("[httpd] create account\n");	// tmp test
	if (strlen(account) <= 0) {
		show_error_msg("Input5");

		websWrite(wp, "<script>\n");
		websWrite(wp, "create_account_error(\'%s\');\n", error_msg);
		websWrite(wp, "</script>\n");

		clean_error_msg();
		return -1;
	}
	if (strlen(password) <= 0) {
		show_error_msg("Input14");

		websWrite(wp, "<script>\n");
		websWrite(wp, "create_account_error(\'%s\');\n", error_msg);
		websWrite(wp, "</script>\n");

		clean_error_msg();
		return -1;
	}

	not_ej_initial_folder_var_file();	// J++

	if (add_account(account, password) < 0) {
		show_error_msg("Action2");

		websWrite(wp, "<script>\n");
		websWrite(wp, "create_account_error(\'%s\');\n", error_msg);
		websWrite(wp, "</script>\n");

		clean_error_msg();
		return -1;
	}
	websWrite(wp, "<script>\n");
	websWrite(wp, "create_account_success();\n");
	websWrite(wp, "</script>\n");

	return 0;
}

int ej_set_account_permission(int eid, webs_t wp, int argc, char **argv) {
	char *mount_path;
	char *account = websGetVar(wp, "account", "");
	char *pool = websGetVar(wp, "pool", "");
	char *folder = websGetVar(wp, "folder", "");
	char *protocol = websGetVar(wp, "protocol", "");
	char *permission = websGetVar(wp, "permission", "");
	int right;

	if (strlen(account) <= 0) {
		show_error_msg("Input5");

		websWrite(wp, "<script>\n");
		websWrite(wp, "set_account_permission_error(\'%s\');\n", error_msg);
		websWrite(wp, "</script>\n");

		clean_error_msg();
		return -1;
	}
	if (test_if_exist_account(account) != 1) {
		show_error_msg("Input6");

		websWrite(wp, "<script>\n");
		websWrite(wp, "set_account_permission_error(\'%s\');\n", error_msg);
		websWrite(wp, "</script>\n");

		clean_error_msg();
		return -1;
	}

	if (strlen(pool) <= 0) {
		show_error_msg("Input7");

		websWrite(wp, "<script>\n");
		websWrite(wp, "set_account_permission_error(\'%s\');\n", error_msg);
		websWrite(wp, "</script>\n");

		clean_error_msg();
		return -1;
	}
	if (get_mount_path(pool, &mount_path) < 0) {
		fprintf(stderr, "Can't get the mount_path of %s.\n", pool);

		show_error_msg("System1");

		websWrite(wp, "<script>\n");
		websWrite(wp, "set_account_permission_error(\'%s\');\n", error_msg);
		websWrite(wp, "</script>\n");

		clean_error_msg();
		return -1;
	}

	if (strlen(folder) <= 0) {
		show_error_msg("Input9");

		websWrite(wp, "<script>\n");
		websWrite(wp, "set_account_permission_error(\'%s\');\n", error_msg);
		websWrite(wp, "</script>\n");
		free(mount_path);

		clean_error_msg();
		return -1;
	}
	if (strlen(protocol) <= 0) {
		show_error_msg("Input1");

		websWrite(wp, "<script>\n");
		websWrite(wp, "set_account_permission_error(\'%s\');\n", error_msg);
		websWrite(wp, "</script>\n");
		free(mount_path);

		clean_error_msg();
		return -1;
	}
	if (strcmp(protocol, "cifs") && strcmp(protocol, "ftp") && strcmp(protocol, "dms")) {
		show_error_msg("Input2");

		websWrite(wp, "<script>\n");
		websWrite(wp, "set_account_permission_error(\'%s\');\n", error_msg);
		websWrite(wp, "</script>\n");
		free(mount_path);

		clean_error_msg();
		return -1;
	}

	if (strlen(permission) <= 0) {
		show_error_msg("Input12");

		websWrite(wp, "<script>\n");
		websWrite(wp, "set_account_permission_error(\'%s\');\n", error_msg);
		websWrite(wp, "</script>\n");
		free(mount_path);

		clean_error_msg();
		return -1;
	}
	right = atoi(permission);
	if (right < 0 || right > 3) {
		show_error_msg("Input13");

		websWrite(wp, "<script>\n");
		websWrite(wp, "set_account_permission_error(\'%s\');\n", error_msg);
		websWrite(wp, "</script>\n");
		free(mount_path);

		clean_error_msg();
		return -1;
	}

	if (set_permission(account, mount_path, folder, protocol, right) < 0) {
		show_error_msg("Action1");

		websWrite(wp, "<script>\n");
		websWrite(wp, "set_account_permission_error(\'%s\');\n", error_msg);
		websWrite(wp, "</script>\n");
		free(mount_path);

		clean_error_msg();
		return -1;
	}

	websWrite(wp, "<script>\n");
	websWrite(wp, "set_account_permission_success();\n");
	websWrite(wp, "</script>\n");
	free(mount_path);
	return 0;
}

// 2010.09 James. {
int start_mac_clone(int eid, webs_t wp, int argc, char **argv) {
	char *cmd[] = {"start_mac_clone", NULL};
	int pid;

	dbg("start mac clone...\n");
	_eval(cmd, NULL, 0, &pid);
	return 0;
}

int setting_lan(int eid, webs_t wp, int argc, char **argv) {
	char lan_ipaddr_t[16];
	char lan_netmask_t[16];
	unsigned int lan_ip_num;
	unsigned int lan_mask_num;
	unsigned int lan_subnet;
	char wan_ipaddr_t[16];
	char wan_netmask_t[16];
	unsigned int wan_ip_num;
	unsigned int wan_mask_num;
	unsigned int wan_subnet;
	const unsigned int MAX_SUBNET = 3232300800;
	const unsigned int MIN_LAN_IP = 3232235521;
	struct in_addr addr;
	unsigned int new_lan_ip_num;
	unsigned int new_dhcp_start_num;
	unsigned int new_dhcp_end_num;
	char new_lan_ip_str[16];
	char new_dhcp_start_str[16];
	char new_dhcp_end_str[16];
	
	memset(lan_ipaddr_t, 0, 16);
	strcpy(lan_ipaddr_t, nvram_safe_get("lan_ipaddr_t"));
	memset(&addr, 0, sizeof(addr));
	lan_ip_num = ntohl(inet_aton(lan_ipaddr_t, &addr));
	lan_ip_num = ntohl(addr.s_addr);
	memset(lan_netmask_t, 0, 16);
	strcpy(lan_netmask_t, nvram_safe_get("lan_netmask_t"));
	memset(&addr, 0, sizeof(addr));
	lan_mask_num = ntohl(inet_aton(lan_netmask_t, &addr));
	lan_mask_num = ntohl(addr.s_addr);
	lan_subnet = lan_ip_num&lan_mask_num;
dbg("http: get lan_subnet=%x!\n", lan_subnet);
	memset(wan_ipaddr_t, 0, 16);
	strcpy(wan_ipaddr_t, nvram_safe_get("wan_ipaddr_t"));
	memset(&addr, 0, sizeof(addr));
	wan_ip_num = ntohl(inet_aton(wan_ipaddr_t, &addr));
	wan_ip_num = ntohl(addr.s_addr);
	memset(wan_netmask_t, 0, 16);
	strcpy(wan_netmask_t, nvram_safe_get("wan_netmask_t"));
	memset(&addr, 0, sizeof(addr));
	wan_mask_num = ntohl(inet_aton(wan_netmask_t, &addr));
	wan_mask_num = ntohl(addr.s_addr);
	wan_subnet = wan_ip_num&wan_mask_num;
dbg("http: get wan_subnet=%x!\n", wan_subnet);
	
	if (strcmp(nvram_safe_get("wan_ready"), "1")
			|| lan_subnet != wan_subnet
			) {
		websWrite(wp, "0");
		return 0;
	}
	
	if (lan_subnet >= MAX_SUBNET)
		new_lan_ip_num = MIN_LAN_IP;
	else
		new_lan_ip_num = lan_ip_num+(~lan_mask_num)+1;
	
	new_dhcp_start_num = new_lan_ip_num+1;
	new_dhcp_end_num = new_lan_ip_num+(~inet_network(lan_netmask_t))-2;
dbg("%u, %u, %u.\n", new_lan_ip_num, new_dhcp_start_num, new_dhcp_end_num);
	memset(&addr, 0, sizeof(addr));
	addr.s_addr = htonl(new_lan_ip_num);
	memset(new_lan_ip_str, 0, 16);
	strcpy(new_lan_ip_str, inet_ntoa(addr));
	memset(&addr, 0, sizeof(addr));
	addr.s_addr = htonl(new_dhcp_start_num);
	memset(new_dhcp_start_str, 0, 16);
	strcpy(new_dhcp_start_str, inet_ntoa(addr));
	memset(&addr, 0, sizeof(addr));
	addr.s_addr = htonl(new_dhcp_end_num);
	memset(new_dhcp_end_str, 0, 16);
	strcpy(new_dhcp_end_str, inet_ntoa(addr));
dbg("%s, %s, %s.\n", new_lan_ip_str, new_dhcp_start_str, new_dhcp_end_str);
	nvram_set("lan_ipaddr", new_lan_ip_str);
	nvram_set("dhcp_start", new_dhcp_start_str);
	nvram_set("dhcp_end", new_dhcp_end_str);
	
	websWrite(wp, "1");
	
	nvram_commit_safe();
	
	set_reboot_timestamp();
	sys_reboot();
	return 0;
}
// 2010.09 James. }

// qos svg support 2010.08 Viz vvvvvvvvvvvv
void asp_ctcount(webs_t wp, int argc, char_t **argv)
{
	static const char *states[10] = {
		"NONE", "ESTABLISHED", "SYN_SENT", "SYN_RECV", "FIN_WAIT",
		"TIME_WAIT", "CLOSE", "CLOSE_WAIT", "LAST_ACK", "LISTEN" };
	int count[13];	// tcp(10) + udp(2) + total(1) = 13 / max classes = 10
	FILE *f;
	char s[512];
	char *p;
	int i;
	int n;
	int mode;
	unsigned long rip;
	unsigned long lan;
	unsigned long mask;
	int ret;

	if (argc != 1) return;
	mode = atoi(argv[0]);

	memset(count, 0, sizeof(count));
	

	  if ((f = fopen("/proc/net/ip_conntrack", "r")) != NULL) {   
		// ctvbuf(f);	// if possible, read in one go

		if (nvram_match("t_hidelr", "1")) {
			mask = inet_addr(nvram_safe_get("lan_netmask"));
			rip = inet_addr(nvram_safe_get("lan_ipaddr"));
			lan = rip & mask;
		}
		else {
			rip = lan = mask = 0;
		}

		while (fgets(s, sizeof(s), f)) {
			if (rip != 0) {
				// src=x.x.x.x dst=x.x.x.x	// DIR_ORIGINAL
				if ((p = strstr(s + 14, "src=")) == NULL) continue;
				if ((inet_addr(p + 4) & mask) == lan) {
					if ((p = strstr(p + 13, "dst=")) == NULL) continue;
					if (inet_addr(p + 4) == rip) continue;
				}
			}

			if (mode == 0) {
				// count connections per state
				if (strncmp(s, "tcp", 3) == 0) {
					for (i = 9; i >= 0; --i) {
						if (strstr(s, states[i]) != NULL) {
							count[i]++;
							break;
						}
					}
				}
				else if (strncmp(s, "udp", 3) == 0) {
					if (strstr(s, "[UNREPLIED]") != NULL) {
						count[10]++;
					}
					else if (strstr(s, "[ASSURED]") != NULL) {
						count[11]++;
					}
				}
				count[12]++;
			}
			else {
				// count connections per mark
				if ((p = strstr(s, " mark=")) != NULL) {
					n = atoi(p + 6) & 0xFF;
					if (n <= 10) count[n]++;
				}
			}
		}

		fclose(f);
	}

	if (mode == 0) {
		p = s;
		for (i = 0; i < 12; ++i) {
			p += sprintf(p, ",%d", count[i]);
		}
		ret += websWrite(wp, "\nconntrack = [%d%s];\n", count[12], s);
	}
	else {
		p = s;
		for (i = 1; i < 11; ++i) {
			p += sprintf(p, ",%d", count[i]);
		}
		ret += websWrite(wp, "\nnfmarks = [%d%s];\n", count[0], s);
	}
}

void ej_qos_packet(int eid, webs_t wp, int argc, char_t **argv)
{
	FILE *f;
	char s[256];
	unsigned long rates[10];
	unsigned long u;
	char *e;
	int n;
	char comma;
	char *a[1];
	int ret;

	a[0] = "1";
  asp_ctcount(wp, 1, a);

	memset(rates, 0, sizeof(rates));
	sprintf(s, "tc -s class ls dev %s", nvram_safe_get("wan0_ifname"));
	if ((f = popen(s, "r")) != NULL) {
		n = 1;
		while (fgets(s, sizeof(s), f)) {
			if (strncmp(s, "class htb 1:", 12) == 0) {
				n = atoi(s + 12);
			}
			else if (strncmp(s, " rate ", 6) == 0) {
				if ((n % 10) == 0) {
					n /= 10;
					if ((n >= 1) && (n <= 10)) {
						u = strtoul(s + 6, &e, 10);
						if (*e == 'K') u *= 1000;
							else if (*e == 'M') u *= 1000 * 1000;
						rates[n - 1] = u;
						n = 1;
					}
				}
			}
		}
		pclose(f);
	}

	comma = ' ';
	ret += websWrite(wp, "\nqrates = [0,");
	for (n = 0; n < 10; ++n) {
		ret += websWrite(wp, "%c%lu", comma, rates[n]);
		comma = ',';
	}
	ret += websWrite(wp, "];");
}

void ej_ctdump(int eid, webs_t wp, int argc, char **argv)
{
	FILE *f;
	char s[512];
	char *p, *q;
	int mark;
	int findmark;
	unsigned int proto;
	unsigned int time;
	char src[16];
	char dst[16];
	char sport[16];
	char dport[16];
	unsigned long rip;
	unsigned long lan;
	unsigned long mask;
	char comma;
	int ret;

	if (argc != 1) return;

	findmark = atoi(argv[0]);

	mask = inet_addr(nvram_safe_get("lan_netmask"));
	rip = inet_addr(nvram_safe_get("lan_ipaddr"));
	lan = rip & mask;
	if (nvram_match("t_hidelr", "0")) rip = 0;	// hide lan -> router?

	ret += websWrite(wp, "\nctdump = [");
	comma = ' ';
	if ((f = fopen("/proc/net/ip_conntrack", "r")) != NULL) {
		//ctvbuf(f);
		while (fgets(s, sizeof(s), f)) {
			if ((p = strstr(s, " mark=")) == NULL) continue;
			if ((mark = (atoi(p + 6) & 0xFF)) > 10) mark = 0;
			if ((findmark != -1) && (mark != findmark)) continue;

			if (sscanf(s, "%*s %u %u", &proto, &time) != 2) continue;

			if ((p = strstr(s + 14, "src=")) == NULL) continue;		// DIR_ORIGINAL
			if ((inet_addr(p + 4) & mask) != lan) {
				// make sure we're seeing int---ext if possible
				if ((p = strstr(p + 41, "src=")) == NULL) continue;	// DIR_REPLY
			}
			else if (rip != 0) {
				if ((q = strstr(p + 13, "dst=")) == NULL) continue;
//				cprintf("%lx=%lx\n", inet_addr(q + 4), rip);
				if (inet_addr(q + 4) == rip) continue;
			}

			if ((proto == 6) || (proto == 17)) {
				if (sscanf(p + 4, "%s dst=%s sport=%s dport=%s", src, dst, sport, dport) != 4) continue;
			}
			else {
				if (sscanf(p + 4, "%s dst=%s", src, dst) != 2) continue;
				sport[0] = 0;
				dport[0] = 0;
			}
			ret += websWrite(wp, "%c[%u,%u,'%s','%s','%s','%s',%d]", comma, proto, time, src, dst, sport, dport, mark);
			comma = ',';
		}
	}
	ret += websWrite(wp, "];\n");
}

void ej_cgi_get(int eid, webs_t wp, int argc, char **argv)
{
	const char *v;
	int i;
	int ret;

	for (i = 0; i < argc; ++i) {
		v = get_cgi(argv[i]);
		if (v) ret += websWrite(wp, v);
	}
}

// traffic monitor
static int ej_netdev(int eid, webs_t wp, int argc, char_t **argv)
{
  FILE * fp;
  char buf[256];
  unsigned long rx, tx;
  char *p;
  char *ifname;
  char comma;
  int ret;

  ret += websWrite(wp, "\nnetdev = {\n");
  if ((fp = fopen("/proc/net/dev", "r")) != NULL) {
		fgets(buf, sizeof(buf), fp);	
		fgets(buf, sizeof(buf), fp);	
		comma = ' ';
			while (fgets(buf, sizeof(buf), fp)) {
				if ((p = strchr(buf, ':')) == NULL) continue;
				*p = 0;
				if ((ifname = strrchr(buf, ' ')) == NULL) ifname = buf;
			   		else ++ifname;       
          	   		if (sscanf(p + 1, "%lu%*u%*u%*u%*u%*u%*u%*u%lu", &rx, &tx) != 2) continue; 
					ret += websWrite(wp, "%c'%s':{rx:0x%lx,tx:0x%lx}", comma, ifname, rx, tx);
					comma = ',';
					ret += websWrite(wp, "\n");
			}
		fclose(fp);
		ret += websWrite(wp, "}");
  }
  return 0;
}

void ej_bandwidth(int eid, webs_t wp, int argc, char_t **argv)
{
	char *name;
	int sig;
	FILE *stream; 

	if (strcmp(argv[0], "speed") == 0) {
		sig = SIGUSR1;
		name = "/var/spool/rstats-speed.js";
	}
	else {
		sig = SIGUSR2;
		name = "/var/spool/rstats-history.js";
	}
	unlink(name);
	killall("rstats", sig);
//	eval("killall", sig, "rstats");
	f_wait_exists(name, 5);
	do_f(name, wp);
	unlink(name);
}

void ej_backup_nvram(int eid, webs_t wp, int argc, char_t **argv)
{
	char *list;
	char *p, *k;
	const char *v;

	if ((argc != 1) || ((list = strdup(argv[0])) == NULL)) return;
	websWrite(wp, "\nnvram = {\n");
	p = list;
	while ((k = strsep(&p, ",")) != NULL) {
		if (*k == 0) continue;
		v = nvram_get(k);
		if (!v) {
			v = "";
		}
		websWrite(wp, "\t%s: '", k);
		websWrite(wp, v);
//		web_puts((p == NULL) ? "'\n" : "',\n");
		websWrite(wp, "',\n");
	}
	free(list);
	websWrite(wp, "\thttp_id: '");
	websWrite(wp, nvram_safe_get("http_id"));
	websWrite(wp, "'};\n");
//	web_puts("};\n");
}
// end svg support by Viz ^^^^^^^^^^^^^^^^^^^^

static int
ej_select_list(int eid, webs_t wp, int argc, char_t **argv)
{
	char *id;
	int ret = 0;	
	char out[64], idxstr[12], tmpstr[64], tmpstr1[64];
	int i, curr, hit, noneFlag;
	char *ref1, *ref2, *refnum;

	if (ejArgs(argc, argv, "%s", &id) < 1) {
		websError(wp, 400, "Insufficient args\n");
		return -1;
	}

	if (strcmp(id, "Storage_x_SharedPath")==0)
	{
		ref1 = "sh_path_x";
		ref2 = "sh_path";
		refnum = "sh_num";
		curr = atoi(nvram_get(ref1));
		sprintf(idxstr, "%d", curr);
		strcpy(tmpstr1, nvram_get(strcat_r(ref2, idxstr, tmpstr)));
		sprintf(out, "%s", tmpstr1);															      
		ret += websWrite(wp, out);
		return ret;
	}
	else if (strncmp(id, "Storage_x_AccUser", 17)==0)
	{
		sprintf(tmpstr, "sh_accuser_x%s", id + 17);
		ref2 = "acc_username";
		refnum = "acc_num";

		curr = atoi(nvram_get(tmpstr));
		noneFlag =1;
	}
	else if (strcmp(id, "Storage_x_AccAny")==0)
	{
		
		 ret = websWrite(wp, "<option value=\"Guest\">Guest</option>");
		 return ret;
	}
	else if (strcmp(id, "Storage_AllUser_List")==0)
	{

		strcpy(out, "<option value=\"Guest\">Guest</option>");
		ret += websWrite(wp, out);

		for (i=0;i<atoi(nvram_get("acc_num"));i++)
		{
			 sprintf(idxstr, "%d", i);
			 strcpy(tmpstr1, nvram_get(strcat_r("acc_username", idxstr, tmpstr)));
			 sprintf(out, "<option value=\"%s\">%s</option>", tmpstr1, tmpstr1);
			 ret += websWrite(wp, out);
		}
		return ret;
	}
	else 
	{
		 return ret;     
	}
	
	hit = 0;
 
	for (i=0;i<atoi(nvram_get(refnum));i++)
	{     
 		 sprintf(idxstr, "%d", i);
		 strcpy(tmpstr1, nvram_get(strcat_r(ref2, idxstr, tmpstr)));
	     	 sprintf(out, "<option value=\"%d\"", i);

		 if (i==curr) 
		 {
			hit = 1;
			sprintf(out, "%s selected", out);
		 }
		 sprintf(out,"%s>%s</option>", out, tmpstr1);       
 
		 ret += websWrite(wp, out);
	}     

	if (noneFlag)
	{
		cprintf("hit : %d\n", hit);
		if (!hit) sprintf(out, "<option value=\"99\" selected>None</option>");
		else sprintf(out, "<option value=\"99\">None</option>");

		ret += websWrite(wp, out);
	}	
	return ret;
}

struct ej_handler ej_handlers[] = {
	{ "nvram_get_x", ej_nvram_get_x},
	{ "nvram_get_f", ej_nvram_get_f},
	{ "nvram_get_list_x", ej_nvram_get_list_x},
	{ "nvram_get_buf_x", ej_nvram_get_buf_x},
	{ "nvram_get_table_x", ej_nvram_get_table_x},
	{ "nvram_match_x", ej_nvram_match_x},
	{ "nvram_double_match_x", ej_nvram_double_match_x},
	{ "nvram_match_both_x", ej_nvram_match_both_x},
	{ "nvram_match_list_x", ej_nvram_match_list_x},
	{ "select_channel", ej_select_channel},
	{ "urlcache", ej_urlcache},     
	{ "uptime", ej_uptime},
	{ "sysuptime", ej_sysuptime},
	{ "nvram_dump", ej_dump},
	{ "load_script", ej_load},
	{ "select_list", ej_select_list},
//tomato qosvvvvvvvvvvv 2010.08 Viz
        {"qrate", ej_qos_packet},
        {"ctdump", ej_ctdump},
        { "netdev", ej_netdev},
        { "bandwidth", ej_bandwidth},
        { "nvram", ej_backup_nvram},
//tomato qos^^^^^^^^^^^^ end Viz

#ifdef ASUS_DDNS //2007.03.27 Yau add
	{ "nvram_get_ddns", ej_nvram_get_ddns},
	{ "nvram_char_to_ascii", ej_nvram_char_to_ascii},
#endif
//2008.08 magic{
	{ "update_variables", update_variables_ex},
	{ "convert_asus_variables", convert_asus_variables},
	{ "asus_nvram_commit", asus_nvram_commit},
	{ "notify_services", ej_notify_services},
	{ "detect_if_wan", detect_if_wan},
	{ "detect_wan_connection", detect_wan_connection},
	{ "detect_dhcp_pppoe", detect_dhcp_pppoe},
	{ "get_wan_status_log", get_wan_status_log},
	{ "wanlink", wanlink_hook},
	{ "wan_action", wan_action_hook},
	{ "check_hwnat", check_hwnat},
	{ "dm_running", dm_running},
	{ "get_parameter", ej_get_parameter},
	{ "login_state_hook", login_state_hook},
	{ "dumpleases", ej_dumpleases},
	{ "get_nvram_list", ej_get_nvram_list},
	{ "dhcp_leases", ej_dhcp_leases},
	{ "get_arp_table", ej_get_arp_table},
	{ "get_static_client", ej_get_static_client},
	{ "get_changed_status", ej_get_changed_status},
	{ "wl_auth_list", ej_wl_auth_list},
	{ "wl_scan_5g", ej_wl_scan_5g},
	{ "wl_scan_2g", ej_wl_scan_2g},
	{ "shown_time", ej_shown_time},
	{ "shown_language_option", ej_shown_language_option},
	{ "disk_pool_mapping_info", ej_disk_pool_mapping_info},
	{ "available_disk_names_and_sizes", ej_available_disk_names_and_sizes},
	{ "get_printer_info", ej_get_printer_info},
	{ "get_AiDisk_status", ej_get_AiDisk_status},
	{ "set_AiDisk_status", ej_set_AiDisk_status},
	{ "get_all_accounts", ej_get_all_accounts},
	{ "safely_remove_disk", ej_safely_remove_disk},
	{ "get_permissions_of_account", ej_get_permissions_of_account},
	{ "set_account_permission", ej_set_account_permission},
	{ "get_folder_tree", ej_get_folder_tree},
	{ "get_share_tree", ej_get_share_tree},
	{ "initial_account", ej_initial_account},
	{ "create_account", ej_create_account},	/*no ccc*/
	{ "delete_account", ej_delete_account}, /*n*/
	{ "modify_account", ej_modify_account}, /*n*/
	{ "create_sharedfolder", ej_create_sharedfolder},	/*y*/
	{ "delete_sharedfolder", ej_delete_sharedfolder},	/*y*/
	{ "modify_sharedfolder", ej_modify_sharedfolder},	/* no ccc*/
	{ "set_share_mode", ej_set_share_mode},
#ifndef RTCONFIG_USB_MODEM
	{ "start3g", ej_start3g},
	{ "stop3g", ej_stop3g},
#endif
	{ "initial_folder_var_file", ej_initial_folder_var_file},	/* J++ */
//2008.08 magic}
// 2010.09 James. {
	{ "start_mac_clone", start_mac_clone},
	{ "setting_lan", setting_lan},
// 2010.09 James. }
	{ NULL, NULL }
};

#endif /* !WEBS */

void websSetVer(void)
{
	FILE *fp;
	unsigned long *imagelen;
	char dataPtr[4];
	char verPtr[64];
	char productid[13];
	char fwver[12];

	strcpy(productid, "WLHDD");
	strcpy(fwver, "0.1.0.1");

	if ((fp = fopen("/dev/mtd3", "rb"))!=NULL)
	{
		if (fseek(fp, 4, SEEK_SET)!=0) goto write_ver;
		fread(dataPtr, 1, 4, fp);
		imagelen = (unsigned long *)dataPtr;

		cprintf("image len %x\n", *imagelen);
		if (fseek(fp, *imagelen - 64, SEEK_SET)!=0) goto write_ver;
		cprintf("seek\n");
		if (!fread(verPtr, 1, 64, fp)) goto write_ver;
		cprintf("ver %x %x", verPtr[0], verPtr[1]);
		strncpy(productid, verPtr + 4, 12);
		productid[12] = 0;
		sprintf(fwver, "%d.%d.%d.%d", verPtr[0], verPtr[1], verPtr[2], verPtr[3]);

		cprintf("get fw ver: %s\n", productid);
		fclose(fp);
	}
write_ver:
	nvram_set_f("general.log", "productid", productid);
	nvram_set_f("general.log", "firmver", fwver);	
}

/* 
 * Kills process whose PID is stored in plaintext in pidfile
 * @param	pidfile	PID file, signal
 * @return	0 on success and errno on failure
 */
int
kill_pidfile_s(char *pidfile, int sig)
{
	FILE *fp = fopen(pidfile, "r");
	char buf[256];

	if (fp && fgets(buf, sizeof(buf), fp)) {
		pid_t pid = strtoul(buf, NULL, 0);
		fclose(fp);
		return kill(pid, sig);
  	} else
		return errno;
}

#ifdef DLM
void
umount_disc_parts(int usb_port)
{
	eval("ejusb");
}
#endif

