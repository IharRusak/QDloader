/******************************************************************************
  @file    main.c
  @brief   entry point.

  DESCRIPTION
  QDloader Tool for USB and PCIE of Quectel wireless cellular modules.

  INITIALIZATION AND SEQUENCING REQUIREMENTS
  None.

  ---------------------------------------------------------------------------
  Copyright (c) 2016 - 2021 Quectel Wireless Solution, Co., Ltd.  All Rights Reserved.
  Quectel Wireless Solution Proprietary and Confidential.
  ---------------------------------------------------------------------------
******************************************************************************/
#include "std_c.h"
#include <getopt.h>
#include <sys/stat.h>
#include "usbfs/usbfs.h"

extern int dloader_main(int usbfd, FILE *fw_fp, const char *modem_name);
extern int fbfdownloader_main(int usbfd, FILE *fw_fp, const char *modem_name);
extern int aboot_main(int usbfd, FILE *fw_fp, const char *modem_name);

pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;
FILE *log_fp = NULL;
char log_buf[1024];
char g_specify_port[64] = {0};
char modem_name_para[32] = {0};
int q_erase_all = 0;
int fw_file_skip = 0;
int is_R03 = 0;
extern int usb_interface;
char file_path[256] = {0};
int mcu_is_little_endian = 0;

int ql_capture_usbmon_log(const char* usbmon_logfile);
void ql_stop_usbmon_log();
int check_mcu_endian();

static struct quectel_modem_info quectel_modems[] =
{
    {"RG500U", "2c7c/900/",  4, "AT+QDownLOAD=1\r"},
    {"RG500U", "1782/4d00/", 0, NULL},
    {"EC200U", "2c7c/901/",  2, "AT+QDownLOAD=1\r"},
    {"EC200D", "2c7c/902/",  2, "AT+QDownLOAD=1\r"},
    {"EC200D", "1782/4d00/", 0, NULL},
    {"EC200U", "525/a4a7/",  1, NULL},
    {"EC200T", "2c7c/6026/", 3, "AT+CFUN=1,1\r"},
    {"EC200T", "1286/812a/", 0, NULL},
    {"UG89TA", "2c7c/6120/", 4, "AT+CFUN=1,1\r"},
    {"UC200A", "2c7c/6006/", 4, "AT+CFUN=1,1\r"},
    {"EG060V", "2c7c/6004/", 3, "AT+CFUN=1,1\r"},
    {"EG060V", "1286/8181/", 0, NULL},
    {"EC200S", "2c7c/6002/", 3, "AT+QDownLOAD=1\r"},
    {"EC200S", "2c7c/6001/", 3, "AT+QDownLOAD=1\r"},
    {"EC200S", "2ecc/3017/", 1, NULL},
    {"EC200S", "2ecc/3004/", 1, NULL},
    {"EC200A", "2c7c/6005/", 3, "AT+CFUN=1,1\r"},
    {"EC200A", "2ecc/3001/", 0, NULL},
    {"EC200A", "2ecc/3002/", 0, NULL},
    {"RM500K", "2c7c/7001/", 0, NULL},
    {"RG500L", "2c7c/7003/", 0, NULL},
    {"RG620T", "2c7c/7006/", 0, NULL},
    {"EC800G", "2c7c/904/",  2, "AT+QDownLOAD=1\r"},
    {"EC800G", "1782/4d16/", 1, NULL},
    {NULL,      NULL,        0, NULL},
};

const struct quectel_modem_info * find_quectel_modem(const char *PRODUCT) {
    int i = 0;
    while (quectel_modems[i].modem_name) {
        if (strStartsWith(PRODUCT, quectel_modems[i].PRODUCT)) {
            if (strstr(PRODUCT,"202") && (strstr(PRODUCT,"1782") && strstr(PRODUCT,"4d00")))  //UIS8310 -- 1782/4d00/202   //UDX710 -- 1782/4d00/2430
            {
                if (strncasecmp(quectel_modems[i].modem_name, "EC200D", 6))  // EC200D is 8310, RG500 is udx710, so need to distinguish
                {
                    i++;
                    continue;
                }
            }

            if (strstr(PRODUCT, "2c7c"))   //The State Grid module cannot forcibly specify the diag interface value in emergency download mode
            {
                if ((!strncasecmp(modem_name_para, "EC200T", 6) || !strncasecmp(modem_name_para, "EC200A", 6))
                    || (!strncasecmp(modem_name_para, "RM500U", 6) || !strncasecmp(modem_name_para, "RG200U", 6))
                    || (!strncasecmp(modem_name_para, "EC200N", 6) || !strncasecmp(modem_name_para, "EC200S", 6))
                    || (!strncasecmp(modem_name_para, "EC200D", 6)))
                {
                    if (usb_interface >= 0)
                        quectel_modems[i].usb_interface = usb_interface;
                }
            }

            return &quectel_modems[i];
        }
        i++;
    }
    return NULL;
}
int verbose = 0;
static USBFS_T usbfs_dev[8];

