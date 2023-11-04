/******************************************************************************
  @file    usbfs.c
  @brief   usbfs.

  DESCRIPTION
  QDloader Tool for USB and PCIE of Quectel wireless cellular modules.

  INITIALIZATION AND SEQUENCING REQUIREMENTS
  None.

  ---------------------------------------------------------------------------
  Copyright (c) 2016 - 2021 Quectel Wireless Solution, Co., Ltd.  All Rights Reserved.
  Quectel Wireless Solution Proprietary and Confidential.
  ---------------------------------------------------------------------------
******************************************************************************/
#include "../std_c.h"
#include <dirent.h>
#include <sys/ioctl.h>
#include <poll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <linux/usbdevice_fs.h>
#include <linux/version.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>
#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 20)
#include <linux/usb/ch9.h>
#else
#include <linux/usb_ch9.h>
#endif
#include "usbfs.h"

#define MKDEV(__ma, __mi) (((__ma & 0xfff) << 8) | (__mi & 0xff) | ((__mi & 0xfff00) << 12))

extern int verbose;
extern char g_specify_port[64];
extern char modem_name_para[32];
static int endpoint_in = 1;
static int endpoint_out = 1;
static int endpoint_len = 512;
int usb_interface = 0;

#ifdef EC200T_SHINCO
extern uint32_t inet_addr(const char *);
static int tcp_sockfd = -1;
static int connect_tcp_server(const char *tcp_host, int tcp_port)
{
    int sockfd = -1;
    struct sockaddr_in sockaddr;

    dprintf("%s( tcp_host = %s, tcp_port = %d )\n", __func__, tcp_host, tcp_port);

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1)
        return sockfd;

    memset(&sockaddr, 0, sizeof(sockaddr));
    sockaddr.sin_family = AF_INET;
    sockaddr.sin_addr.s_addr = inet_addr(tcp_host);
    sockaddr.sin_port = htons(tcp_port);

    if(connect(sockfd, (struct sockaddr *) &sockaddr, sizeof(sockaddr)) < 0) {
        close(sockfd);
        dprintf("%s connect %s:%d errno: %d (%s)\n", __func__, tcp_host, tcp_port, errno, strerror(errno));
        return -1;
    }

    dprintf("tcp client: %s %d sockfd = %d\n", tcp_host, tcp_port, sockfd);

    return sockfd;
}
#endif

void sciu2sMessage(int usbfd)
{
    struct usbdevfs_ctrltransfer control;

    // control message that sciu2s.ko will send on tty is open
    control.bRequestType = 0x21;
    control.bRequest = 0x22;
    control.wValue = endpoint_out << 8 | 1;
    control.wIndex = 0;
    control.wLength = 0;
    control.timeout = 0; /* in milliseconds */
    control.data = NULL;

    ioctl(usbfd, USBDEVFS_CONTROL, &control);
}

void sciu2sMessage_unisoc8850(int usbfd)
{
    struct usbdevfs_ctrltransfer control;

    // control message that sciu2s.ko will send on tty is open
    control.bRequestType = 0x21;
    control.bRequest = 0x22;
    control.wValue = endpoint_out << 8 | 1;
    control.wIndex = 0x0002;
    control.wLength = 0;
    control.timeout = 0; /* in milliseconds */
    control.data = NULL;

    ioctl(usbfd, USBDEVFS_CONTROL, &control);
}

void setInterface(int usbfd)
{
    struct usbdevfs_ctrltransfer control;

    control.bRequestType = 0;
    control.bRequest = 9;
    control.wValue = 1;
    control.wIndex = 0;
    control.wLength = 0;
    control.timeout = 0; /* in milliseconds */
    control.data = NULL;

    ioctl(usbfd, USBDEVFS_CONTROL, &control);

    control.bRequestType = 0;
    control.bRequest = 11;
    control.wValue = 0;
    control.wIndex = 1;
    control.wLength = 0;
    control.timeout = 0; /* in milliseconds */
    control.data = NULL;

    ioctl(usbfd, USBDEVFS_CONTROL, &control);
}

void set_line_state(int usbfd)
{
    struct usbdevfs_ctrltransfer control;

    control.bRequestType = 0x21;
    control.bRequest = 0x22;
    control.wValue = 0x3; //DTR&RS
    control.wIndex = 0; //interface
    control.wLength = 0;
    control.timeout = 0; /* in milliseconds */
    control.data = NULL;

    ioctl(usbfd, USBDEVFS_CONTROL, &control);
}

