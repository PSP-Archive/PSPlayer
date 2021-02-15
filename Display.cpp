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

#include "Display.h"

static unsigned int __attribute__((aligned(16))) list1[256*1024];
//static unsigned int __attribute__((aligned(16))) list2[262144];
//int curList=0;

unsigned long Dev2Raw(unsigned long color);
unsigned long Raw2Dev(unsigned long color);

/*
void Display::FillBG(unsigned char *src)
{
	if(frameBuffer==NULL)
		return;

	char *destXBase=(char *)frameBuffer;
	char *destAddr;
	unsigned char *srcXBase=src;
	unsigned char *srcAddr;

	for(int y=0; y<SCR_HEIGHT; y++)
	{
		destAddr=destXBase;
		srcAddr=srcXBase;

		memcpy(destXBase, srcXBase, 4*SCR_WIDTH);
//		memcpy(destXBase+4*SCR_WIDTH, 0, 4*(BUF_WIDTH-SCR_WIDTH));

		destXBase+=SCR_WIDTH*4;
		srcXBase+=SCR_WIDTH*4;
	}
}

void Display::CopyBG()
{
	if(g_bg_base==NULL)
		return;

	char *destXBase=(char *)frameBufPtr;
	char *destAddr;
	unsigned char *srcXBase=(unsigned char *)g_bg_base;
	unsigned char *srcAddr;

	for(int y=0; y<SCR_HEIGHT; y++)
	{
		destAddr=destXBase;
		srcAddr=srcXBase;

		memcpy(destXBase, srcXBase, 4*SCR_WIDTH);
//		memcpy(destXBase+4*SCR_WIDTH, 0, 4*(BUF_WIDTH-SCR_WIDTH));

		destXBase+=BUF_WIDTH*4;
		srcXBase+=SCR_WIDTH*4;
	}
}

void Display::CopyBG()
{
	if(g_vram_base==NULL)
		return;

	char *destXBase=(char *)g_vram_base;
	char *destAddr;
	unsigned char *srcXBase=(unsigned char *)frameBufPtr;
	unsigned char *srcAddr;

	for(int y=0; y<SCR_HEIGHT; y++)
	{
		destAddr=destXBase;
		srcAddr=srcXBase;

		memcpy(destXBase, srcXBase, 4*SCR_WIDTH);
//		memcpy(destXBase+4*SCR_WIDTH, 0, 4*(BUF_WIDTH-SCR_WIDTH));

		destXBase+=BUF_WIDTH*4;
		srcXBase+=BUF_WIDTH*4;
	}
}
*/
Display::Display()
{

}

void Display::Initialize(int pixelFormat)
{
	if(pixelFormat<=2)
	{
		pixelSize=2;
	}
	else
	{
		pixelSize=4;
	}

	frameSize=BUF_WIDTH * SCR_HEIGHT * pixelSize;
	zBufferSize=BUF_WIDTH * SCR_HEIGHT * 2;

	sceGuInit();
	StartRenderList();
//	sceGuStart(GU_DIRECT,list);

	sceGuDrawBuffer(pixelFormat, (void*)0, BUF_WIDTH);
	sceGuDispBuffer(SCR_WIDTH, SCR_HEIGHT, (void*)frameSize, BUF_WIDTH);
//	sceGuDepthBuffer((void*)(frameSize*2), BUF_WIDTH);
	sceGuOffset(2048 - (SCR_WIDTH/2),2048 - (SCR_HEIGHT/2));
	sceGuViewport(2048,2048,SCR_WIDTH,SCR_HEIGHT);
//	sceGuDepthRange(0xc350,0x2710);
	sceGuScissor(0,0,SCR_WIDTH,SCR_HEIGHT);
	sceGuEnable(GU_SCISSOR_TEST);
	sceGuFrontFace(GU_CW);
	sceGuEnable(GU_TEXTURE_2D);
//	sceGuEnable(GU_BLEND);
	//sceGuAlphaFunc(GU_GREATER,0,0xff);
	//sceGuEnable(GU_ALPHA_TEST);
//	sceGuDisable(GU_DEPTH_TEST);
	sceGuDisable(GU_CULL_FACE);
	sceGuClearColor(0);
//	sceGuClear(GU_COLOR_BUFFER_BIT|GU_DEPTH_BUFFER_BIT);
	sceGuClear(GU_COLOR_BUFFER_BIT);

	FinishRenderList();
//	sceGuFinish();
//	sceGuSync(0,0);

	sceGuDisplay(GU_TRUE);

	WaitVBlankAndSwap();

	vrambase=sceGeEdramGetAddr();
//	g_bg_base=(char *)sceGuGetMemory(SCR_WIDTH*SCR_HEIGHT*4);

//	sceDisplayWaitVblankStart();

//	frameBufPtr=(void*)(g_vram_base+frameSize);
}