int strStartsWith(const char *line, const char *prefix)
{
    for ( ; *line != '\0' && *prefix != '\0' ; line++, prefix++) {
        if (*line != *prefix) {
            return 0;
        }
    }

    return *prefix == '\0';
}

const char *get_time(void) {
    static char str[64];
    static unsigned start = 0;
    unsigned now;
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);

    now = (unsigned)ts.tv_sec*1000 + (unsigned)(ts.tv_nsec / 1000000);
    if (start == 0)
        start = now;

    now -= start;
    snprintf(str, sizeof(str), "[%03d:%03d]", now/1000, now%1000);

    return str;
}

static void switch_to_download(int usbfd, const char *download_atc, const char *dev_modem_name)
{
    sendSync(usbfd, "ATI\r", 4, 0);
    sendSync(usbfd, "AT+EGMR=0,5\r", 12, 0);
    sendSync(usbfd, "AT+EGMR=0,7\r", 12, 0);

    while (!access(dev_modem_name, F_OK)) {
        int ret;
        char at_rsp[256];

        dprintf("AT > %s\n", download_atc);
        ret = sendSync(usbfd, download_atc, strlen(download_atc), 0);
        if (ret < 1) break;

        do  {
            ret = recvSync(usbfd, at_rsp, sizeof(at_rsp), 1000);
            if (ret > 0) {
                int i, len = 0;

                for (i = 0; i < ret; i++) {
                    uint8_t ch = at_rsp[i];

                    if (ch != '\r' && ch != '\n')
                        at_rsp[len++] = ch;
                    else if (len > 1) {
                        at_rsp[len++] = 0;
                        dprintf("AT < %s\n", at_rsp);
                        len = 0;
                    }
                }
            }
        } while (ret > 0);

        if (errno != ETIMEDOUT) {sleep(1); break;};
    }
}

static int usage(const char *self) {
    printf("%s -f fw_image\n", self);
    printf("for EC200U, fw image is '.pac',         can be found in the FW package\n");
    printf("for RG500U, fw image is '.pac',         can be found in the FW package\n");
    printf("for EC200S, fw image is 'firmware.bin', can be found in the FW package\n");
    printf("for EC200T, fw image is 'BinFile.bin',  create by windows tool 'FBFMake.exe -r update.blf -f output', .blf file can be found in the FW package \n");
    printf("for RM500K/RG500L, fw image is 'NULL', sysupgrade.bin must be placed in the adb /tmp/ directory, the -b argument is required, like './QDloader -b' \n");
    printf("for the state grid module, the module model needs to be specified, such as '-g EC200T' \n");
    printf("for backup NV, default store in the /tmp directory, '-x' is required to specify the path, such as '-x /root/users' \n");

    return 0;
}

char *q_strrstr(char *dst, char *src)
{
	char *str1=dst;
	char *str2=src;
	char *fast=NULL;
	char *last=NULL;
	assert(dst);
	assert(src);
	while (*str1)
	{
		fast=str1;
		while (*str1&&*str2&&*str1==*str2)
		{
			str1++;
			str2++;
		}
		if (*str2 == '\0')
			last=fast;

		str1=fast+1;
		str2 = src;
	}
	if (*str1 == '\0')
		return last;

    return NULL;
}