void set_line_state_uis8310(int usbfd, int interface_no)
{
    struct usbdevfs_ctrltransfer control;

    control.bRequestType = 0x21;
    control.bRequest = 0x22;
    control.wValue = 0x3; //DTR&RS
    control.wIndex = interface_no; //interface
    control.wLength = 0;
    control.timeout = 0; /* in milliseconds */
    control.data = NULL;

    ioctl(usbfd, USBDEVFS_CONTROL, &control);
}

void dump_data(const unsigned char *data, size_t len, char c)
{
#if 0
	unsigned i;

	for (i = 0; i < len; i++) {
            if (i > 128 && (i+32) < len)
		continue;
	    if (i%16 == 0) {
	      printf("\n%c%04x  ", c, i);
	    }
	    printf(" %02x", data[i]);
	}
#endif
}

#define MAX_USBFS_BULK_SIZE (16 * 1024)
int sendSync(int usbfd, const void *data, size_t len, int need_zlp)
{
	int ret;
        struct usbdevfs_bulktransfer bulk;
        int last_len = 0;
        const void *last_data = 0;

#ifdef EC200T_SHINCO
    if (tcp_sockfd == usbfd) {
        struct pollfd pollfd = {usbfd, POLLOUT, 0};
        int ret = poll(&pollfd, 1, -1);
        return (ret == 1) ? write(usbfd, data, len) : ret;
    }
#endif

	if (need_zlp && ((len%endpoint_len) == 0)) {
           last_len = 1;
           len -= last_len;
           last_data = data + len;
           printf("zlp\n");
        }

	//len = (len > MAX_USBFS_BULK_SIZE) ? MAX_USBFS_BULK_SIZE : len;
        bulk.ep = endpoint_out;
        bulk.len = len;
        bulk.data = (void *)data;
        bulk.timeout = 3000;

        dump_data(data, len, '>');
        ret = ioctl(usbfd, USBDEVFS_BULK, &bulk);
        if (ret != len) printf("bluk out = %d, errno: %d (%s)\n", ret, errno, strerror(errno));

        if (ret == len && last_len) {
            bulk.len = last_len;
            bulk.data = (void *)last_data;
            ioctl(usbfd, USBDEVFS_BULK, &bulk);
        }


    return ret;
}

int recvSync(int usbfd, void *data, size_t len, unsigned timeout)
{
    struct usbdevfs_bulktransfer bulk;
    int ret;

#ifdef EC200T_SHINCO
    if (tcp_sockfd == usbfd) {
        struct pollfd pollfd = {usbfd, POLLIN, 0};
        int ret = poll(&pollfd, 1, timeout ? timeout : 1);
        return (ret == 1) ? read(usbfd, data, len) : ret;
    }
#endif

    len = (len > MAX_USBFS_BULK_SIZE) ? MAX_USBFS_BULK_SIZE : len;
    bulk.ep = 0x80 | endpoint_in;
    bulk.len = len;
    bulk.data = data;
    bulk.timeout = timeout;

    ret = ioctl(usbfd, USBDEVFS_BULK, &bulk);
    if (ret == -1 && errno != ETIMEDOUT)
        printf("bluk in = %d, errno: %d (%s)\n", ret, errno, strerror(errno));

    if (ret > 0)
        dump_data(data, ret, '<');
    return ret;
}