void Display::Finalize()
{
	sceGuTerm();
}

void Display::WaitVBlankAndSwap()
{
	sceDisplayWaitVblankStart();
	frameBufPtr=(void*)((unsigned int)vrambase+(unsigned int)sceGuSwapBuffers());
}

/*
void Display::Initialize(int pixelFormat)
{
	if(pixelFormat<=2)
	{
		pixelSize=2;
	}
	else
	{
		pixelSize=4;
	}

	g_vram_base = (char *) (0x40000000 | (u32) sceGeEdramGetAddr());
	sceDisplaySetMode(0, SCR_WIDTH, SCR_HEIGHT);
	sceDisplaySetFrameBuf((void *) g_vram_base, BUF_WIDTH, pixelFormat, 1);

	frameBufPtr=(void*)frameBuffer;

	FillBackground(0xffff4050, 0, 0, 480, 272);
}

void Display::Finalize()
{

}

void Display::WaitVBlankAndSwap()
{
	CopyBG();
	sceDisplayWaitVblankStart();
//	frameBufPtr=(void*)(g_vram_base+(int)sceGuSwapBuffers());
}
*/
void Display::StartRenderList()
{
	sceGuStart(GU_DIRECT,list1);

/*	if(curList==0)
	{
		sceGuStart(GU_DIRECT,list1);
		curList=1;
	}
	else
	{
		sceGuStart(GU_DIRECT,list2);
		curList=0;
	}*/
	
}

void Display::FinishRenderList()
{
	sceGuFinish();
	sceGuSync(0, 0);
}

void *Display::GetDrawBufferBasePtr()
{
	return frameBufPtr;
}

void Display::FillBackground(unsigned long color)
{
	unsigned long *destBase=(unsigned long *)frameBufPtr;//(char *)+display.pixelSize*(py*BUF_WIDTH+px);
	unsigned long *dest;
	for(int y=0; y<SCR_HEIGHT; y++)
	{
		dest=destBase;
		for(int x=0; x<SCR_WIDTH; x++)
		{
			*dest=color;

			dest++;
		}

		destBase+=BUF_WIDTH;
	}
}

void Display::FillBackground(unsigned long color, int px, int py, int sx, int sy)
{
	unsigned long *destBase=(unsigned long *)((char *)frameBufPtr+pixelSize*(py*BUF_WIDTH+px));
	unsigned long *dest;
	for(int y=0; y<sy; y++)
	{
		dest=destBase;
		for(int x=0; x<sx; x++)
		{
			*dest=color;

			dest++;
		}

		destBase+=BUF_WIDTH;
	}
}

void Display::FillBackgroundPtr(void *destPtr, unsigned long color, int px, int py, int sx, int sy)
{
	unsigned long *destBase=(unsigned long *)((char *)destPtr+pixelSize*(py*BUF_WIDTH+px));
	unsigned long *dest;
	for(int y=0; y<sy; y++)
	{
		dest=destBase;
		for(int x=0; x<sx; x++)
		{
			*dest=color;

			dest++;
		}

		destBase+=BUF_WIDTH;
	}
}




