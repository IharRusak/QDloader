/******************************************************************************
  @file    WtptpDownloader.c
  @brief   WtptpDownloader.

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
#include "../usbfs/usbfs.h"
#include "BinFile.h"
#include "wtp.h"

#define _T
static const NOTIFICATIONTABLE WTPTPNotificationMsg[] =
{
    {PlatformBusy,_T("Platform is busy")},
    {PlatformReady,_T("Platform is Ready")},
    {PlatformResetBBT,_T("Reset BBT")},
    {PlatformEraseAllFlash,_T("Erase All flash begin")},
    {PlatformDisconnect,_T("platform is disconnect")},
    {PlatformEraseAllFlashDone,_T("Erase All flash done")},
    {PlatformEraseAllFlashWithoutBurn,_T("Erase All flash without burning begin")},
    {PlatformEraseAllFlashWithoutBurnDone,_T("Erase All flash without burning Done")},
    {PlatformFuseOperationStart,_T("Platform Fuse Operation Start")},
    {PlatformFuseOperationDone,_T("Platform Fuse Operation Done")},
    {PlatformUEInfoStart,_T("Platform UE Info Operation Start")},
    {PlatformUEInfoDone,_T("Platform UE Info Operation Done")}
};
static WTPSTATUS gWtpStatus;

static unsigned int update_size = 0;

const struct stFlatformBootLoaderInfo gstFlatformBLInfoSet[] = {
	{PLAT_ULC2,		ULC2_BOOTLOADER_DATE,	ULC2_BOOTLOADER_PROCESSOR,		ULC2_BOOTLOADER_VERSION},
	{PLAT_HELAN4,	HELAN4_BOOTLOADER_DATE,	HELAN4_BOOTLOADER_PROCESSOR,	HELAN4_BOOTLOADER_VERSION},
	{PLAT_HELAN3,	HELAN3_BOOTLOADER_DATE,	HELAN3_BOOTLOADER_PROCESSOR,	HELAN3_BOOTLOADER_VERSION},
	{PLAT_ULC1,		ULC1_BOOTLOADER_DATE,	ULC1_BOOTLOADER_PROCESSOR,		ULC1_BOOTLOADER_VERSION},
	{PLAT_NEZHA3,	NEZHA3_BOOTLOADER_DATE,	NEZHA3_BOOTLOADER_PROCESSOR,	NEZHA3_BOOTLOADER_VERSION},
	{PLAT_EDEN_A0,	EDENA0_BOOTLOADER_DATE,	EDENA0_BOOTLOADER_PROCESSOR,	EDENA0_BOOTLOADER_VERSION},
	{PLAT_HELAN2,	HELAN2_BOOTLOADER_DATE,	HELAN2_BOOTLOADER_PROCESSOR,	HELAN2_BOOTLOADER_VERSION},
	/*{PLAT_NEZHA2,	NEZHA2_BOOTLOADER_DATE,	NEZHA2_BOOTLOADER_PROCESSOR,	NEZHA2_BOOTLOADER_VERSION},*/
	{PLAT_EDEN,		EDEN_BOOTLOADER_DATE,	EDEN_BOOTLOADER_PROCESSOR,		EDEN_BOOTLOADER_VERSION},
	{PLAT_HELANLTE,	HELAN_BOOTLOADER_DATE,	HELAN_BOOTLOADER_PROCESSOR,		HELAN_BOOTLOADER_VERSION},
	{PLAT_HELAN,	HELAN_BOOTLOADER_DATE,	HELAN_BOOTLOADER_PROCESSOR,		HELAN_BOOTLOADER_VERSION},
	{PLAT_EMEI,		EMEI_BOOTLOADER_DATE,	EMEI_BOOTLOADER_PROCESSOR,		EMEI_BOOTLOADER_VERSION},
	{PLAT_NEVO_OTA,	NEVO_BOOTLOADER_DATE,	NEVO_BOOTLOADER_PROCESSOR,		NEVO_BOOTLOADER_VERSION},
	{PLAT_NEVO,		NEVO_BOOTLOADER_DATE,	NEVO_BOOTLOADER_PROCESSOR,		NEVO_BOOTLOADER_VERSION},
	{PLAT_MMP3,		MMP3_BOOTLOADER_DATE,	MMP3_BOOTLOADER_PROCESSOR,		MMP3_BOOTLOADER_VERSION},
	{PLAT_PXA92x,	PXA92X_BOOTLOADER_DATE,	PXA92X_BOOTLOADER_PROCESSOR,	PXA92X_BOOTLOADER_VERSION},
	{PLAT_MMP2,		MMP2_BOOTLOADER_DATE,	MMP2_BOOTLOADER_PROCESSOR,		MMP2_BOOTLOADER_VERSION},
	{PLAT_WUKONG,	WKNG_BOOTLOADER_DATE,	WKNG_BOOTLOADER_PROCESSOR,		WKNG_BOOTLOADER_VERSION}
};