static int usbfs_decode(int usbfd, int interface_no) {
    uint8_t *desc_p;
    int desc_len, len = 0;
    int bInterfaceNumber = -1;

    endpoint_in = -1;
    endpoint_out = -1;

    desc_p = (uint8_t *)malloc(2048);
    if (!desc_p)
        return -1;

    desc_len = read(usbfd, desc_p, 2048);

    while (len < desc_len) {
        struct usb_descriptor_header *h = (struct usb_descriptor_header *)(desc_p + len);

        if (h->bLength == sizeof(struct usb_device_descriptor) && h->bDescriptorType == USB_DT_DEVICE) {
            struct usb_device_descriptor *device = (struct usb_device_descriptor *)h;

            if (verbose) printf("P: idVendor=%04x idProduct=%04x\n", device->idVendor, device->idProduct);
        }
        else if (h->bLength == sizeof(struct usb_config_descriptor) && h->bDescriptorType == USB_DT_CONFIG) {
            struct usb_config_descriptor *config = (struct usb_config_descriptor *)h;

            if (verbose) printf("C: bNumInterfaces: %d\n", config->bNumInterfaces);
        }
        else if (h->bLength == sizeof(struct usb_interface_descriptor) && h->bDescriptorType == USB_DT_INTERFACE) {
            struct usb_interface_descriptor *interface = (struct usb_interface_descriptor *)h;

            if (verbose) printf("I: If#= %d Alt= %d #EPs= %d Cls=%02x Sub=%02x Prot=%02x\n",
                interface->bInterfaceNumber, interface->bAlternateSetting, interface->bNumEndpoints,
                interface->bInterfaceClass, interface->bInterfaceSubClass, interface->bInterfaceProtocol);
            bInterfaceNumber = interface->bInterfaceNumber;
        }
        else if (h->bLength == USB_DT_ENDPOINT_SIZE && h->bDescriptorType == USB_DT_ENDPOINT) {
                struct usb_endpoint_descriptor *endpoint = (struct usb_endpoint_descriptor *)h;

                if (verbose) printf("E: Ad=%02x Atr=%02x MxPS= %d Ivl=%dms\n",
                    endpoint->bEndpointAddress, endpoint->bmAttributes, endpoint->wMaxPacketSize, endpoint->bInterval);

                if (bInterfaceNumber == interface_no) {
                    if ((endpoint->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK) == USB_ENDPOINT_XFER_BULK) {
                        if (endpoint->bEndpointAddress & USB_ENDPOINT_DIR_MASK) {
                            endpoint_in = endpoint->bEndpointAddress;
			    if (verbose) printf("endpoint_in:  %x\n", endpoint_in);
                        }
                        else {
                            endpoint_out = endpoint->bEndpointAddress;
                            endpoint_len = le16toh(endpoint->wMaxPacketSize);
			    if (verbose) printf("endpoint_out: %x\n", endpoint_out);
			    if(verbose) printf("endpoint_len: %d\n", endpoint_len);
                         }
                }
                else if ((endpoint->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK) == USB_ENDPOINT_XFER_INT){
                }
            }
        }
        else {
        }

        len += h->bLength;

        if (endpoint_in != -1 && endpoint_out != -1)
        {
            if (desc_p)
                free(desc_p);

            return 0;
        }
    }

    if (desc_p)
        free(desc_p);

    return -1;
}

int usbfs_open(const USBFS_T *usbfs_dev, int interface_no) {
    int ret;
    int usbfd;
    struct usbdevfs_getdriver usbdrv;

    usbfd = open(usbfs_dev->dev_name, O_RDWR | O_NOCTTY);
    printf("open(%s) = %d\n", usbfs_dev->dev_name, usbfd);
    if (usbfd == -1) {
        printf("usbfd = %d, errno: %d (%s)\n", usbfd, errno, strerror(errno));
        return -1;
    }

    if (usbfs_decode(usbfd ,interface_no) == -1) {
        printf("fail to find usb interface %d\n", interface_no);
        close(usbfd);
#ifdef EC200T_SHINCO
        if (strStartsWith(usbfs_dev->PRODUCT, "2c7c/6026/")) { //EC200T
            tcp_sockfd = connect_tcp_server("192.168.225.1", 10080);
            if (tcp_sockfd != -1)
                 fcntl(tcp_sockfd, F_SETFL, fcntl(tcp_sockfd,F_GETFL) | O_NONBLOCK);
            return tcp_sockfd;
        }
#endif
        return -1;
    }

    usbdrv.interface = interface_no;
    ret = ioctl(usbfd, USBDEVFS_GETDRIVER, &usbdrv);
    if (ret && errno != ENODATA) {
        printf("USBDEVFS_GETDRIVER ret=%d errno: %d (%s)\n", ret, errno, strerror(errno));
        close(usbfd);
        return -1;
    }
    else if (ret == 0) {
        struct usbdevfs_ioctl operate;

        operate.data = NULL;
        operate.ifno = interface_no;
        operate.ioctl_code = USBDEVFS_DISCONNECT;

        ret = ioctl(usbfd, USBDEVFS_IOCTL, &operate);
        if (ret) {
            printf("USBDEVFS_DISCONNECT ret=%d errno: %d (%s)\n", ret, errno, strerror(errno));
            close(usbfd);
            return -1;
        }
    }

    ret =ioctl(usbfd, USBDEVFS_CLAIMINTERFACE, &interface_no);
    if (ret) {
        printf("USBDEVFS_CLAIMINTERFACE ret=%d, errno: %d (%s)\n", ret, errno, strerror(errno));
        close(usbfd);
        return -1;
    }

    if (strStartsWith(usbfs_dev->PRODUCT, "1782/4d00/")) { //RG500U
        sciu2sMessage(usbfd);
    }
    else if (strStartsWith(usbfs_dev->PRODUCT, "2c7c/904/"))
    {
        sciu2sMessage_unisoc8850(usbfd);
    }
    else if (strStartsWith(usbfs_dev->PRODUCT, "2ecc/3017/") //EC200S
              || strStartsWith(usbfs_dev->PRODUCT, "2ecc/3004/"))
    {
        set_line_state(usbfd);
    }
    else if (strStartsWith(usbfs_dev->PRODUCT, "2c7c/902/"))
    {
        set_line_state_uis8310(usbfd, interface_no);
    }


    return usbfd;
}

