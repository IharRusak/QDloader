/******************************************************************************
  @file    BinFile.h
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
#ifndef _BIN_FILE_H_
#define  _BIN_FILE_H_
typedef unsigned long		    DWORD;
typedef uint8_t UINT8;
typedef uint16_t UINT16;
typedef uint32_t UINT32;
#define TRUE 1
#define FALSE 0
#define BOOL int
#define BYTE uint8_t
#define CHAR char

#define NUM_OF_SUPPORTED_FLASH_DEVS	   4
#define MAX_NUMBER_OF_FLASH_DEVICES_IN_MASTER_HEADER    4
#define MAX_NUMBER_OF_FLASH_DEVICES                     10
#define MAX_RESEVERD_LEN	4
#define MAX_NUM_SKIP_BLOCKS      32

#define MAX_NUMBER_OF_IMAGE_STRUCTS_IN_DEVICE_HEADER    30

typedef enum
{
	PLAT_PXA92x_Old=0,
	PLAT_PXA92x ,
	PLAT_MMP2 ,
	PLAT_WUKONG,
	PLAT_NEVO,//
	PLAT_MMP3,
	PLAT_NEZHA,
	PLAT_NEVO_OTA,
	PLAT_EMEI,
	PLAT_HELAN,
	PLAT_EDEN, //PLAT_EDEN_Zx
	PLAT_HELANLTE,
	PLAT_NEZHA2,
	PLAT_HELAN2,
	PLAT_EDEN_A0,
	PLAT_NEZHA3,
	PLAT_ULC1,
	PLAT_HELAN3,
	PLAT_HELAN4,
	PLAT_ULC2,
	PLAT_UNKNOWN = 0xff
} ePlatForm;


#define MMP2_BOOTLOADER_PROCESSOR        _T("MMP2")
#define MMP2_BOOTLOADER_VERSION          _T("3215")
#define MMP2_BOOTLOADER_DATE             _T("12052009")

#define  WKNG_BOOTLOADER_PROCESSOR       _T("WKNG")
#define  WKNG_BOOTLOADER_VERSION         _T("3301")
#define  WKNG_BOOTLOADER_DATE            _T("06172011")

#define NEVO_BOOTLOADER_PROCESSOR        _T("NEVO")
#define NEVO_BOOTLOADER_VERSION          _T("3301")
#define NEVO_BOOTLOADER_DATE             _T("08182011")

#define MMP3_BOOTLOADER_PROCESSOR        _T("MMP3")
#define MMP3_BOOTLOADER_VERSION          _T("3302")
#define MMP3_BOOTLOADER_DATE             _T("03082012")


#define PXA92X_BOOTLOADER_PROCESSOR      _T("TVTD")
#define PXA92X_BOOTLOADER_VERSION        _T("3211")
#define PXA92X_BOOTLOADER_DATE           _T("06242009")

#define EMEI_BOOTLOADER_PROCESSOR        _T("EMEI")
#define EMEI_BOOTLOADER_VERSION          _T("3304")
#define EMEI_BOOTLOADER_DATE             _T("07022012")

#define HELAN_BOOTLOADER_PROCESSOR       _T("HELN")
#define HELAN_BOOTLOADER_VERSION         _T("3307")
#define HELAN_BOOTLOADER_DATE            _T("01152013")

#define EDEN_BOOTLOADER_PROCESSOR        _T("EDEN")
#define EDEN_BOOTLOADER_VERSION          _T("3307")
#define EDEN_BOOTLOADER_DATE             _T("04162013")

#define NEZHA3_BOOTLOADER_PROCESSOR   	 _T("NZA3")
#define NEZHA3_BOOTLOADER_VERSION        _T("3311")
#define NEZHA3_BOOTLOADER_DATE        	 _T("06012014")


#define HELAN2_BOOTLOADER_PROCESSOR      _T("HLN2")
#define HELAN2_BOOTLOADER_VERSION        _T("3308")
#define HELAN2_BOOTLOADER_DATE           _T("02122014")

#define EDENA0_BOOTLOADER_PROCESSOR      _T("EDEN")
#define EDENA0_BOOTLOADER_VERSION        _T("3307")
#define EDENA0_BOOTLOADER_DATE           _T("02142014")

#define ULC1_BOOTLOADER_PROCESSOR        _T("ULC1")
#define ULC1_BOOTLOADER_VERSION          _T("3313")
#define ULC1_BOOTLOADER_DATE             _T("09172014")

#define HELAN3_BOOTLOADER_PROCESSOR      _T("HLN3")
#define HELAN3_BOOTLOADER_VERSION        _T("3313")
#define HELAN3_BOOTLOADER_DATE           _T("11052014")

#define HELAN4_BOOTLOADER_PROCESSOR      _T("HLN4")
#define HELAN4_BOOTLOADER_VERSION        _T("3313")
#define HELAN4_BOOTLOADER_DATE           _T("25032015")

#define ULC2_BOOTLOADER_PROCESSOR        _T("ULC2")
#define ULC2_BOOTLOADER_VERSION          _T("3313")
#define ULC2_BOOTLOADER_DATE             _T("08052015")

typedef enum _EDeviceType
{
	kNone    = 0,
	kBootRom	 = 1,
	kBootLoader   = 2,
}EDeviceType;

#pragma pack (1)
typedef struct _FileStruct
{
    uint32_t Head1, Head2;
    uint32_t Mid1, Mid2;
    uint32_t Tail1, Tail2;
}  __attribute__ ((packed)) FileStruct;

typedef struct _FilePoint
{
    uint32_t Head1_offset;
    uint32_t Tail2_offset;
}  __attribute__ ((packed)) FilePoint;

typedef struct
{
	char Unique[24];
	UINT16 Flash_Device_Spare_Area_Size[NUM_OF_SUPPORTED_FLASH_DEVS];
	UINT16 Format_Version;
	UINT16 Size_of_Block;
	UINT32 Bytes_To_Program;
	UINT32 Bytes_To_Verify;
	UINT32 Number_of_Bytes_To_Erase;
	UINT32 Main_Commands;
	UINT32 nOfDevices;	 /* number of devices to burn in parallel */
	UINT32 DLerVersion;	 /* Version of downloader current 1		  */
	UINT32 deviceHeaderOffset[MAX_NUMBER_OF_FLASH_DEVICES_IN_MASTER_HEADER]; /* offset in Sector 0 for each flash device header  */
}  __attribute__ ((packed)) MasterBlockHeader;
typedef MasterBlockHeader *PMasterBlockHeader;

