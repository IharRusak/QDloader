/******************************************************************************
  @file    asr1.c
  @brief   asr.

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

char *g_DkbTimPath;
char *g_DkbFilePath;
char *g_FbfTimPath;
char *g_FbfFilePath;
static struct CBinFileWtp gBinFileWtp;
struct fbf_file_params  fbf_file_pa_tmp;
struct fbf_file_params  fbf_final_file_pa_tmp;

static int my_fseek(FILE *f, long off, int whence)
{
    //dprintf("off=%lx, whe=%d\n", off, whence);
    return fseek(f, off, whence);
}

static size_t my_fread(void *destv, size_t size, size_t nmemb, FILE *f) {
    int ret = fread(destv, size, nmemb, f);
    return ret;
}

static int BinFileWtp_InitParameter2(struct CBinFileWtp *me, FILE *fw_fp)
{
    int nPosBuf;
    uint32_t uValue = 0, i;

    me->m_fBinFile = fw_fp;
    my_fseek(me->m_fBinFile, -1 * sizeof(uint32_t), SEEK_END);
    my_fread(&uValue, sizeof(uint32_t), 1, me->m_fBinFile);
    if (verbose) dprintf("Read from File Value is %x\n", uValue);
    uValue = le32toh(uValue);
    if (verbose) dprintf("Convert Value is %x\n", uValue);
    my_fseek(me->m_fBinFile, uValue, SEEK_SET);
    if (verbose) dprintf("Offset of Map is %u\n", uValue);

    my_fread(&uValue, sizeof(uint32_t), 1, me->m_fBinFile);
    uValue = le32toh(uValue);
    if (verbose) dprintf("Size of Map is %u\n", uValue);
    me->nSize = uValue;
    if (me->nSize >= 4) {
    }
    else
    {
        dprintf("only support image number greater than or equal to 4, but get %d\n", me->nSize);
        return 0;
    }

    for (i = 0; i < me->nSize; i++)
    {
        int j;
        char buffer[512];

        memset(buffer, 0, sizeof(buffer));

        my_fread(&uValue, sizeof(uint32_t), 1, me->m_fBinFile);
        uValue = le32toh(uValue);

        my_fread(buffer, 1, uValue, me->m_fBinFile);

        nPosBuf = 0;
        for (j = 0; j < uValue; j++)
        {
            if (buffer[j] != 0)
                me->FbfList[i][nPosBuf++] = buffer[j];
        }
        char asr_FbfList[256] = {0};
        me->FbfList[i][nPosBuf] = 0;
        while (nPosBuf > 0) {
            if (me->FbfList[i][nPosBuf] == '\\') {
                strcpy(asr_FbfList, &(me->FbfList[i][nPosBuf+1]));
                strcpy(me->FbfList[i], asr_FbfList);
            }
            nPosBuf--;
        }

        if (verbose) dprintf("Find image name:%s\n", me->FbfList[i]);
        my_fread(&uValue, sizeof(uint32_t), 1, me->m_fBinFile);
        me->m_mapFile2Struct[i].Head1 = le32toh(uValue);

        my_fread(&uValue, sizeof(uint32_t), 1, me->m_fBinFile);
        me->m_mapFile2Struct[i].Head2 = le32toh(uValue);

        my_fread(&uValue, sizeof(uint32_t), 1, me->m_fBinFile);
        me->m_mapFile2Struct[i].Mid1 = le32toh(uValue);

        my_fread(&uValue, sizeof(uint32_t), 1, me->m_fBinFile);
        me->m_mapFile2Struct[i].Mid2 = le32toh(uValue);

        my_fread(&uValue, sizeof(uint32_t), 1, me->m_fBinFile);
        me->m_mapFile2Struct[i].Tail1 = le32toh(uValue);

        my_fread(&uValue, sizeof(uint32_t), 1, me->m_fBinFile);
        me->m_mapFile2Struct[i].Tail2 = le32toh(uValue);
        /*dprintf("%-24s Head={%08x, %08x}, Mid={%08x, %08x}, Tail={%08x, %08x}\n",
            me->FbfList[i],
            me->m_mapFile2Struct[i].Head1, me->m_mapFile2Struct[i].Head2,
            me->m_mapFile2Struct[i].Mid1, me->m_mapFile2Struct[i].Mid2,
            me->m_mapFile2Struct[i].Tail1, me->m_mapFile2Struct[i].Tail2);*/
    }

    return 1;
}