static int WriteUSB(struct CBinFileWtp *me,WTPCOMMAND* pWtpCmd,size_t dwSize)
{
    int ret = sendSync(me->usbfd, pWtpCmd, dwSize, 0);
    return (ret == dwSize);
}

static int WriteUSBWithNoRsp (struct CBinFileWtp *me,BYTE *pData,size_t dwSize)
{
    int ret = sendSync(me->usbfd, pData, dwSize, 0);
    return (ret == dwSize);
}

static int ReadUSB (struct CBinFileWtp *me,WTPSTATUS *pWtpStatus)
{
    int bRes = TRUE;
    int dwBytes = -1;

    memset(pWtpStatus,0,16);
    dwBytes = recvSync(me->usbfd,pWtpStatus,DATALENGTH, 5000);
    if(dwBytes <= 0)
        bRes = FALSE;

    return bRes;
}

static int SendWtpMessage(struct CBinFileWtp *me, WTPCOMMAND *pWtpCmd, WTPSTATUS *pWtpStatus)
{
    size_t dwSize;

    if (pWtpCmd->CMD == PREAMBLE)
        dwSize  = 4;
    else
        dwSize = 8 + le32toh(pWtpCmd->LEN);

    if (!WriteUSB (me,pWtpCmd,dwSize)) {
        dprintf("CMD: 0x%x - WriteUSB failed\n", pWtpCmd->CMD);
        return FALSE;
    }

    memset(pWtpStatus, 0, sizeof(WTPSTATUS) );
    if (!ReadUSB(me,pWtpStatus))
    {
        dprintf("CMD: 0x%x - ReadUSB failed\n", pWtpCmd->CMD);
        return FALSE;
    }

    if (pWtpCmd->CMD != pWtpStatus->CMD)
    {
        dprintf("CMD: 0x%x - Get CMD %d failed\n", pWtpCmd->CMD, pWtpStatus->CMD);
        return FALSE;
    }

    if ((pWtpCmd->CMD == PREAMBLE) || (pWtpCmd->CMD == MESSAGE && pWtpStatus->Status == NACK))
        ;
    else if (pWtpStatus->Status != ACK)
    {
        dprintf("CMD: 0x%x - Get Status %d failed\n", pWtpCmd->CMD, pWtpStatus->Status);
    }

    if ((pWtpStatus->Flags & 0x1 ) == MESSAGEPENDING ) {
        //dprintf("message pending\n");
    }

    return TRUE;
}

static int SendPreamble (struct CBinFileWtp *me, WTPSTATUS* pWtpStatus)
{
    int IsOk;
    WTPCOMMAND WtpCmd = {PREAMBLE,0xD3,0x02,0x2B,htole32(0x00D3022B)};

    dprintf("WtpCmd: %s\n", __func__);
    IsOk = SendWtpMessage(me,&WtpCmd,pWtpStatus);
    if(IsOk)
    {
        IsOk = (pWtpStatus->Status == ACK && pWtpStatus->SEQ != 0xD3 && pWtpStatus->CID != 0x02);
    }

    return IsOk;
}