int usbfs_close(int usbfd, int interface_no) {
#ifdef EC200T_SHINCO
    if (tcp_sockfd == usbfd)
        tcp_sockfd = -1;
#endif
    close(usbfd);
    return 0;
}

#define MAX_PATH 256
typedef struct {
/*
MAJOR=189
MINOR=1
DEVNAME=bus/usb/001/002
DEVTYPE=usb_device
DRIVER=usb
PRODUCT=2c7c/415/318
TYPE=239/2/1
BUSNUM=001
*/
    char DEVNAME[32];
    char DEVTYPE[32];
    char DRIVER[32];
    int MAJOR;
    int MINOR;
    char PRODUCT[32];
} UEVENT_T;

extern int qusb_read_speed_atime(char *module_sys_path, struct timespec *out_time, int *out_speed);
extern int usbfs_find_quectel_modules(USBFS_T *usbfs_dev, size_t max, int usb3_speed, struct timespec usb3_process_start_time);
extern int sendSync(int usbfd, const void *pBuf, size_t len, int need_zlp);
extern int recvSync(int usbfd, void *data, size_t len, unsigned timeout);
static int quectel_get_sysinfo_by_uevent(const char *uevent, UEVENT_T *pSysInfo) {
    FILE *fp;
    char line[MAX_PATH];

    memset(pSysInfo, 0x00, sizeof(UEVENT_T));

    fp = fopen(uevent, "r");
    if (fp == NULL) {
        printf("fail to fopen %s, errno: %d (%s)\n", uevent, errno, strerror(errno));
        return 0;
    }

    //dbg_time("%s\n", uevent);
    while (fgets(line, sizeof(line), fp)) {
        if (line[strlen(line) - 1] == '\n' || line[strlen(line) - 1] == '\r') {
            line[strlen(line) - 1] = '\0';
        }

        //dbg_time("%s\n", line);
        if (strStartsWith(line, "MAJOR=")) {
            	pSysInfo->MAJOR = atoi(&line[strlen("MAJOR=")]);
        }
        else if (strStartsWith(line, "MINOR=")) {
            	pSysInfo->MINOR = atoi(&line[strlen("MINOR=")]);
        }
        else if (strStartsWith(line, "DEVNAME=")) {
            if(pSysInfo)
            	strncpy(pSysInfo->DEVNAME, &line[strlen("DEVNAME=")], sizeof(pSysInfo->DEVNAME));
        }
        else if (strStartsWith(line, "DEVTYPE=")) {
            strncpy(pSysInfo->DEVTYPE, &line[strlen("DEVTYPE=")], sizeof(pSysInfo->DEVTYPE));
        }
        else if (strStartsWith(line, "PRODUCT=")) {
            strncpy(pSysInfo->PRODUCT, &line[strlen("PRODUCT=")], sizeof(pSysInfo->PRODUCT));
        }
    }

    fclose(fp);

    return 1;
}

