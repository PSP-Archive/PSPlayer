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

#ifndef __BMP_LIB_H__
#define __BMP_LIB_H__

#include "Display.h"

class ImageFile
{
public:
	int imageType;
	SceUID blockid;
	unsigned char *imageData;
//	int bufferSize;
//	int deleteOnExit;
	int sizeX, sizeY, bufferWidth;

	int LoadImage(const char *fileName);
	int LoadBMPFile(const char *fileName);
	int LoadJPGFile(const char *fileName);
	int LoadPNGFile(const char *fileName);
	int LoadGIFFile(const char *fileName);
	int LoadTGAFile(const char *fileName);

	ImageFile();
	~ImageFile();

	unsigned char *AllocImageBuffer();
	void FreeImageBuffer();
	void SetImageSize(int x, int y);
	void Reset();
/*	void SetCustomBuffer(int _bufferSize, unsigned char *buf)
	{
		bufferSize=_bufferSize;
		imageData=buf;

		if(bufferSize>0 && buf)
		{
            deleteOnExit=0;
		}
		else
		{
			bufferSize=0;
			imageData=NULL;
			deleteOnExit=0;
		}
	}*/

	void BitBltF32T16(void *dest, int dpitch, int x=0, int y=0, int sx=SCR_WIDTH, int sy=SCR_HEIGHT, int ix=0, int iy=0, int spitch=0);
	void BitBltF32T32(void *dest, int dpitch, int x=0, int y=0, int sx=SCR_WIDTH, int sy=SCR_HEIGHT, int ix=0, int iy=0, int spitch=0);
	void BitBlt8888(Display &display, int x=0, int y=0, int sx=SCR_WIDTH, int sy=SCR_HEIGHT, int ix=0, int iy=0);
	void BitBlt565(unsigned short *dest, int x=0, int y=0, int sx=SCR_WIDTH, int sy=SCR_HEIGHT, int ix=0, int iy=0);

	int IsLoaded() { return (sizeX && sizeY); }
public:
	static ImageFile bgImage1;
	static ImageFile bgImage2;
	static ImageFile bgImage3;
	static ImageFile bgImage4;

//#define IBSIZE 1024*1024*4
//	static unsigned char bgImageBuffer[IBSIZE];
	static int GetImageInfo(const char *fileName, int &sx, int &sy, int &bit);
	static int GetBMPInfo(const char *fileName, int &sx, int &sy, int &bit);
	static int GetJPGInfo(const char *fileName, int &sx, int &sy, int &bit);
	static int GetPNGInfo(const char *fileName, int &sx, int &sy, int &bit);
	static int GetGIFInfo(const char *fileName, int &sx, int &sy, int &bit);
};

int IsMatchExt(const char *fileName, const char *ext);
int GetImageTypeIndex(const char *fileName);
#endif// __BMP_LIB_H__