static int GetWtpMessage (struct CBinFileWtp *me,WTPSTATUS *pWtpStatus)
{
    int IsOk ;
    WTPCOMMAND WtpCmd = {MESSAGE,0,0x00,0x00,htole32(0x00000000)};

    IsOk = SendWtpMessage(me, &WtpCmd, pWtpStatus);
    if (!IsOk)
        return IsOk;

    if (pWtpStatus->DLEN == 6)
    {
        int i;
        uint32_t ReturnCode = le32toh(*(uint32_t *)&pWtpStatus->Data[2]);

        if (pWtpStatus->Data[0] == WTPTPNOTIFICATION)
        {
            int bFound = FALSE;

            for (i = 0; i < (sizeof(WTPTPNotificationMsg)/sizeof(WTPTPNotificationMsg[0])); i++)
            {
                if (WTPTPNotificationMsg[i].NotificationCode == ReturnCode)
                {
                    dprintf("Notification: %s 0x%X\n",WTPTPNotificationMsg[i].Description,ReturnCode);
                    bFound = TRUE;
                    break;;
                }
            }

            if(!bFound)
            {
                dprintf("Notification Code: 0x%X\n",ReturnCode);
            }
        }
        else if (pWtpStatus->Data[0]== WTPTPERRORREPORT)
        {
            dprintf("User customized ErrorCode: 0x%X\n", ReturnCode);
        }
        else if (WTPTPNOTIFICATION_UPLOAD == pWtpStatus->Data[0])
        {
            dprintf("Flash_NandID: 0x%X\n",ReturnCode);
        }
        else if (WTPTPNOTIFICATION_FLASHSIZE == pWtpStatus->Data[0])
        {
            dprintf("Flash_Size: 0x%X bytes\n",ReturnCode);
        }
        else if (WTPTPNOTIFICATION_BURNTSIZE == pWtpStatus->Data[0])
        {
            static uint32_t Burnt_Size = 0;
            // make a round to burnt size to multiple of 4096(FBF_Sector size)
            if(ReturnCode%4096)
            {
                uint32_t pad_size = 4096 - (ReturnCode%4096);
                ReturnCode += pad_size;
            }
            update_size = ReturnCode;
            Burnt_Size += ReturnCode;
            dprintf("Burnt_Size: %d bytes\n", Burnt_Size);
        }
        else {
            dprintf("WTPTP Data: 0x%X\n",pWtpStatus->Data[0]);
        }
    }
    else if (pWtpStatus->DLEN != 0)
    {
        dprintf("WTPTP DLEN: %d\n",pWtpStatus->DLEN);
    }

    return IsOk;
}

static int GetVersion (struct CBinFileWtp *me,WTPSTATUS *pWtpStatus)
{
    int IsOk;
    WTPCOMMAND WtpCmd = {GETVERSION,0,0x00,0x00,htole32(0x00000000)};

    dprintf("WtpCmd: %s\n", __func__);
    IsOk = SendWtpMessage(me, &WtpCmd, pWtpStatus);
    if (IsOk) {
        IsOk = (pWtpStatus->Status == ACK);
    }

    return IsOk;
}

static int SelectImage (struct CBinFileWtp *me, WTPSTATUS* pWtpStatus, UINT32 *pImageType)
{
    int IsOk = TRUE;
    WTPCOMMAND WtpCmd = {SELECTIMAGE,0x00,0x00,0x00,htole32(0x00000000)};

    dprintf("WtpCmd: %s\n", __func__);
    IsOk = SendWtpMessage(me, &WtpCmd, pWtpStatus);
    if (IsOk) {
        IsOk = (pWtpStatus->Status == ACK && pWtpStatus->DLEN == 4);
        if (IsOk) {
            *pImageType = le32toh(*(uint32_t *)&pWtpStatus->Data[0]);
        }
    }

    return IsOk;
}

static int VerifyImage (struct CBinFileWtp *me, WTPSTATUS* pWtpStatus, BYTE AckOrNack)
{
    int IsOk;
    WTPCOMMAND WtpCmd = {VERIFYIMAGE,0,0x00,0x00,htole32(0x00000001),{AckOrNack}};

    dprintf("WtpCmd: %s ack=%d\n", __func__, AckOrNack);
    IsOk = SendWtpMessage(me, &WtpCmd, pWtpStatus);
    if (IsOk) {
        IsOk = (pWtpStatus->Status == ACK);
    }

    return IsOk;
}