int BinFileWtp_FseekBin(struct CBinFileWtp *me, const char *pszBinFile, long _Offset, int _Origin)
{
    int ret;
    //uint32_t kfs, kfp;
    uint32_t i;
    FileStruct *pFile = NULL;
    FilePoint  *pFilePoint = NULL;

    //dprintf("%s file=%s, offst=%ld, orig=%d\n", __func__, pszBinFile, _Offset, _Origin);
    for (i = 0; i < me->nSize; i++)
    {
        if (!strcmp(me->FbfList[i], pszBinFile)) {
            pFile = &me->m_mapFile2Struct[i];
            pFilePoint = &me->m_mapFile2Point[i];
        }
    }

    if (!pFile) {
        dprintf("%s fail to find %s\n", __func__, pszBinFile);
        return -1;
    }

    if (!pFilePoint) {
        dprintf("%s fail to find %s\n", __func__, pszBinFile);
        return -1;
    }

	//kfp = kh_get(str2n, me->m_mapFile2PointPos, pszBinFile);
	//kfs = kh_get(str2fs, me->m_mapFile2Struct, pszBinFile);
	//if (kfp == kh_end(me->m_mapFile2PointPos) || kfs == kh_end(me->m_mapFile2Struct))
	//	return -1;
	if (SEEK_SET == _Origin)
	{
		ret = my_fseek(me->m_fBinFile, pFile->Head1 + _Offset, SEEK_SET);
        pFilePoint->Head1_offset = pFile->Head1 + _Offset;
	}
	else if (SEEK_END == _Origin)
	{
		ret = my_fseek(me->m_fBinFile, pFile->Tail2 + _Offset, SEEK_SET);
        pFilePoint->Tail2_offset = pFile->Tail2 + _Offset;   //This branch will not enter
	}
	else
	{
	    dprintf("%s unspport SEEK_CUR\n", __func__);
        ret = -1;
		//fseek(me->m_fBinFile, kh_value(me->m_mapFile2PointPos, kfp) + _Offset, SEEK_SET);
	}
	return ret;
}

size_t BinFileWtp_ReadBinFile(struct CBinFileWtp *me, void *buffer, size_t size, size_t count, const char *pszBinFile)
{   
    uint32_t i;
    uint32_t ret = 0;
    uint32_t uRemain = 0;
    FileStruct *pFile = NULL;
    FilePoint  *pFilePoint = NULL;

    uRemain = size * count;

    for (i = 0; i < me->nSize; i++)
    {
        if (!strcmp(me->FbfList[i], pszBinFile)) {
            pFile = &me->m_mapFile2Struct[i];
            pFilePoint = &me->m_mapFile2Point[i];
        }
    }

    if (!pFile) {
        dprintf("%s fail to find %s\n", __func__, pszBinFile);
        return ret;
    }

    if (!pFilePoint) {
        dprintf("%s fail to find %s\n", __func__, pszBinFile);
        return ret;
    }

    if (0 == pFile->Head2 && 0 == pFile->Mid1 && 0 == pFile->Mid2 && 0 == pFile->Tail1)  //read FBF_timheader.bin
    {
        fseek(me->m_fBinFile, pFilePoint->Head1_offset, SEEK_SET);
		ret = fread(buffer, size, count, me->m_fBinFile);
		pFilePoint->Head1_offset += size * count;
    }
    else         //Read FBF_h_bin in 3 segments
    {
        fseek(me->m_fBinFile, pFilePoint->Head1_offset, SEEK_SET);
        if (pFilePoint->Head1_offset >= pFile->Head1 && pFilePoint->Head1_offset < pFile->Head2)
		{
			if (uRemain < pFile->Head2 - pFilePoint->Head1_offset)
			{
				ret += fread(buffer, sizeof(BYTE), uRemain, me->m_fBinFile);
				pFilePoint->Head1_offset += uRemain;
				uRemain = 0;
			}
			else
			{
				ret += fread(buffer, sizeof(BYTE), pFile->Head2 - pFilePoint->Head1_offset, me->m_fBinFile);
				uRemain -= (pFile->Head2 - pFilePoint->Head1_offset);
				buffer = (void *)((char *)buffer + pFile->Head2 - pFilePoint->Head1_offset);
				pFilePoint->Head1_offset = pFile->Mid1;
				fseek(me->m_fBinFile, pFilePoint->Head1_offset, SEEK_SET);
			}
		}
		if (pFilePoint->Head1_offset >= pFile->Mid1 && pFilePoint->Head1_offset < pFile->Mid2)
		{
			if (uRemain < pFile->Mid2 - pFilePoint->Head1_offset)
			{
				ret += fread(buffer, sizeof(BYTE), uRemain, me->m_fBinFile);
				pFilePoint->Head1_offset += uRemain;
				uRemain = 0;
			}
			else
			{
				ret += fread(buffer, sizeof(BYTE), pFile->Mid2 - pFilePoint->Head1_offset, me->m_fBinFile);
				uRemain -= (pFile->Mid2 - pFilePoint->Head1_offset);
				buffer = (void *)((char *)buffer + pFile->Mid2 - pFilePoint->Head1_offset);
				pFilePoint->Head1_offset = pFile->Tail1;
				fseek(me->m_fBinFile, pFilePoint->Head1_offset, SEEK_SET);
			}
		}
		if (pFilePoint->Head1_offset >= pFile->Tail1 && pFilePoint->Head1_offset < pFile->Tail2)
		{
			ret += fread(buffer, sizeof(BYTE), uRemain, me->m_fBinFile);
			pFilePoint->Head1_offset += uRemain;
			uRemain = 0;
		}
    }
    
    return ret;
}

