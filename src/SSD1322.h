#ifndef INCLUDE_SSD1322_H_
#define INCLUDE_SSD1322_H_

#include "typedefs.h"
#include "display.h"

#define RST_GPIO		5
#define RST_GPIO_MUX	PERIPHS_IO_MUX_GPIO5_U
#define RST_GPIO_FUNC	FUNC_GPIO5

#define CONTRAST_LEVEL_INIT		0

void SSD1322_init(void);
void SSD1322_setStartLine(uchar line);
void SSD1322_setOnOff(DispState state);
void SSD1322_setContrast(uchar value);
void SSD1322_partialDispEn(uchar startRow, uchar endRow);
void SSD1322_partialDispDis(void);
void SSD1322_setRemap(uchar paramA, uchar paramB);



#endif /* INCLUDE_SSD1322_H_ */
