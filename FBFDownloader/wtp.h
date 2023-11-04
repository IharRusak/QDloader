/******************************************************************************
  @file    wtp.h
  @brief   define.

  DESCRIPTION
  QDloader Tool for USB and PCIE of Quectel wireless cellular modules.

  INITIALIZATION AND SEQUENCING REQUIREMENTS
  None.

  ---------------------------------------------------------------------------
  Copyright (c) 2016 - 2021 Quectel Wireless Solution, Co., Ltd.  All Rights Reserved.
  Quectel Wireless Solution Proprietary and Confidential.
  ---------------------------------------------------------------------------
******************************************************************************/
#ifndef WTPDEF_H // Only make declarations once.
#define WTPDEF_H

#define DATALENGTH 0x4000
//#define RETURNDATALENGTH 0x1FFA  // Update for upload data size is 0x2000
#define MESSAGELENGTH 256
#define ISMSGPORT  FALSE // Use this to enable port handshaking errors for debugging.

#define USB	            	0
#define SERIALPORT	        	1
#define UNKNOWNPORT	       -1

#define ACK	            	0
#define NACK	        	1
#define MESSAGEPENDING		1
#define MESSAGESTRING		0
#define MESSAGEERRORCODE	1

// Image ID definitions
#define TIMH	        	0x54494D48
#define DKBI	        	0x444B4249
#define OBMI	        	0x4F424D49
#define FBFI                0x46424649
#define FBFD                0x46424644
#define PART                0x50415254
#define TZII	        	0x545A4949
#define WTMI	        	0x57544D49


#define NOFASTDOWNLOAD  'N'      //added for fast download proj--used to turn off fast download
#define GET           0      //added for fast download proj--flag for fast download.
#define SET           1      //added for fast download proj--flag for fast download.
#define FDMODE        1      //dummy/default value when getting the value for fast download.

typedef enum
{
   DISABLED	        = 1,        // Standard WTPTP download
   MSG_ONLY	        = 2,        //Turn on message mode(listen for msgs forever)
   MSG_AND_DOWNLOAD = 3 //Msg mode and WTPTP download
} MESSAGE_MODES;

// Protocol command definitions
#define PREAMBLE	                	0x00
#define PUBLICKEY	                	0x24
#define PASSWORD	                	0x28
#define SIGNEDPASSWORD	            	0x25
#define GETVERSION	                	0x20
#define SELECTIMAGE	                	0x26
#define VERIFYIMAGE	                	0x27
#define DATAHEADER	                	0x2A
#define DATA	                    	0x22
#define MESSAGE	                    	0x2B
#define OTPVIEW	                    	0x2C
#define DEBUGBOOT	                	0x2D
#define IMEIBIND	                	0x2E
#define IMEIDATA	                	0x2F
#define DONE	                    	0x30
#define DISCONNECT	                	0x31
#define UPLOADDATAHEADER                0x32
#define UPLOADDATA                      0x33
#define PROTOCOLVERSION                 0x34
#define GETPARAMETERS                   0x35
#define GETBADBLOCK                 	0x37
#define GETIMAGECRC	                	0x38
#define UNKNOWNCOMMAND	            	0xFF

#define  UPLOAD_WITH_SPAREAREA             1
#define  UPLOAD_WITHOUT_SPAREAREA          0

typedef enum {
    WTPTPNOTIFICATION    =              0,
    WTPTPERRORREPORT          =         1,
    WTPTPNOTIFICATION_BURNTSIZE	=       2,
    WTPTPNOTIFICATION_UPLOAD	    =   3,
    WTPTPNOTIFICATION_FLASHSIZE	=       4,
    WTPTPREPORT_EXTENDED_FUSE_INFO =    5,
    WTPTPREPORT_UEINFO   =              6,
} WTPTP;

typedef struct
{
	unsigned int ErrorCode;
	char Description[100];
} ERRORCODETABLE;

typedef enum {
 PlatformBusy	=                	0x1,
 PlatformReady	=                	0x2,
 PlatformResetBBT	=            	0x3,
 PlatformEraseAllFlash	=        	0x4,
 PlatformDisconnect	    =        	0x5,
 PlatformEraseAllFlashDone	=    	0x6,
 PlatformEraseAllFlashWithoutBurn =	0x7,
 PlatformEraseAllFlashWithoutBurnDone =  0x8,
 PlatformFuseOperationStart  =         0x9,
 PlatformFuseOperationDone    =        0xA,
 PlatformUEInfoStart           =       0xB,
 PlatformUEInfoDone         =          0xc,
} WTPTPNotification;

typedef struct
{
	WTPTPNotification NotificationCode;
	char Description[100];
} NOTIFICATIONTABLE;


#pragma pack(push,1)

typedef struct
{
	uint8_t CMD;
	uint8_t SEQ;
	uint8_t CID;
	uint8_t Flags;
	uint32_t LEN;
	uint8_t Data[DATALENGTH];
} WTPCOMMAND;


typedef struct
{
	uint8_t CMD;
	uint8_t SEQ;
	uint8_t CID;
	uint8_t Status;
	uint8_t Flags;
	uint8_t DLEN;
	uint8_t Data[DATALENGTH];
} WTPSTATUS;

typedef struct
{
    uint8_t MajorVersion;
    uint8_t MinorVersion;
    short Build;
} PROTOCOL_VERSION;

typedef struct
{
    unsigned int BufferSize;
    unsigned int Rsvd1;
    unsigned int Rsvd2;
    unsigned int Rsvd3;
} TARGET_PARAMS;

struct stFlatformBootLoaderInfo{
	ePlatForm ePlatFormType;
	const char*	  strBLDate;
	const char*    strBLProcessor;
	const char*	  strBLVersion;
};

struct fbf_file_info{
	char FBF_h_bin[256];
    unsigned int ImageID;
};

struct fbf_file{
	char FBF_timheader_bin[256];
    struct fbf_file_info fbf_fi_info;
};

struct fbf_file_params{
    struct fbf_file fbf_fl[30];
	unsigned int FBF_timheader_bin_count;
    unsigned int FBF_h_bin_count;
};

#pragma pack(pop)

extern const struct stFlatformBootLoaderInfo gstFlatformBLInfoSet[];

extern int WtptpDownLoad_GetDeviceBootType(struct CBinFileWtp *me, struct fbf_file_params  *fbf_file_pa, struct fbf_file_params  *fbf_final_file_pa);
#endif
