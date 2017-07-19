#ifndef INCLUDE_FONTS_H_
#define INCLUDE_FONTS_H_

#include "typedefs.h"

typedef struct Font Font;
struct Font
{
	const uint **blocks;
	int count;
};

extern const Font arial10;
extern const Font arial10b;

extern const Font arial13;
extern const Font arial13b;


//#define REPLACEMENT_CHAR	0xFFFD
#define REPLACEMENT_CHAR	' '
#define LINK_PICTOGRAM_CODE	0xFFFE



#endif /* INCLUDE_FONTS_H_ */