uint32_t BinFileWtp_GetFileSize(struct CBinFileWtp *me, const char *pszBinFile)
{
    uint32_t i;
    FileStruct *pFile = NULL;

    //dprintf("%s file=%s\n", __func__, pszBinFile);
    for (i = 0; i < me->nSize; i++)
    {
        if (!strcmp(me->FbfList[i], pszBinFile)) {
            pFile = &me->m_mapFile2Struct[i];
        }
    }

    if (!pFile) {
        dprintf("%s fail to find %s\n", __func__, pszBinFile);
        return -1;
    }

    i = pFile->Head2 - pFile->Head1 + pFile->Mid2 - pFile->Mid1 +pFile->Tail2 - pFile->Tail1;

	return i;
}

static void EndianConvertMasterBlockHeader(MasterBlockHeader *mbHeader)
{
	int i;
	for (i = 0; i < NUM_OF_SUPPORTED_FLASH_DEVS; i++){
		mbHeader->Flash_Device_Spare_Area_Size[i] = le16toh(mbHeader->Flash_Device_Spare_Area_Size[i]);
	}
	mbHeader->Format_Version = le16toh(mbHeader->Format_Version);
	mbHeader->Size_of_Block = le16toh(mbHeader->Size_of_Block);
	mbHeader->Bytes_To_Program = le32toh(mbHeader->Bytes_To_Program);
	mbHeader->Bytes_To_Verify = le32toh(mbHeader->Bytes_To_Verify);
	mbHeader->Number_of_Bytes_To_Erase = le32toh(mbHeader->Number_of_Bytes_To_Erase);
	mbHeader->Main_Commands = le32toh(mbHeader->Main_Commands);
	mbHeader->nOfDevices = le32toh(mbHeader->nOfDevices);
	mbHeader->DLerVersion = le32toh(mbHeader->DLerVersion);
	for (i = 0; i < MAX_NUMBER_OF_FLASH_DEVICES_IN_MASTER_HEADER; i++){
		mbHeader->deviceHeaderOffset[i] = le32toh(mbHeader->deviceHeaderOffset[i]);
	}
}