static int get_port_from_cmd(char* g_specify_port)
{
    if (g_specify_port[0] == '\0' || strncasecmp(g_specify_port, "/dev", 4) != 0)
        return -1;
    FILE* fp = NULL;
    char cmd_buf[80] = {0};
    char recv_buf[256] = {0};
    snprintf(cmd_buf, sizeof(cmd_buf), "ls -l /sys/class/tty/%s", g_specify_port+5);
    fp = popen(cmd_buf, "r");
    if (fp == NULL)
        return -1;
    int recv_length = fread(recv_buf, 1, sizeof(recv_buf), fp);
    if (recv_length <= 0)
        return -1;
    char* ptr = q_strrstr(recv_buf, "/usb");
    if (ptr == NULL)
        return -1;

    int ptr_len = strlen(ptr);
    memset(g_specify_port, 0, 64);
    if (ptr[13] == ':')
    {
        if (ptr_len < 14)
            return -1;

        strncpy(g_specify_port, ptr+10, 3);
        dprintf("g_specify_port:%s\n",g_specify_port);
    }
    else if (ptr[13] == '.')
    {
        if (ptr_len < 22)
            return -1;

        if (ptr[21] == ':')
        {
            strncpy(g_specify_port, ptr+16, 5);
            dprintf("g_specify_port:%s\n",g_specify_port);
        }
        else if (ptr[21] == '.')
        {
            if (ptr_len < 32)
                return -1;

            if (ptr[31] == ':')
            {
                strncpy(g_specify_port, ptr+24, 7);
                dprintf("g_specify_port:%s\n",g_specify_port);
            }
            else
            {
                if (ptr_len < 44)
                    return -1;

                strncpy(g_specify_port, ptr+34, 9);
                dprintf("g_specify_port:%s\n",g_specify_port);
            }
        }
    }

    return 0;
}

static int is_EG912YEMAB_upgrade(int usbfd, FILE *fw_fp)
{
    int ret = -1;
    int is_R01 = 0;
    char at_rsp[256] = {0};
    ret = sendSync(usbfd, "ATI\r", 4, 0);
    if (ret < 1) return -1;

    do  {
        ret = recvSync(usbfd, at_rsp, sizeof(at_rsp), 1000);
        if (ret > 0) {
            int i, len = 0;

            for (i = 0; i < ret; i++) {
                uint8_t ch = at_rsp[i];

                if (ch != '\r' && ch != '\n')
                    at_rsp[len++] = ch;
                else if (len > 1) {
                    at_rsp[len++] = 0;
                    dprintf("AT < %s\n", at_rsp);
                    if (len >= 20 && !strncmp(at_rsp, "Revision: EG912YEMAB", 20))
                    {
                        if (strstr(at_rsp, "R01"))
                            is_R01 = 1;
                        else if (strstr(at_rsp, "R03"))
                            is_R03 = 1;
                        break;
                    }
                    len = 0;
                }
            }
        }
    } while (ret >= 0);

    if (is_R01 == 0 && is_R03 == 0) //Non eg912y series, exit directly
        return 0;

    fseek(fw_fp, 0, SEEK_END);
    long firmware_size = ftell(fw_fp);
    fseek(fw_fp, firmware_size - sizeof(quec_updater_head_s), SEEK_SET);

    quec_updater_head_s  head_update;
    memset(&head_update, 0, sizeof(quec_updater_head_s));
    ret = fread(&head_update, 1, sizeof(quec_updater_head_s), fw_fp);
    if (ret <= 0)
        return -2;

    if (strstr(head_update.flagStr, "EG912YEMAB"))
    {
       if (is_R03 == 0)
       {
           printf("Incorrect version package, exit upgrade\n");
           return -3;
       }
    }
    else
    {
       if (is_R01 == 0)
        {
           printf("Incorrect version package, exit upgrade\n");
           return -4;
        }
    }

    return 1;
}

static const char *ctimespec(const struct timespec *ts, char *time_name, size_t len) {
    time_t ltime = ts->tv_sec;
    struct tm *currtime = localtime(&ltime);

    snprintf(time_name, len, "%04d%02d%02d_%02d:%02d:%02d",
    	(currtime->tm_year+1900), (currtime->tm_mon+1), currtime->tm_mday,
    	currtime->tm_hour, currtime->tm_min, currtime->tm_sec);
    return time_name;
}

