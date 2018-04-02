#ifndef SRC_SH1106_H_
#define SRC_SH1106_H_

#include "typedefs.h"
#include "display.h"

typedef int (*initDoneFunc)(void);
int SH1106_init(int nonblock, initDoneFunc initCb, Orientation orientation, int setDispOn);
int SSD1306_init(int nonblock, initDoneFunc initCb, Orientation orientation, int setDispOn);
int SH1106_SSD1306_setOnOff(DispState state);
int SH1106_SSD1306_setContrast(uchar value);

typedef enum {
	eNormalDir = 0xA0,
	eReverseDir = 0xA1
}SegmentRemap;
int SH1106_SSD1306_setSegmentRemap(SegmentRemap remap);

typedef enum{
	eScanFrom0 = 0xC0,
	eScanTo0 = 0xC8
}ComOutScanDir;
int SH1106_SSD1306_setComOutScanDir(ComOutScanDir scanDir);
int SH1106_SSD1306_setDispVoffset(uchar offset);

void SH1106_SSD1306_cpyMemBuf(uchar *mem, int memWidth, int memRow, uchar dispPage, int pages);



#endif /* SRC_SH1106_H_ */