static void EndianConvertDeviceHeader_V11(DeviceHeader_V11 *pDeviceHeader)
{
	int i;
	pDeviceHeader->DeviceFlags = le32toh(pDeviceHeader->DeviceFlags);
	for (i = 0; i < 16; i++){
		pDeviceHeader->DeviceParameters[i] = le32toh(pDeviceHeader->DeviceParameters[i]);
	}
	pDeviceHeader->FlashOpt.EraseAll = le32toh(pDeviceHeader->FlashOpt.EraseAll);
	pDeviceHeader->FlashOpt.ResetBBT = le32toh(pDeviceHeader->FlashOpt.ResetBBT);
	pDeviceHeader->FlashOpt.NandID = le32toh(pDeviceHeader->FlashOpt.NandID);
	for (i = 0; i < MAX_RESEVERD_LEN-1; i++){
		pDeviceHeader->FlashOpt.Reserved[i] = le32toh(pDeviceHeader->FlashOpt.Reserved[i]);
	}
	pDeviceHeader->FlashOpt.Skip_Blocks_Struct.Total_Number_Of_SkipBlocks = le32toh(pDeviceHeader->FlashOpt.Skip_Blocks_Struct.Total_Number_Of_SkipBlocks);
	for (i = 0; i < MAX_NUM_SKIP_BLOCKS; i++){
		pDeviceHeader->FlashOpt.Skip_Blocks_Struct.Skip_Blocks[i] = le32toh(pDeviceHeader->FlashOpt.Skip_Blocks_Struct.Skip_Blocks[i]);
	}
	pDeviceHeader->ProductionMode = le32toh(pDeviceHeader->ProductionMode);
	for (i = 0; i < MAX_RESEVERD_LEN-2; i++){
		pDeviceHeader->Reserved[i] = le32toh(pDeviceHeader->Reserved[i]);
	}
	pDeviceHeader->nOfImages = le32toh(pDeviceHeader->nOfImages);
	for (i = 0; i < MAX_NUMBER_OF_IMAGE_STRUCTS_IN_DEVICE_HEADER; i++) {
        int j;

        pDeviceHeader->imageStruct[i].Image_ID = le32toh(pDeviceHeader->imageStruct[i].Image_ID);
		pDeviceHeader->imageStruct[i].Image_In_TIM = le32toh(pDeviceHeader->imageStruct[i].Image_In_TIM);
		pDeviceHeader->imageStruct[i].Flash_partition = le32toh(pDeviceHeader->imageStruct[i].Flash_partition);
		pDeviceHeader->imageStruct[i].Flash_erase_size = le32toh(pDeviceHeader->imageStruct[i].Flash_erase_size);
		pDeviceHeader->imageStruct[i].commands = le32toh(pDeviceHeader->imageStruct[i].commands);
		pDeviceHeader->imageStruct[i].First_Sector = le32toh(pDeviceHeader->imageStruct[i].First_Sector);
		pDeviceHeader->imageStruct[i].length = le32toh(pDeviceHeader->imageStruct[i].length);
		pDeviceHeader->imageStruct[i].Flash_Start_Address = le32toh(pDeviceHeader->imageStruct[i].Flash_Start_Address);
		for (j = 0; j < MAX_RESEVERD_LEN; j++){
			pDeviceHeader->imageStruct[i].Reserved[j] = le32toh(pDeviceHeader->imageStruct[i].Reserved[j]);
		}
		pDeviceHeader->imageStruct[i].ChecksumFormatVersion2 = le32toh(pDeviceHeader->imageStruct[i].ChecksumFormatVersion2);
	}
}