static int DataHeader (struct CBinFileWtp *me, WTPSTATUS* pWtpStatus, unsigned int uiRemainingData, uint32_t *uiBufferSize)
{
    int IsOk;
    WTPCOMMAND WtpCmd = {DATAHEADER,++me->m_iSequence,0x00,0x00,htole32(4)};

    //dprintf("WtpCmd: %s uiRemainingData=%u\n", __func__, uiRemainingData);
    set_transfer_allbytes(uiRemainingData);

    WtpCmd.Flags = 0x4;
    *(uint32_t *)&WtpCmd.Data[0] = htole32(uiRemainingData);

    IsOk = SendWtpMessage(me, &WtpCmd, pWtpStatus);
    if (IsOk) {
        IsOk = (pWtpStatus->Status == ACK && pWtpStatus->DLEN == 4);
        if (IsOk) {
            *uiBufferSize = le32toh(*(uint32_t *)&pWtpStatus->Data[0]);
        }
    }

    return IsOk;
}

static int Data (struct CBinFileWtp *me, WTPSTATUS* pWtpStatus, uint8_t *pData,UINT32 Length, int isLastData, int fmode)
{
    int IsOk;

    IsOk = TRUE;

    WTPCOMMAND WtpCmd = {DATA,++me->m_iSequence,0x00,0x00,htole32(Length)};

    if (isLastData)
        dprintf("WtpCmd: %s Length=%u, isLastData=%d\n", __func__, Length, isLastData);

    if (fmode)
    {
       IsOk = WriteUSBWithNoRsp (me,pData,(DWORD)Length);
        if (IsOk) {
            if (isLastData) {
                IsOk = ReadUSB (me,pWtpStatus);
                if (IsOk) {
                    IsOk = (pWtpStatus->CMD == DATA && pWtpStatus->Status == ACK);
                }
            }
        }
    }
    else
    {
        memcpy(WtpCmd.Data,pData,Length);
        if (WriteUSB (me,&WtpCmd,8 + (DWORD)Length) == FALSE)
        {
            IsOk = FALSE;
        }
        else
        {
            if (ReadUSB (me,pWtpStatus) == FALSE)
            {
                IsOk = FALSE;
            }
        }
    }

    return IsOk;
}

static int Done (struct CBinFileWtp *me,WTPSTATUS* pWtpStatus)
{
    int IsOk;
    WTPCOMMAND WtpCmd = {DONE,0,0x00,0x00,htole32(0x00000000)};

    dprintf("WtpCmd: %s\n", __func__);
    IsOk = SendWtpMessage (me, &WtpCmd, pWtpStatus);
    if (IsOk) {
        IsOk = (pWtpStatus->Status == ACK);
    }

    return IsOk;
}

static int Disconnect (struct CBinFileWtp *me,WTPSTATUS* pWtpStatus)
{
    int IsOk;
    WTPCOMMAND WtpCmd = {DISCONNECT,0,0x00,0x00,htole32(0x00000000)};

    dprintf("WtpCmd: %s\n", __func__);
    IsOk = SendWtpMessage (me, &WtpCmd, pWtpStatus);
    if (IsOk) {
        IsOk = (pWtpStatus->Status == ACK);
    }

    return IsOk;
}

unsigned short GetPacketSize(void)
{
    unsigned short a = (((0x4000+63)/64) * 64)-8;
	return a;
}

UINT32 WtptpDownLoad_GetWTPTPMSGReturnCode(struct CBinFileWtp *me,WTPSTATUS *pWtpStatus)
{
    UINT32 ReturnCode = 0;
    int i   ;
    for ( i = 0; i < 4; i++)
    {
        ReturnCode |= pWtpStatus->Data[i+2] << (8 * i);
    }
    return ReturnCode;
}

