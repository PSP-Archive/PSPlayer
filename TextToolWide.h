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

#ifndef __TEXT_TOOL_WIDE_H__
#define __TEXT_TOOL_WIDE_H__

class TextToolWide
{
public:
	unsigned char *byteArray;
	unsigned char *conversionTable;

	int pixelFormat;
	int pixelSize;
	char *baseAddr;

	int fontSizeX;
	int fontSizeY;
	int asciiSpan;
	int rowByte;

	int topX, topY;

	int spacingX;
	int spacingY;

	unsigned long shadowColor;

	char cpString[32];

	TextTool();

	void Initialize(const char *fileName);
	void InitConv(char *fromCode);
	void Finalize();

	void SetPixelFormat(int _pixelFormat);
	void SetBaseAddr(char *addr);

	unsigned long GetDeviceColor(unsigned long color);
	void GetBB(wchar_t x, int bit, int &byteOffset, unsigned char &mask);
	void PutCharWideRun(int x, int y, unsigned long color, wchar_t c);
	void PutCharWide(int x, int y, unsigned long color, wchar_t c);

	void SetTextRefCoord(int x, int y) { topX=x; topY=y; }
	void SetSpacing(int sx, int sy) { spacingX=sx; spacingY=sy; }

	void PutStringMByte(int x, int y, unsigned long color, char *str, int len=0);
	void PutStringWide(int x, int y, unsigned long color, wchar_t *str, int len=0);

	void PutStringMByteCC(int cx, int cy, unsigned long color, char *str, int len=0);
	void PutStringWideCC(int cx, int cy, unsigned long color, wchar_t *str, int len=0);

	void PutPixel(char *dest, unsigned long color);
};


char *Euc_kr2Unicode2(wchar_t * dest, unsigned char* src, int len, int &converted);

#endif//ifdef __TEXT_TOOL_WIDE_H__
