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

#ifndef __TEXTTOOL_MBYTE_H__
#define __TEXTTOOL_MBYTE_H__

class TextToolMByte
{
public:
	SceUID blockid;
	unsigned char *byteArray;
	char asciiSpan[128];

	int pixelFormat;
	int pixelSize;
	char *baseAddr;

	int fontSizeX;
	int fontSizeY;
	int rowByte;

	int topX, topY;

	int spacingX;
	int spacingY;

	unsigned long shadowColor;
	int deepShadow;

	TextToolMByte();

	void Initialize(const char *fileName, int flag);
	void Finalize();

	void SetPixelFormat(int _pixelFormat);
	void SetBaseAddr(char *addr);

	void SetTextRefCoord(int x, int y) { topX=x; topY=y; }
	void SetSpacing(int sx, int sy) { spacingX=sx; spacingY=sy; }

	void PutPixel(char *dest, unsigned long color);

	unsigned long GetDeviceColor(unsigned long color);
	void GetMB(const char *c, int bit, int &byteOffset, unsigned char &mask);
	void PutCharMByteRun(int x, int y, unsigned long color, char *c);
	void PutCharMByteRun2(int x, int y, unsigned long color, unsigned long shadowColor, char *c);
	void PutCharMByte(int x, int y, unsigned long color, char *c);

	void PutStringMByte(int x, int y, unsigned long color, char *str, int len=0);
	void PutStringMByteWithSpace(int x, int y, unsigned long color, char *str, int len=0);

	void PutStringMByteCC(int cx, int cy, unsigned long color, char *str, int len=0);

};

#endif//__TEXTTOOL_MBYTE_H__