static int FBF_h_bin_down_count;
static int WtptpDownLoad_DownloadImage(struct CBinFileWtp *me, WTPSTATUS *pWtpStatus, int DeviceType, struct fbf_file_params  *fbf_final_file_pa)
{
    UINT32 ImageType = 0;
    uint32_t uiBufferSize, lFileSize, uiRemainingBytes;
    const char *szFileName = NULL;
    BYTE* pSendDataBuffer;
    int fmode = 0;
    int i;

    SelectImage(me, pWtpStatus,  &ImageType);
    dprintf("\tImageType: %x\n", ImageType);

    if (ImageType == TIMH || ImageType == DKBI)
    {
        switch (ImageType) {
            case TIMH:
                if (DeviceType == 1)
                    szFileName = g_DkbTimPath;
                else if (DeviceType == 2)
                    szFileName = g_FbfTimPath;
            break;
            case DKBI:
                szFileName = g_DkbFilePath;
            break;
            default:
                dprintf("which file to download?");
                szFileName = NULL;
            break;
        }
    }
    else
    {
        UINT32 dwImageId;

        for (i=0;i<fbf_final_file_pa->FBF_h_bin_count;i++)
        {
            BinFileWtp_FseekBin(me,fbf_final_file_pa->fbf_fl[i].fbf_fi_info.FBF_h_bin,0L,SEEK_SET);
            BinFileWtp_ReadBinFile(me,&dwImageId, sizeof(UINT32), 1, fbf_final_file_pa->fbf_fl[i].fbf_fi_info.FBF_h_bin);
            dwImageId = le32toh(dwImageId);
            dprintf("%s dwImageId = %x\n", __func__, dwImageId);
            fbf_final_file_pa->fbf_fl[i].fbf_fi_info.ImageID = dwImageId;
        }

        for (i=0;i<fbf_final_file_pa->FBF_h_bin_count;i++)
        {
            if (fbf_final_file_pa->fbf_fl[i].fbf_fi_info.ImageID == ImageType)
            {
                szFileName = fbf_final_file_pa->fbf_fl[i].fbf_fi_info.FBF_h_bin;
                break;
            }
        }

        if (i == fbf_final_file_pa->FBF_h_bin_count)
        {
            dprintf("No matching imageid\n");
            return -1;
        }

        FBF_h_bin_down_count++;
    }

    if (!szFileName)
        return -1;

    dprintf("%s szFileName:%s\n", __func__, szFileName);

    lFileSize = BinFileWtp_GetFileSize(me, szFileName);
    if ((ImageType == TIMH) || (ImageType == PART))
    {
        //fseek (hFile,0L,SEEK_SET); // Set position to SOF
        BinFileWtp_FseekBin(me,szFileName, 0L, SEEK_SET);
        uiRemainingBytes = (unsigned int)lFileSize;
    }
    else
    {
        //fseek (hFile,4L,SEEK_SET); // Set position to SOF
        BinFileWtp_FseekBin(me,szFileName, 4L, SEEK_SET);
        uiRemainingBytes = (unsigned int)lFileSize - 4;
    }

    VerifyImage (me, pWtpStatus, ACK);

    if(kBootLoader ==  DeviceType
        || PLAT_EDEN_A0 ==  me->m_ePlatformType
        || PLAT_HELAN2 ==  me->m_ePlatformType
        || PLAT_ULC1  ==  me->m_ePlatformType
        || PLAT_HELAN3 ==  me->m_ePlatformType
        || PLAT_HELAN4== me->m_ePlatformType		)
    {
        //bFastDownload = TRUE;
        fmode = 1;
    }
    else
    {
        fmode = 0;
    }

    uiBufferSize = 0;
    DataHeader(me, pWtpStatus, uiRemainingBytes, &uiBufferSize);

    dprintf("\tFlags: %d, BufferSize is 0x%X\n", pWtpStatus->Flags,uiBufferSize);

    if(kBootRom == DeviceType && PLAT_EDEN_A0 ==  me->m_ePlatformType)
    {
        usleep(100000); //100ms
    }

    int loop =0;

    pSendDataBuffer = (BYTE*)malloc(sizeof(BYTE)*uiBufferSize);
    while (uiRemainingBytes > 0)
    {

        if (! fmode  && loop)
        {
            DataHeader(me, pWtpStatus, uiRemainingBytes, &uiBufferSize);

            //workaround for eden a0 bootrom failed
            if(kBootRom==  DeviceType && PLAT_EDEN_A0 ==  me->m_ePlatformType)
            {
                usleep(100000); //100ms
            }
        }
        loop++;

        if(uiBufferSize > uiRemainingBytes)
        {
            uiBufferSize = uiRemainingBytes;
        }
        uiRemainingBytes -= uiBufferSize; // Calculate remaining bytes

        size_t BytesRead = BinFileWtp_ReadBinFile(me,pSendDataBuffer, 1, uiBufferSize, szFileName);

        //if (!Data(me, pWtpStatus, pSendDataBuffer, BytesRead, uiRemainingBytes == 0)) return -1;

        unsigned int totalSize = 0;
        unsigned int mark = 0;

        // We do not split packets based on packetSize for FastDownload
        unsigned int packetSize = fmode ? uiBufferSize : GetPacketSize();
        totalSize = uiBufferSize;

        if (totalSize > packetSize)
        {
            do
            {
                if (!Data(me, pWtpStatus, pSendDataBuffer + mark, packetSize, uiRemainingBytes == 0, fmode))
                {
                    dprintf("%s 1 Data failed\n", __func__);
                    return -1;
                }

                mark += packetSize;
                totalSize-=packetSize;

                if (totalSize <= packetSize)
                    packetSize = totalSize;

            }
            while (totalSize > 0);

        }
        else
        {
            if (!Data(me, pWtpStatus, pSendDataBuffer, BytesRead, uiRemainingBytes == 0, fmode))
            {
                dprintf("%s 2 Data failed\n", __func__);
                return -1;
            }
        }
    }
    free(pSendDataBuffer);

    if (!Done(me, pWtpStatus)) return -1;

    if (ImageType == TIMH || ImageType == DKBI)
    {}
    else
    {
        do    //wait modem to ready
        {
            GetWtpMessage(me, pWtpStatus);
        }while(fbf_final_file_pa->FBF_h_bin_count > FBF_h_bin_down_count && WtptpDownLoad_GetWTPTPMSGReturnCode(me,pWtpStatus) != PlatformReady);
    }

    return 0;
}

