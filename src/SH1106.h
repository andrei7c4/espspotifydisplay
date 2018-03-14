#ifndef SRC_SH1106_H_
#define SRC_SH1106_H_

#include "typedefs.h"
#include "display.h"

typedef void (*initDoneFunc)(void);
int SH1106_init(int nonblock, initDoneFunc initCb, int setDispOn);
int SH1106_setOnOff(DispState state);
int SH1106_setContrast(uchar value);

typedef enum {
	eNormalDir = 0xA0,
	eReverseDir = 0xA1
}SegmentRemap;
int SH1106_setSegmentRemap(SegmentRemap remap);

typedef enum{
	eScanFrom0 = 0xC0,
	eScanTo0 = 0xC8
}ComOutScanDir;
int SH1106_setComOutScanDir(ComOutScanDir scanDir);

void SH1106_cpyMemBuf(uchar *mem, int memWidth, int memRow, uchar dispPage, int pages);


#endif /* SRC_SH1106_H_ */