typedef struct
{
	UINT32 flash_partition_size; /* flash partition size */
	UINT32 commands;		 	 /* bit switches */
	UINT32 First_Sector;		    /* First sector of the image in the block device */
	UINT32 length;			        /* Block length in bytes */
	UINT32 Flash_Start_Address;     /* start address in flash */
	UINT32 Flash_block_size;        /* flash device block size */
	UINT32 ChecksumFormatVersion2;  /* new format version image checksum (left for backwards compatibility) */
} ImageStruct_VPXA92x;

typedef struct
{
	UINT32 Image_ID;				/* image id*/
    UINT32 Image_In_TIM;			/* indicate this image is in TIM or not*/
    UINT32 Flash_partition;        /* partition number of the image */
    UINT32 Flash_erase_size;      /* erase size of the image */
    UINT32 commands;                /* bit switches */
    UINT32 First_Sector;            /* First sector of the image in the block device */
    UINT32 length;                 /*  Image length in bytes */
    UINT32 Flash_Start_Address;    /* start address in flash */
    UINT32 Reserved[MAX_RESEVERD_LEN];
    UINT32 ChecksumFormatVersion2;  /* new format version image checksum (left for backwards compatibility) */
} ImageStruct_V11;
typedef ImageStruct_V11 *PImageStruct_V11;

typedef struct
{
	UINT32 Total_Number_Of_SkipBlocks; // total numbers of skip blocks
	UINT32 Skip_Blocks[MAX_NUM_SKIP_BLOCKS];
} SkipBlocksInfoStruct;

typedef struct
{
	UINT32 EraseAll; // erase all flag for user partition
    UINT32 ResetBBT; // indicate if reset BBT
	UINT32 NandID;	 // nand flash ID
	UINT32 Reserved[MAX_RESEVERD_LEN - 1];
	SkipBlocksInfoStruct  Skip_Blocks_Struct;
} __attribute__ ((packed)) FlashOptStruct;

typedef struct
{
	UINT32 DeviceFlags;                 /* NAND, eMMC, SPI-NOR, etc*/
	UINT32 DeviceParameters[16];        /*  Device Parameters, reserve 16 U32 here, will be defined depending on different devices */
	FlashOptStruct  FlashOpt;
	UINT32 ProductionMode;             // production mode
	UINT8 OptValue;	// choice: 0 - Not reset after download, 1 - Reset after download
	UINT8 ChipID;
	UINT8 BBCS_EN;
	UINT8 CRCS_EN;
	UINT32 Reserved[MAX_RESEVERD_LEN-2];
	UINT32 nOfImages;        /* number of images */
	ImageStruct_V11 imageStruct[MAX_NUMBER_OF_IMAGE_STRUCTS_IN_DEVICE_HEADER]; /* array of image structs */
} __attribute__ ((packed)) DeviceHeader_V11;