static int InitInstanceParams(struct CBinFileWtp *me, PInstanceParams lpInstanceParam, struct fbf_file_params  *fbf_file_pa) {
    int i;
    char *strFile;
    //memset(&fbf_file_pa, 0, sizeof(struct fbf_file_params));

    for (i = 0; i < me->nSize; i++)
    {
        strFile = me->FbfList[i];
        //dprintf("Init %s\n", me->FbfList[i]);

    	if (strstr(strFile, "DKB_timheader.bin") != NULL || strstr(strFile, "DKB_ntimheader.bin") != NULL)
    	    g_DkbTimPath = strFile;
        else if (strstr(strFile,"Dkb.bin"))
    	    g_DkbFilePath = strFile;
    	else if (strstr(strFile, "FBF_timheader.bin") != NULL || strstr(strFile, "FBF_Ntimheader.bin") != NULL)
        {
            memmove(fbf_file_pa->fbf_fl[fbf_file_pa->FBF_timheader_bin_count].FBF_timheader_bin, strFile, strlen(strFile));
            fbf_file_pa->FBF_timheader_bin_count += 1;
            g_FbfTimPath = strFile;
        }
        else if (strstr(strFile,"FBF") != NULL && strstr(strFile,"_h.bin") != NULL)   //include FBF_h_bin  FBF0_h.bin  0x08080xFFFF_FBF_h.bin 0x08080xFFFF_FBF1_h.bin
        {
            memmove(fbf_file_pa->fbf_fl[fbf_file_pa->FBF_h_bin_count].fbf_fi_info.FBF_h_bin, strFile, strlen(strFile));
            fbf_file_pa->FBF_h_bin_count += 1;
            g_FbfFilePath = strFile;
        }
        else
        {
            dprintf("Un-support file %s\n", me->FbfList[i]);
            return 0;
        }
    }

    lpInstanceParam->pszDKBTim = g_DkbTimPath;
    lpInstanceParam->pszDKBbin = g_DkbFilePath;

    assert((g_FbfFilePath != NULL));

    BinFileWtp_FseekBin(me, g_FbfFilePath, 4L, SEEK_SET);
    BinFileWtp_ReadBinFile(me,&me->masterBlkHeader,sizeof(MasterBlockHeader),1, g_FbfFilePath);
    EndianConvertMasterBlockHeader(&me->masterBlkHeader);
    //dprintf("MasterBlkHeader.SizeOfBlock=%x\n",me->masterBlkHeader.Size_of_Block);

    BinFileWtp_FseekBin(me, g_FbfFilePath, me->masterBlkHeader.deviceHeaderOffset[0] + 4,SEEK_SET);
    BinFileWtp_ReadBinFile(me,&me->deviceHeaderBuf,sizeof(DeviceHeader_V11), 1, g_FbfFilePath);
    EndianConvertDeviceHeader_V11(&me->deviceHeaderBuf);
    //dprintf("deviceHeaderBuf.DeviceFlags=%x\n",me->deviceHeaderBuf.DeviceFlags);

    lpInstanceParam->PlaformType = me->deviceHeaderBuf.ChipID;
    if (lpInstanceParam->PlaformType != PLAT_NEZHA3) {
       dprintf("only support PLAT_NEZHA3\n");
       return 0;
    }
    lpInstanceParam->Commands |= (me->deviceHeaderBuf.FlashOpt.EraseAll?1:0);
    lpInstanceParam->Commands |= (me->deviceHeaderBuf.FlashOpt.ResetBBT? (1<<2):0);
    if (me->deviceHeaderBuf.FlashOpt.NandID)
    {
        lpInstanceParam->FlashType = 0;
    }
    /* Note: the BBCS and CRCS on/off are controlled by the blf via which fbf file was created.
        Here for fbfdownloader pc side, just assume both are ON in order to avoid extract the information from TIMH.
        The CallbackProc only try to read the infos when BBCS or CRCS success although the on/off at PC are always set to ON
        lpInstanceParam->GetBadBlk = 1;
        lpInstanceParam->ImageCrcEn = 1;
    */
    lpInstanceParam->GetBadBlk = me->deviceHeaderBuf.BBCS_EN;
    lpInstanceParam->ImageCrcEn = me->deviceHeaderBuf.CRCS_EN;

    return 0;
}

