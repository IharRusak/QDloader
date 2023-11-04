LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)
LOCAL_SRC_FILES:= \
./dloader/pac.c ./dloader/nv.c ./dloader/test.c ./dloader/crc.c ./dloader/xml.c \
./FBFDownloader/WtptpDownloader.c ./FBFDownloader/asr1.c \
./aboot/ethos.c ./aboot/uip.c ./aboot/aboot.c \
./usbfs/usbfs.c main.c percent.c
LOCAL_CFLAGS += -pie -fPIE -Wall -Wno-strict-aliasing
LOCAL_LDFLAGS += -pie -fPIE
LOCAL_MODULE_TAGS:= optional
LOCAL_MODULE:= QDloader
include $(BUILD_EXECUTABLE)