typedef unsigned int 		   	UINT_T;
typedef struct
{
 UINT_T Version;
 UINT_T	Identifier;					// "TIMH"
 UINT_T Trusted;					// 1- Trusted, 0 Non
 UINT_T IssueDate;
 UINT_T OEMUniqueID;
} VERSION_I, *pVERSION_I;			// 0x10 bytes

typedef struct
{
 UINT_T WTMFlashSign;
 UINT_T WTMEntryAddr;
 UINT_T WTMEntryAddrBack;
 UINT_T WTMPatchSign;
 UINT_T WTMPatchAddr;
 UINT_T BootFlashSign;
} FLASH_I, *pFLASH_I;				// 0x10 bytes

typedef struct
{
 VERSION_I      VersionBind;         			// 0
 FLASH_I        FlashInfo;           			// 0x10
 UINT_T         NumImages;           			// 0x20
 UINT_T         NumKeys;						// 0x24
 UINT_T         SizeOfReserved;					// 0x28
} CTIM, *pCTIM;									// 0x2C

typedef struct
{
 UINT_T ImageID;					// Indicate which Image
 UINT_T NextImageID;				// Indicate next image in the chain
 UINT_T FlashEntryAddr;			 	// Block numbers for NAND
 UINT_T LoadAddr;
 UINT_T ImageSize;
 UINT_T ImageSizeToHash;
 UINT_T HashAlgorithmID;            // See HASHALGORITHMID_T
 UINT_T Hash[8];					// Reserve 256 bits for the hash
 UINT_T PartitionNumber;			// This is new for V3.2.0
} IMAGE_INFO_3_2_0, *pIMAGE_INFO_3_2_0;			// 0x40 bytes

typedef struct
{
 UINT_T ImageID;					// Indicate which Image
 UINT_T NextImageID;				// Indicate next image in the chain
 UINT_T FlashEntryAddr;			 	// Block numbers for NAND
 UINT_T LoadAddr;
 UINT_T ImageSize;
 UINT_T ImageSizeToHash;
 UINT_T HashAlgorithmID;            // See HASHALGORITHMID_T
 UINT_T Hash[8];					// Reserve 256 bits for the hash
} IMAGE_INFO_3_1_0, *pIMAGE_INFO_3_1_0; 	// 0x3C bytes

typedef struct
{
 UINT_T ImageID;					// Indicate which Image
 UINT_T NextImageID;				// Indicate next image in the chain
 UINT_T FlashEntryAddr;			 	// Block numbers for NAND
 UINT_T LoadAddr;
 UINT_T ImageSize;
 UINT_T ImageSizeToHash;
 UINT_T HashAlgorithmID;            // See HASHALGORITHMID_T
 UINT_T Hash[16];					// Reserve 512 bits for the hash, this is new for V3.4.0
 UINT_T PartitionNumber;
} IMAGE_INFO_3_4_0, *pIMAGE_INFO_3_4_0;			// 0x60 bytes

typedef enum
{
    Marvell_DS = 0,
    PKCS1_v1_5_Caddo = 1,
    PKCS1_v2_1_Caddo = 2,
    PKCS1_v1_5_Ippcp = 3,
    PKCS1_v2_1_Ippcp = 4,
    ECDSA_256 = 5,
    ECDSA_521 = 6
}
ENCRYPTALGORITHMID_T;

#define Intel_DS Marvell_DS

typedef enum
{
	MRVL_SHA160 = 20,
	MRVL_SHA256 = 32,
	MRVL_SHA512 = 64
}
HASHALGORITHMID_T;


typedef struct
{
	UINT_T ImageID;						// Indicate which Image
	UINT_T NextImageID;					// Indicate next image in the chain
	UINT_T FlashEntryAddr;					// Block numbers for NAND
	UINT_T LoadAddr;
	UINT_T ImageSize;
	UINT_T ImageSizeToHash;
	HASHALGORITHMID_T HashAlgorithmID;		// See HASHALGORITHMID_T
	UINT_T Hash[16];						// Reserve 512 bits for the hash
	UINT_T PartitionNumber;
	ENCRYPTALGORITHMID_T EncAlgorithmID;	// See ENCRYPTALGORITHMID_T
	UINT_T EncryptStartOffset;
	UINT_T EncryptSize;
} IMAGE_INFO_3_5_0, *pIMAGE_INFO_3_5_0;			// 0x60 bytes