static BOOL WtptpDownLoad_ParseTim (struct CBinFileWtp *me, const char *strTimFile, const char *strImageFile)
{
    UINT32 dwTimId;
    UINT32 dwImageId;
    CTIM TimHeader;
    uint32_t Version;
    int i, nImages;

    BinFileWtp_FseekBin(me, strTimFile, sizeof(UINT32), SEEK_SET);
    BinFileWtp_ReadBinFile(me,&dwTimId,sizeof(UINT32) , 1, strTimFile);
    dwTimId = le32toh(dwTimId);
    dprintf("dwTimId: %x\n", dwTimId);
    if (TIMH != dwTimId) {
        return 0;
    }

    BinFileWtp_FseekBin(me, strTimFile, 0L, SEEK_SET);
    BinFileWtp_ReadBinFile(me, &TimHeader, sizeof(CTIM), 1, strTimFile);
    nImages = le32toh(TimHeader.NumImages);
    dprintf("Number of Images listed in TIM: %d\n",nImages);
    if (nImages < 2) {
        dprintf("Number of Images is unknown:%d\n", nImages);
        return 0;
    }

    dprintf("\t{Version = %x, Identifier = %x, Trusted = %x, IssueDate = %x, OEMUniqueID = %x}\n",
        TimHeader.VersionBind.Version, TimHeader.VersionBind.Identifier, TimHeader.VersionBind.Trusted,
        TimHeader.VersionBind.IssueDate, TimHeader.VersionBind.OEMUniqueID);

    Version = le32toh(TimHeader.VersionBind.Version);
    if (Version >= TIM_3_1_01 && le32toh(TimHeader.VersionBind.Version) < TIM_3_2_00)
    {
        IMAGE_INFO_3_1_0 ImageInfo;
        for (i = 0; i < nImages; i++)
        {
            BinFileWtp_ReadBinFile(me,&ImageInfo,sizeof(IMAGE_INFO_3_1_0),1,strTimFile);
        }
    }
    else if (Version >= TIM_3_2_00 && Version < TIM_3_4_00)
    {
        IMAGE_INFO_3_2_0 ImageInfo;
        for (i = 0; i < nImages; i++)
        {
            BinFileWtp_ReadBinFile(me,&ImageInfo,sizeof(IMAGE_INFO_3_2_0),1,strTimFile);
        }
    }
    else if(TIM_3_4_00 == Version)
    {
        IMAGE_INFO_3_4_0 ImageInfo;
        for (i = 0; i < nImages; i++)
        {
            BinFileWtp_ReadBinFile(me,&ImageInfo,sizeof(IMAGE_INFO_3_4_0),1,strTimFile);

            dprintf("\t{ImageID = %x, NextImageID = %x, FlashEntryAddr = %x, LoadAddr = %x, ImageSize = %x}\n",
                ImageInfo.ImageID, ImageInfo.NextImageID, ImageInfo.FlashEntryAddr, ImageInfo.LoadAddr, ImageInfo.ImageSize);
        }
    }
    else if(TIM_3_5_00 == le32toh(TimHeader.VersionBind.Version))
    {
        IMAGE_INFO_3_5_0 ImageInfo;
        for (i = 0; i < nImages; i++)
        {
            BinFileWtp_ReadBinFile(me,&ImageInfo,sizeof(IMAGE_INFO_3_5_0),1,strTimFile);
        }
    }
    else
    {
        dprintf("Tim Version is unknown:0X%X\n", TimHeader.VersionBind.Version);
        return FALSE;
    }

    BinFileWtp_FseekBin(me, strImageFile,0L,SEEK_SET);
    BinFileWtp_ReadBinFile(me,&dwImageId, sizeof(UINT32), 1, strImageFile);
    dwImageId = le32toh(dwImageId);
    dprintf("dwImageId: %x\n", dwImageId);

    return 1;
}

static BOOL WtptpDownLoad_InitializeBL(struct CBinFileWtp *me,PInstanceParams pInstParam, struct fbf_file_params  *fbf_file_pa) {
    int i;
    //me->m_nFlashDataPageSize = pInstParam->FlashPageSize;
    me->m_ePlatformType = (ePlatForm)pInstParam->PlaformType;
    //me->m_FlashType = (eFlashType)pInstParam->FlashType;

    dprintf("Parse DKB Tim image.\n");
    WtptpDownLoad_ParseTim(me, g_DkbTimPath, g_DkbFilePath);
    dprintf("Parse FBF Tim image.\n");

    for(i = 0; i < fbf_file_pa->FBF_timheader_bin_count; i++)
    {
        WtptpDownLoad_ParseTim(me, fbf_file_pa->fbf_fl[i].FBF_timheader_bin, fbf_file_pa->fbf_fl[i].fbf_fi_info.FBF_h_bin);
    }

    return 0;
}

int fbfdownloader_main(int usbfd, FILE *fw_fp, const char *modem_name)
{
    struct CBinFileWtp *me = &gBinFileWtp;
    struct fbf_file_params  *fbf_file_pa = &fbf_file_pa_tmp;
    struct fbf_file_params  *fbf_final_file_pa = &fbf_final_file_pa_tmp;
    
    memset(fbf_file_pa, 0, sizeof(struct fbf_file_params));
    
    int ret;

   me->usbfd = usbfd;
   BinFileWtp_InitParameter2(me, fw_fp);
   InitInstanceParams(me, &me->instanceParam, fbf_file_pa);
   WtptpDownLoad_InitializeBL(me, &me->instanceParam, fbf_file_pa);
   ret =  WtptpDownLoad_GetDeviceBootType(me, fbf_file_pa, fbf_final_file_pa);

   return ret;
}
