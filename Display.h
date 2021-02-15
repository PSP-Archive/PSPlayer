/*
 * PSPlayer
 * Copyright 2005-2008 kkman
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef _DISPLAY_H_
#define _DISPLAY_H_

// display properties...

#define BUF_WIDTH (512)
#define SCR_WIDTH (480)
#define SCR_HEIGHT (272)
//#define PIXEL_SIZE (4) /* change this if you change to another screenmode */
//#define FRAME_SIZE (BUF_WIDTH * SCR_HEIGHT * PIXEL_SIZE)
//#define ZBUF_SIZE (BUF_WIDTH SCR_HEIGHT * 2) /* zbuffer seems to be 16-bit? */

class Display
{
public:
	int pixelFormat;
	int pixelSize;
	int frameSize;
	int zBufferSize;

	void *vrambase;
	void *frameBufPtr;

	Display();
	
	void Initialize(int pixelFormat);
	void Finalize();

	void *GetDrawBufferBasePtr();
	void WaitVBlankAndSwap();

	void StartRenderList();
	void FinishRenderList();

	unsigned long Dev2Raw(unsigned long color);
	unsigned long Raw2Dev(unsigned long color);

	void FillBackground(unsigned long color);
	void FillBackground(unsigned long color, int x, int y, int sx, int sy);
	void FillBackgroundPtr(void *ptr, unsigned long color, int x, int y, int sx, int sy);

	void FillBG(unsigned char *src);
	void CopyBG();
};

inline unsigned long BlendColor(unsigned long &dest, unsigned long &src)
{
	unsigned long alpha=src>>24;
	unsigned long onema=0x100-alpha;

	unsigned long blue =((dest>>16&0xff)*onema+(src>>16&0xff)*alpha)>>8;
	unsigned long green=((dest>> 8&0xff)*onema+(src>> 8&0xff)*alpha)>>8;
	unsigned long red  =((dest    &0xff)*onema+(src    &0xff)*alpha)>>8;
	
	return dest=blue<<16|green<<8|red;
}


#endif// _DISPLAY_H_
