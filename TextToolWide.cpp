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

#include <wchar.h>

#include "TextTool.h"
#include "Display.h"

#include "converters.h"

// return size.
char *Euc_kr2Unicode2(wchar_t * dest, unsigned char* src, int len, int &converted)
{
	int index = 0;
	int size = strlen((char*)src);
	int i;
	unsigned char * cP = src;
	unsigned short code;
		
	for (i = 0; i < size ; i ++)
	{
		if(i>=len)
			break;

		if  (0x0d <= cP[i] && cP[i] < 128 ) {
			euc_kr_mbtowc(dest + index++, cP + i, 2);
		}
		else if  ('\t' == cP[i]) {
			euc_kr_mbtowc(dest + index++, cP + i, 2);
		}
		
		else if ( cP[i] >=  128 && cP[i+1] != 0 ){
			euc_kr_mbtowc(dest + index++, cP + i, 2);
			i++;	
		}
	}

	dest[index]=0;

	converted=index;

	return (char*)(src+i);
	//return index;
}

// return size.
char *Euc_kr2Unicode3(wchar_t * dest, unsigned char* src, int len, int &converted)
{
	int index = 0;
	int size = strlen((char*)src);
	int i;
	unsigned char * cP = src;
	unsigned short code;
		
	for (i = 0; i < size ; i ++)
	{
		if(i>=len)
			break;

		if  (0x0d <= cP[i] && cP[i] < 128 ) {
			cp949_mbtowc(dest + index++, cP + i, 2);
		}
		else if  ('\t' == cP[i]) {
			cp949_mbtowc(dest + index++, cP + i, 2);
		}
		
		else if ( cP[i] >=  128 && cP[i+1] != 0 ){
			cp949_mbtowc(dest + index++, cP + i, 2);
			i++;	
		}
	}

	dest[index]=0;

	converted=index;

	return (char*)(src+i);
	//return index;
}

TextToolWide::TextTool()
{
	byteArray=NULL;
	conversionTable=NULL;

	pixelFormat=3;
	baseAddr=NULL;

	spacingX=spacingY=0;

	topX=topY=0;

	shadowColor=0xb0000000;
	asciiSpan=6;
}

void TextToolWide::Initialize(const char *fileName)
{
	int file = sceIoOpen(fileName, PSP_O_RDONLY, 0777);

	if(file<0)
		return;

	int temp, rSize;
	rSize=sceIoRead(file, &temp, 4);

	rSize=sceIoRead(file, &fontSizeX, 4);
	rSize=sceIoRead(file, &fontSizeY, 4);
	rSize=sceIoRead(file, &asciiSpan, 4);

	rowByte=(fontSizeX*65536/8+1);

	byteArray=(unsigned char *)malloc(rowByte*fontSizeY);

	if(byteArray==NULL)
		return;

	rSize=sceIoRead(file, byteArray, rowByte*fontSizeY);

	sceIoClose(file);
}

/*
void TextToolWide::InitConv(char *fromCode)
{
	iconv_close(convDesc);

	strcpy(cpString, fromCode);

	convDesc=iconv_open("UTF-8", fromCode);
}
*/

void TextToolWide::Finalize()
{
	//iconv_close(convDesc);

	if(byteArray)
		free(byteArray);
}

void TextToolWide::SetPixelFormat(int _pixelFormat)
{
	pixelFormat=_pixelFormat;

	if(pixelFormat<3)
		pixelSize=2;
	else
		pixelSize=4;
}

void TextToolWide::SetBaseAddr(char *addr)
{
    baseAddr=addr;
}

inline unsigned long TextToolWide::GetDeviceColor(unsigned long color)
{
	return color;
}

inline void TextToolWide::PutPixel(char *dest, unsigned long color)
{
	*(unsigned long*)dest=BlendColor(*(unsigned long*)dest, color);
}

static unsigned char MaskArray[8]={1<<7, 1<<6, 1<<5, 1<<4, 1<<3, 1<<2, 1<<1, 1<<0};
inline void TextToolWide::GetBB(wchar_t x, int bit, int &byteOffset, unsigned char &mask)
{
	int bitn=x*fontSizeX+bit;

	byteOffset=bitn/8;
	mask=MaskArray[bitn%8];
}

void TextToolWide::PutCharWideRun(int px, int py, unsigned long color, wchar_t c)
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
			GetBB(c, x, byteOffset, mask);

			if(byteArray[y*rowByte+byteOffset] & mask)
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

void TextToolWide::PutCharWide(int px, int py, unsigned long color, wchar_t c)
{
	if(shadowColor)
		PutCharWideRun(px+1, py+1, shadowColor, c);
	PutCharWideRun(px, py, color, c);
}

void TextToolWide::PutStringMByte(int x, int y, unsigned long color, char *str, int len)
{
	wchar_t wideChar[128];

	int slen=strlen(str);

	if(len==0 || len>slen)
		len=slen;

	int wb;
	Euc_kr2Unicode2(wideChar, (unsigned char*)str, len, wb);

	PutStringWide(x, y, color, wideChar, wb);
}

void TextToolWide::PutStringWide(int x, int y, unsigned long color, wchar_t *str, int len)
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
		}*/
	}

}

void TextToolWide::PutStringMByteCC(int cx, int cy, unsigned long color, char *str, int len)
{
	int x=cx*(spacingX+fontSizeX)+topX;
	int y=cy*(spacingY+fontSizeY)+topY;

	PutStringMByte(x, y, color, str, len);
}

void TextToolWide::PutStringWideCC(int cx, int cy, unsigned long color, wchar_t *str, int len)
{
	int x=cx*(spacingX+fontSizeX)+topX;
	int y=cy*(spacingY+fontSizeY)+topY;

	PutStringWide(x, y, color, str, len);
}