EDeviceType WtptpDownLoad_GetDeviceType(struct CBinFileWtp *me,ePlatForm ePlatfromType,const char* strDate,const char* strVersion ,const char* strProcessor)
{
    int idx;
	for (idx = 0; idx < sizeof(gstFlatformBLInfoSet)/sizeof(gstFlatformBLInfoSet[0]); ++idx)
	{
		if (gstFlatformBLInfoSet[idx].ePlatFormType == ePlatfromType)
		{
			if((NULL != strstr(strDate, gstFlatformBLInfoSet[idx].strBLDate))
				&&(NULL != strstr(strProcessor,gstFlatformBLInfoSet[idx].strBLProcessor) )
				&&(NULL != strstr(strVersion,gstFlatformBLInfoSet[idx].strBLVersion)) )
			{
				return kBootLoader;
			}
			else
			{
				return kBootRom;
			}
		}
	}
	return kNone;
}

int WtptpDownLoad_GetDeviceBootType(struct CBinFileWtp *me, struct fbf_file_params  *fbf_file_pa, struct fbf_file_params  *fbf_final_file_pa)
{
    int i = 0;
    int k = 0;
    int device_type = 0;
    WTPSTATUS *pWtpStatus = &gWtpStatus;
    static unsigned int new_size = 0;
    static unsigned int total_size = 0;
    unsigned int swd_ddr_vendor_id = 0, swd_flash_id = 0x0;
    int dowmload_image_num = 2;    //default is 2

    while (i++ < dowmload_image_num) {
        me->m_iSequence = 0;

        SendPreamble(me, pWtpStatus);

        GetWtpMessage(me, pWtpStatus);

        GetVersion(me, pWtpStatus);

        dprintf("\tVersion: %c%c%c%c\n", pWtpStatus->Data[3],pWtpStatus->Data[2],pWtpStatus->Data[1],pWtpStatus->Data[0]);
        dprintf("\tDate: %08X\n",le32toh(*(uint32_t *)&pWtpStatus->Data[4]));
        dprintf("\tProcessor: %c%c%c%c\n",pWtpStatus->Data[11],pWtpStatus->Data[10],pWtpStatus->Data[9],pWtpStatus->Data[8]);
        dprintf("\tswd_ddr_vendor_id: %c%c%c%c\n",pWtpStatus->Data[15],pWtpStatus->Data[14],pWtpStatus->Data[13],pWtpStatus->Data[12]);
        dprintf("\tswd_ddr_size: %02x%02x%02x%02x\n",pWtpStatus->Data[19],pWtpStatus->Data[18],pWtpStatus->Data[17],pWtpStatus->Data[16]);
        dprintf("\tswd_flash_id : %02x%02x%02x%02x\n",pWtpStatus->Data[23],pWtpStatus->Data[22],pWtpStatus->Data[21],pWtpStatus->Data[20]);


        char swd_ddr_vendor_id_str[24];
        char swd_flash_id_str[24];
        memset(swd_ddr_vendor_id_str, 0, 24);
        memset(swd_flash_id_str, 0, 24);

        unsigned int swd_ddr_vendor_id_tmp = 0;
        swd_ddr_vendor_id_tmp = (pWtpStatus->Data[13] - 0x30) * 16 + (pWtpStatus->Data[12] - 0x30);
        swd_ddr_vendor_id = (swd_ddr_vendor_id_tmp << 8);
        swd_ddr_vendor_id = swd_ddr_vendor_id + pWtpStatus->Data[16];
        swd_flash_id = (pWtpStatus->Data[21] << 8) + pWtpStatus->Data[20];
        snprintf(swd_ddr_vendor_id_str, sizeof(swd_ddr_vendor_id_str), "%04x", swd_ddr_vendor_id);
        snprintf(swd_flash_id_str, sizeof(swd_flash_id_str), "%04x", swd_flash_id);
        dprintf("swd_ddr_vendor_id_str:0x%s  swd_flash_id_str:0x%s\n", swd_ddr_vendor_id_str, swd_flash_id_str);

        char *ptr = NULL;
        //memset(&fbf_final_file_pa, 0, sizeof(struct fbf_file_params));
        memset(fbf_final_file_pa, 0, sizeof(struct fbf_file_params));

        for(k = 0; k < fbf_file_pa->FBF_timheader_bin_count; k++)
        {
            ptr = strstr(fbf_file_pa->fbf_fl[k].FBF_timheader_bin, "0x");
            if (ptr != NULL)
            {
                if (!strncasecmp(ptr + 2, swd_ddr_vendor_id_str, 4))
                {
                    g_FbfTimPath = fbf_file_pa->fbf_fl[k].FBF_timheader_bin;
                    memmove(fbf_final_file_pa->fbf_fl[fbf_final_file_pa->FBF_timheader_bin_count].FBF_timheader_bin, fbf_file_pa->fbf_fl[k].FBF_timheader_bin, strlen(fbf_file_pa->fbf_fl[k].FBF_timheader_bin));
                    fbf_final_file_pa->FBF_timheader_bin_count += 1;
                }
            }
            else
            {
                g_FbfTimPath = fbf_file_pa->fbf_fl[k].FBF_timheader_bin;
                memmove(fbf_final_file_pa->fbf_fl[fbf_final_file_pa->FBF_timheader_bin_count].FBF_timheader_bin, fbf_file_pa->fbf_fl[k].FBF_timheader_bin, strlen(fbf_file_pa->fbf_fl[k].FBF_timheader_bin));
                fbf_final_file_pa->FBF_timheader_bin_count += 1;
            }
        }

        for(k = 0; k < fbf_file_pa->FBF_h_bin_count; k++)
        {
            ptr = strstr(fbf_file_pa->fbf_fl[k].fbf_fi_info.FBF_h_bin, "0x");
            if (ptr != NULL)
            {
                if (!strncasecmp(ptr + 2, swd_ddr_vendor_id_str, 4))
                {
                    //g_FbfFilePath = fbf_file_pa->fbf_fl[k].fbf_fi_info.FBF_h_bin;
                    memmove(fbf_final_file_pa->fbf_fl[fbf_final_file_pa->FBF_h_bin_count].fbf_fi_info.FBF_h_bin, fbf_file_pa->fbf_fl[k].fbf_fi_info.FBF_h_bin, strlen(fbf_file_pa->fbf_fl[k].fbf_fi_info.FBF_h_bin));
                    fbf_final_file_pa->FBF_h_bin_count += 1;
                }
            }
            else
            {
                memmove(fbf_final_file_pa->fbf_fl[fbf_final_file_pa->FBF_h_bin_count].fbf_fi_info.FBF_h_bin, fbf_file_pa->fbf_fl[k].fbf_fi_info.FBF_h_bin, strlen(fbf_file_pa->fbf_fl[k].fbf_fi_info.FBF_h_bin));
                fbf_final_file_pa->FBF_h_bin_count += 1;
            }
        }

        //dprintf("FBF head:      %24s  FBF: %s\n", __func__, g_FbfTimPath, g_FbfFilePath);

        char Processor[24];
        char Version[24];
        char Date[24];
        memset(Processor, 0, 24);
        memset(Version, 0, 24);
        memset(Date, 0, 24);

        sprintf(Version, "%c%c%c%c", pWtpStatus->Data[3], pWtpStatus->Data[2], pWtpStatus->Data[1], pWtpStatus->Data[0]);
        sprintf(Date, "%02x%02x%02x%02x", pWtpStatus->Data[7], pWtpStatus->Data[6], pWtpStatus->Data[5], pWtpStatus->Data[4]);
        sprintf(Processor, "%c%c%c%c", pWtpStatus->Data[11], pWtpStatus->Data[10], pWtpStatus->Data[9], pWtpStatus->Data[8]);

        const char* strProcessor,*strVersion,*strDate;
        strProcessor = Processor;
        strVersion = Version;
        strDate = Date;

        EDeviceType eBootDeviceType = WtptpDownLoad_GetDeviceType(me,me->m_ePlatformType,strDate,strVersion,strProcessor);
        if (eBootDeviceType == 1)
        {
            if (WtptpDownLoad_DownloadImage(me, pWtpStatus, eBootDeviceType, fbf_final_file_pa)) return -1;
        }
        else if (eBootDeviceType == 2)
        {
            if (WtptpDownLoad_DownloadImage(me, pWtpStatus, eBootDeviceType, fbf_final_file_pa)) return -1;
        }else
            dprintf("BootDeviceType error ...\n");

        device_type = eBootDeviceType;

        dowmload_image_num = fbf_final_file_pa->FBF_timheader_bin_count + fbf_final_file_pa->FBF_h_bin_count;
        if (dowmload_image_num == 0 && eBootDeviceType == 1)   //Indicates that it is in emergency download mode (3002), using DKB
        {
            dowmload_image_num = 2;
        }
        dprintf("%s dowmload_image_num = %d\n", __func__, dowmload_image_num);
    }

    if (device_type == 1)
    {
        Disconnect(me, pWtpStatus);
        return 10;
    }
    else if (device_type == 2)
    {
       total_size = 256*1024;
        update_transfer_bytes(total_size);
        while (1) {
            if (!GetWtpMessage (me, pWtpStatus))
            {
                dprintf ("GetWtpMessage failed before burning flash\n");
                return -1;
            }

            new_size += (32*1024); //12MB/120s
            if (new_size >= (256*1024)) {
                update_transfer_bytes(new_size);
                total_size += new_size;
                new_size = 0;
           }

            if (update_size) {
                if (update_size > total_size) {
                    update_transfer_bytes(update_size - total_size);
                    total_size = 0;
                }

                update_size = 0;
            }

            if (pWtpStatus->Data[0] == WTPTPNOTIFICATION) {
                uint32_t ReturnCode = le32toh(*(uint32_t *)&pWtpStatus->Data[2]);
                if (ReturnCode == PlatformReady || ReturnCode == PlatformDisconnect) {
                    break;
                }
            }

            if ((pWtpStatus->Flags & 0x1 ) != MESSAGEPENDING )
                usleep(500*1000);
        }

        Disconnect(me, pWtpStatus);
    }

    return 0;
}
