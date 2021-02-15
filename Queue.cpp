#include <pspkernel.h>
#include <pspdebug.h>
#include <pspaudiolib.h>
#include <pspaudio.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <psppower.h>
#include <pspusb.h>
#include <pspusbstor.h>
#include <pspsdk.h>
#include <psphprm.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>

#include <pspge.h>
#include <pspgu.h>
#include <ctype.h>

#include <time.h>

#include "Queue.h"

#define WaitForFree(s, x) while(GetFreeSize()<s) sceKernelDelayThread(x*1000)
#define WaitForAvail(s, x) while(GetAvailSize()<s) sceKernelDelayThread(x*1000)

ImageQueue::ImageQueue()
{
	semaid=-1;

	num_iq=0;
	maxMemUse=MAX_MEMUSE;
	for(int i=0; i<NUM_IMG_QUEUE; i++)
	{
		qdata[i].mem=NULL;
		qdata[i].pict=NULL;
	}

	pix_fmt=-1;
	width=-1;
	height=-1;
}

ImageQueue::~ImageQueue()
{
	Destroy();
}

void ImageQueue::Init()
{
	CreateSema();
	consumedMem=0;
}

void ImageQueue::Destroy()
{
	QDestroy();
	DeleteSema();
}

void ImageQueue::QDestroy()
{
	Lock();

	for(int i=0; i<NUM_IMG_QUEUE; i++)
	{
		av_free(qdata[i].mem);
		av_free(qdata[i].pict);

		qdata[i].pict=NULL;
		qdata[i].mem=NULL;
	}
	consumedMem=0;
	rpos=wpos=num_iq=0;
	Unlock();
}

void StrOut(char *t);
void ImageQueue::QInit(int _pix_fmt, int _width, int _height)
{
	pix_fmt=_pix_fmt;
	width=_width;
	height=_height;

	Lock();

	if(num_iq==0)
	{
		int v, framesize, reduceBuffer=0, memsize;
		unsigned char *frameBuf;

		framesize=avpicture_get_size(pix_fmt, width, height);
		memsize = framesize+64;

		fprintf(stderr, "framesize %d memsize %d\n", framesize, memsize);

		for(v=0; v<NUM_IMG_QUEUE; v++)
		{
			if(consumedMem>maxMemUse)
			{
				reduceBuffer=1;
				break;
			}

			qdata[v].pict=avcodec_alloc_frame();
			qdata[v].mem=av_malloc(memsize);

			if(qdata[v].pict==NULL || qdata[v].mem==NULL)
			{
				if(qdata[v].mem)
				{
					av_free(qdata[v].mem);
					qdata[v].mem=NULL;
				}
				if(qdata[v].pict)
				{
					av_free(qdata[v].pict);
					qdata[v].pict=NULL;
				}
				reduceBuffer=1;
				//fprintf(stderr, "go reduce buffer. %d\n", v);
				break;
			}
			else
			{
				unsigned long ptr=(unsigned long)qdata[v].mem;
				ptr = (ptr+63)&0xffffffc0;

				consumedMem+=memsize;
				avpicture_fill((AVPicture *)qdata[v].pict, (unsigned char*)ptr, pix_fmt, width, height);

				//if(v==0)
				//{
				//	fprintf(stderr, "linesize %d/%d/%d\n", qdata[v].pict->linesize[0], qdata[v].pict->linesize[1], qdata[v].pict->linesize[2]);
				//}
				//fprintf(stderr, "v%d: pict %08x mem %08x ptr %08x yb %08x ub %08x vb %08x\n", v, qdata[v].pict, qdata[v].mem, ptr, qdata[v].pict->data[0], qdata[v].pict->data[1], qdata[v].pict->data[2]);
			}
		}

		if(reduceBuffer)
		{
			int newavail=0;
			for(int i=0; i<200; i++)
			{
				if(v<=1)
					break;

				v--;

				av_free(qdata[v].mem); qdata[v].mem=NULL;
				av_free(qdata[v].pict); qdata[v].pict=NULL;

				newavail+=memsize;

				fprintf(stderr, "reducing... %d\n", newavail);
				if(newavail>REFREE_MEMUSE)
					break;
			}
		}

		for(int i=0; i<v; i++)
		{
			memset(qdata[i].mem, 0, memsize);
		}

		fprintf(stderr, "num allocated buffer... %d\n", v);
		num_iq=v;
	}

	rpos=wpos=0;

	Unlock();
}

void ImageQueue::QFlush()
{
	Lock();

	rpos=wpos=0;

	Unlock();
}

ImageQueue::FrameHolder *ImageQueue::GetFreeSample()
{
	FrameHolder *ret=NULL;
	Lock();

	ret=&qdata[wpos];

	Unlock();

	return ret;
}

ImageQueue::FrameHolder *ImageQueue::GetCurrentSample()
{
	FrameHolder *ret=NULL;
	Lock();

	ret=&qdata[rpos];

	Unlock();

	return ret;
}