int qusb_read_speed_atime(char *module_sys_path, struct timespec *out_time, int *out_speed)
{
    char speed[256];
    int fd;
    struct stat stat;

    snprintf(speed, sizeof(speed), "%s/speed", module_sys_path);
    fd = open(speed, O_RDONLY);
    if (fd == -1)
        return 0;

    if (read(fd, speed, sizeof(speed))) {}
    fstat(fd, &stat);
    close(fd);

    *out_speed = atoi(speed);

    #ifdef ANDROID
    struct timespec out_time_tmp;
    memset(&out_time_tmp, 0, sizeof(struct timespec));
    out_time_tmp.tv_sec = stat.st_atime;
    *out_time = out_time_tmp;
    #else
    *out_time = stat.st_atim;
    #endif

    dprintf("speed: %d, st_atime: %s\n", *out_speed, ctimespec(out_time, speed, sizeof(speed)));
    return 1;
}

int main(int argc, char *argv[])
{
    int i, ret = -1, opt, count, usbfd;
    const USBFS_T *udev;
    const quectel_modem_info *mdev;
    const char *fw_file = NULL;
    FILE *fw_fp = NULL;
    const char *usbmon_logfile = NULL;
    char *backup_file_path = NULL;
    int usb3_speed = 0;
    struct timespec usb3_atime;
    struct timespec usb3_process_start_time;

#ifdef EC200T_SHINCO
    log_fp = fopen("/data/update/meig_linux_update_for_arm/update_log.txt", "wb");
    dprintf("EC200T_SHINCO 2021/4/29\n");
#endif
    dprintf("Version: QDloader_Linux_Android_V1.0.11\n");

    optind = 1; //call by popen(), optind mayby is not 1
    while (-1 != (opt = getopt(argc, argv, "s:f:u:g:x:ebvh")))
    {
        switch (opt)
        {
        case 's':
            snprintf(g_specify_port, sizeof(g_specify_port), "%s", optarg);
            dprintf("optarg:%s\n", optarg);
            if (g_specify_port[0] != '\0')
            {
                if (strncasecmp(g_specify_port, "/dev", 4) == 0)    //Compatible with PCIE module
                {
                    if (get_port_from_cmd(g_specify_port) < 0)
                        memset(g_specify_port, 0, sizeof(g_specify_port));
                    else
                    {
                        char g_specify_port_tmp[64] = {0};
                        snprintf(g_specify_port_tmp, sizeof(g_specify_port_tmp), "/sys/bus/usb/devices/%.40s", g_specify_port);
                        memset(g_specify_port, 0, 64);
                        memmove(g_specify_port, g_specify_port_tmp, 64);
                    }
                }
                else if (strncasecmp(g_specify_port, "/sys/bus/usb/devices/", 20) == 0)
                {
                    ;
                }
                else
                {
                    dprintf("-s: wrong parameter specified\n");
                    memset(g_specify_port, 0, sizeof(g_specify_port));
                    exit(-1);
                }
            }
            dprintf("g_specify_port:%s\n", g_specify_port);
        break;
        case 'f':
            fw_file = strdup(optarg);
        break;
        case 'u':
            usbmon_logfile = strdup(optarg);
            dprintf("usbmon_logfile:%s\n", usbmon_logfile);
        break;
        case 'x':
            backup_file_path = strdup(optarg);
        break;
        case 'g':
            snprintf(modem_name_para, sizeof(modem_name_para), "%s", optarg);
            dprintf("modem_name_para:%s\n", modem_name_para);
        break;
        case 'e':
            q_erase_all = 1;
            dprintf("q_erase_all = %d\n", q_erase_all);
        break;
        case 'b':
            fw_file_skip = 1;
            dprintf("fw_file_skip = %d\n", fw_file_skip);
        break;
        case 'v':
            verbose++;
            dprintf("v=%d\n", verbose);
        break;
        default:
            return usage(argv[0]);
        break;
         }
    }

    mcu_is_little_endian = check_mcu_endian();
    if (mcu_is_little_endian == 1)
    {   dprintf("mcu is little endian\n");}
    else
    {   dprintf("mcu is big endian\n");}

    if (usbmon_logfile)
        ql_capture_usbmon_log(usbmon_logfile);

_repair_modem:
    if (!fw_file && !fw_file_skip) {
        return usage(argv[0]);
    }

    if (!fw_file_skip)
    {
        fw_fp = fopen(fw_file, "rb");
        if (!fw_fp) {
            dprintf("fopen(%s) fail, errno: %d (%s)\n", fw_file, errno, strerror(errno));
            return -1;
        }
    }

    if (backup_file_path)
    {
        if (access(backup_file_path, F_OK) && errno == ENOENT)
            mkdir(backup_file_path, 0755);

        memmove(file_path, backup_file_path, strlen(backup_file_path));
        if (file_path[strlen(file_path) -1] == '/')
            file_path[strlen(file_path) -1] = '\0';

        printf("%s file_path:%s\n", __func__, file_path);
    }
    else
    {
        file_path[0] = '\0';
    }

    if (g_specify_port[0] != '\0')
    {
        if (qusb_read_speed_atime(g_specify_port, &usb3_atime, &usb3_speed) ==0)
        {
            dprintf("no %s exist!\n", g_specify_port);
            return -1;
        }

        clock_gettime(CLOCK_REALTIME, &usb3_process_start_time);
    }

_scan_modem:
    dprintf("scan quectel modem\n");
    for (i = 0; i < 50; i++) {
        count = usbfs_find_quectel_modules(usbfs_dev, 8, usb3_speed, usb3_process_start_time);
        if (count > 0) break;
        usleep(200*1000);
    }
    if (count == 0) {
        dprintf("not quectel modem find!\n");
        return -1;
    }
    udev = &usbfs_dev[0];

    mdev = find_quectel_modem(udev->PRODUCT);
    if (!mdev)
    {    goto _scan_modem;}

    usb_interface = -1;

    if (!fw_file_skip)
    {
        usbfd = usbfs_open(udev, mdev->usb_interface);
        if (usbfd == -1)
            goto _scan_modem; //return -1;

        if (!strncmp(udev->PRODUCT, "2c7c/6001/", 10) || !strncmp(udev->PRODUCT, "2c7c/6002/", 10))
        {
            ret = is_EG912YEMAB_upgrade(usbfd, fw_fp);
            if (ret <= -1)
                goto _scan_modem; //return -1;
        }

        if (mdev->download_atc) {
            switch_to_download(usbfd, mdev->download_atc, udev->dev_name);
            usbfs_close(usbfd, mdev->usb_interface);
            goto _scan_modem;
        }

        update_transfer_bytes(0);

        if (!strcmp(mdev->modem_name, "RG500U") || !strcmp(mdev->modem_name, "RM500U")) {
            ret = dloader_main(usbfd, fw_fp, mdev->modem_name);
        }
        else if (!strcmp(mdev->modem_name, "EC200U") || !strcmp(mdev->modem_name, "EC800G")  || !strcmp(mdev->modem_name, "EC200D")) {
            ret = dloader_main(usbfd, fw_fp, mdev->modem_name);
        }
        else if (!strcmp(mdev->modem_name, "EC200T") || !strcmp(mdev->modem_name, "EC200A") || !strcmp(mdev->modem_name, "EG060V")) {
            ret = fbfdownloader_main(usbfd, fw_fp, mdev->modem_name);
        }
        else if (!strcmp(mdev->modem_name, "EC200S")) {
            ret = aboot_main(usbfd, fw_fp, mdev->modem_name);
        }

        fclose(fw_fp);
        usbfs_close(usbfd, mdev->usb_interface);
    }

    if (!strcmp(mdev->modem_name, "RM500K") || !strcmp(mdev->modem_name, "RG500L") || !strcmp(mdev->modem_name, "RG620T")) {
        ret = system("adb shell /sbin/sysupgrade /tmp/sysupgrade.bin");
    }

    if (ret == 0 || ret == 10) {
        for (i = 0; i < 100; i++) {
            if (access(udev->dev_name, F_OK)) break; //wait reboot
            usleep(10*1000);
        }
    }

    if (ret == 10)
        goto _repair_modem;

    if (ret == 0)
        update_transfer_bytes(0xffffffff);
    update_transfer_result(ret == 0);
    dprintf("Upgrade module %s\n", (ret == 0) ? "successfully" : "fail");
    if (log_fp && log_fp != stdout)
        fclose(log_fp);

    if (usbmon_logfile)
        ql_stop_usbmon_log();

    return ret;
}