static char* itoa(int num,char* str,int radix)
{
    char index[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    unsigned unum;
    int i=0,j,k;

    if (radix == 10 && num < 0)
    {
        unum = (unsigned) - num;
        str[i++] = '-';
    }
    else
        unum = (unsigned)num;

    do
    {
        str[i++] = index[unum%(unsigned)radix];
        unum /= radix;
    }while(unum);

    str[i] = '\0';

    if (str[0] == '-')
        k=1;
    else
        k=0;

    char temp;
    for(j=k;j<=(i-1)/2;j++)
    {
        temp = str[j];
        str[j] = str[i-1+k-j];
        str[i-1+k-j] = temp;
    }

    return str;
}

static int get_dev_path_branch(char* module_dev_path, int MAJOR, int MINOR)
{
    if (module_dev_path[0] == '\0' || strncasecmp(module_dev_path, "/dev", 4) != 0)
        return -1;
    FILE* fp = NULL;
    char cmd_buf[320] = {0};
    char recv_buf[256] = {0};
    char buf_tmp[256] = {0};
    char itoa_major_buf[32] = {0};
    char itoa_minor_buf[32] = {0};
    snprintf(cmd_buf, sizeof(cmd_buf), "ls -l %s", module_dev_path);
    fp = popen(cmd_buf, "r");
    if (fp == NULL)
        return -1;
    int recv_length = fread(recv_buf, 1, sizeof(recv_buf), fp);
    if (recv_length <= 0)
    {    return -1;}

    if (MINOR < 10)     //189,   3
        snprintf(buf_tmp, sizeof(buf_tmp), "%s,   %s", itoa(MAJOR, itoa_major_buf, 10), itoa(MINOR, itoa_minor_buf, 10));
    else if (MINOR < 100 && MINOR >= 10)  // 189,  25
        snprintf(buf_tmp, sizeof(buf_tmp), "%s,  %s", itoa(MAJOR, itoa_major_buf, 10), itoa(MINOR, itoa_minor_buf, 10));
    else if (MINOR >= 100)  //189, 257
        snprintf(buf_tmp, sizeof(buf_tmp), "%s, %s", itoa(MAJOR, itoa_major_buf, 10), itoa(MINOR, itoa_minor_buf, 10));

    if (strstr(recv_buf, buf_tmp))
        return 1;

    return 0;
}

static int quectel_get_dev_path(char *module_dev_path, int MAJOR, int MINOR) {
    char infname[256];
    DIR *infdir = NULL;
    struct dirent *de = NULL;

    sprintf(infname, "/dev");
    infdir = opendir(infname);
    if (infdir == NULL)
        return -1;

    while ((de = readdir(infdir))) {
        if (strStartsWith(de->d_name, "usbdev")) {
            sprintf(module_dev_path, "/dev/%s",de->d_name);
            int ret = get_dev_path_branch(module_dev_path, MAJOR, MINOR);
            if (ret < 0)
                return -1;
            else if (ret == 0)
                continue;

            return 1;
        }
    }

    if (infdir) closedir(infdir);
    return 0;
}

// the return value is the number of quectel modules
int usbfs_find_quectel_modules(USBFS_T *usbfs_dev, size_t max, int usb3_speed, struct timespec usb3_process_start_time)
{
    const char *base = "/sys/bus/usb/devices";
    DIR *busdir = NULL;
    struct dirent *de = NULL;
    int count = 0;
    char module_dev_path[300] = {0};
    char PRODUCT[32];
    char dev_name_mknod[32] = {0};
    int dev_name_id = 1;
    static struct timespec this_atime[8];
    static USBFS_T usbfs_dev_tmp[8];
    int modem_require_count = 0;

    busdir = opendir(base);
    if (busdir == NULL)
        return -1;

    while ((de = readdir(busdir))) {
        static char uevent[MAX_PATH+9];
        UEVENT_T sysinfo;

        if (!isdigit(de->d_name[0])) continue;

        snprintf(uevent, sizeof(uevent), "%s/%.16s/uevent", base, de->d_name);
        if (!quectel_get_sysinfo_by_uevent(uevent, &sysinfo))
            continue;

        if (!sysinfo.MAJOR || !sysinfo.PRODUCT[0]|| !sysinfo.DEVTYPE[0])
            continue;

        if (sysinfo.MAJOR != 189 || strcmp(sysinfo.DEVTYPE, "usb_device"))
            continue;

        strcpy(PRODUCT, sysinfo.PRODUCT);
        if (strstr(sysinfo.PRODUCT,"3c93") && modem_name_para[0])
        {
            if (!strncasecmp(modem_name_para, "RM500U", 6))
            {
                usb_interface = 3;
                memset(sysinfo.PRODUCT, 0 , sizeof(sysinfo.PRODUCT));
                snprintf(sysinfo.PRODUCT, sizeof(sysinfo.PRODUCT), "%s", "2c7c/900/");
            }
            else if (!strncasecmp(modem_name_para, "EC200D", 6))
            {
                if ((strstr(PRODUCT,"3c93") && strstr(PRODUCT,"ffff")))
                {
                    usb_interface = 3;
                }
                else if ((strstr(PRODUCT,"3763") && strstr(PRODUCT,"3c93")))
                {
                    usb_interface = 1;
                }
                memset(sysinfo.PRODUCT, 0 , sizeof(sysinfo.PRODUCT));
                snprintf(sysinfo.PRODUCT, sizeof(sysinfo.PRODUCT), "%s", "2c7c/902/");
            }
            else if (!strncasecmp(modem_name_para, "RG200U", 6))
            {
                if ((strstr(PRODUCT,"3c93") && strstr(PRODUCT,"ffff")))
                {
                    usb_interface = 3;
                }
                else if ((strstr(PRODUCT,"3763") && strstr(PRODUCT,"3c93")))
                {
                    usb_interface = 2;
                }
                memset(sysinfo.PRODUCT, 0 , sizeof(sysinfo.PRODUCT));
                snprintf(sysinfo.PRODUCT, sizeof(sysinfo.PRODUCT), "%s", "2c7c/900/");
            }
            else if (!strncasecmp(modem_name_para, "EC200T", 6))
            {
                if ((strstr(PRODUCT,"3c93") && strstr(PRODUCT,"ffff")))
                {
                    usb_interface = 3;
                }
                else if ((strstr(PRODUCT,"3763") && strstr(PRODUCT,"3c93")))
                {
                    usb_interface = 1;
                }
                memset(sysinfo.PRODUCT, 0 , sizeof(sysinfo.PRODUCT));
                snprintf(sysinfo.PRODUCT, sizeof(sysinfo.PRODUCT), "%s", "2c7c/6026/");
            }
            else if (!strncasecmp(modem_name_para, "EC200A", 6))
            {
                if ((strstr(PRODUCT,"3c93") && strstr(PRODUCT,"ffff")))
                {
                    usb_interface = 3;
                }
                else if ((strstr(PRODUCT,"3763") && strstr(PRODUCT,"3c93")))
                {
                    usb_interface = 1;
                }
                memset(sysinfo.PRODUCT, 0 , sizeof(sysinfo.PRODUCT));
                snprintf(sysinfo.PRODUCT, sizeof(sysinfo.PRODUCT), "%s", "2c7c/6005/");
            }
            else if (!strncasecmp(modem_name_para, "EC200N", 6))        //GW EC200N  not used
            {
                if ((strstr(PRODUCT,"3c93") && strstr(PRODUCT,"ffff")))
                {
                    usb_interface = 3;
                }
                else if ((strstr(PRODUCT,"3763") && strstr(PRODUCT,"3c93")))
                {
                    usb_interface = 1;
                }
                memset(sysinfo.PRODUCT, 0 , sizeof(sysinfo.PRODUCT));
                snprintf(sysinfo.PRODUCT, sizeof(sysinfo.PRODUCT), "%s", "2c7c/6002/");
            }
            else if (!strncasecmp(modem_name_para, "EC200S", 6))        //GW EC200S  not used
            {
                if ((strstr(PRODUCT,"3c93") && strstr(PRODUCT,"ffff")))
                {
                    usb_interface = 3;
                }
                else if ((strstr(PRODUCT,"3763") && strstr(PRODUCT,"3c93")))
                {
                    usb_interface = 1;
                }
                memset(sysinfo.PRODUCT, 0 , sizeof(sysinfo.PRODUCT));
                snprintf(sysinfo.PRODUCT, sizeof(sysinfo.PRODUCT), "%s", "2c7c/6002/");
            }
        }
        else if (strstr(sysinfo.PRODUCT,"3c93") && !modem_name_para[0])
        {
            printf("The state grid module needs to specify the module, like -g EC200T\n");
            return -1;
        }

        if (find_quectel_modem(sysinfo.PRODUCT))
        {
            memset(&usbfs_dev[count], 0, sizeof(USBFS_T));
            snprintf(usbfs_dev[count].sys_path, sizeof(usbfs_dev[count].sys_path),
                        "%s/%.16s", base, de->d_name);
            if ('\0' != sysinfo.DEVNAME[0])
                snprintf(usbfs_dev[count].dev_name, sizeof(usbfs_dev[count].dev_name),
                            "/dev/%.16s", sysinfo.DEVNAME);

            if (access(usbfs_dev[count].dev_name, R_OK))
            {
                int ret = quectel_get_dev_path(module_dev_path, sysinfo.MAJOR, sysinfo.MINOR);
                if (ret == 1)
                {
                    memset(usbfs_dev[count].dev_name, 0, sizeof(usbfs_dev[count].dev_name));
                    strncpy(usbfs_dev[count].dev_name, module_dev_path, 32);
                }else if (ret == 0)
                {
                    snprintf(dev_name_mknod, sizeof(dev_name_mknod), "/dev/usbdev-quectel-%d", dev_name_id);
                    if (g_specify_port[0] != '\0')
                    {
                        if(!access(dev_name_mknod, F_OK))
                        {
                            snprintf(dev_name_mknod, sizeof(dev_name_mknod), "/dev/usbdev-quectel-%d", ++dev_name_id);
                        }
                    }

                    unlink(dev_name_mknod);
                    usleep(500000);
                    if (mknod(dev_name_mknod, S_IFCHR|0666, MKDEV(sysinfo.MAJOR, sysinfo.MINOR)))
                        return -1;
                    else
                        continue;
                }else
                    return -1;
            }
            strncpy(usbfs_dev[count].PRODUCT, sysinfo.PRODUCT, 32);
            printf("%s %s\n", usbfs_dev[count].sys_path, PRODUCT);

            if (usb3_speed >= 5000 && access(g_specify_port, R_OK))
            {
                if (strstr(sysinfo.PRODUCT,"1782") || strstr(sysinfo.PRODUCT,"525")
                    || strstr(sysinfo.PRODUCT,"1286") || strstr(sysinfo.PRODUCT,"2ecc"))
                {
                    int speed;

                    snprintf(uevent, sizeof(uevent), "%.24s/%.16s", base, de->d_name);
                    if (qusb_read_speed_atime(uevent, &this_atime[modem_require_count], &speed))
                    {
                        memmove(&usbfs_dev_tmp[modem_require_count], &usbfs_dev[count], sizeof(USBFS_T));
                        modem_require_count++;
                    }
                    else
                        continue;
                }
                else
                {
                    printf("skip %.24s/%s for PRODUCT %s\n", base, de->d_name, sysinfo.PRODUCT);
                    continue;
                }
            }

            count++;

            if (g_specify_port[0] != '\0')
            {
                if (usb3_speed >= 5000 && access(g_specify_port, R_OK))
                {

                }
                else
                {
                    if (strstr(usbfs_dev[count-1].sys_path, g_specify_port))
                        break;
                    else
                    {
                        count--;
                        memset(&usbfs_dev[count], 0, sizeof(USBFS_T));
                    }
                }
            }

            if (count == max)
               break;
        }
        else if (strStartsWith(sysinfo.PRODUCT, "2c7c/")) {
            printf("%s/%.16s %s\n", base, de->d_name, PRODUCT);
        }
    }

    if (modem_require_count == 1)
    {
        dprintf("modem_require_count = %d\n", modem_require_count);

        if (usb3_process_start_time.tv_sec > this_atime[0].tv_sec
        || (usb3_process_start_time.tv_sec == this_atime[0].tv_sec
        && usb3_process_start_time.tv_nsec > this_atime[0].tv_nsec))
        {
            modem_require_count = modem_require_count - 1;
            count = 0;
            memset(&usbfs_dev[count], 0, sizeof(USBFS_T));
        }
        else
        {
            memmove(&usbfs_dev[0], &usbfs_dev_tmp[0], sizeof(USBFS_T));
            count = 1;
        }
    }
    else if (modem_require_count > 1)
    {
        int i, k;
        static struct timespec this_atime_tmp;
        static USBFS_T usbfs_dev_tmp1;

        dprintf("modem_require_count = %d\n", modem_require_count);
        for(i=0; i<modem_require_count-1; i++)
        {
            for(k=0; k<modem_require_count-i-1; k++)
            {
                if (this_atime[k].tv_sec > this_atime[k+1].tv_sec
                || (this_atime[k].tv_sec == this_atime[k+1].tv_sec
                && this_atime[k].tv_nsec > this_atime[k+1].tv_nsec))
                {
                    this_atime_tmp.tv_sec = this_atime[k].tv_sec;
                    this_atime_tmp.tv_nsec = this_atime[k].tv_nsec;
                    this_atime[k].tv_sec = this_atime[k+1].tv_sec;
                    this_atime[k].tv_nsec = this_atime[k+1].tv_nsec;
                    this_atime[k+1].tv_sec = this_atime_tmp.tv_sec;
                    this_atime[k+1].tv_nsec = this_atime_tmp.tv_nsec;

                    memmove(&usbfs_dev_tmp1, &usbfs_dev_tmp[k], sizeof(USBFS_T));
                    memmove(&usbfs_dev_tmp[k], &usbfs_dev_tmp[k+1], sizeof(USBFS_T));
                    memmove(&usbfs_dev_tmp[k+1], &usbfs_dev_tmp1, sizeof(USBFS_T));
                }
            }
        }

        if (usb3_process_start_time.tv_sec > this_atime[modem_require_count-1].tv_sec
        || (usb3_process_start_time.tv_sec == this_atime[modem_require_count-1].tv_sec
        && usb3_process_start_time.tv_nsec > this_atime[modem_require_count-1].tv_nsec))
        {
            count = 0;
            memset(&usbfs_dev[count], 0, sizeof(USBFS_T));
        }
        else
        {
            memmove(&usbfs_dev[0], &usbfs_dev_tmp[modem_require_count-1], sizeof(USBFS_T));
            count = 1;
        }
    }

    closedir(busdir);

    return count;
}

int usbmon_fd = -1;
int usbmon_logfile_fd = -1;

void *catch_log(void *arg)
{
    int nreads = 0;
    char tbuff[256];
    size_t off = strlen("[999.999] ");

    tbuff[off - 1] = ' ';
    while(1) {
        nreads = read(usbmon_fd, tbuff + off, sizeof(tbuff) - off);
        if (nreads == -1 && errno == EINTR)
            continue;
        if (nreads <= 0)
            break;

        tbuff[off + nreads] = '\0';
        memcpy(tbuff, get_time(), off - 1);

        if (write(usbmon_logfile_fd, tbuff, strlen(tbuff)) == -1) { };
    }

    return NULL;
}

int ql_capture_usbmon_log(const char* usbmon_logfile)
{
    const char *usbmon_path = "/sys/kernel/debug/usb/usbmon/0u";
    pthread_t pt;
    pthread_attr_t attr;

    if (access("/sys/kernel/debug/usb", F_OK)) {
        printf("debugfs is not mount, please execute \"mount -t debugfs none_debugs /sys/kernel/debug\"\n");
        return -1;
    }
    if (access("/sys/kernel/debug/usb/usbmon", F_OK)) {
        printf("usbmon is not load, please execute \"modprobe usbmon\" or \"insmod usbmon.ko\"\n");
        return -1;
    }

    usbmon_fd = open(usbmon_path, O_RDONLY);
    if (usbmon_fd < 0) {
        printf("open %s error(%d) (%s)\n", usbmon_path, errno, strerror(errno));
        return -1;
    }

    usbmon_logfile_fd = open(usbmon_logfile, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (usbmon_logfile_fd < 0) {
        printf("open %s error(%d) (%s)\n", usbmon_logfile, errno, strerror(errno));
        close(usbmon_fd);
        return -1;
    }

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_create(&pt, &attr, catch_log, NULL);

    return 0;
}

void ql_stop_usbmon_log()
{
    if (usbmon_logfile_fd > 0)
        close(usbmon_logfile_fd);
    if (usbmon_fd > 0)
        close(usbmon_fd);
}

int check_mcu_endian()
{
    union
    {
        int a;
        char b;
    }u;

    u.a = 1;
    return u.b;
}
