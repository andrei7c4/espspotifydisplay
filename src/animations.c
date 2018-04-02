#include <osapi.h>
#include <os_type.h>
#include "graphics.h"
#include "display.h"
#include "animations.h"
#include "config.h"

#define VSCROLL_INTERVAL		20
#define HSCROLL_INTERVAL		50

LOCAL os_timer_t scrollTmr;
LOCAL int isScrolling = FALSE;

extern int curTrackIsPlaying(void);


LOCAL void hScrollStart(void);
LOCAL void hScrollContinue(void);
LOCAL void ICACHE_FLASH_ATTR scrollTmrCb(void (*scrollStep)(Label *))
{
	int updateTitle = TitleLabel.scrollEn;
	int updateArtist = ArtistLabel.scrollEn;
	int updateAlbum = AlbumLabel.scrollEn;
	if (TitleLabel.scrollEn)
	{
		scrollStep(&TitleLabel);
	}
	if (ArtistLabel.scrollEn)
	{
		scrollStep(&ArtistLabel);
	}
	if (AlbumLabel.scrollEn)
	{
		scrollStep(&AlbumLabel);
	}
	dispUpdateLabels(updateTitle, updateArtist, updateAlbum);

	if (!TitleLabel.scrollEn && !ArtistLabel.scrollEn && !AlbumLabel.scrollEn)
	{
		scrollStop();		// scroll round done
		if (config.scrollMode & eHScroll)
		{
			if (TitleLabel.scrollInt || ArtistLabel.scrollInt || AlbumLabel.scrollInt)
			{
				hScrollContinue();		// continue interrupted horizontal scroll
			}
			else if (curTrackIsPlaying())
			{
				hScrollStart();		// start a new scroll
			}
		}
	}
}

LOCAL void ICACHE_FLASH_ATTR vScrollStep(Label *label)
{
	int	y = label->offset;
	int width = MainGfxBuf.memWidth;
	uchar *dst = MainGfxBuf.buf + (y*width);
	uchar *src = dst + width;
	for (; y < (label->offset + label->buf.height - 1); y++, src += width, dst += width)
	{
		os_memcpy(dst, src, width);
	}
	os_memcpy(dst, label->buf.buf + (label->vScrollY*label->buf.memWidth), width);
	label->vScrollY++;
	if (label->vScrollY >= label->buf.height)
	{
		label->scrollEn = FALSE;
	}
}

void ICACHE_FLASH_ATTR vScrollStart(int scrollTitle, int scrollArtist, int scrollAlbum)
{
	if (scrollTitle || scrollArtist || scrollAlbum)
	{
		// check if we are interrupting horizontal scroll
		TitleLabel.scrollInt = (TitleLabel.scrollEn && !scrollTitle);
		ArtistLabel.scrollInt = (ArtistLabel.scrollEn && !scrollArtist);
		AlbumLabel.scrollInt = (AlbumLabel.scrollEn && !scrollAlbum);

		TitleLabel.scrollEn = scrollTitle;
		TitleLabel.vScrollY = 0;
		ArtistLabel.scrollEn = scrollArtist;
		ArtistLabel.vScrollY = 0;
		AlbumLabel.scrollEn = scrollAlbum;
		AlbumLabel.vScrollY = 0;
		os_timer_disarm(&scrollTmr);
		os_timer_setfn(&scrollTmr, (os_timer_func_t *)scrollTmrCb, vScrollStep);
		os_timer_arm(&scrollTmr, VSCROLL_INTERVAL, 1);
		isScrolling = TRUE;
	}
}




LOCAL void ICACHE_FLASH_ATTR hScrollStep(Label *label)
{
	uchar *mainBuf = MainGfxBuf.buf + (label->offset*MainGfxBuf.memWidth);
	uchar *scrBuf = label->buf.buf + label->hScrollX;
	int x, y;

	for (y = 0; y < label->buf.height; y++, scrBuf += label->buf.memWidth)
	{
		for (x = 0; x < (MainGfxBuf.memWidth-1); x++, mainBuf++)
		{
			*mainBuf <<= 1;
			*mainBuf |= ((*(mainBuf+1) >> 7) & 1);
		}
		*mainBuf <<= 1;
		*mainBuf |= ((*scrBuf >> label->hScrollBit) & 1);
		mainBuf++;
	}

	label->hScrollBit--;
	if (label->hScrollBit < 0)
	{
		label->hScrollBit = 7;
		label->hScrollX++;
		if (label->hScrollX >= label->buf.memWidth)
		{
			label->hScrollX = 0;

		}
		else if (label->hScrollX == MainGfxBuf.memWidth)
		{
			label->scrollEn = FALSE;
		}
	}
}

LOCAL void ICACHE_FLASH_ATTR hScrollStep1(void)
{
	if (TitleLabel.scrollEn)
	{
		TitleLabel.hScrollX = MainGfxBuf.memWidth;
		TitleLabel.hScrollBit = 7;
	}
	if (ArtistLabel.scrollEn)
	{
		ArtistLabel.hScrollX = MainGfxBuf.memWidth;
		ArtistLabel.hScrollBit = 7;
	}
	if (AlbumLabel.scrollEn)
	{
		AlbumLabel.hScrollX = MainGfxBuf.memWidth;
		AlbumLabel.hScrollBit = 7;
	}
	os_timer_setfn(&scrollTmr, (os_timer_func_t *)scrollTmrCb, hScrollStep);
	os_timer_arm(&scrollTmr, HSCROLL_INTERVAL, 1);
}

LOCAL void ICACHE_FLASH_ATTR hScrollStart(void)
{
	TitleLabel.scrollEn = (TitleLabel.buf.width > DISP_WIDTH);
	ArtistLabel.scrollEn = (ArtistLabel.buf.width > DISP_WIDTH);
	AlbumLabel.scrollEn = (AlbumLabel.buf.width > DISP_WIDTH);
	if (TitleLabel.scrollEn || ArtistLabel.scrollEn || AlbumLabel.scrollEn)
	{
		os_timer_setfn(&scrollTmr, (os_timer_func_t *)hScrollStep1, NULL);
		os_timer_arm(&scrollTmr, 5000, 0);
		isScrolling = TRUE;
	}
}

LOCAL void ICACHE_FLASH_ATTR hScrollContinue(void)
{
	TitleLabel.scrollEn = TitleLabel.scrollInt;
	ArtistLabel.scrollEn = ArtistLabel.scrollInt;
	AlbumLabel.scrollEn = AlbumLabel.scrollInt;
	TitleLabel.scrollInt = FALSE;
	ArtistLabel.scrollInt = FALSE;
	AlbumLabel.scrollInt = FALSE;
	if (TitleLabel.scrollEn || ArtistLabel.scrollEn || AlbumLabel.scrollEn)
	{
		os_timer_setfn(&scrollTmr, (os_timer_func_t *)scrollTmrCb, hScrollStep);
		os_timer_arm(&scrollTmr, HSCROLL_INTERVAL, 1);
		isScrolling = TRUE;
	}
}

void ICACHE_FLASH_ATTR hScrollEnable(void)
{
	if (!isScrolling)
	{
		if (TitleLabel.scrollInt || ArtistLabel.scrollInt || AlbumLabel.scrollInt)
		{
			hScrollContinue();		// continue interrupted horizontal scroll
		}
		else
		{
			hScrollStart();		// start a new scroll
		}
	}
}

void ICACHE_FLASH_ATTR scrollStop(void)
{
	os_timer_disarm(&scrollTmr);
	isScrolling = FALSE;
}

