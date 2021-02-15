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

#include <pspkernel.h>
#include <pspdebug.h>
#include <pspaudiolib.h>
#include <pspaudio.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <pspgu.h>
#include <pspge.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>

//#include <wchar.h>

#include "TextToolMByte.h"
#include "Display.h"

static unsigned char __attribute__((aligned(16))) MaskArray[8]={1<<7, 1<<6, 1<<5, 1<<4, 1<<3, 1<<2, 1<<1, 1<<0};
static unsigned char __attribute__((aligned(16))) sysBuffer[1179840];

void PrintErrMsgWait(const char *msg);

TextToolMByte::TextToolMByte()
{
	blockid=0;
	byteArray=NULL;

	pixelFormat=3;
	baseAddr=NULL;

	spacingX=spacingY=0;

	topX=topY=0;

	shadowColor=0xb0000000;
}

// flag 0: use sys buffer, 1:read header only, 2:read entire.
void TextToolMByte::Initialize(const char *fileName, int flag)
{
	int file = sceIoOpen(fileName, PSP_O_RDONLY, 0777);

	if(file<0)
	{
		return;
	}

	int temp, rSize;
	rSize=sceIoRead(file, &temp, 4);

	rSize=sceIoRead(file, &fontSizeX, 4);
	rSize=sceIoRead(file, &fontSizeY, 4);

	if(flag==1)
	{
		sceIoClose(file);
		return;
	}

	rSize=sceIoRead(file, asciiSpan, 128);

	rowByte=(fontSizeX*65536/8+1);

	if(flag==2)
	{
		blockid=sceKernelAllocPartitionMemory(2, "tt_block", PSP_SMEM_Low, rowByte*fontSizeY, NULL);
		if (blockid > 0)
		{
			byteArray = (unsigned char *)sceKernelGetBlockHeadAddr(blockid);
		}
		//byteArray=(unsigned char *)malloc(rowByte*fontSizeY);
	}
	else if(rowByte*fontSizeY<=1179840)
		byteArray=sysBuffer;

	if(byteArray)
	{
		rSize=sceIoRead(file, byteArray, rowByte*fontSizeY);
	}

	sceIoClose(file);

	//char tmpbuf[256];
	//sprintf(tmpbuf, "load success %s %d %08x %08x", fileName, rSize, byteArray, sysBuffer);
	//PrintErrMsgWait(tmpbuf);

}

void TextToolMByte::Finalize()
{
	if(blockid>0)
	{
		sceKernelFreePartitionMemory(blockid);
	}
	blockid=0;
//	if(byteArray && byteArray!=sysBuffer)
//		free(byteArray);
}

void TextToolMByte::SetPixelFormat(int _pixelFormat)
{
	pixelFormat=_pixelFormat;

	if(pixelFormat<3)
		pixelSize=2;
	else
		pixelSize=4;
}

void TextToolMByte::SetBaseAddr(char *addr)
{
    baseAddr=addr;
}

inline unsigned long TextToolMByte::GetDeviceColor(unsigned long color)
{
	return color;
}

inline void TextToolMByte::PutPixel(char *dest, unsigned long color)
{
	*(unsigned long*)dest=color;
//	*(unsigned long*)dest=BlendColor(*(unsigned long*)dest, color);
}

inline void TextToolMByte::GetMB(const char *c, int bit, int &byteOffset, unsigned char &mask)
{
	if( 0<=*c && *c<128  )
	{
		// ascii
		int bitn=(*c)*fontSizeX+bit;

		byteOffset=bitn/8;
		mask=MaskArray[bitn%8];
	}
	else
	{
		unsigned short cc;
		memcpy(&cc, c, 2);

		int bitn=cc*fontSizeX+bit;

		byteOffset=bitn/8;
		mask=MaskArray[bitn%8];
	}
}

void TextToolMByte::PutCharMByteRun(int px, int py, unsigned long color, char *c)
{
	if(byteArray==NULL)
		return;

	if( !(px>=0 && py>=0 && px<SCR_WIDTH && py<SCR_HEIGHT) )
		return;

	char *xbase=baseAddr+pixelSize*(py*BUF_WIDTH+px);
	char *addr;
	int byteOffset;
	unsigned char mask;
	for(int y=0; y<fontSizeY; y++)
	{
		addr=xbase;
		if( !(py+y<SCR_HEIGHT))
			continue;

		for(int x=0; x<fontSizeX; x++)
		{
			GetMB(c, x, byteOffset, mask);

			if((byteArray[y*rowByte+byteOffset] & mask))
			{
				PutPixel(addr, color);
			}
			else
			{

			}

			addr+=pixelSize;
		}
		xbase+=BUF_WIDTH*pixelSize;
	}
}

