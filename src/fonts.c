#include <ets_sys.h>
#include "fonts/font_10_20_52F.h"
#include "fonts/font_10b_20_52F.h"
#include "fonts/font_13_20_52F.h"
#include "fonts/font_13b_20_52F.h"
#include "fonts/font_10_530_33FF.h"
#include "fonts/font_10b_530_33FF.h"
#include "fonts/font_13_530_33FF.h"
#include "fonts/font_13b_530_33FF.h"
#include "fonts/font_13_4E00_9FA5.h"
#include "fonts/font_13b_4E00_9FA5.h"
#include "fonts/font_10_AC00_D7A3.h"
#include "fonts/font_10b_AC00_D7A3.h"
#include "fonts/font_13_AC00_D7A3.h"
#include "fonts/font_13b_AC00_D7A3.h"
#include "fonts/font_10_E801_FFEE.h"
#include "fonts/font_10b_E801_FFEE.h"
#include "fonts/font_13_E801_FFEE.h"
#include "fonts/font_13b_E801_FFEE.h"
#include "fonts/font_10_FFFC_FFFE.h"
#include "fonts/font_13_FFFC_FFFE.h"
#include "fonts.h"
#include "common.h"

#define FLASH_OFFSET    0x40200000UL
#define BLOCK(block)    (const uint*)((uint)block-FLASH_OFFSET)


const uint *arial10_blocks[] = {
    BLOCK(font_10_20_52F),
	BLOCK(font_10_530_33FF),
    BLOCK(font_13_4E00_9FA5),	// CJK, size 13 only
    BLOCK(font_10_AC00_D7A3),
    BLOCK(font_10_E801_FFEE),
    BLOCK(font_10_FFFC_FFFE)};
const Font arial10 = {arial10_blocks, NELEMENTS(arial10_blocks)};

const uint *arial10b_blocks[] = {
	BLOCK(font_10b_20_52F),
	BLOCK(font_10b_530_33FF),
	BLOCK(font_13b_4E00_9FA5),	// CJK, size 13 only
	BLOCK(font_10b_AC00_D7A3),
	BLOCK(font_10b_E801_FFEE),
	BLOCK(font_10_FFFC_FFFE)};
const Font arial10b = {arial10b_blocks, NELEMENTS(arial10b_blocks)};


const uint *arial13_blocks[] = {
	BLOCK(font_13_20_52F),
	BLOCK(font_13_530_33FF),
	BLOCK(font_13_4E00_9FA5),
	BLOCK(font_13_AC00_D7A3),
	BLOCK(font_13_FFFC_FFFE),
	BLOCK(font_13_E801_FFEE)};
const Font arial13 = {arial13_blocks, NELEMENTS(arial13_blocks)};

const uint *arial13b_blocks[] = {
	BLOCK(font_13b_20_52F),
	BLOCK(font_13b_530_33FF),
	BLOCK(font_13b_4E00_9FA5),
	BLOCK(font_13b_AC00_D7A3),
	BLOCK(font_13_FFFC_FFFE),
	BLOCK(font_13b_E801_FFEE)};
const Font arial13b = {arial13b_blocks, NELEMENTS(arial13b_blocks)};