#define TIM_3_1_01			0x30101
#define TIM_3_2_00			0x30200			// Support for Partitioning
#define TIM_3_3_00			0x30300			// Support for ECDSA-256 and 64 bit addressing
#define TIM_3_4_00			0x30400			// Support for ECDSA-521
#define TIM_3_5_00			0x30500			// Support for Encrypted Boot

#define MAX_HASH  256
#define MAX_HASH_IN_BITS  512	// 512 bits

// Image ID definitions
#define TIMH		0x54494D48
#define DKBI		0x444B4249
#define OBMI		0x4F424D49
#define FBFI        0x46424649
#define FBFD        0x46424644
#define PART        0x50415254
#define TZII		0x545A4949
#define WTMI		0x57544D49

#pragma pack ()

typedef struct _InstanceParams
{
    //t_DdrFlashDownloadImgInfoList * pDownloadImagesList;
	const char*  	pszDKBbin;		// Download file  for Bootrom
	const char* 	pszDKBTim;		// Primary flasher file path(*.bin, *.axf, *.elf) (can be either Comm. or App flasher)
	//const TCHAR* 	pszOBMFile;			// OBM file path
	//const TCHAR* 	pszWTMFile;			// WTM file path
	//const TCHAR*    pszTZIIFile;        //TZII file path
	//const TCHAR* 	pszJTAGKeyFile;		// OEM Key file path to do re-eanble JTAG in case JTAG is disable
	//CALLBACKPROC	CallbackProc;		// Callback function that will back relevant process information
	//pWTPTPREAMBLECOMMAND pWtptpPreaCmd; // Customize Preamble Command
	unsigned int    PlaformType;        /* 2-MMP2   */
										/* 3-WUKONG */
										/* 4-PXA978   */
										/* 5-MMP3   */
	                                    /* 6-PXA1802  */
	                                    /* 8-PXA988/986 */
	                                    /* 9-PXA1088 */
	                                    /* 10-PXA1928 */
	                                    /* 11-PXA1920 */
										/* 15-PXA1826 */
	unsigned int    FlashType;          // 0-NAND, 1-eMMC,3-SPINOR,4-ONENAND,others-Unknown
	unsigned int    Commands;           // flags description /* bit switches */, user only need to set bit3 if user want to use customized preamble
	                                      /*bit 0:Erase all flash flag*/
	                                      /*bit 1: Upload flag */
	                                      /*bit 2: Reset BBT flag */
	                                      /*bit 3: Customized preamble flag*/
	                                      /*bit 4: erase all flash only flag */
	                                      /*bit 5: JTAG Re enable flag */
	                                      /*bit 6 ~ bit31 reserved */
    unsigned int    FlashPageSize;      // Nand Flash Data page size
	unsigned int    DownloadMode;       // Single_Download 1,multi_download 0;
	unsigned int    ProductionMode;     // ;
	unsigned int    GetBadBlk;			//get flash bad block enabled 1; otherwise: 0
	unsigned int    ImageCrcEn;       // Image CRC enabled: 1,Image CRC disabled: 0;
	//unsigned int    ReservedVal[MAX_RESERVED_DATA];
	//t_UpLoadDataSpecList* pUploadSpecs; // Upload spec Info
}InstanceParams, *PInstanceParams;

struct CBinFileWtp
{
    FILE* m_fBinFile;
    uint32_t m_uFileOffset;
    uint32_t nSize;
    char FbfList[20][256];
    FileStruct m_mapFile2Struct[20];
    FilePoint m_mapFile2Point[20];

    MasterBlockHeader masterBlkHeader;
    DeviceHeader_V11 deviceHeaderBuf;
    InstanceParams instanceParam;

    int usbfd;
    uint8_t m_iSequence;
    ePlatForm   m_ePlatformType;
};

extern uint32_t BinFileWtp_GetFileSize(struct CBinFileWtp *me, const char *pszBinFile);
extern size_t BinFileWtp_ReadBinFile(struct CBinFileWtp *me, void *buffer, size_t size, size_t count, const char *pszBinFile);
extern int BinFileWtp_FseekBin(struct CBinFileWtp *me, const char *pszBinFile, long _Offset, int _Origin);
extern char *g_DkbTimPath;
extern char *g_DkbFilePath;
extern char *g_FbfTimPath;
extern char *g_FbfFilePath;
extern int verbose;
#endif