void TextToolMByte::PutCharMByteRun2(int px, int py, unsigned long color, unsigned long shadowColor, char *c)
{
	if(byteArray==NULL)
		return;

	if( !(px>=0 && py>=0 && px<SCR_WIDTH && py<SCR_HEIGHT) )
		return;

	char *xbase=baseAddr+pixelSize*(py*BUF_WIDTH+px);
	char *addr;
	int byteOffset;
	unsigned char mask;
	for(int y=0; y<=fontSizeY; y++)
	{
		addr=xbase;
		if( !(py+y<SCR_HEIGHT))
			continue;

		for(int x=0; x<=fontSizeX; x++)
		{
			GetMB(c, x, byteOffset, mask);

			if((x<fontSizeX&&y<fontSizeY) && (byteArray[y*rowByte+byteOffset] & mask))
			{
//				unsigned long tttt;
//				PutPixel((char*)&tttt, color);
				PutPixel(addr, color);
			}
			else if(x>0 && y>0)
			{
				GetMB(c, x-1, byteOffset, mask);

				if((byteArray[(y-1)*rowByte+byteOffset] & mask))
				{
					PutPixel(addr, shadowColor);
				}
			}

			addr+=pixelSize;
		}
		xbase+=BUF_WIDTH*pixelSize;
	}
}

void TextToolMByte::PutCharMByte(int px, int py, unsigned long color, char *c)
{
	if(shadowColor)
	{
		PutCharMByteRun2(px+1, py+1, color, shadowColor, c);
	}
	else
	{
		PutCharMByteRun(px, py, color, c);
	}
}

void TextToolMByte::PutStringMByte(int x, int y, unsigned long color, char *str, int len)
{
	int slen=strlen(str);

	if(len==0 || len>slen)
		len=slen;

	int cx=x, cy=y;
	for(int i=0; i<len;)
	{
		if(str[i]=='\n')
		{
			cx=x;
			cy+=fontSizeY+spacingY;
		}
		else if(str[i]=='\t')
		{
			cx+=fontSizeX*2;
		}
		else if(str[i]==' ')
		{
			cx+=asciiSpan[str[i]]+spacingX;
		}
		else if(  0 <= str[i] )
		{
			PutCharMByte(cx, cy, color, str+i);
			cx+=asciiSpan[str[i]]+spacingX;
		}
		else
		{
			PutCharMByte(cx, cy, color, str+i);
			cx+=fontSizeX;
			i++;
		}
		i++;
	}
}

void TextToolMByte::PutStringMByteWithSpace(int x, int y, unsigned long color, char *str, int len)
{
	int slen=strlen(str);

	if(len==0 || len>slen)
		len=slen;

	int cx=x, cy=y;
	for(int i=0; i<len;)
	{
		if(str[i]=='\n')
		{
			cx=x;
			cy+=fontSizeY+spacingY;
		}
		else if(str[i]=='\t')
		{
			cx+=fontSizeX*2;
		}
/*		else if(str[i]==' ')
		{
			cx+=asciiSpan[str[i]]+spacingX;
		}*/
		else if(  0 <= str[i] )
		{
			PutCharMByte(cx, cy, color, str+i);
			cx+=asciiSpan[str[i]]+spacingX;
		}
		else
		{
			PutCharMByte(cx, cy, color, str+i);
			cx+=fontSizeX;
			i++;
		}
		i++;
	}
}

/*
void TextToolMByte::PutStringWide(int x, int y, unsigned long color, wchar_t *str, int len)
{
	if(len==0)
		len=wcslen(str);

	int   cursorX = x;
	int   cursorY = y;
	int i; unsigned int code;
	for ( i = 0; i < len; i++)
	{
		code = str[i];

		if (code == 0x0020 )
			cursorX += fontSizeX/2+spacingX;
		else if  (code == (unsigned int)'\t')
			cursorX += fontSizeX*2+spacingX;
		else if ( 0x000d < code &&  code < 256)
		{
			PutCharWide(cursorX,cursorY,color,code);
			cursorX += asciiSpan+spacingX;
		}
		else if (code >= 256)// && code < 55296)
		{
			PutCharWide(cursorX,cursorY,color,code);
			cursorX += fontSizeX+spacingX;
		}

		if (code ==  (unsigned int)'\n') // 0x000d
		{
			cursorX = x;
			cursorY += fontSizeY+spacingY;
		}
/*		else if (SCR_WIDTH-cursorX < fontSizeX+margin )
		{
			cursorX = x;
			cursorY += fontSizeY;
		}* /
	}

}
*/

void TextToolMByte::PutStringMByteCC(int cx, int cy, unsigned long color, char *str, int len)
{
	int x=cx*(spacingX+fontSizeX)+topX;
	int y=cy*(spacingY+fontSizeY)+topY;

	PutStringMByte(x, y, color, str, len);
}
/*
void TextToolMByte::PutStringWideCC(int cx, int cy, unsigned long color, wchar_t *str, int len)
{
	int x=cx*(spacingX+fontSizeX)+topX;
	int y=cy*(spacingY+fontSizeY)+topY;

	PutStringWide(x, y, color, str, len);
}
*/



