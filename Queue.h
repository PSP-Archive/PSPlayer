#ifndef __QUEUE_H__
#define __QUEUE_H__

extern "C" {
#include "avformat.h"
};

#define DEFINE_SEMAPHORE \
	int semaid;	\
	void Lock() { sceKernelWaitSema(semaid, 0, 0); sceKernelSignalSema(semaid, 1); } \
	void Unlock() { sceKernelSignalSema(semaid, -1); } \
	void CreateSema() { if(semaid<0) semaid = sceKernelCreateSema("", 0, 0, 20, 0); } \
	void DeleteSema() { if(semaid>=0) { sceKernelDeleteSema(semaid); semaid=-1; } }

#define DEFINE_GET_AVAIL_SIZE(total, wpos, rpos) int GetAvailSize() { if(wpos>=rpos) return wpos-rpos; else return total+wpos-rpos; }
#define DEFINE_GET_FREE_SIZE(total, wpos, rpos) int GetFreeSize() { return total-GetAvailSize()-1; }
#define DEFINE_READ_ADVANCE(total, wpos, rpos) void ReadAdvance(int size) { Lock(); rpos=(rpos+size)%total; Unlock();}
#define DEFINE_WRITE_ADVANCE(total, wpos, rpos) void WriteAdvance(int size) { Lock(); wpos=(wpos+size)%total; Unlock(); }

#define NUM_IMG_QUEUE 8
#define MAX_MEMUSE 16*1024*1024
#define REFREE_MEMUSE 1*1024*1024

//#define NUM_IMG_QUEUE 10
//#define MAX_MEMUSE 2*1024*1024
//#define REFREE_MEMUSE 0

class ImageQueue
{
public:
	struct FrameHolder
	{
		void *mem;
		AVFrame *pict;
		int64_t dts;
		int64_t frametime;	// in ms.
	};

	int pix_fmt, width, height;
	FrameHolder qdata[NUM_IMG_QUEUE];
	int rpos, wpos, num, num_iq;
	int consumedMem, maxMemUse;

	ImageQueue();
	~ImageQueue();

	void Init();
	void Destroy();

	void QInit(int _pix_fmt, int _width, int _height);
	void QFlush();
	void QDestroy();

	FrameHolder *GetFreeSample();
	FrameHolder *GetCurrentSample();

	DEFINE_SEMAPHORE

	DEFINE_GET_FREE_SIZE(num_iq, wpos, rpos)
	DEFINE_GET_AVAIL_SIZE(num_iq, wpos, rpos)
	DEFINE_READ_ADVANCE(num_iq, wpos, rpos)
	DEFINE_WRITE_ADVANCE(num_iq, wpos, rpos)
};

#endif//__QUEUE_H__
