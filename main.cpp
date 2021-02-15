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

// part of this file is adapted from other software package.

#include <pspsdk.h>
#include <pspkernel.h>
#include <pspdebug.h>
#include <pspaudio.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <psppower.h>
#include <pspusb.h>
#include <pspusbstor.h>
#include <pspsdk.h>
#include <psphprm.h>
#include <psprtc.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <malloc.h>

#include <pspge.h>
#include <pspgu.h>
#include <ctype.h>

#include <time.h>

#include "my_pspaudiolib.h"

#include "config.h"

#include "mad.h"
#include "id3tag.h"
#include "resample.h"

#include "Display.h"
#include "TextToolMByte.h"
#include "ImageFile.h"

#include "my_resample.h"

#include "codegen.h"
#include "pspvfpu.h"

extern "C" {

#if _PSP_FW_VERSION <= 150

inline void psplForce333() { scePowerSetClockFrequency(333, 333, 166); }

#define scePowerSetClockFrequency(x, y, z) { scePowerSetCpuClockFrequency(x); scePowerSetBusClockFrequency(z); }

PSP_MODULE_INFO("PSPLAYER", 0x1000, 2, 0);
PSP_MAIN_THREAD_ATTR(0 /*| THREAD_ATTR_VFPU*/);

#define USB_ENABLED 1

#else // 3.xx over.

#include <kubridge.h>

PSP_MODULE_INFO("PSPLAYER", 0x200, 1, 1);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER /*| THREAD_ATTR_VFPU*/);

#define USB_ENABLED 1

void setKernelCpuClock(int cpu);
void setKernelBusClock(int bus);

inline void psplForce333() { setKernelCpuClock(333); setKernelBusClock(166); }
#define scePowerSetClockFrequency(x, y, z) { int bb=z; bb=bb<54?54:bb; bb=166<bb?166:bb; setKernelCpuClock(x); setKernelBusClock(bb); }

//inline void psplForce333() { scePowerSetClockFrequency(333, 333, 166); }

#define scePowerSetCpuClockFrequency(cpuclock) fgfg
#define scePowerSetBusClockFrequency(busclock) fgfg

#endif// _PSP_FW_VERSION <= 150


#define SEAMLESS_PLAY 0

int imposeGetVolume();

int GetLCDBrightness();
int IsDisplayDisabled();
void psplEnableDisplay();
void psplDisableDisplay();

/* Define the main thread's attribute value (optional) */
PSP_MAIN_THREAD_PRIORITY(0x20);

PSP_MAIN_THREAD_STACK_SIZE_KB(256);
PSP_HEAP_SIZE_KB(1024*10);

}

#define min(x, y) (x<y?x:y)
#define max(x, y) (x>y?x:y)

#define STRING_LENGTH 256

struct Vertex
{
	float u, v;
	unsigned int color;
	float x,y,z;
};

#define VRAM_OFFSET (512*272*2*4)
u32 *vramtex=NULL;

u32 __attribute__((aligned(16))) titleBuffer[512*32];
u32 __attribute__((aligned(16))) workBuffer[512*272];
//unsigned int __attribute__((aligned(16))) imgBuffer[512*272];

char init_path[STRING_LENGTH];
Display display;
TextToolMByte textTool;
TextToolMByte textTool16;

int isPSPSlim=0;

SceUInt64 currtick;
SceUInt64 prevtick;
int idleSec=0;

int showBG=3;
int useLargeFont=0;
int lineGap=2;
int extMenu=0;
int titleType=0;
int sleepCounter=0;

unsigned long bgColor=0xff605030;
unsigned long titleColor=0xffffffff;
unsigned long playtextColor=0xff00ffff;
unsigned long generalTextColor=0xffffffff;
unsigned long selectedTextColor=0xff00c0ff;
unsigned long textViewerColor=0xffffffff;
unsigned long playingTextColor=0xffff5050;

int showTagOnly=1;

enum FilterMode { FM_ALL=0, FM_PLAYABLE, FM_MP3, FM_M3U, FM_TXT, FM_BMP, FM_INT=0x7fffffff};
FilterMode filterMode=FM_ALL;
int showDateSize=1;

//char usbErrorMsg[STRING_LENGTH];
int usbInitialized=0;
u32 usbState = 0;

int showMode=4;

int timeZoneOffset=9;
int remoteTextControl=0;
int autoSaveMode=1;
int powerSaveModeTime=120;
int suspendTime=300;
int displayOffWhenHold=1;
int forceTrimLeft=1;
int fullScreenText=0;

int normalCPUClock=20;
int normalBusClock=20;

int powerSaveCPUClock=10;
int powerSaveBusClock=10;

int libmadClockBoost=40;
int oggvorbisClockBoost=50;
int meClockBoost=0;
int aviClockBoost=50;

int musicClockBoost=meClockBoost;

int useLibMAD=0;
int mpeg4vmecsc=0;

int isSeekable=0;

int powerMode=2;
int frameModified=1;
int curBG=-1;
char bgPath[STRING_LENGTH]="";
void LoadBackground();

float clockBoostRemainTime=0;
float audioStopTime=0;
int audioRestartPos=0;

float volumeBoost=1.0f;

int ix=0, iy=0;
int hideUI=0;
float zoommax=1, zoommin=1, zoomfactor=1;
char infoMsg[STRING_LENGTH];
float infoMsgTime=0;

/* Define printf, just to make typing easier */
#define printf	pspDebugScreenPrintf
void PrintErrMsgWait(const char *msg);

int exit_callback(int arg1, int arg2, void *common);
int power_callback(int unknown, int pwrflags, void *common);
void RemoteControl(unsigned int &rButtons, unsigned int rBPress);

/* Callback thread */
int callbackThread(SceSize args, void *argp) {
	int cbid;

	cbid = sceKernelCreateCallback("Exit Callback", exit_callback, NULL);
	sceKernelRegisterExitCallback(cbid);

    cbid = sceKernelCreateCallback("Power Callback", power_callback, NULL);
    scePowerRegisterCallback(0, cbid);

	sceKernelSleepThreadCB();

	return 0;
}

/* Sets up the callback thread and returns its thread id */
int setupCallbacks(void)
{
	int thid = 0;

#if _PSP_FW_VERSION <= 150

	thid = sceKernelCreateThread("update_thread", callbackThread, 0x11, 0xFA0, THREAD_ATTR_USER, 0);

#else

	thid = sceKernelCreateThread("update_thread", callbackThread, 0x11, 0xFA0, 0, 0);

#endif

	if (thid >= 0) {
		sceKernelStartThread(thid, 0, 0);
	}
	return thid;
}

void BoostCPU(float time)
{
	if( clockBoostRemainTime==0 )
	{
		psplForce333();
	}
//	scePowerSetClockFrequency(266, 266, 133);

	clockBoostRemainTime=time;
}

char *FindNextLine(char *src, int size);

namespace std
{
	class string2
	{
	public:
		char str[STRING_LENGTH+16];

		string2() { str[0]=0; }
		string2(const char *s) {strncpy(str, s, STRING_LENGTH); str[STRING_LENGTH]=0;}
/*		string &operator=(const string &s)
		{
			strncpy(str, s.str, STRING_LENGTH);
			str[STRING_LENGTH]=0;
			return *this;
		}*/
		string2 &operator=(const char *s)
		{
			strncpy(str, s, STRING_LENGTH);
			str[STRING_LENGTH]=0;
			return *this;
		}
		const char *c_str() { return str; }
	};

	class StringClamper
	{
	public:
		string2 cur;
		string2 ret;

		int iStartPos;
		int tcsLen;
		int tsize;
		int ipause;

		StringClamper(int size) { tsize=size; memset(ret.str, ' ', tsize); ret.str[tsize]=0; }

		void Tick()
		{
			if( tcsLen<=tsize )
			{
				return;
			}

			if(tcsLen<=iStartPos+tsize)
			{
				if(2<ipause++)
				{
					iStartPos=0;
					ipause=0;
				}
				return;
			}

			if( iStartPos==0 && ipause++<2 )
			{
				ipause++;
			}
			else
			{
				if( *(cur.str+iStartPos) < 0)
				{
					iStartPos++;
				}
				iStartPos++;
				ipause=0;
			}
		}
		int MyTcsLen(const char *s)
		{
			if(!s)
				return 0;

			return (int)strlen(s);
			//int count=0;
			//while(*s)
			//{
			//	if(*s<0)
			//	{
			//		s++;
			//		count++;
			//	}
			//	s++;
			//	count++;
			//}
			//return count;
		}
		char *MyTcsOffset(char *s, int offset)
		{
			if(!s)
				return 0;
			while(*s && offset>0)
			{
				if(*s<0)
				{
					s++;
					offset--;
				}
				s++;
				offset--;
			}
			return s;
		}
		void StrSet(const char *s)
		{
			if(s && strncmp(s, cur.c_str(), STRING_LENGTH))
			{
				ipause=0;
				iStartPos=0;
				tcsLen=MyTcsLen(s);

				cur=s;

				if(tcsLen<=tsize)
				{
					memset(ret.str, ' ', tsize);
					ret.str[tsize]=0;
					memcpy(ret.str, s, tcsLen);
				}
			}
		}
		const char *Get()
		{
			if(tcsLen<=tsize)
				return ret.c_str();
			
			char *istart=cur.str+iStartPos;

			memset(ret.str, ' ', tsize);

			char *iend=MyTcsOffset(istart, tsize);
			if(tsize<iend-istart)
			{
				memcpy(ret.str, istart, iend-istart-2);
			}
			else
			{
				memcpy(ret.str, istart, iend-istart);
			}

			ret.str[tsize]=0;
			return ret.c_str();
		}
	};
}

#define NUM_BOOKMARK 120
class Bookmark
{
public:
	struct BookmarkData
	{
		int fileSize;
		int ftellPos;
	};

	BookmarkData data[NUM_BOOKMARK];

	int numMark;

	void ReadFromFile(char *fileName)
	{

	}
	void WriteFromFile(char *fileName)
	{

	}
	void InitMark()
	{
		numMark=0;
		for(int i=0; i<NUM_BOOKMARK; i++)
		{
			data[i].fileSize=0;
		}
	}
	int GetMark(int fileSize)
	{
		for(int i=0; i<numMark; i++)
		{
			if(data[i].fileSize==fileSize)
				return data[i].ftellPos;
		}
		return 0;
	}
	void AddMark(int fileSize, int ftellPos)
	{
		int i;
		for(i=0; i<numMark; i++)
		{
			if(data[i].fileSize==fileSize)
				break;
		}

/*		if(numMark<NUM_BOOKMARK)
		{
			data[numMark].fileSize=fileSize;
			data[numMark].ftellPos=ftellPos;
			numMark++;
			return;
		}*/

		if(i<numMark-1)
		{
			memmove(&data[i], &data[i+1], (numMark-i-1)*sizeof(BookmarkData));
			data[numMark-1].fileSize=fileSize;
			data[numMark-1].ftellPos=ftellPos;
		}
		else if(i==numMark-1)
		{
			data[numMark-1].fileSize=fileSize;
			data[numMark-1].ftellPos=ftellPos;
		}
		else if(i==NUM_BOOKMARK)
		{
			memmove(&data[0], &data[1], (NUM_BOOKMARK-1)*sizeof(BookmarkData));
			data[NUM_BOOKMARK-1].fileSize=fileSize;
			data[NUM_BOOKMARK-1].ftellPos=ftellPos;
		}
		else
		{
			data[numMark].fileSize=fileSize;
			data[numMark].ftellPos=ftellPos;
			numMark++;
		}
	}
};

Bookmark bookmark;

//std::StringClamper mp3NameClamper(68);
//std::StringClamper fileNameClamper1(40);
//std::StringClamper fileNameClamper2(68);
std::StringClamper mp3TopClamper(24);
std::StringClamper textNameClamper(25);

void BuildPath(char *path, const char *basePath, const char *fileName)
{
	snprintf(path, STRING_LENGTH, "%s/%s", basePath, fileName);
}
void InitialLoad(int loadmp3, int loadtext);

// rounding & dithering from madcurplayer->..
struct audio_stats {
  unsigned long clipped_samples;
  mad_fixed_t peak_clipping;
  mad_fixed_t peak_sample;
};
struct audio_dither {
  mad_fixed_t error[3];
  mad_fixed_t random;
};

#define _NEW_READ_
#ifdef _NEW_READ_

#include "MyFile.h"

#endif

//#define USE_ALLOC_MEM
#define MPEG_BUFSZ 1024*2
#define HEADER_POS_LOG_SIZE 60*25
#define MY_MUSICFILEBUF 128*1024
unsigned char musicfbuf[MY_MUSICFILEBUF*2];

//unsigned char __attribute__((aligned(16))) smallBuf1[MPEG_BUFSZ+8];
//unsigned char __attribute__((aligned(16))) smallBuf2[MPEG_BUFSZ+8];
class Player
{
public:

	MyURLContext _uc;

	// current file.
	std::string2 pathName;
	std::string2 fileName;
	int fileCurPos;
	int fileLength;

	int fileIndex;

	int estimatedSeconds;
	int samplingRate;
	int bitRate;
	int numChannel;

	int currentDecodedSample;
	int playedSample;
	int headerSample;
	int copiedSample;

	int bufferStartFilePos;
	int dataBufLen;
	unsigned char dataBuffer[MPEG_BUFSZ+8];
	int maxDataBufLen;
	int memuid;

	int abortFlag;
	int isPlaying;

	int seekStat;
	int seekSample;
	int skipCount;

	int headerSampleWritePos;
	int headerFilePos[HEADER_POS_LOG_SIZE];
	int headerSampleData[HEADER_POS_LOG_SIZE];

	int haveVideo;
	int haveSubtitle;

	struct resample_state rstate1;
	struct resample_state rstate2;
	struct audio_stats stats;//={0,0,0};
	struct audio_dither l_dither;//={{0,0,0},0};
	struct audio_dither r_dither;//={{0,0,0},0};

	std::string2 title;
	std::string2 artist;
	std::string2 album;
	std::string2 track;
	std::string2 year;
	std::string2 genre;
	std::string2 comment;

	Player()
	{
		pathName=fileName="";
		abortFlag=0;
		isPlaying=0;

		playedSample=copiedSample=0;

		haveVideo=0;

		InitMyURLContext(&_uc);
	}

	int InitFileData(const std::string2 &mp3Name, int openfile=1)
	{
		pathName=mp3Name;

		// possible id3 tag parsing...

		fileCurPos=0;

		estimatedSeconds=0;
		samplingRate=0;
		bitRate=0;
		numChannel=2;

		currentDecodedSample=0;
		playedSample=0;
		headerSample=0;
		copiedSample=0;

		bufferStartFilePos=0;
		dataBufLen=0;
//		dataBuffer=smallBuf;
		maxDataBufLen=MPEG_BUFSZ;
		memuid=-1;

		memset(&rstate1, 0, sizeof(struct resample_state));
		memset(&rstate2, 0, sizeof(struct resample_state));
		memset(&stats, 0, sizeof(struct audio_stats));
		memset(&l_dither, 0, sizeof(struct audio_dither));
		memset(&r_dither, 0, sizeof(struct audio_dither));

		title=artist=album=track=year=genre=comment="";

		fileLength=0;

		abortFlag=0;

		seekStat=0;
		seekSample=-1;
		skipCount=0;

		headerSampleWritePos=0;
		memset(headerFilePos, 0, sizeof(headerFilePos));
		memset(headerSampleData, 0, sizeof(headerSampleData));

		haveVideo=0;
		haveSubtitle=0;

		char *str=strrchr(pathName.c_str(), '/');
		if(str)
		{
			fileName=std::string2(str+1);
		}
		else
		{
			fileName=std::string2(pathName.c_str());
		}

		file_close(&_uc);

		InitMyURLContext(&_uc);

		if(openfile==0)
		{
			FILE *fp=fopen(pathName.c_str(), "rb");
			if(fp)
			{
				fseek(fp, 0, SEEK_END);
				fileLength=ftell(fp);
				fclose(fp);
			}

			return 1;
		}

		InitMyURLContext(&_uc);
		_uc._FBUFSIZE=MY_MUSICFILEBUF;
		_uc.fbuf=musicfbuf;
		_uc.fbuf2=musicfbuf+MY_MUSICFILEBUF;

		if ( file_open(&_uc, pathName.c_str(), URL_RDONLY )<0 )
		{
			return 0;
		}

		fileLength=file_seek(&_uc, 0, SEEK_END);

		file_seek(&_uc, 0, SEEK_SET);

		return 1;
	}

	void CloseFileData()
	{
		file_close(&_uc);
	}
};

Player player1;
Player *curplayer=&player1;
Player *showplayer=&player1;

#if SEAMLESS_PLAY==1

Player player2;

void SwapCurPlayer() { if(curplayer==&player1) curplayer=&player2; else curplayer=&player1; }
void SwapShowPlayer() { if(showplayer==&player1) showplayer=&player2; else showplayer=&player1; }
void ResetPlayer() { curplayer=&player1; showplayer=&player1; }

#else

void SwapCurPlayer() {  }
void SwapShowPlayer() {  }
void ResetPlayer() {  }

#endif

// MP3_FRAME_SIZE = 44100/75 FRAME_BUFFER_SIZE=75*n초
#define MP3_FRAME_SIZE 588
#define FRAME_BUFFER_SIZE 375
#define TOTAL_SAMPLE_COUNT (MP3_FRAME_SIZE*FRAME_BUFFER_SIZE)
#define ONE_SECOND_SAMPLE_COUNT (MP3_FRAME_SIZE*75)
//#define TWO_SECOND_SAMPLE_COUNT (MP3_FRAME_SIZE*150)
#define HALF_SAMPLE_COUNT (MP3_FRAME_SIZE*FRAME_BUFFER_SIZE/2)
#define LOW_SAMPLE_COUNT (MP3_FRAME_SIZE*FRAME_BUFFER_SIZE*15/10)
#define HIGH_SAMPLE_COUNT (MP3_FRAME_SIZE*FRAME_BUFFER_SIZE*9/10)

struct tStereoSample
{
	short l;
	short r;
} ;

struct tStereoSample __attribute__((aligned(16))) samples[TOTAL_SAMPLE_COUNT];
int maxSampleCount=TOTAL_SAMPLE_COUNT;
int sampleReadPos;
int sampleWritePos;

int IsReducedBuffer() { return maxSampleCount!=TOTAL_SAMPLE_COUNT; }
int GetAvailSize()
{
	if(sampleWritePos>=sampleReadPos)
		return sampleWritePos-sampleReadPos;
	else
		return maxSampleCount+(sampleWritePos-sampleReadPos);
}
int GetFreeSize()
{
	return maxSampleCount-GetAvailSize()-1;
}
int CopySampleData(void *dest, int sampleCount)
{
	if(GetAvailSize()<sampleCount)
	{
		memset(dest, 0, sampleCount*4);

		if(GetAvailSize()==0)
			return 0;

		sampleCount=GetAvailSize();
	}

	int hSize=maxSampleCount-sampleReadPos;
	if(hSize>=sampleCount)
	{
		memcpy(dest, samples+sampleReadPos, sampleCount*sizeof(struct tStereoSample));
		sampleReadPos+=sampleCount;
	}
	else
	{
		int lSize=sampleCount-hSize;
		char *dest2=(char*)dest+hSize*sizeof(struct tStereoSample);
		memcpy(dest, samples+sampleReadPos, hSize*sizeof(struct tStereoSample));
		memcpy(dest2, samples, lSize*sizeof(struct tStereoSample));

		sampleReadPos=(sampleReadPos+sampleCount)%maxSampleCount;
	}

	return sampleCount;
}
void PushSampleData(void *src, int sampleCount)
{
	if(GetFreeSize()<sampleCount)
		return;

	int hSize=maxSampleCount-sampleWritePos;
	if(hSize>=sampleCount)
	{
		memcpy(samples+sampleWritePos, src, sampleCount*sizeof(struct tStereoSample));
		sampleWritePos+=sampleCount;
	}
	else
	{
		int lSize=sampleCount-hSize;
		char *src2=(char*)src+hSize*sizeof(struct tStereoSample);
		memcpy(samples+sampleWritePos, src, hSize*sizeof(struct tStereoSample));
		memcpy(samples, src2, lSize*sizeof(struct tStereoSample));

		sampleWritePos=(sampleWritePos+sampleCount)%maxSampleCount;
	}
}

int semaid=-1;
int decoderThread=-1;
int id3Thread=-1;
int usbThread=-1;
int hprmThread=-1;
void ClearBuffers()
{
	sceKernelWaitSema(semaid, 0, 0);

	sceKernelSignalSema(semaid, 1);

	memset(samples, 0, sizeof(samples));
	sampleReadPos=sampleWritePos=0;

	sceKernelSignalSema(semaid, -1);
}

//void AdjustBuffer(int isBig)
//{
//	sceKernelWaitSema(semaid, 0, 0);
//
//	sceKernelSignalSema(semaid, 1);
//
//	sampleReadPos=sampleWritePos=0;
//
//	if(isBig)
//	{
//		maxSampleCount=TOTAL_SAMPLE_COUNT;
//	}
//	else
//	{
//		maxSampleCount=MP3_FRAME_SIZE*8;
//	}
//
//	sceKernelSignalSema(semaid, -1);
//}

inline void _ClampCopyWithVolFloat(stereo_sample *dest, register float vb, register int l, register int r)
{
	l = (int)(l*vb);
	r = (int)(r*vb);

	dest->l=l>32767?32767:(l<-32767?-32767:l);
	dest->r=r>32767?32767:(r<-32767?-32767:r);
}


enum AudioPlayMode { APM_NORMAL, APM_PAUSED, APM_INITIALBUFFERING};
AudioPlayMode audioPlayMode=APM_PAUSED;
int blankaudio=0;
float __attribute__((aligned(16))) __fconst[4]={0,};

void LoadConst(float *fptr)
{
	__fconst[0]=__fconst[0]=__fconst[0]=__fconst[0]=volumeBoost;

	__asm__ volatile (
		"lv.q C000,  0 + %0\n"

		: "+m"(*fptr) );
}

inline void BoostVolumeVFPU(stereo_sample *dest)
{
	__asm__ volatile
	(
		"lv.q C010,  0 + %0\n"
		"vs2i.p C100, C010\n"
		"vs2i.p C110, C012\n"
		cgen_asm(vi2f_q(Q_C100, Q_C100, 0))
		cgen_asm(vi2f_q(Q_C110, Q_C110, 0))
		//"vi2f.q C100, C100\n"
		//"vi2f.q C110, C110\n"

		"vscl.q C100, C100, S000\n"
		"vscl.q C110, C110, S000\n"

		cgen_asm(vf2in_q(Q_C100, Q_C100, 0))
		cgen_asm(vf2in_q(Q_C110, Q_C110, 0))

		"vi2s.q C010, C100\n"
		"vi2s.q C012, C110\n"

		"sv.q C010,  0 + %0\n"

		: : "m"(*dest) :
	);
}

inline void BoostVolumeVFPU(stereo_sample *dest, int length)
{
	register void *ptr1 __asm__ ("a0") = (void *)dest;
	register int ptr2 __asm__ ("v0") = length/4;

	__asm__ volatile
	(
		//".set push\n"
		".set noreorder\n"

		"start:\n"

		"sub $2, $2, 1\n"

		cgen_asm(lv_q(Q_C010, 0*16, R_a0))
//		"lv.q C010,  0 + %0\n"
		"vs2i.p C100, C010\n"
		"vs2i.p C110, C012\n"
		cgen_asm(vi2f_q(Q_C100, Q_C100, 0))
		cgen_asm(vi2f_q(Q_C110, Q_C110, 0))
		//"vi2f.q C100, C100\n"
		//"vi2f.q C110, C110\n"

		"vscl.q C100, C100, S000\n"
		"vscl.q C110, C110, S000\n"

		cgen_asm(vf2in_q(Q_C100, Q_C100, 0))
		cgen_asm(vf2in_q(Q_C110, Q_C110, 0))

		"vi2s.q C010, C100\n"
		"vi2s.q C012, C110\n"

		cgen_asm(sv_q(Q_C010, 0*16, R_a0, 0))
//		"sv.q C010,  0 + %0\n"

		"bnez $2, start\n"
		"addiu $4, $4, 16\n"

		//".set pop\n"

		: "=r"(ptr1), "=r"(ptr2) : "r"(ptr1), "r"(ptr2) : "memory"
	);
}

u64 bftick, tick0;
//int bufferStartSampleIndex=0;
void audioCallback(void* buf, unsigned int length, void *)
{
	if(audioPlayMode==APM_INITIALBUFFERING && IsReducedBuffer()==0)
	{
		if(GetAvailSize()>ONE_SECOND_SAMPLE_COUNT && curplayer->haveVideo==0)
		{
			audioPlayMode=APM_NORMAL;
		}
	}

	//if(audioPlayMode==APM_PAUSED || (audioPlayMode==APM_INITIALBUFFERING && curplayer->copiedSample<length*4))
	if(audioPlayMode!=APM_NORMAL)
	{
		if(blankaudio++<4)
			memset(buf, 0, length*4);
		return;
	}

	blankaudio=0;

	if(GetAvailSize()<length)
	{
		sceKernelDelayThread(1000);
	}

	//bufferStartSampleIndex=curplayer->playedSample;
	sceRtcGetCurrentTick(&bftick);

	if(curplayer->playedSample==0)
		sceRtcGetCurrentTick(&tick0);

	sceKernelWaitSema(semaid, 0, 0);

	sceKernelSignalSema(semaid, 1);

	curplayer->playedSample+=CopySampleData(buf, length);

	sceKernelSignalSema(semaid, -1);

#define _USE_VFPU
#ifndef _USE_VFPU

	if( volumeBoost!=1.0f )
	{
		stereo_sample *dest=(stereo_sample*)buf;

		while(length--)
		{
			_ClampCopyWithVolFloat(dest, volumeBoost, dest->l, dest->r);
			dest++;
		}
	}

#else

	if( volumeBoost==1.0f )
	{
		return;
	}

	if( volumeBoost != __fconst[0] )
	{
		LoadConst(__fconst);
	}

	//length=length/4;

	//stereo_sample *dest=(stereo_sample*)buf;
	//while(length--)
	//{
	//	BoostVolumeVFPU(dest);

	//	dest=dest+4;
	//}

	stereo_sample *dest=(stereo_sample*)buf;
	BoostVolumeVFPU(dest, length);

#endif
}

signed long audio_linear_round(unsigned int, mad_fixed_t,
			       struct audio_stats *);
signed long audio_linear_dither(unsigned int, mad_fixed_t,
				struct audio_dither *, struct audio_stats *);

/*
 * NAME:	clip()
 * DESCRIPTION:	gather signal statistics while clipping
 */
static inline
void clip(mad_fixed_t *sample, struct audio_stats *stats)
{
  enum {
    MIN = -MAD_F_ONE,
    MAX =  MAD_F_ONE - 1
  };

  if (*sample >= stats->peak_sample) {
    if (*sample > MAX) {
      ++stats->clipped_samples;
      if (*sample - MAX > stats->peak_clipping)
	stats->peak_clipping = *sample - MAX;

      *sample = MAX;
    }
    stats->peak_sample = *sample;
  }
  else if (*sample < -stats->peak_sample) {
    if (*sample < MIN) {
      ++stats->clipped_samples;
      if (MIN - *sample > stats->peak_clipping)
	stats->peak_clipping = MIN - *sample;

      *sample = MIN;
    }
    stats->peak_sample = -*sample;
  }
}

/*
 * NAME:	audio_linear_round()
 * DESCRIPTION:	generic linear sample quantize routine
 */
# if defined(_MSC_VER)
extern  /* needed to satisfy bizarre MSVC++ interaction with inline */
# endif
inline
signed long audio_linear_round(unsigned int bits, mad_fixed_t sample,
			       struct audio_stats *stats)
{
  /* round */
  sample += (1L << (MAD_F_FRACBITS - bits));

  /* clip */
  clip(&sample, stats);

  /* quantize and scale */
  return sample >> (MAD_F_FRACBITS + 1 - bits);
}

/*
 * NAME:	prng()
 * DESCRIPTION:	32-bit pseudo-random number generator
 */
static inline
unsigned long prng(unsigned long state)
{
  return (state * 0x0019660dL + 0x3c6ef35fL) & 0xffffffff;
}

/*
 * NAME:	audio_linear_dither()
 * DESCRIPTION:	generic linear sample quantize and dither routine
 */

inline
signed long audio_linear_dither(unsigned int bits, mad_fixed_t sample,
				struct audio_dither *dither,
				struct audio_stats *stats)
{
  unsigned int scalebits;
  mad_fixed_t output, mask, random;

  enum {
    MIN = -MAD_F_ONE,
    MAX =  MAD_F_ONE - 1
  };

  /* noise shape */
  sample += dither->error[0] - dither->error[1] + dither->error[2];

  dither->error[2] = dither->error[1];
  dither->error[1] = dither->error[0] / 2;

  /* bias */
  output = sample + (1L << (MAD_F_FRACBITS + 1 - bits - 1));

  scalebits = MAD_F_FRACBITS + 1 - bits;
  mask = (1L << scalebits) - 1;

  /* dither */
  random  = prng(dither->random);
  output += (random & mask) - (dither->random & mask);

  dither->random = random;

  /* clip */
  if (output >= stats->peak_sample) {
    if (output > MAX) {
      ++stats->clipped_samples;
      if (output - MAX > stats->peak_clipping)
	stats->peak_clipping = output - MAX;

      output = MAX;

      if (sample > MAX)
	sample = MAX;
    }
    stats->peak_sample = output;
  }
  else if (output < -stats->peak_sample) {
    if (output < MIN) {
      ++stats->clipped_samples;
      if (MIN - output > stats->peak_clipping)
	stats->peak_clipping = MIN - output;

      output = MIN;

      if (sample < MIN)
	sample = MIN;
    }
    stats->peak_sample = -output;
  }

  /* quantize */
  output &= ~mask;

  /* error feedback */
  dither->error[0] = sample - output;

  /* scale */
  return output >> scalebits;
}


/*
 * This is the input callback. The purpose of this callback is to (re)fill
 * the stream buffer which is to be decoded. In this example, an entire file
 * has been mapped into memory, so we just call mad_stream_buffer() with the
 * address and length of the mapping. When this callback is called a second
 * time, we are finished decoding.
 */
static
enum mad_flow input(void *data,
		    struct mad_stream *stream)
{
	class Player *player=(class Player*)data;

	if(player->seekStat==1)
	{
		sceKernelDelayThread(1000);
		if(GetAvailSize())
		{
			ClearBuffers();
		}

		return MAD_FLOW_CONTINUE;
	}

	if(player->seekStat==2)
	{
		int i;
		for(i=0; i<player->headerSampleWritePos-1; i++)
		{
			if(player->seekSample<player->headerSampleData[i+1] || player->headerSampleData[i+1]==0 )
				break;
		}

		player->bufferStartFilePos=player->fileCurPos=player->headerFilePos[i];
		player->dataBufLen=0;
		stream->next_frame=0;

//		player->headerSampleWritePos=i;
		player->headerSample=player->headerSampleData[i];

#ifdef _NEW_READ_

		file_seek(&player->_uc, player->bufferStartFilePos, PSP_SEEK_SET);

#else

		sceIoLseek(player->file, player->bufferStartFilePos, PSP_SEEK_SET);

#endif

//		if(player->seekSample-player->headerSampleData[i]<)
//		player->skipCount=(player->seekSample-player->headerSampleData[i])/(MP3_FRAME_SIZE*player->numChannel);
		player->seekStat=3;
	}

	if(player->fileCurPos==player->fileLength)
		return MAD_FLOW_STOP;

	if (stream->next_frame)
	{
		player->dataBufLen = &player->dataBuffer[player->dataBufLen] - stream->next_frame;
		memmove(player->dataBuffer, stream->next_frame, player->dataBufLen);

		player->bufferStartFilePos+=stream->next_frame-player->dataBuffer;
	}
	else
        player->bufferStartFilePos+=player->dataBufLen;

#ifdef _NEW_READ_

	int rSize=file_read(&player->_uc, player->dataBuffer+player->dataBufLen, player->maxDataBufLen-player->dataBufLen);

#else

	int rSize=sceIoRead(player->file, player->dataBuffer+player->dataBufLen, player->maxDataBufLen-player->dataBufLen);

#endif

	if(rSize==-1)
	{
		return MAD_FLOW_BREAK;
	}

	player->dataBufLen+=rSize;
	player->fileCurPos+=rSize;

	memset(&player->dataBuffer[player->dataBufLen], 0, 8);

	mad_stream_buffer(stream, player->dataBuffer, player->dataBufLen);

	return MAD_FLOW_CONTINUE;
}

static
//enum mad_flow decode_header(void *data, struct mad_header const *header)
enum mad_flow decode_header2(void *data, struct mad_header const *header, struct mad_stream const*stream)
{
	Player *player = (Player *)data;

	if(audioPlayMode==APM_INITIALBUFFERING ||
		(curplayer->isPlaying==1 && GetAvailSize()<ONE_SECOND_SAMPLE_COUNT))
	{
		BoostCPU(2.5f);
	}

	player->samplingRate=header->samplerate;
	player->bitRate=header->bitrate;
	player->numChannel=(header->mode==MAD_MODE_SINGLE_CHANNEL)?1:2;

	if(player->estimatedSeconds==0)
	{
		player->estimatedSeconds=player->fileLength/(player->bitRate/8);
	}

	if(player->samplingRate!=44100 && player->rstate1.ratio==0)
	{
		resample_init(&player->rstate1, player->samplingRate, 44100);
		resample_init(&player->rstate2, player->samplingRate, 44100);
	}

	// write header pos.
	if( player->headerSampleWritePos*44100 <= player->headerSample)
	{
		if(player->headerSampleWritePos<HEADER_POS_LOG_SIZE)
		{
			player->headerFilePos[player->headerSampleWritePos]=player->bufferStartFilePos+(stream->this_frame-player->dataBuffer);
			player->headerSampleData[player->headerSampleWritePos]=player->headerSample;
			player->headerSampleWritePos++;
		}
	}

	player->headerSample+=mad_timer_count(header->duration, MAD_UNITS_44100_HZ);

	if( player->seekStat==3 )
	{
		if( player->headerSample < player->seekSample)
		{
			return MAD_FLOW_IGNORE;
		}
		else
		{
			player->playedSample=player->copiedSample=player->headerSample;

			player->seekStat=0;

			audioPlayMode=APM_INITIALBUFFERING;

			BoostCPU(0.5f);

			return MAD_FLOW_IGNORE;
		}
	}
	//if(player->skipCount)
	//{
	//	player->skipCount--;
	//	if(player->skipCount==0)
	//	{
	//		player->playedSample=player->copiedSample=player->headerSample;
	//	}
	//	return MAD_FLOW_IGNORE;
	//}

    return MAD_FLOW_CONTINUE;
}

/*
 * The following utility routine performs simple rounding, clipping, and
 * scaling of MAD's high-resolution samples down to 16 bits. It does not
 * perform any dithering or noise shaping, which would be recommended to
 * obtain any exceptional audio quality. It is therefore not recommended to
 * use this routine if high-quality output is desired.
 */

static inline
signed int scale(mad_fixed_t sample)
{
  /* round */
  sample += (1L << (MAD_F_FRACBITS - 16));

  /* clip */
  if (sample >= MAD_F_ONE)
    sample = MAD_F_ONE - 1;
  else if (sample < -MAD_F_ONE)
    sample = -MAD_F_ONE;

  /* quantize */
  return sample >> (MAD_F_FRACBITS + 1 - 16);
}

/*
 * This is the output callback function. It is called after each frame of
 * MPEG audio data has been completely decoded. The purpose of this callback
 * is to output (or play) the decoded PCM audio.
 */

mad_fixed_t lResampleBuf[MAX_NSAMPLES];
mad_fixed_t rResampleBuf[MAX_NSAMPLES];
struct tStereoSample convBuf[MAX_NSAMPLES];
static enum mad_flow output(void *data,
		     struct mad_header const *header,
		     struct mad_pcm *pcm)
{
	class Player *player=(class Player*)data;

	unsigned int nchannels, nsamples;
	mad_fixed_t const *l_ch, *r_ch;

	/* pcm->samplerate contains the sampling frequency */

	nchannels = pcm->channels;
	nsamples  = pcm->length;
	l_ch   = pcm->samples[0];
	if (nchannels == 2)
		r_ch  = pcm->samples[1];
	else
		r_ch = l_ch;

	if(nsamples>MAX_NSAMPLES)
	{
		player->playedSample=100000000;
		while(1);
	}

	if(player->abortFlag)
		return MAD_FLOW_STOP;

	if(pcm->samplerate!=44100)
	{
		int oldSampleSize=nsamples;

		nsamples=resample_block(&player->rstate1, oldSampleSize, l_ch, lResampleBuf);
		nsamples=resample_block(&player->rstate2, oldSampleSize, r_ch, rResampleBuf);

		l_ch=lResampleBuf;
		r_ch=rResampleBuf;
	}

	player->currentDecodedSample+=nsamples;

	while(GetFreeSize()<nsamples)
	{
		sceKernelDelayThread(1000);

		if(player->abortFlag)
			return MAD_FLOW_STOP;
	}

	int skipCount=0;
	int iSamples=0;

	while(nsamples--)
	{
		convBuf[iSamples].l=audio_linear_dither(16, *l_ch++, &player->l_dither, &player->stats);
		convBuf[iSamples].r=audio_linear_dither(16, *r_ch++, &player->r_dither, &player->stats);

		++iSamples;
	}

	if(iSamples)
	{
		sceKernelWaitSema(semaid, 0, 0);

		sceKernelSignalSema(semaid, 1);
		PushSampleData(convBuf, iSamples);

		sceKernelSignalSema(semaid, -1);

		player->copiedSample+=iSamples;
	}

	if( ONE_SECOND_SAMPLE_COUNT < GetAvailSize() )
	{
		sceKernelDelayThread(2000);
	}

	return MAD_FLOW_CONTINUE;
}

int run_sync2(struct mad_decoder *decoder)
{
	if (decoder->input_func == 0 || decoder->error_func==0)
		return 0;

	enum mad_flow (*error_func)(void *, struct mad_stream *, struct mad_frame *);
	void *error_data;
	int bad_last_frame = 0;
	struct mad_stream *stream=(struct mad_stream *)malloc(sizeof(struct mad_stream));
	struct mad_frame *frame=(struct mad_frame *)malloc(sizeof(struct mad_frame));
	struct mad_synth *synth=(struct mad_synth *)malloc(sizeof(struct mad_synth));

	int result = 0;

	class Player *player=(class Player*)decoder->cb_data;

	error_func = decoder->error_func;
	error_data = decoder->cb_data;

	mad_stream_init(stream);
	mad_frame_init(frame);
	mad_synth_init(synth);

	mad_stream_options(stream, decoder->options);

	do
	{
		switch (decoder->input_func(decoder->cb_data, stream))
		{
		case MAD_FLOW_STOP:
			goto done;
		case MAD_FLOW_BREAK:
			goto fail;
		case MAD_FLOW_IGNORE:
			continue;
		case MAD_FLOW_CONTINUE:
			break;
		}

		while(player->seekStat==0 || player->seekStat==3)
		{
//			if (decoder->header_func)
			{
				if (mad_header_decode(&frame->header, stream) == -1)
				{
					if (!MAD_RECOVERABLE(stream->error))
						break;

					switch (error_func(error_data, stream, frame))
					{
					case MAD_FLOW_STOP:
						goto done;
					case MAD_FLOW_BREAK:
						goto fail;
					case MAD_FLOW_IGNORE:
					case MAD_FLOW_CONTINUE:
					default:
						continue;
					}
				}

				switch(decode_header2(decoder->cb_data, &frame->header, stream))
//				switch (decoder->header_func(decoder->cb_data, &frame->header))
				{
				case MAD_FLOW_STOP:
					goto done;
				case MAD_FLOW_BREAK:
					goto fail;
				case MAD_FLOW_IGNORE:
					continue;
				case MAD_FLOW_CONTINUE:
					break;
				}
			}

			if (mad_frame_decode(frame, stream) == -1)
			{
				if (!MAD_RECOVERABLE(stream->error))
					break;

				switch (error_func(error_data, stream, frame))
				{
				case MAD_FLOW_STOP:
					goto done;
				case MAD_FLOW_BREAK:
					goto fail;
				case MAD_FLOW_IGNORE:
					break;
				case MAD_FLOW_CONTINUE:
				default:
					continue;
				}
			}
			else
				bad_last_frame = 0;

			// at this log current frame info.
//			decode_header2(decoder->cb_data, &frame->header, stream);

			if (decoder->filter_func) {
				switch (decoder->filter_func(decoder->cb_data, stream, frame))
				{
				case MAD_FLOW_STOP:
					goto done;
				case MAD_FLOW_BREAK:
					goto fail;
				case MAD_FLOW_IGNORE:
					continue;
				case MAD_FLOW_CONTINUE:
					break;
				}
			}

			mad_synth_frame(synth, frame);

			if (decoder->output_func)
			{
				switch (decoder->output_func(decoder->cb_data,
					&frame->header, &synth->pcm))
				{
				case MAD_FLOW_STOP:
					goto done;
				case MAD_FLOW_BREAK:
					goto fail;
				case MAD_FLOW_IGNORE:
				case MAD_FLOW_CONTINUE:
					break;
				}
			}
		}

//		sceKernelDelayThread(3*1000);
	}
	while (stream->error == MAD_ERROR_BUFLEN);

fail:
	result = -1;

done:
	mad_synth_finish(synth);
	mad_frame_finish(frame);
	mad_stream_finish(stream);

	free(synth);
	free(frame);
	free(stream);

	return result;
}

/*
 * This is the error callback function. It is called whenever a decoding
 * error occurs. The error is indicated by stream->error; the list of
 * possible MAD_ERROR_* errors can be found in the mad.h (or
 * libmad/stream.h) header file.
 */

/*
 * NAME:	show_id3()
 * DESCRIPTION:	display ID3 tag information
 */
static
void show_id3(Player *player, struct id3_tag const *tag)
{
	unsigned int i;
	struct id3_frame const *frame;
	id3_ucs4_t const *ucs4;
	id3_latin1_t *latin1;

#define N_(x) x
	static struct {
		char const *id;
		char const *label;
	} const info[] = {
		{ ID3_FRAME_TITLE,  N_("Title")     },
		{ "TIT3",           0               },  /* Subtitle */
		{ "TCOP",           0               },  /* Copyright */
		{ "TPRO",           0               },  /* Produced */
		{ "TCOM",           N_("Composer")  },
		{ ID3_FRAME_ARTIST, N_("Artist")    },
		{ "TPE2",           N_("Orchestra") },
		{ "TPE3",           N_("Conductor") },
		{ "TEXT",           N_("Lyricist")  },
		{ ID3_FRAME_ALBUM,  N_("Album")     },
		{ ID3_FRAME_TRACK,  N_("Track")     },
		{ ID3_FRAME_YEAR,   N_("Year")      },
		{ "TPUB",           N_("Publisher") },
		{ ID3_FRAME_GENRE,  N_("Genre")     },
		{ "TRSN",           N_("Station")   },
		{ "TENC",           N_("Encoder")   }
	};

	/* text information */

	for (i = 0; i < sizeof(info) / sizeof(info[0]); ++i) {
		union id3_field const *field;
		unsigned int nstrings, j;

		frame = id3_tag_findframe(tag, info[i].id, 0);
		if (frame == 0)
			continue;

		field    = id3_frame_field(frame, 1);
		nstrings = id3_field_getnstrings(field);

		for (j = 0; j < nstrings; ++j) {
			ucs4 = id3_field_getstrings(field, j);
			//assert(ucs4);

			if (strcmp(info[i].id, ID3_FRAME_GENRE) == 0)
				ucs4 = id3_genre_name(ucs4);

			latin1 = id3_ucs4_latin1duplicate(ucs4);
			if (latin1 == 0)
				goto fail;

			if (strcmp(info[i].id, ID3_FRAME_TITLE) == 0)
			{
				snprintf(player->title.str, STRING_LENGTH, "%s", latin1);
			}
			if (strcmp(info[i].id, ID3_FRAME_ARTIST) == 0)
			{
				snprintf(player->artist.str, STRING_LENGTH, "%s", latin1);
			}
			if (strcmp(info[i].id, ID3_FRAME_ALBUM) == 0)
			{
				snprintf(player->album.str, STRING_LENGTH, "%s", latin1);
			}
			if (strcmp(info[i].id, ID3_FRAME_TRACK) == 0)
			{
				snprintf(player->track.str, STRING_LENGTH, "%s", latin1);
			}
			if (strcmp(info[i].id, ID3_FRAME_YEAR) == 0)
			{
				snprintf(player->year.str, STRING_LENGTH, "%s", latin1);
			}
			if (strcmp(info[i].id, ID3_FRAME_GENRE) == 0)
			{
				snprintf(player->genre.str, STRING_LENGTH, "%s", latin1);
			}
			free(latin1);
		}
	}

	/* comments */

	i = 0;
	while ((frame = id3_tag_findframe(tag, ID3_FRAME_COMMENT, i++)))
	{
		ucs4 = id3_field_getstring(id3_frame_field(frame, 2));
//		assert(ucs4);

		if (*ucs4)
			continue;

		ucs4 = id3_field_getfullstring(id3_frame_field(frame, 3));
//		assert(ucs4);

		latin1 = id3_ucs4_latin1duplicate(ucs4);
		if (latin1 == 0)
			goto fail;


		snprintf(player->comment.str, STRING_LENGTH, "%s", latin1);

		free(latin1);
		break;
	}

	if (0) {
fail:
//		error("id3", _("not enough memory to display tag"));
		;
	}
}

static
enum mad_flow error(void *data,
		    struct mad_stream *stream,
		    struct mad_frame *frame)
{
	class Player *player=(class Player*)data;

	signed long tagsize;

	switch (stream->error)
	{
	case MAD_ERROR_BADDATAPTR:
		return MAD_FLOW_CONTINUE;

	case MAD_ERROR_LOSTSYNC:
		tagsize = id3_tag_query(stream->this_frame,
			stream->bufend - stream->this_frame);
		if (tagsize > 0)
		{
			if (stream->bufend - stream->this_frame>tagsize)
			{
				struct id3_tag *tag=id3_tag_parse(stream->this_frame, tagsize);

				if (tag)
				{
					show_id3(player, tag);
					//process_id3(tag, player);
					id3_tag_delete(tag);
				}
			}
			
			mad_stream_skip(stream, tagsize);

			return MAD_FLOW_CONTINUE;
		}

		/* fall through */

	default:
		break;
	}

	if (stream->error == MAD_ERROR_BADCRC)
	{
		return MAD_FLOW_IGNORE;
	}

	return MAD_FLOW_CONTINUE;
}

int PlayMP3File_MAD(std::string2 &fileName)
{
	musicClockBoost=libmadClockBoost;
	BoostCPU(2.5f);

	curplayer->isPlaying=1;
	curplayer->playedSample=curplayer->copiedSample=0;

	frameModified=1;

	isSeekable=1;

	audioPlayMode=APM_INITIALBUFFERING;

	if(curplayer->InitFileData(fileName)==0)
	{
		curplayer->isPlaying=0;

		sceKernelDelayThread(1000*1000);

		return 0;
	}

	/* try reading ID3 tag information now (else read later from stream) */
	{
		struct id3_file *file;

		file = id3_file_open(fileName.c_str(), ID3_FILE_MODE_READONLY);
		if (file)
		{
			struct id3_tag *tag=id3_file_tag(file);

			show_id3(curplayer, tag);
			id3_file_close(file);
		}
	}

	struct mad_decoder decoder;
	mad_decoder_init(&decoder, curplayer,
		input,
		0, // header
		0, // filter
		output,
		error,
		0  // message
		);

	int result = run_sync2(&decoder);

	mad_decoder_finish(&decoder);

	curplayer->CloseFileData();

	curplayer->isPlaying=2;

	if(curplayer->abortFlag==0)
	{
		while(GetAvailSize()>0 && curplayer->abortFlag==0)
		{
			sceKernelDelayThread(25000);
		}

		curplayer->isPlaying=0;

		SwapCurPlayer();
	}
	else
	{
		curplayer->playedSample=curplayer->copiedSample=0;
		curplayer->isPlaying=0;

		ClearBuffers();
	}

	audioPlayMode=APM_PAUSED;

	return 1;
}

#define USE_PSP_AUDIOCODEC
#ifdef USE_PSP_AUDIOCODEC

#include <pspaudiocodec.h> 
#include <psputility.h> 

my_resample_state rstate;

SceUID LoadStartAudioModule(char *modname, int partition)
{
    SceKernelLMOption option;
    SceUID modid;

    memset(&option, 0, sizeof(option));
    option.size = sizeof(option);
    option.mpidtext = partition;
    option.mpiddata = partition;
    option.position = 0;
    option.access = 1;

    modid = sceKernelLoadModule(modname, 0, &option);
    if (modid < 0)
        return modid;

    return sceKernelStartModule(modid, 0, NULL, NULL, NULL);
}

//Load and start needed modules:
static int HW_ModulesInit=0;
int myUnloadAVModules()
{
	if( HW_ModulesInit )
	{
		if(sceUtilityUnloadAvModule(PSP_AV_MODULE_MPEGBASE)<0)
		{
			fprintf(stderr, "sceUtilityUnloadAvModule(PSP_AV_MODULE_MPEGBASE) fail\n");
		}
		if(sceUtilityUnloadAvModule(PSP_AV_MODULE_AVCODEC)<0)
		{
			fprintf(stderr, "sceUtilityUnloadAvModule(PSP_AV_MODULE_AVCODEC) fail\n");
		}

		fprintf(stderr, "module unloaded\n");

		HW_ModulesInit=0;
	}
}

int myInitAVModules()
{
	if (!HW_ModulesInit)
	{
#if _PSP_FW_VERSION <= 150

		int result;
#define KKK(x) if(result<0) fprintf(stderr, "fail on %s\n", x); else fprintf(stderr, "succ on %s\n", x);
		result=pspSdkLoadStartModule("flash0:/kd/me_for_vsh.prx", PSP_MEMORY_PARTITION_KERNEL);
		KKK("flash0:/kd/me_for_vsh.prx");
		result=pspSdkLoadStartModule("flash0:/kd/audiocodec.prx", PSP_MEMORY_PARTITION_KERNEL);
		KKK("flash0:/kd/audiocodec.prx");
		result=pspSdkLoadStartModule("flash0:/kd/videocodec.prx", PSP_MEMORY_PARTITION_KERNEL);
		KKK("flash0:/kd/videocodec.prx");
		result=pspSdkLoadStartModule("flash0:/kd/mpegbase.prx", PSP_MEMORY_PARTITION_KERNEL);
		KKK("flash0:/kd/mpegbase.prx");
		result=pspSdkLoadStartModule("flash0:/kd/mpeg_vsh.prx", PSP_MEMORY_PARTITION_USER);
		KKK("flash0:/kd/mpeg_vsh.prx");

		pspSdkFixupImports(result);

		//result=pspSdkLoadStartModule("host0:/mebooter_umdvideo.prx", PSP_MEMORY_PARTITION_KERNEL);
		//KKK("host0:/mebooter_umdvideo.prx");
#else

		sceUtilityLoadAvModule(PSP_AV_MODULE_AVCODEC);
		sceUtilityLoadAvModule(PSP_AV_MODULE_MPEGBASE);

#endif
		HW_ModulesInit = 1;
	}
	return 0;
}

int ID3v2TagSize(char data[10])
{
	char sig[3];

	memcpy(sig, data, 3);

	if(strnicmp("ID3",sig,3))
	{
		return 0;
	}

	char szdata[4];
	int size=0;
	memcpy(szdata, data+6, 4);
	szdata[0]=szdata[0]&0x7f;
	szdata[1]=szdata[1]&0x7f;
	szdata[2]=szdata[2]&0x7f;
	szdata[3]=szdata[3]&0x7f;

	size= ((int)szdata[0]<<21) | ((int)szdata[1]<<14) | ((int)szdata[2]<<7) | ((int)szdata[3]);

	return size+10;	// include header size.
}

int ResyncFFHeader(MyURLContext *h)
{
	offset_t cpos;
	unsigned char a, b;
	while(1)
	{
		cpos=file_tell(h);
		if( file_read(h, &a, 1) != 1 )
		{
			return -1;
		}

		if(a==0xff)
		{
			if( file_read(h, &b, 1) != 1 )
			{
				return -1;
			}
			if( (b&0xf0) == 0xf0 )
			{
				file_seek(h, cpos, SEEK_SET);
				return 0;	
			}
		}
	}

	return -1;
}

int count1=0, count2=0, count3=0;

static int samplerates[4][3] =
{
    {11025, 12000, 8000,},//mpeg 2.5
    {0, 0, 0,}, //reserved
    {22050, 24000, 16000,},//mpeg 2
    {44100, 48000, 32000}//mpeg 1
};
static int bitrates[] = {0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320 };
static int bitrates_v2[] = {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160 };

unsigned long mp3_codec_buffer[65] __attribute__((aligned(64)));

#define MAX_AUDIO_FRAME_SIZE (1024*2)
short audio_out_buffer[MAX_AUDIO_FRAME_SIZE*2] __attribute__((aligned(64)));
short audio_resample_buffer[MAX_AUDIO_FRAME_SIZE*16] __attribute__((aligned(64))); 
//short audio_out_buffer[1152 * 2] __attribute__((aligned(64)));
//short audio_resample_buffer[1152 * 2 * 8] __attribute__((aligned(64))); 
//short audio_out_buffer[1024*2*2] __attribute__((aligned(64)));
//short audio_resample_buffer[1024*2*2*8] __attribute__((aligned(64))); 
//short audio_out_buffer[192000/2] __attribute__((aligned(64)));
//short audio_resample_buffer[192000/2] __attribute__((aligned(64))); 
unsigned char audio_input_buffer[1024*4 /* 2889 */]__attribute__((aligned(64)));

int PlayMP3File_ME(std::string2 &fileName)
{
	u8 mp3_getEDRAM=0; 

	myInitAVModules();

	isSeekable=1;

	musicClockBoost=meClockBoost;
	BoostCPU(0.5f);

	memset(audio_out_buffer, 0, sizeof(audio_out_buffer));

	audioPlayMode=APM_INITIALBUFFERING;

	curplayer->isPlaying=1;
	curplayer->playedSample=curplayer->copiedSample=0;

	if(curplayer->InitFileData(fileName)==0)
	{
		curplayer->isPlaying=0;

		sceKernelDelayThread(1000*1000);

		return 0;
	}

	/* try reading ID3 tag information now (else read later from stream) */
	{
		struct id3_file *file;

		file = id3_file_open(fileName.c_str(), ID3_FILE_MODE_READONLY);
		if (file)
		{
			struct id3_tag *tag=id3_file_tag(file);

			show_id3(curplayer, tag);
			id3_file_close(file);
		}
	}

	memset(&rstate, 0, sizeof(rstate));

	char id3v2ident[10]="012345678";
	file_read(&curplayer->_uc, (unsigned char*)id3v2ident, 10);
	int id3v2size=ID3v2TagSize(id3v2ident);

	file_seek(&curplayer->_uc, id3v2size, PSP_SEEK_SET); 
	curplayer->fileCurPos = file_tell(&curplayer->_uc);

	memset(mp3_codec_buffer, 0, sizeof(mp3_codec_buffer)); 

	if ( sceAudiocodecCheckNeedMem(mp3_codec_buffer, 0x1002) < 0 ) 
		goto wait; 
	if ( sceAudiocodecGetEDRAM(mp3_codec_buffer, 0x1002) < 0 ) 
		goto wait; 
	mp3_getEDRAM = 1; 

	if ( sceAudiocodecInit(mp3_codec_buffer, 0x1002) < 0 ) { 
		goto wait; 
	} 

	frameModified=1;

	unsigned char mp3_header[4]; 
	int frame_size, sample_per_frame;

	while( curplayer->abortFlag==0 )
	{
		const int mp3HeaderSize=4;

		if(curplayer->seekStat==1)
		{
			if(GetAvailSize())
			{
				ClearBuffers();
			}
			sceKernelDelayThread(1000);
			continue;
		}

		if(curplayer->seekStat==2)
		{
			int i;
			for(i=0; i<curplayer->headerSampleWritePos-1; i++)
			{
				if(curplayer->seekSample<curplayer->headerSampleData[i+1] || curplayer->headerSampleData[i+1]==0 )
					break;
			}

			curplayer->fileCurPos=curplayer->headerFilePos[i];
			curplayer->headerSample=curplayer->headerSampleData[i];

			file_seek(&curplayer->_uc, curplayer->fileCurPos, PSP_SEEK_SET);
			curplayer->seekStat=3;

			rstate.ptime=0;
			rstate.last.l=rstate.last.r=0;
		}

		// log current file pos.
		curplayer->fileCurPos = file_tell(&curplayer->_uc);

		if ( file_read(&curplayer->_uc, mp3_header, mp3HeaderSize ) != mp3HeaderSize )
		{ 
			break;
		} 

		if( !(mp3_header[0]==0xff && mp3_header[1]&0xf0==0xf0 ) )
		{
			if( ResyncFFHeader(&curplayer->_uc)<0 )
			{
				break;
			}

			continue;
		}

		int bitratetag = (mp3_header[2] & 0xf0) >> 4; 
		int padding = (mp3_header[2] & 0x2) >> 1; 
		int version = (mp3_header[1] & 0x18) >> 3;
		int samplerate = samplerates[version][ (mp3_header[2] & 0xC) >> 2 ];
		int channeltag = (mp3_header[3] & 0xC0)>>6;
		int nCh=(channeltag==3)?1:2;		// bit 11 is 1 channel(mono), else 2 channels (stereo, joint stereo, dual)
		int instantbitrate;//=bitrates[bitratetag]*1000;

		if ((bitratetag > 14) || (version == 1) || (samplerate == 0) || (bitratetag == 0) )
		{
			// invalid frame. stop.
			break;
		}

		if (version == 3) //mpeg-1
		{
			sample_per_frame = 1152;
			instantbitrate=bitrates[bitratetag]*1000;
			frame_size = 144*instantbitrate/samplerate + padding;
		}
		else
		{
			sample_per_frame = 576;
			instantbitrate=bitrates_v2[bitratetag]*1000;
			frame_size = 72*instantbitrate/samplerate + padding;
		}

		curplayer->numChannel=nCh;
		curplayer->bitRate=instantbitrate;
		curplayer->samplingRate=samplerate;
		if( curplayer->estimatedSeconds==0)
			curplayer->estimatedSeconds=curplayer->fileLength/(curplayer->bitRate/8);

		if(rstate.inited==0)
		{
			my_resample_init(&rstate, 44100, samplerate, 2);
		}

		// write header pos.
		if( curplayer->headerSampleWritePos*44100 <= curplayer->headerSample)
		{
			if(curplayer->headerSampleWritePos<HEADER_POS_LOG_SIZE)
			{
				curplayer->headerFilePos[curplayer->headerSampleWritePos]=curplayer->fileCurPos;
				curplayer->headerSampleData[curplayer->headerSampleWritePos]=curplayer->headerSample;
				curplayer->headerSampleWritePos++;
			}
		}

		curplayer->headerSample+=sample_per_frame;

		if( curplayer->seekStat==3 )
		{
			if( curplayer->headerSample < curplayer->seekSample)
			{
				file_seek( &curplayer->_uc, frame_size-mp3HeaderSize, PSP_SEEK_CUR);
				continue;
			}
			else
			{
				curplayer->playedSample=curplayer->copiedSample=curplayer->headerSample;

				curplayer->seekStat=0;

				audioPlayMode=APM_INITIALBUFFERING;

				BoostCPU(0.5f);
			}
		}

		memcpy(audio_input_buffer, mp3_header, mp3HeaderSize);
		if ( file_read( &curplayer->_uc, audio_input_buffer+mp3HeaderSize, frame_size-mp3HeaderSize ) != frame_size-mp3HeaderSize )
		{ 
			break;
		} 

		mp3_codec_buffer[6] = (unsigned long)audio_input_buffer; 
		mp3_codec_buffer[8] = (unsigned long)audio_out_buffer; 

		mp3_codec_buffer[7] = mp3_codec_buffer[10] = frame_size; 
		mp3_codec_buffer[9] = sample_per_frame * 4; 

		int res = sceAudiocodecDecode(mp3_codec_buffer, 0x1002); 
		if ( res < 0 )
		{ 
			memset(audio_out_buffer, 0, sample_per_frame * 4);
		}

		int rsRet=my_resample(&rstate, audio_resample_buffer, audio_out_buffer, sample_per_frame);
		int iSamples=rsRet<0?-rsRet:rsRet;

		while(GetFreeSize()<iSamples)
		{
			sceKernelDelayThread(1000);

			if(curplayer->abortFlag)
				break;
		}

		if(curplayer->abortFlag)
			break;

		if(iSamples)
		{
			sceKernelWaitSema(semaid, 0, 0);

			sceKernelSignalSema(semaid, 1);
			PushSampleData(rsRet<0?audio_out_buffer:audio_resample_buffer, iSamples);

			sceKernelSignalSema(semaid, -1);

			curplayer->copiedSample+=iSamples;
		}

		if( ONE_SECOND_SAMPLE_COUNT < GetAvailSize() )
		{
			sceKernelDelayThread(1000);
		}
	} 

	curplayer->isPlaying=2;

wait: 

	if ( mp3_getEDRAM ) { 
		sceAudiocodecReleaseEDRAM(mp3_codec_buffer); 
	} 

	my_resample_close(&rstate);

	curplayer->CloseFileData();

	if(curplayer->abortFlag==0)
	{
		while(GetAvailSize()>0 && curplayer->abortFlag==0)
		{
			sceKernelDelayThread(25000);
		}

		curplayer->isPlaying=0;
	}
	else
	{
		curplayer->playedSample=curplayer->copiedSample=0;
		curplayer->isPlaying=0;

		ClearBuffers();
	}

	audioPlayMode=APM_PAUSED;

	return 1;
}

// aac file notes.
// 1. ID3 v1/v2 applies.
// 2. frame size is encoded in frame header
// 3. data buffer should be 64 byte aligned.
// 4. exclude header data from decoder input data.

unsigned long aac_codec_buffer[65] __attribute__((aligned(64))); 
int get_sample_rate(int sr_index)	// from faad2, modified.
{
    static const uint32_t sample_rates[] =
    {
        96000, 88200, 64000, 48000, 44100, 32000,
        24000, 22050, 16000, 12000, 11025, 8000
    };

    if (sr_index < 12)
        return sample_rates[sr_index];

    return 0;
}

int roundbitrate(int bitrate)
{
	static int roundtarget[11]={ 128000, 192000, 320000, 160000, 56000, 64000, 80000, 96000, 112000, 224000, 256000 };

	for(int i=0; i<11; i++)
	{
		if( roundtarget[i]-5000<bitrate && bitrate<roundtarget[i]+5000 )
		{
			return roundtarget[i];
		}
	}

	return bitrate;
}

int PlayAACFile(std::string2 &fileName)
{
	int getEDRAM=0; 
	int isCodecInited=0;
	SceInt64 totalFrameSize=0;
	int frameCount=0;

	myInitAVModules();

	isSeekable=0;

	musicClockBoost=meClockBoost;
	BoostCPU(0.5f);

	memset(audio_out_buffer, 0, sizeof(audio_out_buffer));

	audioPlayMode=APM_INITIALBUFFERING;

	curplayer->isPlaying=1;
	curplayer->playedSample=curplayer->copiedSample=0;

	if(curplayer->InitFileData(fileName)==0)
	{
		curplayer->isPlaying=0;

		sceKernelDelayThread(1000*1000);

		return 0;
	}

	/* try reading ID3 tag information now (else read later from stream) */
	{
		struct id3_file *file;

		file = id3_file_open(fileName.c_str(), ID3_FILE_MODE_READONLY);
		if (file)
		{
			struct id3_tag *tag=id3_file_tag(file);

			show_id3(curplayer, tag);
			id3_file_close(file);
		}
	}

	memset(&rstate, 0, sizeof(rstate));

	char id3v2ident[10]="012345678";
	file_read(&curplayer->_uc, (unsigned char*)id3v2ident, 10);
	int id3v2size=ID3v2TagSize(id3v2ident);

	file_seek(&curplayer->_uc, id3v2size, PSP_SEEK_SET); 
	curplayer->fileCurPos = file_tell(&curplayer->_uc);

	memset(aac_codec_buffer, 0, sizeof(aac_codec_buffer)); 

	if ( sceAudiocodecCheckNeedMem(aac_codec_buffer, 0x1003) < 0 ) 
		goto wait;

	if ( sceAudiocodecGetEDRAM(aac_codec_buffer, 0x1003) < 0 ) 
		goto wait; 
	getEDRAM = 1; 

	frameModified=1;

	unsigned char aac_header[7]; 

	while( curplayer->abortFlag==0 )
	{
		const int aacHeaderSize=7;
		const int sample_per_frame=1024;

		if(curplayer->seekStat==1)
		{
			if(GetAvailSize())
			{
				ClearBuffers();
			}
			sceKernelDelayThread(1000);
			continue;
		}

		if(curplayer->seekStat==2)
		{
			int i;
			for(i=0; i<curplayer->headerSampleWritePos-1; i++)
			{
				if(curplayer->seekSample<curplayer->headerSampleData[i+1] || curplayer->headerSampleData[i+1]==0 )
					break;
			}

			curplayer->fileCurPos=curplayer->headerFilePos[i];
			curplayer->headerSample=curplayer->headerSampleData[i];

			file_seek(&curplayer->_uc, curplayer->fileCurPos, PSP_SEEK_SET);
			curplayer->seekStat=3;

			rstate.ptime=0;
			rstate.last.l=rstate.last.r=0;
		}

		// log current file pos.
		curplayer->fileCurPos = file_tell(&curplayer->_uc);

		if ( file_read(&curplayer->_uc, aac_header, aacHeaderSize ) != aacHeaderSize )
		{ 
			break;
		} 
 
		// two type of header (adts-mp3 like, adif header)
		// 1. adts header (in old aac file), reverse engineered from faad.
		// 12 bit FFF
		// 1 bit : id 13
		// 2 bit : layer 14-15
		// 1 bit : protection_absent 16
		// 2 bit : profile 17-18
		// 4 bit : sf_index 19-22
		// 1 bit : private_bit 23
		// 3 bit : channel_configuration 24-26
		// 1 bit : original 27
		// 1 bit : home 28
		// 2 bit : emphasis, if old_format and id==0 else none.
		// 1 bit : copyright_identification_bit 
		// 1 bit : copyright_identification_start
		// 13 bit : aac_frame_length
		// 11 bit : adts_buffer_fullness
		// 2 bit : no_raw_data_blocks_in_frame
		// 16 bit : crc_check, if protection_absent-=0

		// start here.
		if( !(aac_header[0]==0xff && aac_header[1]&0xf0==0xf0 ) )
		{
			if( ResyncFFHeader(&curplayer->_uc)<0 )
			{
				break;
			}

			continue;
		}

		int id=(aac_header[1]&0x8)>>3;
//		int layer=(aac_header[2]&0xC0)>>6;
		int channel_configtag=(int)(aac_header[2]&0x1)<<2|(int)(aac_header[3]&0xc0)>>6;
		int sf_index=(int)(aac_header[2]&0x3c)>>2;
		int samplerate = get_sample_rate(sf_index);
		int frame_size;	

		if(2<channel_configtag)
		{
			goto wait;
		}

		if( samplerate==0 )
		{
			break;
		}
		if( id==0 )
		{
			// this case emphasis bit(2bit) bit used.
			frame_size=(int)(aac_header[3]&0x1)<<13|(int)(aac_header[4]&0xff)<<5|(int)(aac_header[5]&0xf8)>>3;
		}
		else
		{
			frame_size=(int)(aac_header[3]&0x3)<<11|(int)(aac_header[4]&0xff)<<3|(int)(aac_header[5]&0xe0)>>5;
		}

		totalFrameSize+=frame_size;
		frameCount++;

//		int averagebitrate=8*totalFrameSize/(frameCount*1024/(float)samplerate);
		int averagebitrate=(totalFrameSize*samplerate/frameCount)>>7;

		// exclude header size.
		frame_size = frame_size - 7; 

		curplayer->numChannel=2;
		curplayer->bitRate=roundbitrate(averagebitrate);
		curplayer->samplingRate=samplerate;
		if( curplayer->estimatedSeconds==0 && curplayer->bitRate )
			curplayer->estimatedSeconds=curplayer->fileLength/(curplayer->bitRate/8);

		if(rstate.inited==0)
		{
			my_resample_init(&rstate, 44100, samplerate, 2);
		}

		// write header pos.
		if( curplayer->headerSampleWritePos*44100 <= curplayer->headerSample)
		{
			if(curplayer->headerSampleWritePos<HEADER_POS_LOG_SIZE)
			{
				curplayer->headerFilePos[curplayer->headerSampleWritePos]=curplayer->fileCurPos;
				curplayer->headerSampleData[curplayer->headerSampleWritePos]=curplayer->headerSample;
				curplayer->headerSampleWritePos++;
			}
		}

		curplayer->headerSample+=sample_per_frame;

		if( curplayer->seekStat==3 )
		{
			if( curplayer->headerSample < curplayer->seekSample)
			{
				file_seek( &curplayer->_uc, frame_size, PSP_SEEK_CUR);
				continue;
			}
			else
			{
				curplayer->playedSample=curplayer->copiedSample=curplayer->headerSample;

				curplayer->seekStat=0;

				audioPlayMode=APM_INITIALBUFFERING;

				BoostCPU(0.5f);
			}
		}

		if ( file_read( &curplayer->_uc, audio_input_buffer, frame_size ) != frame_size )
		{ 
			break;
		} 

		aac_codec_buffer[10] = samplerate; 
		if(isCodecInited==0)
		{
			if(sceAudiocodecInit(aac_codec_buffer, 0x1003) < 0 )
			{
				break;
			}
			isCodecInited=1;
		}

		aac_codec_buffer[6] = (unsigned long)audio_input_buffer; 
		aac_codec_buffer[8] = (unsigned long)audio_out_buffer; 

		aac_codec_buffer[7] = frame_size; 
		aac_codec_buffer[9] = sample_per_frame * 4; 

		int res = sceAudiocodecDecode(aac_codec_buffer, 0x1003); 
		if ( res < 0 )
		{ 
			memset(audio_out_buffer, 0, sample_per_frame * 4);
		}

		int rsRet=my_resample(&rstate, audio_resample_buffer, audio_out_buffer, sample_per_frame);
		int iSamples=rsRet<0?-rsRet:rsRet;

		while(GetFreeSize()<iSamples)
		{
			sceKernelDelayThread(1000);

			if(curplayer->abortFlag)
				break;
		}

		if(curplayer->abortFlag)
			break;

		if(iSamples)
		{
			sceKernelWaitSema(semaid, 0, 0);

			sceKernelSignalSema(semaid, 1);
			PushSampleData(rsRet<0?audio_out_buffer:audio_resample_buffer, iSamples);

			sceKernelSignalSema(semaid, -1);

			curplayer->copiedSample+=iSamples;
		}

		if( ONE_SECOND_SAMPLE_COUNT < GetAvailSize() )
		{
			sceKernelDelayThread(1000);
		}
	} 

	curplayer->isPlaying=2;

wait: 

	if ( getEDRAM ) { 
		sceAudiocodecReleaseEDRAM(aac_codec_buffer); 
	} 

	my_resample_close(&rstate);

	curplayer->CloseFileData();

	if(curplayer->abortFlag==0)
	{
		while(GetAvailSize()>0 && curplayer->abortFlag==0)
		{
			sceKernelDelayThread(25000);
		}

		curplayer->isPlaying=0;
	}
	else
	{
		curplayer->playedSample=curplayer->copiedSample=0;
		curplayer->isPlaying=0;

		ClearBuffers();
	}

	audioPlayMode=APM_PAUSED;

	return 1;
}

#include "mp4ff.h"

uint32_t mp4_read_cb(void *user_data, void *buffer, uint32_t length)
{
	return file_read( (MyURLContext *)user_data, (unsigned char *)buffer, length);
}

uint32_t mp4_seek_cb(void *user_data, uint64_t position)
{
	offset_t ret=file_seek( (MyURLContext *)user_data, position, SEEK_SET);

	return (ret==position)?0:1;
}

int GetAudioTrack(mp4ff_t *mp4file)
{
//#define TRACK_UNKNOWN 0
#define TRACK_AUDIO   1
//#define TRACK_VIDEO   2
//#define TRACK_SYSTEM  3

	int32_t _totaltrack=mp4ff_total_tracks(mp4file);

	if( _totaltrack<=0 )
	{
		return -1;
	}

	for(int i=0; i<_totaltrack; i++)
	{
		if( mp4ff_get_track_type(mp4file, i)==TRACK_AUDIO )
		{
			return i;
		}
	}

	return -1;	
}

int PlayMP4File(std::string2 &fileName)
{
	int getEDRAM=0; 
	int isCodecInited=0;
	mp4ff_callback_t mp4cb = {0};
	mp4ff_t *mp4file=NULL;
	int aacTrack=-1;
	int curSample=0;

	myInitAVModules();

	isSeekable=1;

	musicClockBoost=meClockBoost;
	BoostCPU(0.5f);

	memset(audio_out_buffer, 0, sizeof(audio_out_buffer));

	audioPlayMode=APM_INITIALBUFFERING;

	curplayer->isPlaying=1;
	curplayer->playedSample=curplayer->copiedSample=0;

	if(curplayer->InitFileData(fileName)==0)
	{
		curplayer->isPlaying=0;

		sceKernelDelayThread(1000*1000);

		return 0;
	}

	mp4cb.read = mp4_read_cb;
	mp4cb.seek = mp4_seek_cb;
	mp4cb.user_data = (void*)&curplayer->_uc;

	mp4file=mp4ff_open_read(&mp4cb);
	if(!mp4file || (aacTrack=GetAudioTrack(mp4file))<0 )
	{
		curplayer->CloseFileData();

		curplayer->isPlaying=0;
		sceKernelDelayThread(1000*1000);

		return 0;
	}

	int32_t numSamples = mp4ff_num_samples(mp4file, aacTrack);
	int64_t playtime=mp4ff_get_track_duration(mp4file, aacTrack);
	uint32_t avg_bitrate = mp4ff_get_avg_bitrate(mp4file, aacTrack);
	uint32_t samplerate=mp4ff_get_sample_rate(mp4file, aacTrack);
	uint32_t _channel_count=mp4ff_get_channel_count(mp4file, aacTrack);
	uint32_t _audiotype=mp4ff_get_audio_type(mp4file, aacTrack);
	int32_t _tracktype=mp4ff_get_track_type(mp4file, aacTrack);
	int32_t _totaltrack=mp4ff_total_tracks(mp4file);
	int32_t _timescale=mp4ff_time_scale(mp4file, aacTrack);

	if(2<_channel_count)
	{
		goto wait;
	}

	curplayer->numChannel=2;
	curplayer->bitRate=avg_bitrate;
	curplayer->samplingRate=samplerate;
	curplayer->estimatedSeconds=playtime/samplerate;

	memset(&rstate, 0, sizeof(rstate));
	my_resample_init(&rstate, 44100, samplerate, 2);

	memset(aac_codec_buffer, 0, sizeof(aac_codec_buffer)); 

	if ( sceAudiocodecCheckNeedMem(aac_codec_buffer, 0x1003) < 0 ) 
		goto wait;

	if ( sceAudiocodecGetEDRAM(aac_codec_buffer, 0x1003) < 0 ) 
		goto wait; 
	getEDRAM = 1; 
		
	aac_codec_buffer[10] = samplerate; 
	if(sceAudiocodecInit(aac_codec_buffer, 0x1003) < 0 )
	{
		goto wait; 
	}

	frameModified=1;

	curSample=0;

	while( curplayer->abortFlag==0 && curSample<numSamples )
	{
		const int sample_per_frame=1024;

		if(curplayer->seekStat==1)
		{
			if(GetAvailSize())
			{
				ClearBuffers();
			}
			sceKernelDelayThread(1000);
			continue;
		}

		if( curplayer->seekStat==2 )
		{
			curSample=curplayer->seekSample/sample_per_frame;

			curplayer->playedSample=curplayer->copiedSample=curSample*sample_per_frame;

			curplayer->seekStat=0;

			audioPlayMode=APM_INITIALBUFFERING;
	
			BoostCPU(0.5f);

			rstate.ptime=0;
			rstate.last.l=rstate.last.r=0;
		}

		int frame_size = mp4ff_read_sample_v2(mp4file, aacTrack, curSample++, audio_input_buffer);
		if (frame_size==0)
		{
			break;
		}

		curplayer->headerSample+=sample_per_frame;

		aac_codec_buffer[6] = (unsigned long)audio_input_buffer; 
		aac_codec_buffer[8] = (unsigned long)audio_out_buffer; 

		aac_codec_buffer[7] = frame_size; 
		aac_codec_buffer[9] = sample_per_frame * 4; 

		int res = sceAudiocodecDecode(aac_codec_buffer, 0x1003); 
		if ( res < 0 )
		{
			memset(audio_out_buffer, 0, sample_per_frame * 4);
		}

		int rsRet=my_resample(&rstate, audio_resample_buffer, audio_out_buffer, sample_per_frame);
		int iSamples=rsRet<0?-rsRet:rsRet;

		while(GetFreeSize()<iSamples)
		{
			sceKernelDelayThread(1000);

			if(curplayer->abortFlag)
				break;
		}

		if(curplayer->abortFlag)
			break;

		if(iSamples)
		{
			sceKernelWaitSema(semaid, 0, 0);

			sceKernelSignalSema(semaid, 1);
			PushSampleData(rsRet<0?audio_out_buffer:audio_resample_buffer, iSamples);

			sceKernelSignalSema(semaid, -1);

			curplayer->copiedSample+=iSamples;
		}

		if( ONE_SECOND_SAMPLE_COUNT < GetAvailSize() )
		{
			sceKernelDelayThread(1000);
		}
	} 

	curplayer->isPlaying=2;

wait: 

	if ( getEDRAM ) { 
		sceAudiocodecReleaseEDRAM(aac_codec_buffer); 
	} 

	if( mp4file )
        mp4ff_close(mp4file);

	my_resample_close(&rstate);

	curplayer->CloseFileData();

	if(curplayer->abortFlag==0)
	{
		while(GetAvailSize()>0 && curplayer->abortFlag==0)
		{
			sceKernelDelayThread(25000);
		}

		curplayer->isPlaying=0;
	}
	else
	{
		curplayer->playedSample=curplayer->copiedSample=0;
		curplayer->isPlaying=0;

		ClearBuffers();
	}

	audioPlayMode=APM_PAUSED;

	return 1;
}

#endif

#define USE_OGGVORBIS
#ifdef USE_OGGVORBIS

#include "ivorbiscodec.h"
#include "ivorbisfile.h"

size_t ov_read_func(void *ptr, size_t size, size_t nmemb, void *datasource)
{
	return file_read( (MyURLContext *)datasource, (unsigned char *)ptr, size*nmemb);
}

int ov_seek_func(void *datasource, ogg_int64_t offset, int whence)
{
	offset_t ret=file_seek( (MyURLContext *)datasource, offset, whence);

	return (ret==offset)?0:1;
}
int ov_close_func(void *datasource)
{
	file_close( (MyURLContext *)datasource);

	return 0;
}
long ov_tell_func(void *datasource)
{
	return file_tell( (MyURLContext *)datasource);
}

int PlayOGGFile(std::string2 &fileName)
{
	isSeekable=1;

	OggVorbis_File vf;
	ov_callbacks read_callback={0, };

	int eof=0;
	int current_section;

	musicClockBoost=oggvorbisClockBoost;
	BoostCPU(2.5f);

	frameModified=1;

	memset(audio_out_buffer, 0, sizeof(audio_out_buffer));

	audioPlayMode=APM_INITIALBUFFERING;

	curplayer->isPlaying=1;
	curplayer->playedSample=curplayer->copiedSample=0;

	if(curplayer->InitFileData(fileName)==0)
	{
		curplayer->isPlaying=0;

		sceKernelDelayThread(1000*1000);

		return 0;
	}

	read_callback.read_func=ov_read_func;
	read_callback.seek_func=ov_seek_func;
	read_callback.close_func=ov_close_func;
	read_callback.tell_func=ov_tell_func;

	if(ov_open_callbacks(&curplayer->_uc, &vf, NULL, 0, read_callback) < 0)
	{
		sceKernelDelayThread(1000*1000);

		return 0;
	}

	{
		char **ptr=ov_comment(&vf,-1)->user_comments;
		vorbis_info *vi=ov_info(&vf,-1);

		curplayer->numChannel=vi->channels;
		curplayer->bitRate=vi->bitrate_nominal;
		curplayer->samplingRate=vi->rate;
		curplayer->estimatedSeconds=(long)ov_pcm_total(&vf,-1)/vi->rate;
	}

	memset(&rstate, 0, sizeof(rstate));
	my_resample_init(&rstate, 44100, curplayer->samplingRate, curplayer->numChannel);

	while(!eof && curplayer->abortFlag==0)
	{
		if(curplayer->seekStat==1)
		{
			if(GetAvailSize())
			{
				ClearBuffers();
			}
			sceKernelDelayThread(1000);
			continue;
		}

		if( curplayer->seekStat==2 )
		{
			int ret=ov_pcm_seek(&vf,curplayer->seekSample);

			if(ret<0)
			{
				curplayer->abortFlag=1;
				break;
			}

			curplayer->playedSample=curplayer->copiedSample=ov_pcm_tell(&vf);

			curplayer->seekStat=0;

			audioPlayMode=APM_INITIALBUFFERING;

			rstate.ptime=0;
			rstate.last.l=rstate.last.r=0;
		}

		if(audioPlayMode==APM_INITIALBUFFERING ||
			(curplayer->isPlaying==1 && GetAvailSize()<ONE_SECOND_SAMPLE_COUNT))
		{
			BoostCPU(2.0f);
		}

		long ret=ov_read(&vf,(char*)audio_out_buffer,sizeof(audio_out_buffer),&current_section);
//		long ret=ov_read(&vf,(char*)audio_out_buffer,sizeof(audio_out_buffer),0,2,1,&current_section);
		if (ret == 0)
		{
			/* EOF */
			eof=1;
			break;
		}
		else if (ret < 0)
		{
			/* error in the stream.  Not a problem, just reporting it in
			case we (the app) cares.  In this case, we don't. */
//			curplayer->abortFlag=1;
			continue;
		}

		int sample_per_frame=ret/4;
		int rsRet=my_resample(&rstate, audio_resample_buffer, audio_out_buffer, sample_per_frame);
		int iSamples=rsRet<0?-rsRet:rsRet;

		while(GetFreeSize()<iSamples)
		{
			sceKernelDelayThread(1000);

			if(curplayer->abortFlag)
				break;
		}

		if(curplayer->abortFlag)
			break;

		if(iSamples)
		{
			sceKernelWaitSema(semaid, 0, 0);

			sceKernelSignalSema(semaid, 1);
			PushSampleData(rsRet<0?audio_out_buffer:audio_resample_buffer, iSamples);

			sceKernelSignalSema(semaid, -1);

			curplayer->copiedSample+=iSamples;
		}

		if( ONE_SECOND_SAMPLE_COUNT < GetAvailSize() )
		{
			sceKernelDelayThread(1000);
		}
	}

	curplayer->isPlaying=2;

	ov_clear(&vf);

	my_resample_close(&rstate);

	curplayer->CloseFileData();

	if(curplayer->abortFlag==0)
	{
		while(GetAvailSize()>0 && curplayer->abortFlag==0)
		{
			sceKernelDelayThread(25000);
		}

		curplayer->isPlaying=0;
	}
	else
	{
		curplayer->playedSample=curplayer->copiedSample=0;
		curplayer->isPlaying=0;

		ClearBuffers();
	}

	audioPlayMode=APM_PAUSED;

	return 1;
}

#endif

char errorString[256]="no error";
int PlayMovieFile(std::string2 &fileName, int _isMP4);

int PlayOneFile(std::string2 &fileName)
{
	if( IsMatchExt(fileName.c_str(), "mp3") )
	{
		if( useLibMAD )
		{
			return PlayMP3File_MAD(fileName);
		}
		else
		{
			return PlayMP3File_ME(fileName);
		}
	}
	else if( IsMatchExt(fileName.c_str(), "aac") )
	{
		return PlayAACFile(fileName);
	}
	else if( IsMatchExt(fileName.c_str(), "mp4") )
	{
		return PlayMovieFile(fileName, 1);
	}
	else if( IsMatchExt(fileName.c_str(), "m4a") )
	{
		return PlayMP4File(fileName);
	}
	else if( IsMatchExt(fileName.c_str(), "ogg") )
	{
		return PlayOGGFile(fileName);
	}
	else if( IsMatchExt(fileName.c_str(), "avi") )
	{
		return PlayMovieFile(fileName, 0);
	}

	ClearBuffers();

	return 0;
}

#define MAX_PLAYLIST_ENTRY 500
#define MAXDIRNUM MAX_PLAYLIST_ENTRY

enum PlayerCommand
{ PC_NONE, PC_PREV, PC_PLAY, PC_STOP, PC_PAUSE, PC_NEXT, PC_PLAYNEWFILES, PC_RESTART };

PlayerCommand cmd=PC_NONE;
void *cmdData=NULL;
void SetCmd(PlayerCommand _c, void *_d)
{
	if(cmd!=PC_NONE)
		return;
	cmdData=_d;
	cmd=_c;
}

struct MultiFileData
{
	int num;
	std::string2 *names;
};
MultiFileData mfdCallBuffer;

std::string2 fileList[MAX_PLAYLIST_ENTRY];
std::string2 titleString[MAX_PLAYLIST_ENTRY];
int nextFile[MAX_PLAYLIST_ENTRY]={0};
int prevFile[MAX_PLAYLIST_ENTRY]={0};
int titleStringStatus=0;
int numFiles=0, playCursor=0, selectCursor=0, mp3ShowTop, mp3ShowMax=10, mp3YBase=3;
enum PlayerMode { PM_SEQUENTIAL=0, PM_RANDOM };
PlayerMode playerMode=PM_RANDOM;
int autoRepeat=1;

int RemakeNextFile(int start)
{
	int tmp[MAX_PLAYLIST_ENTRY];
	for(int i=0; i<MAX_PLAYLIST_ENTRY; i++)
	{
		tmp[i]=i;
	}

	int count=0, l, r, t;
	while(count<numFiles*3)
	{
		l=rand()%numFiles;
		r=rand()%numFiles;

		t=tmp[r];
		tmp[r]=tmp[l];
		tmp[l]=t;

		count++;
	}

	if(0<=start && start<numFiles)
	{
		for(int i=0; i<numFiles; i++)
		{
			if(tmp[i]==start)
			{
				t=tmp[i];
				tmp[i]=tmp[0];
				tmp[i]=t;
				break;
			}
		}
	}

	for(int i=0; i<numFiles; i++)
	{
		prevFile[tmp[i]]=i-1>=0?tmp[i-1]:-1;
		nextFile[tmp[i]]=i+1<numFiles?tmp[i+1]:-1;
	}

	return tmp[0];
}

int decodeThreadOn=1;
int DecodeThread(SceSize args, void *argp)
{
	int autoPlay=0;

	while(decodeThreadOn)
	{
		// process command.
		if(cmd!=PC_NONE)
		{
			switch(cmd)
			{
			case PC_PLAYNEWFILES:
				{
					MultiFileData *data=(MultiFileData*)cmdData;
					for(int i=0; i<data->num; i++)
					{
						fileList[i]=data->names[i].c_str();
					}

					numFiles=data->num;

					if(numFiles)
					{
/*					if(numFiles==0)
					{
						numFiles=1;
						fileList[0]="Please Load MP3 file.";
					}*/

					playCursor=0;
					selectCursor=0;
					autoPlay=1;

					titleStringStatus=1;

					if(playerMode==PM_RANDOM)
					{
						playCursor=RemakeNextFile(-1);
					}
					else
					{
						playCursor=0;
						RemakeNextFile(-1);
					}
					}
				}
				break;
			case PC_STOP:
				autoPlay=0;
				break;
			case PC_RESTART:
			case PC_PLAY:
				playCursor=selectCursor;
				autoPlay=1;
				break;
			case PC_PREV:
				if(playerMode==PM_RANDOM)
				{
					playCursor=prevFile[playCursor];

					if(playCursor==-1)
						playCursor=RemakeNextFile(-1);
				}
				else
				{
					playCursor=showplayer->fileIndex>0?showplayer->fileIndex-1:numFiles-1;
				}
				break;
			case PC_NEXT:
				if(playerMode==PM_RANDOM)
				{
					playCursor=nextFile[playCursor];

					if(playCursor==-1)
						playCursor=RemakeNextFile(-1);
				}
				else
				{
					playCursor=showplayer->fileIndex+1;
					if(playCursor==numFiles) playCursor=0;
				}
				break;
			}
			cmd=PC_NONE;
			cmdData=NULL;
		}

		if(autoPlay && numFiles>0)
		{
			if(playCursor<mp3ShowTop || playCursor>=mp3ShowTop+mp3ShowMax)//here cursor fix
			{
				mp3ShowTop=playCursor-mp3ShowMax/2;
				if(mp3ShowTop>numFiles-mp3ShowMax)
					mp3ShowTop=numFiles-mp3ShowMax;

				if(mp3ShowTop<0) mp3ShowTop=0;

				if( selectCursor<mp3ShowTop || playCursor>=mp3ShowTop+mp3ShowMax )
				{
					selectCursor=playCursor;
				}
			}

			curplayer->fileIndex=playCursor;

			PlayOneFile(fileList[playCursor]);

			if(cmd==PC_NONE)
			{
				int next=(playerMode==PM_RANDOM)?nextFile[playCursor]:playCursor+1;

				if(next==numFiles || next==-1)
				{
					if(autoRepeat)
					{
						playCursor=(playerMode==PM_RANDOM)?RemakeNextFile(-1):0;
					}
					else
					{
						if(playerMode==PM_RANDOM)
						{
							RemakeNextFile(playCursor);
						}
						autoPlay=0;
					}
				}
				else
				{
					playCursor=next;
				}
/*				if(playerMode==PM_RANDOM)
				{
					if(autoRepeat)
					{
						playCursor=nextFile[playCursor];
						if(playCursor==-1)
							playCursor=RemakeNextFile(-1);
					}
					else
					{
						if(nextFile[playCursor]==-1)
						{
							playCursor=RemakeNextFile(-1);
							autoPlay=0;
						}
						else
						{
							playCursor=nextFile[playCursor];
						}
					}
				}
				else
				{
					if(autoRepeat)
					{
						playCursor=++curplayer->fileIndex;
						playCursor=playCursor%numFiles;
					}
					else
					{
						if(curplayer->fileIndex+1==numFiles)
						{

						}
						else
						{

							autoPlay=0;
						}
					}
				}*/
			}
			else
			{
				if(audioPlayMode==APM_PAUSED)
					ClearBuffers();
			}
		}

		// file end flag...

		if(cmd==PC_NONE)
		{
			sceKernelDelayThread(1000*10);
		}
	}

	decodeThreadOn=-1;

	return 0;
}

void MP3PrevTrack()
{
	if(curplayer->isPlaying==0)
		return;

	if(curplayer->isPlaying && curplayer->playedSample/44100>5 && isSeekable)
	{
		audioPlayMode=APM_PAUSED;
		curplayer->seekStat=1;

		sceKernelDelayThread(1000*100);

		curplayer->seekSample=0;
		curplayer->seekStat=2;

		audioPlayMode=APM_INITIALBUFFERING;
	}
	else
	{
		audioPlayMode=APM_PAUSED;
		cmd=PC_PREV;

		curplayer->abortFlag=1;
	}
}

void MP3NextTrack()
{
	if(curplayer->isPlaying==0)
		return;

	audioPlayMode=APM_PAUSED;
	cmd=PC_NEXT;

	curplayer->abortFlag=1;
}

void MP3PlayTrack()
{
	audioPlayMode=APM_PAUSED;

	cmd=PC_PLAY;

	curplayer->abortFlag=1;
}

void MP3StopTrack()
{
	audioPlayMode=APM_PAUSED;
	cmd=PC_STOP;

	curplayer->abortFlag=1;
}

void Mp3ExtMenuControl(SceCtrlData &pad, unsigned int bPress)
{
	if(bPress & PSP_CTRL_CIRCLE)
	{
		autoRepeat=autoRepeat?0:1;
		extMenu=0;
	}
	if(bPress & PSP_CTRL_TRIANGLE)
	{
		showTagOnly=showTagOnly?0:1;
		extMenu=0;
	}
	if(bPress & PSP_CTRL_DOWN)
	{
		switch(showBG)
		{
		case 0:
			if(strlen(bgPath))
			{
				showBG=1;
			}
			else
			{
				showBG=2;
			}
			curBG=-1;
			break;
		case 1:
			showBG=2;
			curBG=-1;
			break;
		case 2:
		default:
			showBG=0;
			break;
		}
		LoadBackground();

		extMenu=0;
	}
	if(bPress & PSP_CTRL_RIGHT)
	{
		InitialLoad(0, 0);
		extMenu=0;
	}
	if(pad.Ly==0)
	{
		sleepCounter+=180;
		sceKernelDelayThread(1000*100);
	}
	else if(pad.Ly==255)
	{
		if(sleepCounter<240)
			sleepCounter=0;
		else
			sleepCounter-=180;
		sceKernelDelayThread(1000*100);
	}

	if(bPress & PSP_CTRL_SELECT)
	{
		showMode=5;
		extMenu=0;
	}
	if(bPress & PSP_CTRL_START)
	{
		extMenu=0;
	}
}

int isVideoSeeking=0;
int seekDir=0;
void VideoControl(SceCtrlData &pad, unsigned int bPress)
{
	extMenu=0;

	int seekOffsetSeconds=0;
	if(bPress & PSP_CTRL_UP)
	{
		seekOffsetSeconds=-30;
	}
	else if(bPress & PSP_CTRL_DOWN)
	{
		seekOffsetSeconds=30;
	}
	else if(bPress & PSP_CTRL_LEFT)
	{
		if(audioPlayMode == APM_NORMAL)
		{
			seekOffsetSeconds=-5;
		}
		else
		{
			seekOffsetSeconds=-12;
		}
		seekOffsetSeconds=-1;
	}
	else if(bPress & PSP_CTRL_RIGHT)
	{
		seekOffsetSeconds=1;
	}
	else if(bPress & PSP_CTRL_LTRIGGER)
	{
		seekOffsetSeconds=-300;
	} 
	else if(bPress & PSP_CTRL_RTRIGGER)
	{
		seekOffsetSeconds=300;
	}

	int seekkeys=PSP_CTRL_UP|PSP_CTRL_DOWN|PSP_CTRL_LEFT|PSP_CTRL_RIGHT|PSP_CTRL_LTRIGGER|PSP_CTRL_RTRIGGER;
	if( pad.Buttons & seekkeys)
	{
		isVideoSeeking=1;
	}
	else
	{
		isVideoSeeking=0;
	}
	//static int prevseeking=0;
	//if(prevseeking!=isVideoSeeking)
	//{
	//	fprintf(stderr, "isVideoSeeking %d\n", isVideoSeeking);
	//}
	//prevseeking=isVideoSeeking;
	if(isSeekable && seekOffsetSeconds && curplayer->seekStat==0)
	{
		if(seekOffsetSeconds<0)
		{
			seekDir=-1;
		}
		else
		{
			seekDir=1;
		}

		ClearBuffers();

		sceKernelDelayThread(1000*50);

		int seekOffset=curplayer->playedSample+seekOffsetSeconds*44100;

		if(seekOffset<0)
			seekOffset=0;
		if(seekOffset>curplayer->estimatedSeconds*44100)
			seekOffset=curplayer->estimatedSeconds*44100;

		curplayer->seekSample=curplayer->playedSample=curplayer->copiedSample=seekOffset;
		curplayer->seekStat=1;
	}

	//if(bPress & PSP_CTRL_CIRCLE)
	//{
	//	MP3PlayTrack();
	//}
	if(bPress & PSP_CTRL_CROSS)
	{
		MP3StopTrack();
	}
	if(bPress & PSP_CTRL_SQUARE)
	{
		extern int enableVideoTitle;
		enableVideoTitle=(enableVideoTitle+1)%3;

		memset(titleBuffer, 0, 512*32*4);
	}
	if(bPress & PSP_CTRL_TRIANGLE)
	{
		extern int scaleMode;
		void AdjustVideoRenderingFactors();

		scaleMode=(++scaleMode)%5;
		AdjustVideoRenderingFactors();
	}

	if(bPress & PSP_CTRL_START)
	{
		if(audioPlayMode!=APM_INITIALBUFFERING)
		{
			audioPlayMode=(audioPlayMode==APM_PAUSED)?APM_NORMAL:APM_PAUSED;
		}
	}

	//if( (pad.Ly==255 || pad.Ly==0) && (pad.Buttons & (PSP_CTRL_LEFT|PSP_CTRL_RIGHT)) )
	//{
	//	volumeBoost=1.0f;
	//}
	if(pad.Ly==255)
	{
		float step=(volumeBoost<=5.01f)?0.1f:1.0f;
		volumeBoost=max(1.0f, volumeBoost-step);
	}
	else if(pad.Ly==0)
	{
		float step=(volumeBoost<4.99f)?0.1f:1.0f;
		volumeBoost=min(20.0f, volumeBoost+step);
	}
	
	if( fabsf(volumeBoost-1.0f)<0.01f )
	{
		volumeBoost=1;
	}

	//if(isSeekable==0)
	//{
	//	return;
	//}

	//if( curplayer->isPlaying==1 && curplayer->seekStat<2 && (pad.Lx==255 || pad.Lx==0) )
	//{
	//	audioPlayMode=APM_PAUSED;

	//	sceKernelDelayThread(1000*50);

	//	if( GetAvailSize() )
	//	{
	//		ClearBuffers();
	//	}

	//	int cursec=curplayer->playedSample/44100;
	//	int seekOffset=(int)(cursec/5)*5*44100;
	//	seekOffset+=(pad.Lx==0)?(-5*44100):(5*44100);

	//	if(seekOffset<0)
	//		seekOffset=0;

	//	curplayer->seekStat=1;
	//	curplayer->playedSample=curplayer->copiedSample=seekOffset;

	//	if(curplayer->playedSample>curplayer->estimatedSeconds*44100)
	//		curplayer->playedSample=curplayer->estimatedSeconds*44100;
	//}
	//else if(curplayer->seekStat==1)
	//{
	//	curplayer->seekSample=curplayer->playedSample;
	//	curplayer->seekStat=2;
	//}
}

void Mp3PlayerControl(SceCtrlData &pad, unsigned int bPress)
{
//{ PC_NONE, PC_PREV, PC_PLAY, PC_STOP, PC_PAUSE, PC_NEXT, PC_PLAYSINGLE, PC_PLAYMULTIPLE, PC_PLAYLIST, PC_PLAYDIRECTORY };

	if(cmd!=PC_NONE || numFiles==0)
	{
		return;
	}

	if(extMenu)
	{
		Mp3ExtMenuControl(pad, bPress);

		return;
	}

	if(bPress & PSP_CTRL_UP)
	{
		selectCursor--;

		if(selectCursor==-1)
		{
			selectCursor=numFiles-1;
			mp3ShowTop=numFiles-mp3ShowMax; if(mp3ShowTop<0) mp3ShowTop=0;

		}
		if(selectCursor<mp3ShowTop)
		{
			mp3ShowTop=selectCursor;
		}
	}
	else if(bPress & PSP_CTRL_DOWN)
	{
		selectCursor++;

		if(selectCursor==numFiles)
		{
			selectCursor=0;
			mp3ShowTop=0;

		}
		if(selectCursor>=mp3ShowTop+mp3ShowMax)
		{
			mp3ShowTop=selectCursor-mp3ShowMax+1;
		}
	}
	else if(bPress & PSP_CTRL_LEFT)
	{
		if(selectCursor>mp3ShowTop)
			selectCursor=mp3ShowTop;
		else
		{
			selectCursor-=mp3ShowMax;
			mp3ShowTop=selectCursor;

			if(selectCursor<0)
			{
				mp3ShowTop=selectCursor=0;
			}
		}
	}
	else if(bPress & PSP_CTRL_RIGHT)
	{
		if(selectCursor<mp3ShowTop+mp3ShowMax-1)
			selectCursor=mp3ShowTop+mp3ShowMax-1;
		else
		{
			selectCursor+=mp3ShowMax;
			mp3ShowTop+=mp3ShowMax;
		}

		if(selectCursor>=numFiles)
		{
			selectCursor=numFiles-1;
			mp3ShowTop=numFiles-mp3ShowMax;
			if(mp3ShowTop<0)
				mp3ShowTop=0;
		}
	}

	if(bPress & PSP_CTRL_CIRCLE)
	{
		MP3PlayTrack();
	}
	if(bPress & PSP_CTRL_CROSS)
	{
		MP3StopTrack();
	}
	if(bPress & PSP_CTRL_SQUARE)
	{
		playerMode=(playerMode==PM_SEQUENTIAL)?PM_RANDOM:PM_SEQUENTIAL;
	}
	if(bPress & PSP_CTRL_TRIANGLE)
	{
		audioPlayMode=(audioPlayMode==APM_PAUSED)?APM_NORMAL:APM_PAUSED;
	}

	if(bPress & PSP_CTRL_LTRIGGER)
	{
		MP3PrevTrack();
	} 
	if(bPress & PSP_CTRL_RTRIGGER)
	{
		MP3NextTrack();
	}
	if(bPress & PSP_CTRL_START)
	{
		extMenu=1;
		titleType=titleType?0:1;
	}

	//if( (pad.Ly==255 || pad.Ly==0) && (pad.Buttons & (PSP_CTRL_LEFT|PSP_CTRL_RIGHT)) )
	//{
	//	volumeBoost=1.0f;
	//}
	else if(pad.Ly==255)
	{
		float step=(volumeBoost<=5.01f)?0.1f:1.0f;
		volumeBoost=max(1.0f, volumeBoost-step);
	}
	else if(pad.Ly==0)
	{
		float step=(volumeBoost<4.99f)?0.1f:1.0f;
		volumeBoost=min(20.0f, volumeBoost+step);
	}

	if( fabsf(volumeBoost-1.0f)<0.01f )
	{
		volumeBoost=1;
	}

	if(isSeekable==0)
	{
		return;
	}

	if( curplayer->isPlaying==1 && curplayer->seekStat<2 && (pad.Lx==255 || pad.Lx==0) )
	{
		audioPlayMode=APM_PAUSED;

		ClearBuffers();

		sceKernelDelayThread(1000*50);

		int cursec=curplayer->playedSample/44100;
		int seekOffset=(int)(cursec/5)*5*44100;
		seekOffset+=(pad.Lx==0)?(-5*44100):(5*44100);

		if(seekOffset<0)
			seekOffset=0;

		curplayer->playedSample=curplayer->copiedSample=seekOffset;

		if(curplayer->playedSample>curplayer->estimatedSeconds*44100)
			curplayer->playedSample=curplayer->estimatedSeconds*44100;

		curplayer->seekStat=1;
	}
	else if(curplayer->seekStat==1)
	{
		curplayer->seekSample=curplayer->playedSample;
		curplayer->seekStat=2;
	}
}


void ShowMp3()
{
	char sbuf[STRING_LENGTH];
	char tagString[STRING_LENGTH];
	char *cursorString;
	char numberString[10];
	if(numFiles==0)
	{
		textTool.PutStringMByteCC(0, mp3YBase, selectedTextColor, "Please load mp3 file... SELECT: Browse file");
		return;
	}
	
	int yValue=mp3YBase;
	unsigned long color=0;
	int showPlayCursor=-1;
	if(showplayer->isPlaying || (curplayer!=showplayer && showplayer->isPlaying==0 && showplayer->copiedSample && showplayer->playedSample && showplayer->copiedSample) )
	{
		showPlayCursor=showplayer->fileIndex;
	}

	for(int i=mp3ShowTop; i<numFiles && i<mp3ShowTop+mp3ShowMax; i++)
	{
		cursorString="  ";
		if(selectCursor==i && showPlayCursor==i)
		{
			color=playingTextColor;
			textTool.PutStringMByteCC(0, yValue, selectedTextColor, "▶");
		}
		else if(selectCursor==i)
		{
			color=selectedTextColor;
			cursorString="▶";
		}
		else if(showPlayCursor==i)
		{
			color=playingTextColor;
		}
		else
		{
			color=generalTextColor;
		}

		if(numFiles<100)
		{
			snprintf(numberString, 10, "%02d: ", i+1);
		}
		else
		{
			snprintf(numberString, 10, "%03d: ", i+1);
		}

		char *mp3filename=strrchr(fileList[i].c_str(), '/');
		if(mp3filename)
		{
			mp3filename++;
		}
		else
		{
			mp3filename=(char*)fileList[i].c_str();
		}

		if(titleStringStatus==2 && titleString[i].c_str()[0])
		{
			snprintf(tagString, STRING_LENGTH, "(%s)", titleString[i].c_str());
		}
		else
		{
			tagString[0]=0;
		}

		if(showTagOnly && tagString[0])
			snprintf(sbuf, STRING_LENGTH, "%s%sMP3%s", cursorString, numberString, tagString);
		else
			snprintf(sbuf, STRING_LENGTH, "%s%s%s %s", cursorString, numberString, mp3filename, tagString);
		textTool.PutStringMByteCC(0, yValue, color, sbuf);

		yValue++;
	}

	//○□ΧㅁㄴㅇㄹΧㅋㅌㅊㅂⅩ×ㄷㄱㅅㅁ
	snprintf(sbuf, STRING_LENGTH, "○: Play, Χ: Stop, □: RANDOM, △: Pause, L1/R1: Track, START:Ext menu");
	textTool.PutStringMByteCC(0, 20, generalTextColor, sbuf);
	snprintf(sbuf, STRING_LENGTH, "▲▼◀▶: Navigate, Analog: (L/R=Seek),(U/D=Volume Boost), SELECT: Browse file");
	//------------------------------------------------------------------------------------------------------------
	//snprintf(sbuf, STRING_LENGTH, "▲▼◀▶: Navigate, L1: Prev track, R1: Next track, SELECT: Browse file");
	textTool.PutStringMByteCC(0, 21, generalTextColor, sbuf);

/*	for(int i=0; i<7; i++)
	{
		snprintf(sbuf, STRING_LENGTH, "%d %d %d", i, curplayer->headerFilePos[i], curplayer->headerSampleData[i]);
		textTool.PutStringMByteCC(2, 13+i, generalTextColor, sbuf);

	}*/

//	if(curplayer->isPlaying)
	{
		snprintf(sbuf, STRING_LENGTH, "Title   : %s", curplayer->title.c_str());
		textTool.PutStringMByteCC(2, 14, generalTextColor, sbuf);

		snprintf(sbuf, STRING_LENGTH, "Artist  : %s", curplayer->artist.c_str());
		textTool.PutStringMByteCC(2, 15, generalTextColor, sbuf);

		snprintf(sbuf, STRING_LENGTH, "Album   : %s %s %s", curplayer->album.c_str(), curplayer->track.c_str(), curplayer->year.c_str());
		textTool.PutStringMByteCC(2, 16, generalTextColor, sbuf);

		snprintf(sbuf, STRING_LENGTH, "Genre   : %s", curplayer->genre.c_str());
		textTool.PutStringMByteCC(2, 17, generalTextColor, sbuf);

		snprintf(sbuf, STRING_LENGTH, "Comment : %s", curplayer->comment.c_str());
		textTool.PutStringMByteCC(2, 18, generalTextColor, sbuf);
	}

	char extMenuString[] = "\
┌────────────┐\n\
│  ○: Toggle repeat mode│\n\
│  △: Show tag only     │\n\
│  ▼: Toggle background │\n\
│  ▶: Reload option     │\n\
│  Alalog: Sleep counter │\n\
│  SELECT: USB menu      │\n\
│                        │\n\
│                        │\n\
│                        │\n\
│                        │\n\
│                        │\n\
│  START: Close menu     │\n\
└────────────┘\n\
";
	if(extMenu)
	{
		display.FillBackgroundPtr(workBuffer, 0x80000000, 13*12+3, 12*4, 28*6+2, 12*14+7);
		textTool.PutStringMByteCC(13, 4, generalTextColor, extMenuString);
	}

}

int numEntry, curSel, numDir, numFile, showTop, showMax=14;
SceIoDirent	dirEntry[MAXDIRNUM];
std::string2 dirTitleString[MAXDIRNUM];
int dirTitleStringStatus=0;
std::string2 curPath="ms0:/psp/music";
#define IS_DIR(ent) (ent.d_stat.st_attr&FIO_SO_IFDIR)
std::string2 fileNameBuffer[MAX_PLAYLIST_ENTRY];
int maxNameLen=44;
int dirYposBase=5;
int curImageIndex=0;
int IsMatchExt(const char *fileName, const char *ext)
{
	char *point=strrchr(fileName, '.');

	if(point==NULL)
		return 0;

	point++;

	return (stricmp(point, ext)==0);

	return 1;
}

int CompareDir(const SceIoDirent &a, const SceIoDirent &b)
{
	if(IS_DIR(a) && !IS_DIR(b))
	{
		return -1;
	}
	else if(!IS_DIR(a) && IS_DIR(b))
	{
		return 1;
	}

	return stricmp(a.d_name, b.d_name);
}

void SortDirEntry()
{
	SceIoDirent temp;

	for(int i=0; i<numEntry; i++)
	{
		SceIoDirent &left=dirEntry[i];

		if(!strcmp(left.d_name, ".."))
			continue;

		for(int j=i; j<numEntry; j++)
		{
			SceIoDirent &right=dirEntry[j];

			if(CompareDir(left, right)>0)
			{
				temp=right;
				right=left;
				left=temp;
			}
		}
	}
}

void AddDummyDir(char *dirName)
{
	memset(&dirEntry[numEntry], 0, sizeof(dirEntry[0]));
	dirEntry[numEntry].d_stat.st_attr=FIO_SO_IFDIR;
	strcpy(dirEntry[numEntry].d_name, dirName);

	numEntry++;
}

int MakeDummyDir()
{
	AddDummyDir("ms0:");
	AddDummyDir("host0:");
	AddDummyDir("usbhost0:");
	AddDummyDir("nethost0:");

	showTop=0;
	curSel=0;

	return 0;
}

int GetDirFileList(char *path)
{
	int ret,file;

	if(path)
		curPath=path;

	numEntry=0;
	numDir=0;
	numFile=0;
	showTop=0;
	curSel=0;

	if( strcmp(path, "psp:")==0 )
	{
		return MakeDummyDir();
	}

	// Directory read
	file = sceIoDopen(curPath.c_str());
	AddDummyDir("..");

	if(file<0)
		return 0;

	do
	{
		ret=sceIoDread(file, &dirEntry[numEntry]);

		if(ret<=0)
			break;

		if(strcmp(dirEntry[numEntry].d_name, ".")==0)
			continue;
		if(strcmp(dirEntry[numEntry].d_name, "..")==0)
		{
			//numDir++;
			//numEntry++;
			continue;
		}
		if(IS_DIR(dirEntry[numEntry]))
		{
			numDir++;
			numEntry++;
			continue;
		}

		switch(filterMode)
		{
		case FM_ALL:
		default:
			numEntry++;
			numFile++;
			break;
		case FM_MP3:
			if(IsMatchExt(dirEntry[numEntry].d_name, "mp3"))
			{
				numEntry++;
				numFile++;
			}
			break;
		case FM_M3U:
			if(IsMatchExt(dirEntry[numEntry].d_name, "m3u"))
			{
				numEntry++;
				numFile++;
			}
			break;
		case FM_TXT:
			if(IsMatchExt(dirEntry[numEntry].d_name, "txt"))
			{
				numEntry++;
				numFile++;
			}
			break;
		case FM_BMP:
			if(IsMatchExt(dirEntry[numEntry].d_name, "bmp"))
			{
				numEntry++;
				numFile++;
			}
			break;
		case FM_PLAYABLE:
			if(IsMatchExt(dirEntry[numEntry].d_name, "mp3"))
			{
				numEntry++;
				numFile++;
			}
			else if(IsMatchExt(dirEntry[numEntry].d_name, "m3u"))
			{
				numEntry++;
				numFile++;
			}
			else if(IsMatchExt(dirEntry[numEntry].d_name, "txt"))
			{
				numEntry++;
				numFile++;
			}
			else if(IsMatchExt(dirEntry[numEntry].d_name, "bmp"))
			{
				numEntry++;
				numFile++;
			}
			break;
		}
		
	} while(numEntry<MAXDIRNUM);

	sceIoDclose(file);

	SortDirEntry();
	dirTitleStringStatus=1;

	return 1;
}

void Eat0x0d0x0a(char *ptr);
char *EatPrecedingWhites(char *ptr)
{
	char *cur=ptr;

	while(*cur!=0)
	{
		if(*cur==' ' || *cur=='\t')
		{
			cur++;
			continue;
		}

		break;
	}

	return *cur?cur:NULL;
}

void FixM3UName(char *src)
{
	while(*src)
	{
		if(*src=='\\')
		{
			*src='/';
		}
		else  if(0<*src /*&& *src<128*/)
		{
			src++;
		}
		else
		{
			*src++=-127;
			*src++=-95;
		}
	}
}

void ParseM3UFile(const char *fileName, const char *path=NULL)
{
	FILE *file=fopen(fileName, "r");

	if(file==NULL)
		return;

	char strBuf[200];

	mfdCallBuffer.num=0;
	mfdCallBuffer.names=fileNameBuffer;

	while(fgets(strBuf, 200, file))
	{
		Eat0x0d0x0a(strBuf);

		char *text=EatPrecedingWhites(strBuf);

		if(text==NULL)
			continue;

		if(strlen(text)==0)
		{
			continue;
		}

		if(*text=='#')
			continue;

		if(strnicmp("ms0:", text, 4)!=0 && path!=NULL)
		{
			FixM3UName(text);

			int IsPlayableAudio(const char *fileName);
			if( IsPlayableAudio(text)==0 )
				continue;

			if(path)
				BuildPath(fileNameBuffer[mfdCallBuffer.num].str, path, text);
		}
		else
		{
			strncpy(fileNameBuffer[mfdCallBuffer.num].str, text, STRING_LENGTH);
		}

		mfdCallBuffer.num++;
		if(mfdCallBuffer.num==MAX_PLAYLIST_ENTRY)
			break;
	}

	fclose(file);
}

int LoadTextFile(const char *fileName);
void SetLineFromPos(int pos);
void RefineLineFromPos(int pos);
void LoadText();
int IsPlayableAudio(const char *fileName)
{
	if( IsMatchExt(fileName, "mp3") )
		return 1;
	if( IsMatchExt(fileName, "aac") )
		return 1;
	if( IsMatchExt(fileName, "m4a") )
		return 1;
	if( IsMatchExt(fileName, "mp4") )
		return 1;
	if( IsMatchExt(fileName, "ogg") )
		return 1;
	if( IsMatchExt(fileName, "avi") )
		return 1;

	return 0;
}

int SelectFile(SceIoDirent &ent)
{
	char path[256];
	if(strcmp(ent.d_name, "..")==0)
	{
		if( stricmp(curPath.c_str(), "ms0:")==0 || stricmp(curPath.c_str(), "host0:")==0
			|| stricmp(curPath.c_str(), "nethost0:")==0
			|| stricmp(curPath.c_str(), "usbhost0:")==0
			)
		{
			strcpy(path, "psp:");

			GetDirFileList(path);
			return 0;
		}

		strcpy(path, curPath.c_str());
		char * p = strrchr(path, '/');
		if(p)
		{
			*p = 0;
			GetDirFileList(path);
		}

		return 0;
	}

	if(IS_DIR(ent))
	{
		if(strcmp(curPath.c_str(), "psp:")==0)
		{
			strcpy(path, ent.d_name);
		}
		else
		{
			BuildPath(path, curPath.c_str(), ent.d_name);
		}
        
		GetDirFileList(path);

		return 0;
	}

	if( IsPlayableAudio(ent.d_name) )
	{
		BoostCPU(30.0f);

		mfdCallBuffer.num=1;
		mfdCallBuffer.names=fileNameBuffer;
		BuildPath(fileNameBuffer[0].str, curPath.c_str(), ent.d_name);

		audioPlayMode=APM_PAUSED;

		curplayer->abortFlag=1;

		SetCmd(PC_PLAYNEWFILES, (void*)&mfdCallBuffer);

		return 1;
	}
	else if(IsMatchExt(ent.d_name, "m3u"))
	{
		BuildPath(path, curPath.c_str(), ent.d_name);
		ParseM3UFile(path, curPath.c_str());

		audioPlayMode=APM_PAUSED;

		curplayer->abortFlag=1;

		SetCmd(PC_PLAYNEWFILES, (void*)&mfdCallBuffer);

		return 1;
	}
	else if(IsMatchExt(ent.d_name, "txt"))
	{
		BuildPath(path, curPath.c_str(), ent.d_name);

		if(LoadTextFile(path))
		{
			LoadText();
            return 2;
		}
		else
			return 0;
	}
	else if(GetImageTypeIndex(ent.d_name))
	{
		showBG=1;
		curBG=-1;

		BuildPath(bgPath, curPath.c_str(), ent.d_name);
		LoadBackground();

		if(showMode==1)
		{
			hideUI=1;
			curImageIndex=curSel;
		}

		return 0;
	}
/*	else if(IsMatchExt(ent.d_name, "nff"))
	{
		BuildPath(path, curPath.c_str(), ent.d_name);
		textTool.Initialize(path);

		return 0;
	}*/

	return 0;
}

void SelectAll()
{
	int numFound=0;

	for(int i=0; i<numEntry; i++)
	{
		SceIoDirent &ent=dirEntry[i];
		if(IS_DIR(ent))
			continue;

		if( IsPlayableAudio(ent.d_name)==0)
			continue;

		BuildPath(fileNameBuffer[numFound].str, curPath.c_str(), ent.d_name);
		numFound++;

		if(numFound==MAX_PLAYLIST_ENTRY)
			break;
	}
	mfdCallBuffer.num=numFound;
	mfdCallBuffer.names=fileNameBuffer;

	audioPlayMode=APM_PAUSED;

	curplayer->abortFlag=1;

	SetCmd(PC_PLAYNEWFILES, (void*)&mfdCallBuffer);
}

void MakeM3uFile()
{
	int numFound=0;

	for(int i=0; i<numEntry; i++)
	{
		SceIoDirent &ent=dirEntry[i];
		if(IS_DIR(ent))
			continue;

		numFound++;
	}

	if(numFound==0)
		return;

	char buf[STRING_LENGTH];
	char name[STRING_LENGTH];

	strncpy(buf, curPath.c_str()+4, STRING_LENGTH);

	char *dirName=strrchr(buf, '/');
	FILE *file;

	if(dirName==NULL)
	{
		file=fopen("ms0:/PSP/MUSIC/Root.m3u", "w");
	}
	else
	{
		snprintf(name, STRING_LENGTH, "ms0:/PSP/MUSIC%s.m3u", dirName);
		file=fopen(name, "w");
	}

	if(file==NULL)
	{
		return;
	}

	for(int i=0; i<numEntry; i++)
	{
		SceIoDirent &ent=dirEntry[i];
		if(IS_DIR(ent))
			continue;

		BuildPath(buf, curPath.c_str(), ent.d_name);
		fprintf(file, "%s\n", buf);
	}

	fclose(file);
}

int isWaitForDelete=0;
std::string2 delFileName="";
int DirListExtMenuControl(SceCtrlData &pad, unsigned int bPress)
{
	if(bPress & PSP_CTRL_LTRIGGER)
	{
		GetDirFileList("ms0:/psp/music");
		hideUI=0;
		extMenu=0;
	}
	if(bPress & PSP_CTRL_RTRIGGER)
	{
		GetDirFileList("ms0:/text");
		hideUI=0;
		extMenu=0;
	}
	if(bPress & PSP_CTRL_LEFT)
	{
		GetDirFileList("ms0:/psp/photo");
		hideUI=0;
		extMenu=0;
	}

	if(bPress & PSP_CTRL_CIRCLE)
	{
		SelectAll();
		extMenu=0;

		return 1;
		//showDateSize=showDateSize?0:1;
		//extMenu=0;
	}
	if(bPress & PSP_CTRL_CROSS)
	{
		SceIoDirent &ent=dirEntry[curSel];

		if(IS_DIR(ent)==0)
		{
			isWaitForDelete=1;

			delFileName=ent.d_name;
			//BuildPath(delFileName, curPath.c_str(), ent.d_name);
		}
		extMenu=0;
		return 0;
	}
	if(bPress & PSP_CTRL_TRIANGLE)
	{
		showTagOnly=showTagOnly?0:1;
		extMenu=0;
	}
	if(bPress & PSP_CTRL_SQUARE)
	{
		MakeM3uFile();
		extMenu=0;
	}

	if(bPress & PSP_CTRL_DOWN)
	{
		switch(showBG)
		{
		case 0:
			if(strlen(bgPath))
			{
				showBG=1;
			}
			else
			{
				showBG=2;
			}
			curBG=-1;
			LoadBackground();
			break;
		case 1:
			showBG=2;
			curBG=-1;
			LoadBackground();
			break;
		case 2:
		default:
			showBG=0;
			break;
		}

		extMenu=0;
	}
	if(bPress & PSP_CTRL_RIGHT)
	{
		InitialLoad(0, 0);
		extMenu=0;
	}

	if(pad.Ly==0)
	{
		sleepCounter+=180;
		sceKernelDelayThread(1000*100);
	}
	else if(pad.Ly==255)
	{
		if(sleepCounter<240)
			sleepCounter=0;
		else
			sleepCounter-=180;
		sceKernelDelayThread(1000*100);
	}

	if(bPress & PSP_CTRL_SELECT)
	{
		showMode=5;
		extMenu=0;
	}
	if(bPress & PSP_CTRL_START)
	{
		extMenu=0;
	}

	return 0;
}

void FixImageParam()
{
	// fix ix, iy...
	int maxx=(int)(ImageFile::bgImage1.sizeX/zoomfactor);
	int maxy=(int)(ImageFile::bgImage1.sizeY/zoomfactor);
	int ixmax=(int)(ImageFile::bgImage1.sizeX-480*zoomfactor); if(ixmax<0) ixmax=0;
	int iymax=(int)(ImageFile::bgImage1.sizeY-272*zoomfactor); if(iymax<0) iymax=0;
	if(ix<0)
		ix=0;
	if(ix>ixmax)
	{
		ix=ixmax;
	}
	if(iy<0)
		iy=0;
	if(iy>iymax)
		iy=iymax;
}

int DirListImageControl(SceCtrlData &pad, unsigned int bPress)
{
	if(showBG!=1)
		return 0;

	if(!(pad.Buttons & PSP_CTRL_START))
	{
		if(pad.Buttons & PSP_CTRL_LEFT)
		{
			zoomfactor-=0.01f;
			if(zoomfactor<zoommin)
				zoomfactor=zoommin;
		}
		else if(pad.Buttons & PSP_CTRL_RIGHT)
		{
			zoomfactor+=0.01f;
			if(zoomfactor>zoommax)
				zoomfactor=zoommax;
		}
	}
	else
	{
		if(bPress & PSP_CTRL_LEFT)
		{
			if(zoomfactor<=0.5f)
				zoomfactor=0.25f;
			else if(zoomfactor<=1.0f)
				zoomfactor=0.5f;
			else
				zoomfactor=1;
		}
		else if(bPress & PSP_CTRL_RIGHT)
		{
			if(zoomfactor<0.5f)
				zoomfactor=0.5f;
			else if(zoomfactor<1.0f)
				zoomfactor=1.0f;
			else
				zoomfactor=zoommax;
		}
	}

	if(bPress & PSP_CTRL_RTRIGGER)
	{
		int index=curImageIndex;
		while(++index<numEntry)
		{
			if(GetImageTypeIndex(dirEntry[index].d_name))
				break;
		}

		if(index<numEntry)
		{
			curSel=index;
			SelectFile(dirEntry[curSel]);
		}
		return 0;
	}
	else if(bPress & PSP_CTRL_LTRIGGER)
	{
		int index=curImageIndex;
		while(0<--index)
		{
			if(GetImageTypeIndex(dirEntry[index].d_name))
				break;
		}

		if(0<index)
		{
			curSel=index;
			SelectFile(dirEntry[curSel]);
		}
		return 0;
	}

	FixImageParam();

	return 0;
}

void DeleteFile(const char *fileName)
{
	char path_buffer[STRING_LENGTH];

	BuildPath(path_buffer, curPath.c_str(), fileName);
	int ret=sceIoRemove(path_buffer);

	if(ret<0)
	{
		return;
	}

	int prevsel=curSel;
	int prevtop=showTop;

	GetDirFileList((char*)curPath.c_str());

	curSel=prevsel;
	showTop=prevtop;
	if(numEntry<=curSel)
		curSel=numEntry-1;
	if(numEntry<=showTop)
		showTop=numEntry-1;
}

int DirListControl(SceCtrlData &pad, unsigned int bPress)
{
	if( isWaitForDelete )
	{
		if(bPress & PSP_CTRL_CIRCLE)
		{
			isWaitForDelete=0;

			DeleteFile(delFileName.c_str());
		}
		if(bPress & PSP_CTRL_CROSS)
		{
			isWaitForDelete=0;
		}

		return 0;
	}

	if(extMenu)
	{
		return DirListExtMenuControl(pad, bPress);
	}

	if(showBG==1)
	{
		int xadd=0, yadd=0;
		int pix=ix, piy=iy;

		if(pad.Lx<48)
			xadd=-21;
		else if(pad.Lx<72)
			xadd=-11;
		else if(pad.Lx<96)
			xadd=-1;
		if(pad.Ly<48)
			yadd=-21;
		else if(pad.Ly<72)
			yadd=-11;
		else if(pad.Ly<96)
			yadd=-1;
		if(pad.Lx>255-48)
			xadd=21;
		else if(pad.Lx>255-72)
			xadd=11;
		else if(pad.Lx>255-96)
			xadd=1;
		if(pad.Ly>255-48)
			yadd=21;
		else if(pad.Ly>255-72)
			yadd=11;
		else if(pad.Ly>255-96)
			yadd=1;

		ix+=xadd; iy+=yadd;

		FixImageParam();
		if(xadd!=0 || yadd!=0)
			return 0;
	}

	if(bPress & PSP_CTRL_CROSS)
	{
		hideUI=hideUI?0:1;

		return 0;
	}

	if(hideUI)
	{
		return DirListImageControl(pad, bPress);
	}

	if(bPress & PSP_CTRL_START)
	{
		extMenu=1;
		titleType=titleType?0:1;
	}

// PSP_CTRL_TRIANGLE PSP_CTRL_CIRCLE PSP_CTRL_CROSS PSP_CTRL_SQUARE
// PSP_CTRL_LEFT PSP_CTRL_RIGHT PSP_CTRL_UP PSP_CTRL_DOWN
// PSP_CTRL_START PSP_CTRL_SELECT PSP_CTRL_LTRIGGER PSP_CTRL_RTRIGGER
	if(numEntry)
	{
		if(bPress & PSP_CTRL_UP)
		{
			curSel--;

			if(curSel==-1)
			{
				curSel=numEntry-1;
				showTop=numEntry-showMax; if(showTop<0) showTop=0;

			}
			if(curSel<showTop)
			{
				showTop=curSel;
			}
		}
		else if(bPress & PSP_CTRL_DOWN)
		{
			curSel++;

			if(curSel==numEntry)
			{
				curSel=0;
				showTop=0;

			}
			if(curSel>=showTop+showMax)
			{
				showTop=curSel-showMax+1;
			}
		}
		else if(bPress & PSP_CTRL_LEFT)
		{
			if(curSel>showTop)
				curSel=showTop;
			else
			{
				curSel-=showMax;
				showTop=curSel;

				if(curSel<0)
				{
					showTop=curSel=0;
				}
			}
		}
		else if(bPress & PSP_CTRL_RIGHT)
		{
			if(curSel<showTop+showMax-1)
				curSel=showTop+showMax-1;
			else
			{
				curSel+=showMax;
				showTop+=showMax;
			}

			if(curSel>=numEntry)
			{
				curSel=numEntry-1;
				showTop=numEntry-showMax;
				if(showTop<0)
					showTop=0;
			}
		}
	}

	if(bPress & PSP_CTRL_LTRIGGER)
	{
		switch(filterMode)
		{
		case FM_ALL:
		default:
			filterMode=FM_PLAYABLE;
			break;
		case FM_PLAYABLE:
			filterMode=FM_MP3;
			break;
		case FM_MP3:
			filterMode=FM_M3U;
			break;
		case FM_M3U:
			filterMode=FM_ALL;
			break;
		}
		filterMode=FM_ALL;
		GetDirFileList((char*)curPath.c_str());

		return 0;
	}

	if(bPress & PSP_CTRL_TRIANGLE)
	{
		audioPlayMode=(audioPlayMode==APM_PAUSED)?APM_NORMAL:APM_PAUSED;
	}

	if(bPress & PSP_CTRL_CIRCLE)
	{
		return SelectFile(dirEntry[curSel]);
	}
	if(bPress & PSP_CTRL_SQUARE)
	{
		char path[256];

		BuildPath(path, curPath.c_str(), dirEntry[curSel].d_name);

		if(LoadTextFile(path))
		{
			LoadText();
            return 2;
		}
		else
			return 0;
	}

	return 0;
}

void ShowDirList()
{
	int i=0;

	char sbuf[STRING_LENGTH];

	if(hideUI==0)
	{
	snprintf(sbuf, STRING_LENGTH, "%s/", curPath.c_str());
	switch(filterMode)
	{
	case FM_ALL:
	default:
		strcat(sbuf, "*.*");
		break;
	case FM_PLAYABLE:
		strcat(sbuf, "(*.mp3, *.m3u, *.txt, *.bmp)");
		break;
	case FM_MP3:
		strcat(sbuf, "*.mp3");
		break;
	case FM_M3U:
		strcat(sbuf, "*.m3u");
		break;
	case FM_TXT:
		strcat(sbuf, "*.txt");
		break;
	case FM_BMP:
		strcat(sbuf, "*.bmp");
		break;
	}

	char bb[60];
	snprintf(bb, 60, "  (%d dir, %d files)", numDir, numFile);
	strncat(sbuf, bb, STRING_LENGTH);
	textTool.PutStringMByteCC(0, 3, generalTextColor, sbuf);

	int yValue=dirYposBase;
	unsigned long color=0;
	for(int i=showTop; i<numEntry && i<showTop+showMax; i++)
	{
		SceIoDirent &ent=dirEntry[i];

		if(curSel==i)
		{
			color=selectedTextColor;
			textTool.PutStringMByteCC(0, yValue, color, "▶");
		}
		else
			color=generalTextColor;

		if(IS_DIR(ent))
		{
			snprintf(sbuf, STRING_LENGTH, "%s%s", IS_DIR(ent)?"D ":"  ", ent.d_name);
			textTool.PutStringMByteCC(1, yValue, color, sbuf, showDateSize?maxNameLen:0);
		}
		else
		{
			if(dirTitleStringStatus!=2 || dirTitleString[i].c_str()[0]==0)
			{
				snprintf(sbuf, STRING_LENGTH, "  %s", ent.d_name);
			}
			else
			{
				if(showTagOnly)
				{
					if(IsMatchExt(ent.d_name, "mp3"))
					{
						snprintf(sbuf, STRING_LENGTH, "  MP3(%s)", dirTitleString[i].c_str());
					}
					else if(GetImageTypeIndex(ent.d_name))
					{
						snprintf(sbuf, STRING_LENGTH, "  %s (%s)", ent.d_name, dirTitleString[i].c_str());
					}
					else //if(IsMatchExt(ent.d_name, "txt"))
					{
						snprintf(sbuf, STRING_LENGTH, "  TXT(%s)", dirTitleString[i].c_str());
					}
				}
				else
//					snprintf(sbuf, STRING_LENGTH, "  %d %d", (char)ent.d_name[0], (char)ent.d_name[1]);
					snprintf(sbuf, STRING_LENGTH, "  %s (%s)", ent.d_name, dirTitleString[i].c_str());
			}

			textTool.PutStringMByteCC(1, yValue, color, sbuf, showDateSize?maxNameLen:0);
		}

		if(showDateSize && IS_DIR(ent)==0)
		{
			snprintf(sbuf, STRING_LENGTH, "%d-%02d-%02d %s %02d:%02d %dK",
				ent.d_stat.st_mtime.year, ent.d_stat.st_mtime.month, ent.d_stat.st_mtime.day,
				ent.d_stat.st_mtime.hour<12?"am":"pm", ent.d_stat.st_mtime.hour<13?ent.d_stat.st_mtime.hour:ent.d_stat.st_mtime.hour-12, ent.d_stat.st_mtime.minute, (int)ent.d_stat.st_size/1024);

			textTool.PutStringMByteCC(maxNameLen/2+2, yValue, color, sbuf);
		}

		yValue++;
	}
	}

	if(extMenu==0 && hideUI)
		return;

	//○△□ΧㅁㄴㅇㄹΧㅋㅌㅊㅂⅩ×ㄷㄱㅅㅁ
	snprintf(sbuf, STRING_LENGTH, "○: Auto load, Χ: Hide UI, □: Load as Text, △: pause audio, START:Ext menu");
	textTool.PutStringMByteCC(0, 20, generalTextColor, sbuf);
	strncpy(sbuf, "▲▼◀▶: Navigate, L1: Filetype(x), R1: Fit image(x), SELECT: Text viewer", STRING_LENGTH);
	textTool.PutStringMByteCC(0, 21, generalTextColor, sbuf);

	char extMenuString[] = "\
┌────────────┐\n\
│  L1: Go /psp/music     │\n\
│  R1: Go /text          │\n\
│  ◀: Go /psp/photo     │\n\
│  ○: Play all mp3      │\n\
│  Χ: Delete this file  │\n\
│  △: Show tag only     │\n\
│  □: Save m3u list     │\n\
│  ▼: Toggle background │\n\
│  ▶: Reload option     │\n\
│  Alalog: Sleep counter │\n\
│  SELECT: USB menu      │\n\
│  START: Close menu     │\n\
└────────────┘\n\
";
//│  ○: Toggle Date&Size  │\n\

	if(extMenu)
	{
		display.FillBackgroundPtr(workBuffer, 0x80000000, 13*12+3, 12*4, 28*6+2, 12*14+7);
		textTool.PutStringMByteCC(13, 4, generalTextColor, extMenuString);
	}

	char deleteString1[]= "┌──────────────────────────────────────┐";
	char deleteString2[]= "│                                                                            │";
	char deleteString3[]= "└──────────────────────────────────────┘";

	if( isWaitForDelete )
	{
		int ypos=6;

		display.FillBackgroundPtr(workBuffer, 0x80000000, 0, 12*ypos, 512, 12*6);

		char tbuf[STRING_LENGTH];
		snprintf(tbuf, STRING_LENGTH, " %-20s", delFileName.c_str());

		unsigned long color=generalTextColor;

		textTool.PutStringMByteCC(1, ypos+1, color, "Delete file:");
		textTool.PutStringMByteCC(1, ypos+2, color, tbuf);

		textTool.PutStringMByteCC(1, ypos+4, color, "Delete: ○ Cancel: Χ");

		textTool.PutStringMByteCC(0, ypos++, color, deleteString1);
		textTool.PutStringMByteCC(0, ypos++, color, deleteString2);
		textTool.PutStringMByteCC(0, ypos++, color, deleteString2);
		textTool.PutStringMByteCC(0, ypos++, color, deleteString2);
		textTool.PutStringMByteCC(0, ypos++, color, deleteString2);
		textTool.PutStringMByteCC(0, ypos++, color, deleteString3);

	}
}

void CopyID3Title(std::string2 &destString, struct id3_tag const *tag)
{
	unsigned int i;
	struct id3_frame const *frame;
	id3_ucs4_t const *ucs4;
	id3_latin1_t *latin1;

	union id3_field const *field;
	unsigned int nstrings, j;

	frame = id3_tag_findframe(tag, ID3_FRAME_TITLE, 0);
	if (frame == 0)
	{
		destString="";
		return;
	}

	field    = id3_frame_field(frame, 1);
	nstrings = id3_field_getnstrings(field);

	for (j = 0; j < nstrings; ++j)
	{
		ucs4 = id3_field_getstrings(field, j);

		latin1 = id3_ucs4_latin1duplicate(ucs4);
		if (latin1 == 0)
		{
			destString="";
			return;
		}

		snprintf(destString.str, STRING_LENGTH, "%s - ", latin1);

		free(latin1);
	}

	frame = id3_tag_findframe(tag, ID3_FRAME_ARTIST, 0);
	if (frame == 0)
	{
		destString="";
		return;
	}

	field    = id3_frame_field(frame, 1);
	nstrings = id3_field_getnstrings(field);

	for (j = 0; j < nstrings; ++j)
	{
		ucs4 = id3_field_getstrings(field, j);

		latin1 = id3_ucs4_latin1duplicate(ucs4);
		if (latin1 == 0)
		{
			destString="";
			return;
		}

		strcat(destString.str, (char*)latin1);

		free(latin1);
	}
}

void GetTxtTag(std::string2 &destString, const char *flleName)
{
	FILE *file=fopen(flleName, "r");

	if(file)
	{
		int len, ok=0;
		char *cur;
		while(fgets(destString.str, STRING_LENGTH, file))
		{
			Eat0x0d0x0a(destString.str);

			cur=destString.str;
			len=strlen(destString.str);
			while(len)
			{
				if( !(*cur==' '||*cur=='\t') )
				{
					ok=1;
					break;
				}
				len--;
				cur++;
			}

			if(ok)
				break;
		}
		
		fclose(file);
	}
	else
	{
		destString="";
	}
}

void GetImageTag(std::string2 &destString, const char *flleName, const char *name)
{
	int sx, sy, bit;
	if(ImageFile::GetImageInfo(flleName, sx, sy, bit))
	{
		snprintf(destString.str, STRING_LENGTH, "%dx%dx%db", sx, sy, bit);
	}
	else
	{
		destString="";
	}
}

int id3ThreadOn=1;
int DecodeTitleString(SceSize args, void *argp)
{
	while(id3ThreadOn)
	{
		if(titleStringStatus==1)
		{
			for(int i=0; i<numFiles; i++)
			{
				BoostCPU(0.5f);
				titleString[i]="";

				struct id3_file *file;

				file = id3_file_open(fileList[i].c_str(), ID3_FILE_MODE_READONLY);
				if (file)
				{
					struct id3_tag *tag=id3_file_tag(file);

					CopyID3Title(titleString[i], tag);
					id3_file_close(file);
				}
			}
			frameModified=1;
			titleStringStatus=2;
		}
		else if(dirTitleStringStatus==1)
		{
			char buf[STRING_LENGTH];
			for(int i=0; i<numEntry; i++)
			{
				BoostCPU(0.5f);
				SceIoDirent &ent=dirEntry[i];
				dirTitleString[i]="";

				if(IsMatchExt(ent.d_name, "mp3"))
				{
					struct id3_file *file;

					BuildPath(buf, curPath.c_str(), ent.d_name);
					file = id3_file_open(buf, ID3_FILE_MODE_READONLY);
					if (file)
					{
						struct id3_tag *tag=id3_file_tag(file);

						CopyID3Title(dirTitleString[i], tag);
						id3_file_close(file);
					}
				}
				else if(IsMatchExt(ent.d_name, "txt"))
				{
					BuildPath(buf, curPath.c_str(), ent.d_name);
					GetTxtTag(dirTitleString[i], buf);
				}
				else if(GetImageTypeIndex(ent.d_name))
				{
					BuildPath(buf, curPath.c_str(), ent.d_name);
					GetImageTag(dirTitleString[i], buf, ent.d_name);
				}
			}
			frameModified=1;
			dirTitleStringStatus=2;
		}
		else
		{
			sceKernelDelayThread(1000*200);
		}
	}

	return 0;
}

#define LINE_MARKER_LINES 200
#define MAX_LINE_MARKER (10240)
int ftellIndex[MAX_LINE_MARKER+1]={0};
int totalLines=0;
int numMark=0;

#define TEXT_BUFFER_SIZE (LINE_MARKER_LINES+24)
#define TMARGIN_DEFAULT 30
struct LineInfo
{
	int pos;
	char text[80];
};
LineInfo loadedLines[TEXT_BUFFER_SIZE];
std::string2 textFileName;
std::string2 textTag;
MyURLContext txtFile;
int textFileSize=0;
int curLine=0;
int maxLine=17;
int lineLength=78;
int lmargin=3, rmargin=3, tmargin=TMARGIN_DEFAULT, bmargin=0;
int curLoadBase=-1;
void CalcTextLineParam()
{
	if(fullScreenText)
	{
		tmargin=4;
	}
	else
	{
		tmargin=TMARGIN_DEFAULT;
	}

//	return;
	int fontSizeX=useLargeFont?textTool16.fontSizeX:textTool.fontSizeX;
	int fontSizeY=useLargeFont?textTool16.fontSizeY:textTool.fontSizeY;

	int xspace=480-lmargin-rmargin;
	int yspace=272-tmargin-bmargin;

	lineLength=xspace/(fontSizeX/2)-1;
	maxLine=yspace/(fontSizeY+lineGap);

	if(maxLine>TEXT_BUFFER_SIZE)
		maxLine=TEXT_BUFFER_SIZE;

	textTool16.SetSpacing(0, lineGap);
}
char *FindNextLine(char *src, int size)
{
	int ccc=0;
	char *s;
	if(forceTrimLeft)
	{
		s=EatPrecedingWhites(src);
		if(s)
		{
			src=s;
		}
	}

	while(ccc<size)
	{
		s=src+ccc;
		if(*s==0)
			return s;
		else if(0<=*s /*&& *s<128*/)
		{
			ccc++;
		}
		else ccc+=2;
	}

	return src+ccc;
}
void Eat0x0d0x0a(char *ptr)
{
	if(!ptr)
		return;

	int len=strlen(ptr);

	ptr=ptr+len;
	while(len)
	{
		len--;
		ptr--;

		if(*ptr=='\r')
			*ptr=0;
		else if(*ptr=='\n')
			*ptr=0;
		else if(*ptr==' ')
			*ptr=0;
		else
			break;
	}
}

char* GetLine(char* pBuffer, unsigned int uiMaxBytes, MyURLContext *uc)
{
    unsigned int uiBytesRead = 0;
    unsigned int i = 0;

    //assert(uiMaxBytes > 0);

    while (i + 1 < uiMaxBytes)
    {
        char c;
        int uiRead = file_read(uc, (unsigned char *)&c, 1);
		if(uiRead<0)
			break;

        uiBytesRead += uiRead;

        if (uiRead != 1 || c == '\n')
            break;

        if (c != '\r')
            pBuffer[i++] = c;
    }

    pBuffer[i] = 0;

	if( uiBytesRead>0 )
		return pBuffer;
	else
		return NULL;
}

#define TF_STRBUF_LEN 8*1024
#define MY_TXTFILEBUF 64*1024
unsigned char txtfbuf[MY_TXTFILEBUF*2]={0,};
int LoadTextFile(const char *fileName)
{
	file_close(&txtFile);

	InitMyURLContext(&txtFile);
	txtFile._FBUFSIZE=MY_TXTFILEBUF;
	txtFile.fbuf=txtfbuf;
	txtFile.fbuf2=txtfbuf+MY_TXTFILEBUF;

	if ( file_open(&txtFile, fileName, URL_RDONLY )<0 )
	{
		return 0;
	}

	char *tfStrBuf=(char*)malloc(TF_STRBUF_LEN);

	BoostCPU(1.0f);

	char *next, *base, *del;
	int basePos=0, idx=0, pos=0;
	numMark=0;
	while((base=GetLine(tfStrBuf, TF_STRBUF_LEN, &txtFile)))
	{
		Eat0x0d0x0a(base);
		
		next=base;
		do
		{
			pos=basePos+next-base;
			if(idx%LINE_MARKER_LINES==0)
			{
				ftellIndex[numMark]=pos;
				numMark++;
			}
			next=FindNextLine(next, lineLength);
			idx++;
		} while(*next!=0 && numMark<MAX_LINE_MARKER);

		if( !(numMark<MAX_LINE_MARKER))
			break;
		basePos=file_tell(&txtFile);
	}

	file_seek(&txtFile, 0, SEEK_END);
	textFileSize=file_tell(&txtFile);

	ftellIndex[numMark]=pos;
	numMark++;

	free(tfStrBuf);

	//file_close(&txtFile);

	totalLines=idx;
	textFileName=fileName;

	curLoadBase=-1;
	curLine=0;
	int mark=bookmark.GetMark(textFileSize);
	SetLineFromPos(mark);

	GetTxtTag(textTag, fileName);
	if(strlen(textTag.str)>=15)
	{
		char *st=FindNextLine(textTag.str, 11);
		strcpy(st, "...");
	}

	LoadText();
	RefineLineFromPos(mark);

	return 1;
}

int isTextLoading=0;
void LoadText()
{
	if(curLoadBase==curLine/LINE_MARKER_LINES)
		return;

	if(isTextLoading)
		return;

	isTextLoading=1;

	if( txtFile.fdValid==0 )
	{
		file_close(&txtFile);

		InitMyURLContext(&txtFile);
		txtFile._FBUFSIZE=MY_TXTFILEBUF;
		txtFile.fbuf=txtfbuf;
		txtFile.fbuf2=txtfbuf+MY_TXTFILEBUF;

		if ( file_open(&txtFile, textFileName.c_str(), URL_RDONLY )<0 )
		{
			return;
		}
	}

	curLoadBase=curLine/LINE_MARKER_LINES;

	char *tfStrBuf=(char*)malloc(TF_STRBUF_LEN);

	int filePos=ftellIndex[curLoadBase];
	file_seek(&txtFile, filePos, SEEK_SET);

	char *cur, *next, *del, *base;
	int basePos=filePos;
	int lidx=0;
	while( (base=GetLine(tfStrBuf, TF_STRBUF_LEN, &txtFile)) && lidx<TEXT_BUFFER_SIZE)
	{
		cur=base;
		Eat0x0d0x0a(cur);

		do
		{
			loadedLines[lidx].pos=basePos+cur-base;
			next=FindNextLine(cur, lineLength);
			if(forceTrimLeft)
			{
				char *curs=EatPrecedingWhites(cur); if(curs==NULL) curs=cur;
				strncpy(loadedLines[lidx].text, curs, next-curs);
				loadedLines[lidx].text[next-curs]=0;
			}
			else
			{
				strncpy(loadedLines[lidx].text, cur, next-cur);
				loadedLines[lidx].text[next-cur]=0;
			}

			lidx++;

			cur=next;

			if(lidx>=TEXT_BUFFER_SIZE)
				break;

		} while(*next!=0);

		basePos=file_tell(&txtFile);
	}

	while(lidx<TEXT_BUFFER_SIZE)
	{
		loadedLines[lidx].pos=0;
		loadedLines[lidx].text[0]=0;
		lidx++;
	}

	free(tfStrBuf);

	isTextLoading=0;
}

void SetLineFromPos(int pos)
{
	curLine=0;
	for(int i=0; i<numMark-1; i++)
	{
		if(ftellIndex[i]<=pos && pos<ftellIndex[i+1])
		{
			curLine=i*LINE_MARKER_LINES;
			break;
		}
	}
}

void RefineLineFromPos(int pos)
{
	int i;
	for(i=0; i<TEXT_BUFFER_SIZE; i++)
	{
		if(loadedLines[i].pos<=pos && pos<loadedLines[i+1].pos)
		{
			break;
		}
	}
	if(i<TEXT_BUFFER_SIZE)
		curLine+=i;
}

void ShowTextExtMenuControl(SceCtrlData &pad, unsigned int bPress)
{
	if(bPress & PSP_CTRL_CIRCLE)
	{
		MP3PlayTrack();

		extMenu=0;
	}
	if(bPress & PSP_CTRL_CROSS)
	{
		MP3StopTrack();

		extMenu=0;
	}
	if(bPress & PSP_CTRL_SQUARE)
	{
		fullScreenText=fullScreenText?0:1;

		CalcTextLineParam();
		LoadText();

//		playerMode=(playerMode==PM_SEQUENTIAL)?PM_RANDOM:PM_SEQUENTIAL;

		extMenu=0;
	}

	if(bPress & PSP_CTRL_TRIANGLE)
	{
		audioPlayMode=(audioPlayMode==APM_PAUSED)?APM_NORMAL:APM_PAUSED;

		extMenu=0;
	}

	if(bPress & PSP_CTRL_LTRIGGER)
	{
		MP3PrevTrack();

		extMenu=0;
	} 
	if(bPress & PSP_CTRL_RTRIGGER)
	{
		MP3NextTrack();

		extMenu=0;
	}      

	if(bPress & PSP_CTRL_DOWN)
	{
		switch(showBG)
		{
		case 0:
			if(strlen(bgPath))
			{
				showBG=1;
			}
			else
			{
				showBG=2;
			}
			curBG=-1;
			LoadBackground();
			break;
		case 1:
			showBG=2;
			curBG=-1;
			LoadBackground();
			break;
		case 2:
		default:
			showBG=0;
			break;
		}

		extMenu=0;
	}
	if(bPress & PSP_CTRL_RIGHT)
	{
		InitialLoad(0, 0);
		LoadText();
		extMenu=0;
	}

	if(bPress & PSP_CTRL_UP)
	{
		useLargeFont=useLargeFont?0:1;
		CalcTextLineParam();

		std::string2 name=textFileName;
		LoadTextFile(name.c_str());

		LoadText();

		extMenu=0;
	}
	if(bPress & PSP_CTRL_LEFT)
	{
		lineGap=(++lineGap)%6;
		CalcTextLineParam();
		LoadText();

		sceKernelDelayThread(1000*300);
	}

	if(pad.Ly==0)
	{
		sleepCounter+=180;
		sceKernelDelayThread(1000*100);
	}
	else if(pad.Ly==255)
	{
		if(sleepCounter<240)
			sleepCounter=0;
		else
			sleepCounter-=180;
		sceKernelDelayThread(1000*100);
	}

	if(bPress & PSP_CTRL_SELECT)
	{
		showMode=5;
		extMenu=0;
	}
	if(bPress & PSP_CTRL_START)
	{
		extMenu=0;
	}
}

int ShowTextControl(SceCtrlData &pad, unsigned int bPress)
{
	if(extMenu)
	{
		ShowTextExtMenuControl(pad, bPress);
		return 0;
	}

// PSP_CTRL_TRIANGLE PSP_CTRL_CIRCLE PSP_CTRL_CROSS PSP_CTRL_SQUARE
// PSP_CTRL_LEFT PSP_CTRL_RIGHT PSP_CTRL_UP PSP_CTRL_DOWN
// PSP_CTRL_START PSP_CTRL_SELECT PSP_CTRL_LTRIGGER PSP_CTRL_RTRIGGER

	if(totalLines)
	{
		int mlitiplier=1;
		if(pad.Buttons & PSP_CTRL_LTRIGGER && pad.Buttons & PSP_CTRL_RTRIGGER)
		{
			mlitiplier=100;
		}
		else if(pad.Buttons & PSP_CTRL_LTRIGGER)
		{
			mlitiplier=3;
		}
		else if(pad.Buttons & PSP_CTRL_RTRIGGER)
		{
			mlitiplier=10;
		}

		if(bPress & PSP_CTRL_UP || bPress & PSP_CTRL_TRIANGLE || pad.Ly==0)
		{
			curLine-=mlitiplier;

			if(curLine<0)
				curLine=0;

			LoadText();
		}
		else if(bPress & PSP_CTRL_DOWN || bPress & PSP_CTRL_CROSS || pad.Ly==255)
		{
			curLine+=mlitiplier;

			if(curLine>=totalLines-maxLine)
				curLine=totalLines-maxLine;

			LoadText();
		}
		else if(bPress & PSP_CTRL_LEFT || bPress & PSP_CTRL_SQUARE || pad.Lx==0)
		{
			curLine-=maxLine*mlitiplier;

			if(curLine<0)
				curLine=0;

			LoadText();
		}
		else if(bPress & PSP_CTRL_RIGHT || bPress & PSP_CTRL_CIRCLE || pad.Lx==255)
		{
			curLine+=maxLine*mlitiplier;

			if(curLine>=totalLines-maxLine)
				curLine=totalLines-maxLine;

			LoadText();
		}

		if(bPress & PSP_CTRL_LTRIGGER || bPress & PSP_CTRL_RTRIGGER)
		{
			curLine+=maxLine;

			if(curLine>=totalLines-maxLine)
				curLine=totalLines-maxLine;

			LoadText();
		}

		bookmark.AddMark(textFileSize, loadedLines[curLine%LINE_MARKER_LINES].pos);

		if(bPress & PSP_CTRL_START)
		{
			extMenu=1;
			titleType=titleType?0:1;
		}
	}
	return 0;
}

void PrepareLargeFont()
{
	if(textTool16.byteArray==NULL)
	{
		char buf[STRING_LENGTH];
		BuildPath(buf, init_path, "largefon.nff");
		textTool16.Initialize(buf, 2);
	}
}

void ShowTextFile()
{
	if(totalLines==0)
		return;

//	if(useLargeFont==0)
	{
		textTool.SetSpacing(0, lineGap);
		textTool.SetTextRefCoord(lmargin, tmargin);
		textTool16.SetSpacing(0, lineGap);
		textTool16.SetTextRefCoord(lmargin, tmargin);
	}

	if(useLargeFont)
	{
		PrepareLargeFont();
	}

	int yValue=0, linebase=curLine%LINE_MARKER_LINES;
	for(int i=0; i<maxLine; i++)
	{
		if(extMenu && i<2 && fullScreenText)
			continue;

		if(useLargeFont==0)
			textTool.PutStringMByteCC(0, i+yValue, generalTextColor, loadedLines[i+linebase].text);
		else
		{
			textTool16.PutStringMByteCC(0, i+yValue, generalTextColor, loadedLines[i+linebase].text);
		}
	}

//	if(useLargeFont==0)
	{
		textTool.SetSpacing(0, 0);
		textTool.SetTextRefCoord(2, 2);
	}

	//○△□ΧㅁㄴㅇㄹΧㅋㅌㅊㅂⅩ×ㄷㄱㅅㅁ▲▼◀▶
	char extMenuString[] = "\
┌────────────┐\n\
│  ○: Play, Χ: Stop    │\n\
│  △: pause audio       │\n\
│  L1: Prev track        │\n\
│  R1: Next track        │\n\
│  □: Full screen text  │\n\
│  ▲: Toggle font       │\n\
│  ◀: Add line gap      │\n\
│  ▼: Toggle background │\n\
│  ▶: Reload option     │\n\
│  Alalog: Sleep counter │\n\
│  SELECT: USB menu      │\n\
│  START: Close menu     │\n\
└────────────┘\n\
";
	if(extMenu)
	{
		display.FillBackgroundPtr(workBuffer, 0x80000000, 13*12+3, 12*4, 28*6+2, 12*14+7);
		textTool.PutStringMByteCC(13, 4, generalTextColor, extMenuString);
	}

/*	char buf[100];

	sprintf(buf, "s %d", playCursor);
	textTool.PutStringMByteCC(0, 2, generalTextColor, buf);
	
	for(int i=0; i<numFiles; i++)
	{
		sprintf(buf, "%d", i);
		textTool.PutStringMByteCC(i*2, 3, generalTextColor, buf);

		sprintf(buf, "%d", prevFile[i]);
		textTool.PutStringMByteCC(i*2, 4, generalTextColor, buf);

		sprintf(buf, "%d", nextFile[i]);
		textTool.PutStringMByteCC(i*2, 5, generalTextColor, buf);
	}*/



/*

	sprintf(buf, "n %d", mfdCallBuffer.num);
	textTool.PutStringMByteCC(0, 2, generalTextColor, buf);

	textTool.PutStringMByteCC(0, 3, generalTextColor, fileNameBuffer[mfdCallBuffer.num-1].str);
	int y=3;
	for(int i=0; i<strlen(fileNameBuffer[0].str); i++)
	{
		if(i%16==0)
			y++;

		sprintf(buf, "%02x", fileNameBuffer[mfdCallBuffer.num-1].str[i]);
		textTool.PutStringMByteCC((i%16)*2, y, generalTextColor, buf);
	}*/

/*	for(int i=0; i<mfdCallBuffer.num; i++)
	{
		textTool.PutStringMByteCC(0, i+2, generalTextColor, fileNameBuffer[mfdCallBuffer.num].str);
	}*/

//		BuildPath(fileNameBuffer[num].str, curPath.c_str(), strBuf);

}

void EncodeString(char *dest, int bufsize, const char *src)
{
	int len=strlen(src);

	for(int i=0; i<len; i++)
	{
		if(bufsize<=2)
			break;

		*dest=(*src>>4) +'A'; dest++;
		*dest=(*src&0xf)+'A'; dest++;

		src++;
		bufsize-=2;
	}
	*dest=0;
}

void DecodeString(char *dest, int bufsize, const char *src)
{
	int len=strlen(src);

	for(int i=0; i<len/2; i++)
	{
		if(bufsize<=1)
			break;

		*dest=(*src-'A')<<4; src++;
		*dest+=*src-'A'; src++;

		bufsize-=1;
		dest++;
	}
	*dest=0;
}

int OptionControl(SceCtrlData &pad, unsigned int bPress)
{
// PSP_CTRL_TRIANGLE PSP_CTRL_CIRCLE PSP_CTRL_CROSS PSP_CTRL_SQUARE
// PSP_CTRL_LEFT PSP_CTRL_RIGHT PSP_CTRL_UP PSP_CTRL_DOWN
// PSP_CTRL_START PSP_CTRL_SELECT PSP_CTRL_LTRIGGER PSP_CTRL_RTRIGGER
	if(bPress & PSP_CTRL_CIRCLE)
	{
		InitialLoad(1, 1);
		if(totalLines)
			return 2;
		else if(mfdCallBuffer.num)
			return 0;

		return 1;
	}
	if(pad.Buttons & PSP_CTRL_CROSS)
	{
		InitialLoad(0, 0);
		return 1;
	}
/*	if(pad.Buttons & PSP_CTRL_SQUARE)
	{
		InitialLoad(0, 1);
		if(totalLines)
			return 2;
		return 1;
	}
	if(pad.Buttons & PSP_CTRL_TRIANGLE)
	{
		InitialLoad(1, 0);
		if(mfdCallBuffer.num)
			return 0;
		return 1;
	}*/

	return -1;
}

void ShowOption()
{
	char InitialOption[] = "\
┌────────────────────────┐\n\
│                                                │\n\
│                  PSPlayer 2.0                  │\n\
│                                                │\n\
│                                                │\n\
│        ○: Restore previous state              │\n\
│                                                │\n\
│        Χ: Ignore saved state and              │\n\
│            start with file browser             │\n\
│                                                │\n\
│                                                │\n\
└────────────────────────┘\n\
";
	textTool.PutStringMByteCC(7, 6, generalTextColor, InitialOption);
	//○△□ΧㅁㄴㅇㄹΧㅋㅌㅊㅂⅩ×ㄷㄱㅅㅁ

/*	char buf[1000], buf2[1000];
	char tttt[]="ms0:/PSP/GAMES/PSPlayer/option.txt";
	EncodeString(buf, 1000, tttt);
	DecodeString(buf2, 1000, buf);

	textTool.SetSpacing(6, 0);
	textTool.PutStringMByteCC(0, 3, generalTextColor, tttt);
	textTool.PutStringMByteCC(0, 4, generalTextColor, buf2);
	textTool.SetSpacing(0, 0);
	textTool.PutStringMByteCC(0, 5, generalTextColor, buf);*/
}

//helper function to make things easier
#if USB_ENABLED==1 && _PSP_FW_VERSION <= 150

int LoadStartModule(char *path)
{
    u32 loadResult;
    u32 startResult;
    int status;

    loadResult = sceKernelLoadModule(path, 0, NULL);
    if (loadResult & 0x80000000)
	return -1;
    else
	startResult = sceKernelStartModule(loadResult, 0, NULL, &status, NULL);

    if (loadResult != startResult)
	return -2;

    return 0;
}

#endif

#if _PSP_FW_VERSION <= 150

__attribute__ ((constructor))
void loaderInit()
{
	pspKernelSetKernelPC();
	pspSdkInstallNoDeviceCheckPatch();
	pspSdkInstallNoPlainModuleCheckPatch();
	pspSdkInstallKernelLoadModulePatch();

	//pspDebugInstallErrorHandler(exception_handler);
	//pspKernelSetKernelPC();
	//pspSdkInstallNoDeviceCheckPatch();
//	pspDebugInstallKprintfHandler(NULL);
}

#endif

int usbDriverInitialized=0;
int prevUSBState=0;
u32 InitializeUSBDriver()
{
	if(usbDriverInitialized)
	{
		return 0;
	}
	usbDriverInitialized=1;

#if USB_ENABLED==1

	u32 retVal;

#if _PSP_FW_VERSION <= 150

	LoadStartModule("flash0:/kd/semawm.prx");
    LoadStartModule("flash0:/kd/usbstor.prx");
    LoadStartModule("flash0:/kd/usbstormgr.prx");
    LoadStartModule("flash0:/kd/usbstorms.prx");
    LoadStartModule("flash0:/kd/usbstorboot.prx");

#else

	sceKernelStartModule(kuKernelLoadModule ("flash0:/kd/chkreg.prx", 0, NULL), 0, NULL, 0, NULL);
	sceKernelStartModule(kuKernelLoadModule ("flash0:/kd/mgr.prx", 0, NULL), 0, NULL, 0, NULL);
	sceKernelStartModule(kuKernelLoadModule ("flash0:/kd/npdrm.prx", 0, NULL), 0, NULL, 0, NULL);
	sceKernelStartModule(kuKernelLoadModule ("flash0:/kd/semawm.prx", 0, NULL), 0, NULL, 0, NULL);
	sceKernelStartModule(kuKernelLoadModule ("flash0:/kd/usbstor.prx", 0, NULL), 0, NULL, 0, NULL);
	sceKernelStartModule(kuKernelLoadModule ("flash0:/kd/usbstormgr.prx", 0, NULL), 0, NULL, 0, NULL);
	sceKernelStartModule(kuKernelLoadModule ("flash0:/kd/usbstorms.prx", 0, NULL), 0, NULL, 0, NULL);
	sceKernelStartModule(kuKernelLoadModule ("flash0:/kd/usbstorboot.prx", 0, NULL), 0, NULL, 0, NULL); 

	if( kuKernelGetModel()==PSP_MODEL_SLIM_AND_LITE )
	{
		isPSPSlim=1;
	}

#endif

    retVal = sceUsbStart(PSP_USBBUS_DRIVERNAME, 0, 0);
    if (retVal != 0)
	{
//		snprintf(usbErrorMsg, STRING_LENGTH, "Error starting USB Bus driver (0x%08X)\n", retVal);
		return 0;
    }
    retVal = sceUsbStart(PSP_USBSTOR_DRIVERNAME, 0, 0);
    if (retVal != 0)
	{
//		snprintf(usbErrorMsg, STRING_LENGTH, "Error starting USB Mass Storage driver (0x%08X)\n", retVal);
		return 0;
    }
	retVal = sceUsbstorBootSetCapacity(0x800000);
    if (retVal != 0)
	{
//		snprintf(usbErrorMsg, STRING_LENGTH, "Error setting capacity with USB Mass Storage driver (0x%08X)\n", retVal);
		return 0;
    }

	prevUSBState=sceUsbGetState();

	return 1;

#else

	return 0;

#endif

}

u32 FinalizeUSBDriver()
{
#if USB_ENABLED==1

	u32 retVal;

	usbState=sceUsbGetState();

    if (usbState & PSP_USB_ACTIVATED)
	{
    	retVal = sceUsbDeactivate(0x1c8);
    	if (retVal != 0)
		{
//			snprintf(usbErrorMsg, STRING_LENGTH, "Error calling sceUsbDeactivate (0x%08X)\n", retVal);
			return 0;
		}
    }
    retVal = sceUsbStop(PSP_USBSTOR_DRIVERNAME, 0, 0);
    if (retVal != 0)
	{
//		snprintf(usbErrorMsg, STRING_LENGTH, "Error stopping USB Mass Storage driver (0x%08X)\n", retVal);
		return 0;
	}
    retVal = sceUsbStop(PSP_USBBUS_DRIVERNAME, 0, 0);
    if (retVal != 0)
	{
//		snprintf(usbErrorMsg, STRING_LENGTH, "Error stopping USB BUS driver (0x%08X)\n", retVal);
		return 0;
	}

#endif

	return 1;
}

int UsbControl(SceCtrlData &pad, unsigned int bPress)
{
	usbInitialized=InitializeUSBDriver();

	// PSP_CTRL_TRIANGLE PSP_CTRL_CIRCLE PSP_CTRL_CROSS PSP_CTRL_SQUARE
	// PSP_CTRL_LEFT PSP_CTRL_RIGHT PSP_CTRL_UP PSP_CTRL_DOWN
	// PSP_CTRL_START PSP_CTRL_SELECT PSP_CTRL_LTRIGGER PSP_CTRL_RTRIGGER

	u32 pid=0x1c8;

	if( isPSPSlim )
	{
		pid=0x2d2;
	}

	usbState=sceUsbGetState();
	if((usbState&PSP_USB_CABLE_CONNECTED) && !(usbState&PSP_USB_CONNECTION_ESTABLISHED) 
		&& (bPress & PSP_CTRL_CIRCLE))
	{
		if(usbState & PSP_USB_ACTIVATED)
		{
			sceUsbDeactivate(pid);
			sceKernelDelayThread(1000*300);
		}
		sceUsbActivate(pid);
	}
	if(usbState&PSP_USB_CONNECTION_ESTABLISHED)
	{
		if( ((pad.Buttons & PSP_CTRL_LEFT) && (bPress & PSP_CTRL_SQUARE)) || 
			((pad.Buttons & PSP_CTRL_SQUARE) && (bPress & PSP_CTRL_LEFT)) )
		{
			if(usbState & PSP_USB_ACTIVATED)
			{
				sceUsbDeactivate(pid);
			}
		}
	}

	if(bPress&PSP_CTRL_START)
	{
		showMode=1;
	}

	if(prevUSBState!=usbState)
	{
		prevUSBState=usbState;
		return 1;
	}

	return 0;
}

void ShowUsbOption()
{
	char usbOption[] = "\
┌─────────────────────────┐\n\
│                                                  │\n\
│                USB Driver Manager                │\n\
│                                                  │\n\
│                                                  │\n\
│                                                  │\n\
│                                                  │\n\
│                                                  │\n\
│                                                  │\n\
│                                                  │\n\
│                                                  │\n\
│                                                  │\n\
│                                                  │\n\
│                                                  │\n\
└─────────────────────────┘\n\
";
	textTool.PutStringMByteCC(7, 4, generalTextColor, usbOption);

	char sbuf[STRING_LENGTH];
	int connect=0;
	usbState=sceUsbGetState();

	snprintf(sbuf, STRING_LENGTH, "USB Driver:     %s\n", usbState & PSP_USB_ACTIVATED ? "activated     " : "deactivated");
	textTool.PutStringMByteCC(15, 8, generalTextColor, sbuf);
	snprintf(sbuf, STRING_LENGTH, "USB Cable:      %s\n",  usbState & PSP_USB_CABLE_CONNECTED ? "connected    " : "disconnected");
	textTool.PutStringMByteCC(15, 9, generalTextColor, sbuf);
	snprintf(sbuf, STRING_LENGTH, "USB Connection: %s\n", usbState & PSP_USB_CONNECTION_ESTABLISHED ? "established" : "not present");
	textTool.PutStringMByteCC(15, 10, generalTextColor, sbuf);

	if((usbState&PSP_USB_CABLE_CONNECTED) && !(usbState&PSP_USB_CONNECTION_ESTABLISHED))
	{
		snprintf(sbuf, STRING_LENGTH, "Press ○ to connect USB");
		textTool.PutStringMByteCC(15, 12, generalTextColor, sbuf);
	}
	if(usbState&PSP_USB_CONNECTION_ESTABLISHED)
	{
		snprintf(sbuf, STRING_LENGTH, "Press ◀+□ to stop USB");
		textTool.PutStringMByteCC(15, 12, generalTextColor, sbuf);
		snprintf(sbuf, STRING_LENGTH, "(Stopping USB may damage your memory stick.)");
		textTool.PutStringMByteCC(10, 13, generalTextColor, sbuf);
		snprintf(sbuf, STRING_LENGTH, "(Use Windows Safely remove hardware instead.)");
		textTool.PutStringMByteCC(10, 14, generalTextColor, sbuf);
	}
	snprintf(sbuf, STRING_LENGTH, "Press START to go to File browser");
	textTool.PutStringMByteCC(12, 16, generalTextColor, sbuf);

	//○△□ΧㅁㄴㅇㄹΧㅋㅌㅊㅂⅩ×ㄷㄱㅅㅁ▲▼◀▶

/*	char buf[1000], buf2[1000];
	char tttt[]="ms0:/PSP/GAMES/PSPlayer/option.txt";
	EncodeString(buf, 1000, tttt);
	DecodeString(buf2, 1000, buf);

	textTool.SetSpacing(6, 0);
	textTool.PutStringMByteCC(0, 3, generalTextColor, tttt);
	textTool.PutStringMByteCC(0, 4, generalTextColor, buf2);
	textTool.SetSpacing(0, 0);
	textTool.PutStringMByteCC(0, 5, generalTextColor, buf);*/
}

void RemoteControl(unsigned int &rButtons, unsigned int rBPress)
{
/*	PSP_HPRM_PLAYPAUSE  = 0x1,
	PSP_HPRM_FORWARD    = 0x4,
	PSP_HPRM_BACK       = 0x8,
	PSP_HPRM_VOL_UP		= 0x10,
	PSP_HPRM_VOL_DOWN   = 0x20,
	PSP_HPRM_HOLD       = 0x80*/
	if(curplayer->isPlaying)
	{
		if(rBPress & PSP_HPRM_PLAYPAUSE)
		{
			audioPlayMode=(audioPlayMode==APM_PAUSED)?APM_NORMAL:APM_PAUSED;
		}
	}
	else
	{
		if(rBPress & PSP_HPRM_PLAYPAUSE)
		{
			MP3PlayTrack();
		}
	}

	if(rBPress & PSP_HPRM_BACK)
	{
		MP3PrevTrack();
	} 
	if(rBPress & PSP_HPRM_FORWARD)
	{
		MP3NextTrack();
	}
}

void RemoteControlText(unsigned int &rButtons, unsigned int rBPress)
{
/*	PSP_HPRM_PLAYPAUSE  = 0x1,
	PSP_HPRM_FORWARD    = 0x4,
	PSP_HPRM_BACK       = 0x8,
	PSP_HPRM_VOL_UP		= 0x10,
	PSP_HPRM_VOL_DOWN   = 0x20,
	PSP_HPRM_HOLD       = 0x80*/

	if(rBPress & PSP_HPRM_PLAYPAUSE)
	{
		curLine+=maxLine;

		if(curLine>=totalLines-maxLine)
			curLine=totalLines-maxLine;

		LoadText();
	}

	if(rBPress & PSP_HPRM_BACK)
	{
		curLine-=maxLine;

		if(curLine<0)
			curLine=0;

		LoadText();
	} 
	if(rBPress & PSP_HPRM_FORWARD)
	{
		curLine+=maxLine;

		if(curLine>=totalLines-maxLine)
			curLine=totalLines-maxLine;

		LoadText();
	}
}

//#define __HPRM_THREAD
int HPRMThreadOn=1;
#ifdef __HPRM_THREAD
unsigned int remoteButton;
unsigned int oldRemoteButton=0;
unsigned int remoteBPress=0;
int HPRMThread(SceSize args, void *argp)
{
	while(HPRMThreadOn)
	{
		if(sceHprmIsRemoteExist() && autoSaveMode==0)
		{
			sceHprmPeekCurrentKey((u32*)&remoteButton);
			remoteBPress=remoteButton & (~oldRemoteButton);
			oldRemoteButton=remoteButton;
		}
		else
		{
			remoteButton=remoteBPress=oldRemoteButton=0;
		}
		if(remoteButton&PSP_HPRM_HOLD)
		{
			remoteButton=remoteBPress=oldRemoteButton=0;
		}

		if(remoteTextControl && showMode==2)
			RemoteControlText(remoteButton, remoteBPress);
		else
			RemoteControl(remoteButton, remoteBPress);

		sceKernelDelayThread(1000*3);
	}
}
#endif

void GetSysParam()
{

}

void LoadBackground()
{
	if(showBG==0)
	{
		ImageFile::bgImage1.FreeImageBuffer();
		ImageFile::bgImage2.FreeImageBuffer();
		ImageFile::bgImage3.FreeImageBuffer();
		ImageFile::bgImage4.FreeImageBuffer();
	}

	if(showBG==1)
	{
		ImageFile::bgImage2.FreeImageBuffer();
		ImageFile::bgImage3.FreeImageBuffer();
		ImageFile::bgImage4.FreeImageBuffer();
		ImageFile::bgImage1.LoadImage(bgPath);

		if(ImageFile::bgImage1.IsLoaded())
		{
			ix=iy=0;

			zoommin=0.25f;
			zoommax=(float)ImageFile::bgImage1.sizeX/480.0f;
			float zoommax2=(float)ImageFile::bgImage1.sizeY/272.0f;
			if(zoommax>zoommax2)
				zoommax=zoommax2;
			if(zoommax<1) zoommax=1;
			if(zoommax>1)
				zoomfactor=zoommax;
			if(zoomfactor<zoommin)
			{
				zoomfactor=zoommin;
			}
			if(zoomfactor>zoommax)
			{
				zoomfactor=zoommax;
			}

//			ImageFile::bgImage1.BitBltF32T16(vramtex, 512);
		}
		else
		{
			showBG=0;
			bgPath[0]=0;
		}
	}

	if(showBG==2)
	{
		ImageFile::bgImage4.FreeImageBuffer();

		char buf[STRING_LENGTH];

		BuildPath(buf, init_path, "bg_mp3.jpg");
		ImageFile::bgImage1.LoadImage(buf);
		BuildPath(buf, init_path, "bg_file.jpg");
		ImageFile::bgImage2.LoadImage(buf);
		BuildPath(buf, init_path, "bg_text.jpg");
		ImageFile::bgImage3.LoadImage(buf);
	}

	if(showBG==3)
	{
		char buf[STRING_LENGTH];

		BuildPath(buf, init_path, "bg_logo.jpg");
		ImageFile::bgImage4.LoadImage(buf);
	}
}

void SetClockFreq(int cpu, int bus)
{
	scePowerSetClockFrequency(cpu, cpu, bus);
}

void ModifyClockSetting(int isNormal)
{
	int cpuClock, busClock;

	if(isNormal)
	{
		if(curplayer->isPlaying==0)
		{
			cpuClock=normalCPUClock;
			busClock=normalBusClock;

		}
		else
		{
			cpuClock=normalCPUClock+musicClockBoost;
			busClock=normalBusClock+musicClockBoost/2;
		}
	}
	else
	{
		if(curplayer->isPlaying==0)
		{
			cpuClock=powerSaveCPUClock;
			busClock=powerSaveBusClock;
		}
		else
		{
			cpuClock=powerSaveCPUClock+musicClockBoost;
			busClock=powerSaveBusClock+musicClockBoost/2;
		}
	}

	SetClockFreq(cpuClock, busClock);
}

int MainThreadOn=1;
void SaveState();
int pixelFormat=GU_PSM_8888;
int mainThreadId=0;
int mainThreadPriority, decoderThreadPriority, id3ParserPriority;
char *scrName[]={"MUSIC", "FILE", "TEXT", "Image viewer", "", "USB ", "", ""};
int main(int argc, char *argv[])
{
	char buf[STRING_LENGTH];

#if _PSP_FW_VERSION <= 150

	myInitAVModules();

#endif

	mainThreadId=sceKernelGetThreadId();
/*	sceKernelChangeThreadPriority(mainThreadId, 0x20);
	{
		SceKernelThreadInfo mainThreadInfo;
		sceKernelReferThreadStatus(mainThreadId, &mainThreadInfo);
		mainThreadPriority=mainThreadInfo.currentPriority;

		decoderThreadPriority=mainThreadPriority-4;
		id3ParserPriority=mainThreadPriority;
	}*/

//	struct pspvfpu_context *my_vfpu_context=pspvfpu_initcontext();

	decoderThreadPriority=0x20;
	id3ParserPriority=0x20;

	strncpy(init_path, argv[0], STRING_LENGTH);
	char * p = strrchr(init_path, '/');
	*p = 0;

	setupCallbacks();

	pspDebugScreenInit();

	display.Initialize(pixelFormat);

#if _PSP_FW_VERSION <= 150

	SceUID modid = LoadStartModule("support.prx");

#else

	SceUID modid = pspSdkLoadStartModule("support.prx", PSP_MEMORY_PARTITION_KERNEL);

#endif

	if (modid < 0){
		PrintErrMsgWait("support module fail...");
	}

	sceCtrlSetSamplingCycle(0);
	sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

	semaid = sceKernelCreateSema("MyMutex", 0, 0, 20, 0);

	decoderThread = sceKernelCreateThread("decode_thread", DecodeThread, decoderThreadPriority, 256*1024, 
		PSP_THREAD_ATTR_USER/*|THREAD_ATTR_VFPU*/, 0);
	if (decoderThread >= 0)
	{
		sceKernelStartThread(decoderThread, 0, 0);
	}
	else
	{
		PrintErrMsgWait("Decode thread creation error...");
	}
	id3Thread = sceKernelCreateThread("id3_thread", DecodeTitleString, id3ParserPriority, 256*1024, 
		PSP_THREAD_ATTR_USER, 0);
	if (id3Thread >= 0)
	{
		sceKernelStartThread(id3Thread, 0, 0);
	}
	else
	{
		PrintErrMsgWait("Id3 thread creation error...");
	}
#ifdef __HPRM_THREAD
	hprmThread = sceKernelCreateThread("hprm_thread", HPRMThread, 0x12, 4*1024, PSP_THREAD_ATTR_USER, 0);
	if (hprmThread >= 0)
	{
		sceKernelStartThread(hprmThread, 0, 0);
	}
	else
	{
		PrintErrMsgWait("hprm thread creation error...");
	}
#endif

	BuildPath(buf, init_path, "font.nff");
	textTool.Initialize(buf, 0);
	textTool.SetPixelFormat(pixelFormat);
	textTool.SetSpacing(0, 0);
	textTool.SetTextRefCoord(2, 2);
	textTool.SetBaseAddr((char*)display.GetDrawBufferBasePtr());

	BuildPath(buf, init_path, "largefon.nff");
	textTool16.Initialize(buf, 1);
	textTool16.SetPixelFormat(pixelFormat);
	textTool16.SetSpacing(0, 2);
	textTool16.SetTextRefCoord(lmargin, tmargin);
//	textTool16.SetTextRefCoord(3, 30);
	textTool16.SetBaseAddr((char*)display.GetDrawBufferBasePtr());

	bookmark.InitMark();

/*	while(1)
	{
		// check with HOME button.
		display.WaitVBlankAndSwap();
	}*/

	vramtex=(u32*)(((u32)sceGeEdramGetAddr() + VRAM_OFFSET));

	pspAudioInit();
	pspAudioSetChannelCallback(0, audioCallback, NULL);

#ifndef __HPRM_THREAD
	unsigned int remoteButton;
	unsigned int oldRemoteButton=0;
	unsigned int remoteBPress=0;
#endif

//#define RGBA(r, g, b, a) (a<<24|b<<16|g<8|r)
//	for (int y = 0; y < 272; ++y)
//	{
//		for (int x = 0; x < 480; ++x)
//		{
//			if(y<32)
//			titleBuffer[y*512+x]=RGBA(0xffffffff;
//			workBuffer [y*512+x]=0x80808080;
//			vramtex    [y*512+x]=0xff0000ff;
//		}
//	}

	LoadBackground();
	void RegisterAVCodec();
	RegisterAVCodec();

	//printf("bg1 %d %d %08x\n", ImageFile::bgImage1.sizeX, ImageFile::bgImage1.sizeY, ImageFile::bgImage1.imageData);
	//printf("bg2 %d %d %08x\n", ImageFile::bgImage2.sizeX, ImageFile::bgImage2.sizeY, ImageFile::bgImage2.imageData);
	//printf("bg3 %d %d %08x\n", ImageFile::bgImage3.sizeX, ImageFile::bgImage3.sizeY, ImageFile::bgImage3.imageData);
	//printf("edram %08x vramtex %08x\n", (int)sceGeEdramGetAddr(), (int)vramtex1);
	//while(MainThreadOn)
	//{
	//	static int count=0;
	//	if(count++<10)
	//		printf("%d\n", count);

	//	//memset(workBuffer, 0, 512*272*4);
	//	//memcpy(vramtex, workBuffer, 512*272*4);

	//	sceKernelDelayThread(33000);
	//}
	//sceKernelExitGame();

	display.FillBackgroundPtr(titleBuffer, 0, 0, 0, 512, 32);
	display.FillBackgroundPtr(workBuffer, 0, 0, 0, 512, 272);

	int minc=1000;
	int minb=1000;

	int padbits=0;
	padbits=padbits|(PSP_CTRL_UP|PSP_CTRL_DOWN|PSP_CTRL_LEFT|PSP_CTRL_RIGHT);
	padbits=padbits|(PSP_CTRL_CIRCLE|PSP_CTRL_CROSS|PSP_CTRL_TRIANGLE|PSP_CTRL_SQUARE);
	padbits=padbits|(PSP_CTRL_LTRIGGER|PSP_CTRL_RTRIGGER|PSP_CTRL_START|PSP_CTRL_SELECT|PSP_CTRL_HOLD);

	SceCtrlData pad;
	sceCtrlReadBufferPositive(&pad, 1);

	unsigned int oldButtons = 0;
	unsigned int bPress=0;

	float elapTime=0.001f, FPS=0, elapTimeFPS=0;
	int secAdd=0, bgRendered, fcount=0, titleUpdate;

	float repeatTimer=0.5f;
	unsigned int repeatButton=0;
	float videoTitleTimer=0;

	idleSec=0;
	sceRtcGetCurrentTick(&prevtick);

	srand((int)prevtick);

	ModifyClockSetting(1);

//	sceKernelDcacheWritebackAll();

//#define SHOW_POWERSAVE
	while(MainThreadOn)
	{
		scePowerTick(0);

#ifdef SHOW_POWERSAVE
		if(powerMode>=0)
#else
		if(powerMode>0)
#endif
		{
			if( curplayer->haveVideo )
			{
				void WaitForFrameSwap();
				WaitForFrameSwap();
			}

			display.WaitVBlankAndSwap();

			extern int firstVideoUp;
			if( curplayer->haveVideo && firstVideoUp==1)
			{
				firstVideoUp=2;
			}

			//extern int64_t waitingFrameTime;
			//if( 0<waitingFrameTime )
			//{
			//	extern int nermsg;
			//	extern char strbuf[2048][40];

			//	u64 etime;
			//	sceRtcGetCurrentTick(&etime);

			//	int64_t tdiff=(etime-bftick)/1000;
			////	int64_t spos=(int64_t)bufferStartSampleIndex*1000/44100+tdiff;
			//	int64_t spos=(int64_t)(curplayer->playedSample-2048)*1000/44100+tdiff; if(spos<0) spos=0;

			//	//int64_t spos2=(etime-tick0)/1000;
			//	//int64_t tdiff2=spos2-spos;

			//	if(nermsg==2000)
			//		nermsg=0;

			//	snprintf(strbuf[nermsg++], 40, "%d %d %d", (int)waitingFrameTime, (int)spos, (int)(spos-waitingFrameTime));
			//}
		}

		sceRtcGetCurrentTick(&currtick);
		elapTime=(float)(currtick-prevtick)/1e6f;
		elapTimeFPS+=elapTime;

		if(clockBoostRemainTime>0)
		{
			if(clockBoostRemainTime<elapTime)
			{
				ModifyClockSetting(powerMode?1:0);

				clockBoostRemainTime=0;
			}
			else
			{
				clockBoostRemainTime-=elapTime;
			}
		}

		if(curplayer->isPlaying)
		{
			audioStopTime=0;
		}
		else
		{
			audioStopTime+=elapTime;
		}

		fcount++;
		titleUpdate=0;

		sceCtrlReadBufferPositive(&pad, 1);
		pad.Buttons=pad.Buttons&padbits;

		if( (pad.Buttons & PSP_CTRL_HOLD) )
		{
			pad.Lx=128;
			pad.Ly=128;

			if(displayOffWhenHold && idleSec<powerSaveModeTime)
			{
				idleSec=powerSaveModeTime;
			}
		}
		bPress=pad.Buttons & (~oldButtons);
		oldButtons=pad.Buttons;

		if(bPress & PSP_CTRL_UP) repeatButton=PSP_CTRL_UP;
		else if(bPress & PSP_CTRL_DOWN) repeatButton=PSP_CTRL_DOWN;
		else if(bPress & PSP_CTRL_LEFT) repeatButton=PSP_CTRL_LEFT;
		else if(bPress & PSP_CTRL_RIGHT) repeatButton=PSP_CTRL_RIGHT;
		else if(curplayer->haveVideo && bPress & PSP_CTRL_LTRIGGER) repeatButton=PSP_CTRL_LTRIGGER;
		else if(curplayer->haveVideo && bPress & PSP_CTRL_RTRIGGER) repeatButton=PSP_CTRL_RTRIGGER;
		else if(!(pad.Buttons & repeatButton)) repeatButton=0;

		if(showMode!=2 && (pad.Buttons & repeatButton) )
		{
			if( 0<repeatTimer)
			{
				repeatTimer-=elapTime;

				if(repeatTimer<0)
				{
					bPress=bPress|repeatButton;
					repeatTimer=0.1f;
				}
			}
		}
		else
		{
			repeatTimer=0.5f;
		}

#define IsActive() ( (pad.Buttons&0xFDFDFFFF) || pad.Lx==255 || pad.Ly==255 || pad.Lx==0 || pad.Ly==0 )
		if( IsActive() )
		{
			frameModified=1;
			BoostCPU(0.5f);
		}
#ifndef __HPRM_THREAD
		if(sceHprmIsRemoteExist())// && autoSaveMode==0)
		{
			sceHprmPeekCurrentKey((u32*)&remoteButton);
			remoteBPress=remoteButton & (~oldRemoteButton);
			oldRemoteButton=remoteButton;
		}
		else
		{
			remoteButton=remoteBPress=oldRemoteButton=0;
		}
		if(remoteButton&PSP_HPRM_HOLD)
		{
			remoteButton=remoteBPress=oldRemoteButton=0;
		}

		if(remoteBPress)
		{
			frameModified=1;
			BoostCPU(0.5f);
		}
#endif

		if(elapTimeFPS>1.0f)
		{
			secAdd=(int)elapTimeFPS;

			FPS=fcount/elapTimeFPS;

			fcount=0;
			elapTimeFPS=0;

			if(secAdd>5)
				secAdd=5;
		}
		else
		{
			secAdd=0;
		}
		prevtick=currtick;

		// for video...
		if(curplayer->isPlaying && curplayer->haveVideo)
		{
			idleSec=0;
			extMenu=0;
			showMode=0;

			if( IsActive() )
			{
				videoTitleTimer=1.5f;
			}
			else if(0<videoTitleTimer)
			{
				videoTitleTimer-=elapTime;
			}

			//if(bPress & PSP_CTRL_CIRCLE)
			//{
			//	void WriteBitMapFile(const char *fileName, int width, int height, int linesize, void *ptr);
			//	WriteBitMapFile("host0:/cap.bmp", 480, 272, 512, display.GetDrawBufferBasePtr());
			//}
		}

		// neg(PSP_CTRL_HOLD)==0xFFFDFFFF
		if( IsActive() 
			/*|| (remoteTextControl && showMode==2 && remoteButton&13)*/ )
		{
			idleSec=0;
		}
		else
		{
			idleSec+=secAdd;
		}

		if(secAdd>0)
		{
			titleUpdate=1;
			
			mp3TopClamper.Tick();

			if(0<sleepCounter)
			{
				sleepCounter-=secAdd;

				if(sleepCounter<=0)
				{
					sleepCounter=0;
					scePowerSetClockFrequency(222, 222, 111);

					MP3StopTrack();
					ClearBuffers();

					curplayer->_uc.fdValid=0;
					txtFile.fdValid=0;

					idleSec=0;
					sceKernelDelayThread(1000*300);

					scePowerRequestSuspend();

					sceKernelSleepThread();
					BoostCPU(1);

					continue;
				}
			}
		}

		if(secAdd>0 || idleSec==0)
		{
			if(autoSaveMode==0 || idleSec<powerSaveModeTime)
			{
				if(powerMode!=2)
				{
//					titleColor=0xffff0000;
					ModifyClockSetting(1);

					sceGuDisplay(1);

					psplEnableDisplay();

//					sceKernelChangeThreadPriority(decoderThread, mainThreadPriority);
//					sceKernelChangeThreadPriority(id3Thread, mainThreadPriority);

					if(powerMode==0)
					{
						powerMode=2;
						continue;
					}
				}
				powerMode=2;
			}
			else if(idleSec<suspendTime && clockBoostRemainTime==0)
			{
				if(powerMode!=0)
				{
//					titleColor=0xff00ff00;

#ifndef SHOW_POWERSAVE

					psplDisableDisplay();

					sceGuDisplay(0);
#endif

					ModifyClockSetting(0);
				}
				powerMode=0;
			}
			else if(audioStopTime>10 && clockBoostRemainTime==0)
			{
				scePowerSetClockFrequency(222, 222, 111);

				MP3StopTrack();
				ClearBuffers();

				curplayer->_uc.fdValid=0;
				txtFile.fdValid=0;

				idleSec=0;
				sceKernelDelayThread(1000*300);

				scePowerRequestSuspend();

				sceKernelSleepThread();
				BoostCPU(1);

				continue;
			}

			if(usbInitialized)
			{
				usbState=sceUsbGetState();
				if(usbState&PSP_USB_CONNECTION_ESTABLISHED)
				{
					BoostCPU(3);
				}
			}
		}

#ifndef SHOW_POWERSAVE
		if(powerMode<=0)
		{
			sceKernelDelayThread(1000*33);
			continue;
		}
#endif

#ifndef __HPRM_THREAD
		if(remoteTextControl && showMode==2)
		{
			RemoteControlText(remoteButton, remoteBPress);
		}
		else
			RemoteControl(remoteButton, remoteBPress);
#endif

		if( (bPress & PSP_CTRL_SELECT) && !extMenu && showMode<=2)
		{
			showMode=(showMode+1)%3;
			extMenu=0;
			titleUpdate=1;
		}

#if 1
		switch(showMode)
		{
		case 0:
			if(curplayer->isPlaying && curplayer->haveVideo)
			{
				VideoControl(pad, bPress);
			}
			else
			{
				Mp3PlayerControl(pad, bPress);
			}
			break;
		case 1:
			{
			int ret=DirListControl(pad, bPress);

			if(ret==1)
				showMode=0;
			else if(ret==2)
				showMode=2;
			}
			break;
		case 2:
			ShowTextControl(pad, bPress);
			break;
		case 4:
			{
				int ret=OptionControl(pad, bPress);
				if(ret!=-1)
				{
					LoadBackground();

					showMode=ret;
				}
			}
			break;
		case 5:
			if(UsbControl(pad, bPress))
				frameModified=1;
			break;
		default:
			break;
		}

		if(showMode!=1)
			hideUI=0;

		// update end

		if(curBG!=showMode && showBG>=2)
		{
			switch(showMode)
			{
			case 0:
				if(ImageFile::bgImage1.IsLoaded())
				{
					ImageFile::bgImage1.BitBltF32T16(vramtex, 512);
					curBG=showMode;
					sceKernelDcacheWritebackAll();
				}
				else
				{
					curBG=-1;
				}
				break;
			case 1:
			default:
				if(ImageFile::bgImage2.IsLoaded())
				{
					ImageFile::bgImage2.BitBltF32T16(vramtex, 512);
					curBG=showMode;
					sceKernelDcacheWritebackAll();
				}
				else
				{
					curBG=-1;
				}
				break;
			case 2:
				if(ImageFile::bgImage3.IsLoaded())
				{
					ImageFile::bgImage3.BitBltF32T16(vramtex, 512);
					curBG=showMode;
					sceKernelDcacheWritebackAll();
				}
				else
				{
					curBG=-1;
				}
				break;
			case 4:
				if(ImageFile::bgImage4.IsLoaded())
				{
					ImageFile::bgImage4.BitBltF32T16(vramtex, 512);
					curBG=showMode;
					sceKernelDcacheWritebackAll();
				}
				else
				{
					curBG=-1;
				}
				break;
			}
		}

#endif
		{
			extern int isMovieOpening;
			if(isMovieOpening)
			{
				count1++;
				titleUpdate=1;

				sceKernelDelayThread(300*1000);
			}
		}
		sceKernelDcacheWritebackAll();

		display.StartRenderList();

#define SLICE_SIZE 32
		unsigned int j;
		struct Vertex* vertices;

		if(curplayer->haveVideo)
		{
//			sceGuClearColor(0);
//			sceGuClear(GU_COLOR_BUFFER_BIT);
//
//			// nothing at this time.
//			sceGuDisable(GU_BLEND);
//			sceGuTexMode(GU_PSM_8888,0,0,0);
////			sceGuTexImage(0,512,512,512,imgBuffer); // width, height, buffer width, tbp
//			sceGuTexImage(0,512,512,512,vramtex); // width, height, buffer width, tbp
//			sceGuTexFunc(GU_TFX_REPLACE,GU_TCC_RGB);
//			sceGuTexFilter(GU_NEAREST,GU_NEAREST);
//			sceGuTexWrap(GU_CLAMP,GU_CLAMP);
//			sceGuTexScale(1,1);
//			sceGuTexOffset(0,0);
//			sceGuAmbientColor(0xffffffff);
//
//			for (j = 0; j < 480; j = j+SLICE_SIZE)
//			{
//				vertices = (struct Vertex*)sceGuGetMemory(2 * sizeof(struct Vertex));
//
//				vertices[0].u = j; vertices[0].v = 0;
//				vertices[0].color = 0;
//				vertices[0].x = j; vertices[0].y = 0; vertices[0].z = -0.5f;
//				vertices[1].u = j+SLICE_SIZE; vertices[1].v = 272;
//				vertices[1].color = 0;
//				vertices[1].x = j+SLICE_SIZE; vertices[1].y = 272; vertices[1].z = -0.5f;
//
//				sceGuDrawArray(GU_SPRITES,GU_TEXTURE_32BITF|GU_COLOR_8888|GU_VERTEX_32BITF|GU_TRANSFORM_2D,
//					2, 0, vertices);
//			}
		}
		else if(showBG==1)// && ImageFile::bgImage1.bufferWidth<1024)// always loaded...
		{
			sceGuClearColor(0);
			sceGuClear(GU_COLOR_BUFFER_BIT);
			unsigned char *texStart;
			sceGuDisable(GU_BLEND);
			sceGuTexMode(GU_PSM_8888,0,0,0);
			sceGuTexImage(0, 512, 512, ImageFile::bgImage1.bufferWidth, ImageFile::bgImage1.imageData); // width, height, buffer width, tbp
			sceGuTexFunc(GU_TFX_REPLACE,GU_TCC_RGB);
			sceGuTexFilter(GU_LINEAR,GU_LINEAR);
			sceGuTexWrap(GU_CLAMP,GU_CLAMP);
			sceGuTexScale(1,1);
			sceGuTexOffset(0,0);
			sceGuAmbientColor(0xffffffff);

			float factor=zoomfactor;
			int il, xleft=0, pagebase=ix/512, iybase;
			for(int igy=0; (float)igy<(272*factor)/512.0f; igy++)
			{
				iybase=iy+igy*512;
				xleft=0;
				for(int ig=0; (float)ig<((ix&511)+480*factor)/512.0f; ig++)
				{
					il=(ig==0?(ix&511):0);
					texStart=ImageFile::bgImage1.imageData+
						(pagebase+ig)*(4*512*ImageFile::bgImage1.sizeY)+
						4*(iybase*512);
					sceGuTexImage(0, 512, 512, 512, texStart);
					for(j=il; j<512; j=j+(int)(SLICE_SIZE*factor))
					{
						vertices = (struct Vertex*)sceGuGetMemory(2 * sizeof(struct Vertex));

						vertices[0].u = j; vertices[0].v = 0;
						vertices[0].color = 0;
						vertices[0].x = xleft+(j-il)/factor; vertices[0].y = igy*512/factor; vertices[0].z = -0.5f;
						vertices[1].u = j+SLICE_SIZE*factor; vertices[1].v = 512;
						vertices[1].color = 0;
						vertices[1].x = xleft+(j-il)/factor+SLICE_SIZE; vertices[1].y = (igy+1)*512/factor; vertices[1].z = -0.5f;
						if(vertices[1].y>272)
						{
							vertices[1].y=272;
						}
						vertices[1].v = (vertices[1].y-vertices[0].y)*factor;

						sceGuDrawArray(GU_SPRITES,GU_TEXTURE_32BITF|GU_COLOR_8888|GU_VERTEX_32BITF|GU_TRANSFORM_2D,
							2, 0, vertices);
					}
					xleft+=(int)((512-il)/factor);
				}
			}

			//			float factor=1.4583333333333333333333333333333f, intpart, floatpart;
			/*			float factor=1;
			//			for(int ig=0; (float)ig<ImageFile::bgImage1.sizeX/512.0f; ig++)
			for(int ig=0; (float)ig<factor; ig++)
			{
			texStart=ImageFile::bgImage1.imageData+
			//					1*(4*512*ImageFile::bgImage1.sizeY)+
			4*(iy*512);
			sceGuTexImage(0, 512, 512, 512, texStart);
			for (j = 0; j < 512/factor; j = j+SLICE_SIZE)
			{
			if(j>480)
			break;

			vertices = (struct Vertex*)sceGuGetMemory(2 * sizeof(struct Vertex));

			vertices[0].u = j*factor; vertices[0].v = 0;
			vertices[0].color = 0;
			vertices[0].x = j+ig*512/factor; vertices[0].y = 0; vertices[0].z = -0.5f;
			vertices[1].u = (j+SLICE_SIZE)*factor; vertices[1].v = 272*factor;
			vertices[1].color = 0;
			vertices[1].x = j+ig*512/factor+SLICE_SIZE; vertices[1].y = 272; vertices[1].z = -0.5f;

			sceGuDrawArray(GU_SPRITES,GU_TEXTURE_32BITF|GU_COLOR_8888|GU_VERTEX_32BITF|GU_TRANSFORM_2D,
			2, 0, vertices);
			}

			//				break;
			}*/
			/*			for (j = 0; j < 480; j = j+SLICE_SIZE)
			{
			vertices = (struct Vertex*)sceGuGetMemory(2 * sizeof(struct Vertex));

			vertices[0].u = j; vertices[0].v = 0;
			vertices[0].color = 0;
			vertices[0].x = j; vertices[0].y = 0; vertices[0].z = -0.5f;
			vertices[1].u = j+SLICE_SIZE; vertices[1].v = 272;
			vertices[1].color = 0;
			vertices[1].x = j+SLICE_SIZE; vertices[1].y = 272; vertices[1].z = -0.5f;

			sceGuDrawArray(GU_SPRITES,GU_TEXTURE_32BITF|GU_COLOR_8888|GU_VERTEX_32BITF|GU_TRANSFORM_2D,
			2, 0, vertices);
			}*/
		}
		//		else if(showBG==2)
		else if(showBG)
		{
			sceGuDisable(GU_BLEND);
			sceGuTexMode(GU_PSM_8888,0,0,0);
//			sceGuTexImage(0,512,512,512,imgBuffer); // width, height, buffer width, tbp
			sceGuTexImage(0,512,512,512,vramtex); // width, height, buffer width, tbp
			sceGuTexFunc(GU_TFX_REPLACE,GU_TCC_RGB);
			sceGuTexFilter(GU_NEAREST,GU_NEAREST);
			sceGuTexWrap(GU_CLAMP,GU_CLAMP);
			sceGuTexScale(1,1);
			sceGuTexOffset(0,0);
			sceGuAmbientColor(0xffffffff);

			for (j = 0; j < 480; j = j+SLICE_SIZE)
			{
				vertices = (struct Vertex*)sceGuGetMemory(2 * sizeof(struct Vertex));

				vertices[0].u = j; vertices[0].v = 0;
				vertices[0].color = 0;
				vertices[0].x = j; vertices[0].y = 0; vertices[0].z = -0.5f;
				vertices[1].u = j+SLICE_SIZE; vertices[1].v = 272;
				vertices[1].color = 0;
				vertices[1].x = j+SLICE_SIZE; vertices[1].y = 272; vertices[1].z = -0.5f;

				sceGuDrawArray(GU_SPRITES,GU_TEXTURE_32BITF|GU_COLOR_8888|GU_VERTEX_32BITF|GU_TRANSFORM_2D,
					2, 0, vertices);
			}
		}
		else
		{
			sceGuClearColor(bgColor);
			sceGuClear(GU_COLOR_BUFFER_BIT);
		}

		if( curplayer->isPlaying && curplayer->haveVideo)
		{
			void RenderVideoImage();
			RenderVideoImage();

			if(curplayer->haveSubtitle)
			{
				void PrepareSubBuf();
				PrepareSubBuf();

				extern int64_t waitingFrameTime;
				void UpdateSubtitle(int refVideoTime);
				UpdateSubtitle((int)(waitingFrameTime/10));

				void RenderSubtitle();
				RenderSubtitle(); 
			}
		}

		extern int enableVideoTitle;
		int drawTitle=curplayer->haveVideo?(enableVideoTitle||videoTitleTimer>0):1;
		int drawFrame=curplayer->haveVideo?0:1;
		if( drawTitle )
		{
			sceGuEnable(GU_BLEND);
			sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
			sceGuTexMode(GU_PSM_8888,0,0,0);
			sceGuTexFunc(GU_TFX_REPLACE,GU_TCC_RGBA);
			sceGuTexFilter(GU_NEAREST,GU_NEAREST);
			sceGuTexWrap(GU_CLAMP,GU_CLAMP);
			sceGuTexScale(1,1);
			sceGuTexOffset(0,0);
			sceGuAmbientColor(0xffffffff);

			//sceGuDisable(GU_BLEND);
			if(extMenu || !( (showMode==2&&fullScreenText) || (showMode==1&&hideUI) ) )
			{
				sceGuTexImage(0,512,512,512,titleBuffer); // width, height, buffer width, tbp
				for (j = 0; j < 480; j = j+SLICE_SIZE)
				{
					vertices = (struct Vertex*)sceGuGetMemory(2 * sizeof(struct Vertex));

					vertices[0].u = j; vertices[0].v = 0;
					vertices[0].color = 0;
					vertices[0].x = j; vertices[0].y = 0; vertices[0].z = 0;
					vertices[1].u = j+SLICE_SIZE; vertices[1].v = 32;
					vertices[1].color = 0;
					vertices[1].x = j+SLICE_SIZE; vertices[1].y = 32; vertices[1].z = 0;

					sceGuDrawArray(GU_SPRITES,GU_TEXTURE_32BITF|GU_COLOR_8888|GU_VERTEX_32BITF|GU_TRANSFORM_2D,
						2, 0, vertices);
				}
			}
		}

		if( drawFrame )
		{
			sceGuTexImage(0,512,512,512,workBuffer); // width, height, buffer width, tbp
			for (j = 0; j < 480; j = j+SLICE_SIZE)
			{
				vertices = (struct Vertex*)sceGuGetMemory(2 * sizeof(struct Vertex));

				vertices[0].u = j; vertices[0].v = 0;
				vertices[0].color = 0;
				vertices[0].x = j; vertices[0].y = 0; vertices[0].z = 0;
				vertices[1].u = j+SLICE_SIZE; vertices[1].v = 272;
				vertices[1].color = 0;
				vertices[1].x = j+SLICE_SIZE; vertices[1].y = 272; vertices[1].z = 0;

				sceGuDrawArray(GU_SPRITES,GU_TEXTURE_32BITF|GU_COLOR_8888|GU_VERTEX_32BITF|GU_TRANSFORM_2D,
					2, 0, vertices);
			}
		}

		display.FinishRenderList();

#if 1

		if( drawTitle && curplayer->haveVideo && (titleUpdate || frameModified) )
		{
			display.FillBackgroundPtr(titleBuffer, 0, 0, 0, 512, 32);
			textTool.SetBaseAddr((char*)titleBuffer);

			void DrawVideoTitle(float fps);
			DrawVideoTitle(FPS);
		}
		else
		if( drawTitle && curplayer->haveVideo==0 && (titleUpdate || frameModified) )
		{
			display.FillBackgroundPtr(titleBuffer, 0, 0, 0, 512, 32);
			textTool.SetBaseAddr((char*)titleBuffer);
			int ccc=scePowerGetCpuClockFrequency();
			int ddd=scePowerGetBusClockFrequency();
			int eee=sceKernelDevkitVersion();
			//extern int xxxxx;
			//		int maxmem1=sceKernelMaxFreeMemSize()/1024;
			//		int maxmem2=sceKernelTotalFreeMemSize()/1024;
			//		unsigned long *pix=(unsigned long *)((unsigned int)sceGeEdramGetAddr() + 4*(512*20+32)), ccc;
			//		unsigned long *pix2=(unsigned long *)((unsigned int)sceGeEdramGetAddr() + 4*(512*20+64)), ccc2;
			//		ccc=*pix;
			//		ccc2=*pix;
			//snprintf(buf, STRING_LENGTH, "[%08x:%08x]->[%08x:%08x] %.3f %d %3.2f %d %d", (unsigned long)(prevtick>>32), (unsigned long)(prevtick), (unsigned long)(currtick>>32), (unsigned long)(currtick), elapTime, idleSec, clockBoostRemainTime, curBG, showBG);
			//snprintf(buf, STRING_LENGTH, "F:%3.2f=C:%d/T:%3.2f %d %d", FPS, fcount, elapTimeFPS, ImageFile::bgImage1.sizeX, ImageFile::bgImage1.sizeY);
			//		snprintf(buf, STRING_LENGTH, "F:%3.2f %dK/%dK", FPS, maxmem1, maxmem2);
			//		snprintf(buf, STRING_LENGTH, "F:%3.2f %dK/%dK (%d, %d) %d %s", FPS, maxmem1, maxmem2, ImageFile::bgImage1.sizeX, ImageFile::bgImage1.sizeY, xxxxx, kkkk);
			//		snprintf(buf, STRING_LENGTH, "%d %d %08x %s", remoteTextControl, autoSaveMode, remoteButton, sceHprmIsRemoteExist()?"ON":"OFF");
			//		snprintf(buf, STRING_LENGTH, "pri %02x", mainThreadPriority);
			//		snprintf(buf, STRING_LENGTH, "%d %d %3.2f", powerMode, GetAvailSize(), (float)GetAvailSize()/(float)TOTAL_SAMPLE_COUNT);
			//		snprintf(buf, STRING_LENGTH, "%d %08x %s", usbInitialized, usbState, scrName[showMode]);
			//snprintf(buf, STRING_LENGTH, "C:%d B:%d F:%3.2f B:%.1f S:%.1f", ccc, ddd, FPS, clockBoostRemainTime, (float)(showplayer->copiedSample-showplayer->playedSample)/44100.0f);
			//snprintf(buf, STRING_LENGTH, "P:%d S:%d M:%d cp %d pl %d(%.1f) S:%.1f (H:%d S:%d) %d %d %d",
			//	curplayer==&player1?1:2, showplayer==&player1?1:2, (int)audioPlayMode, 
			//	showplayer->copiedSample, showplayer->playedSample, showplayer->playedSample/44100.f, (float)(showplayer->copiedSample-showplayer->playedSample)/44100.0f,
			//	showplayer->headerSample, showplayer->seekSample, showplayer->headerSampleWritePos, showplayer->seekStat, showplayer->fileCurPos);
			if( !titleType && !(usbState&PSP_USB_ACTIVATED) )
			{
				if(	count2==0 )
				{
					snprintf(buf, STRING_LENGTH, "PSPLAYER %s ", scrName[showMode]);
				}
				else
				{
					snprintf(buf, STRING_LENGTH, "%d %d %d %.1f %s ", count1, count2, count3, (float)(curplayer->copiedSample-curplayer->playedSample)/44100.0f, errorString);
				}
			}
			else
			{
				snprintf(buf, STRING_LENGTH, "[%03d/%03d] [%.1ffps/%.1f] %s", ccc, ddd, FPS,
					curplayer->isPlaying?(float)(curplayer->copiedSample-curplayer->playedSample)/44100.0f:0, (usbState&PSP_USB_ACTIVATED?"USB":"") );
			}

			if(showMode==0)
			{
				char volstr[30];
				int vol=imposeGetVolume();
				snprintf(volstr, 30, "VB:%.1f VOL[%02d/30]", volumeBoost, vol);
				strcat(buf, volstr);
			}
			else if(showMode==2 && textFileSize)
			{
				char txtstr[STRING_LENGTH];
				if(fullScreenText==0 || extMenu)
				{
					char nameEntry[STRING_LENGTH];
					int tline=totalLines/maxLine; if(totalLines%maxLine) tline+=1;
					if(showTagOnly)
						strcpy(nameEntry, textTag.c_str());
					else
					{
						char *nameOnly=strrchr(textFileName.c_str(), '/');
						strncpy(nameEntry, nameOnly+1, STRING_LENGTH);
					}

					if(strlen(nameEntry)>=20)
					{
						char *st=FindNextLine(nameEntry, 11);
						strcpy(st, "...");
					}

					if(titleType)
					{
						nameEntry[0]=0;
					}
					snprintf(txtstr, STRING_LENGTH, "%s P(%d/%d)", nameEntry, curLine/maxLine+1, tline);
					strcat(buf, txtstr);
				}
			}
			textTool.PutStringMByteCC(0, 0, titleColor, buf);

			pspTime curTime;
			sceRtcGetCurrentClock(&curTime, timeZoneOffset*60);

			if(extMenu || sleepCounter)
			{
				snprintf(buf, STRING_LENGTH, "%s %02d:%02d Sleep: %dmin",
					curTime.hour<12?"am":"pm", curTime.hour>12?curTime.hour-12:curTime.hour,
					curTime.minutes, (sleepCounter+59)/60);
				textTool.PutStringMByteCC(25, 0, titleColor, buf);
			}
			else
			{
				snprintf(buf, STRING_LENGTH, "%d/%02d %s %02d:%02d:%02d", curTime.month, curTime.day,
					curTime.hour<12?"am":"pm", curTime.hour>12?curTime.hour-12:curTime.hour,
					curTime.minutes, curTime.seconds);
				textTool.PutStringMByteCC(25, 0, titleColor, buf);
			}

			if(scePowerIsBatteryExist())
			{
				snprintf(buf, STRING_LENGTH, "%s %02d%%", sceHprmIsRemoteExist()?"R":" ", scePowerGetBatteryLifePercent());
				textTool.PutStringMByteCC(37, 0, 0xffff80e0, buf);
			}
			else
			{
				snprintf(buf, STRING_LENGTH, "%s ≠♨", sceHprmIsRemoteExist()?"R":" ");
				textTool.PutStringMByteCC(37, 0, 0xffff80e0, buf);
			}

			if(showplayer->isPlaying)
			{
				int i=showplayer->fileIndex;
				char tagString[STRING_LENGTH];

				if(titleStringStatus==2 && titleString[i].c_str()[0])
				{
					snprintf(tagString, STRING_LENGTH, "(%s)", titleString[i].c_str());
				}
				else
				{
					tagString[0]=0;
				}

				if(showTagOnly && tagString[0])
				{
					snprintf(buf, STRING_LENGTH, "%s", titleString[i].c_str());
				}
				else
				{
					snprintf(buf, STRING_LENGTH, "%s%s", showplayer->fileName.c_str(), tagString[0]?tagString:"" );
				}

				mp3TopClamper.StrSet(buf);

				snprintf(buf, STRING_LENGTH, "%s (%d/%d) (%d:%02d/%d:%02d) %dkbps %d㎑ %s %s %s", mp3TopClamper.Get(),
					showplayer->fileIndex+1, numFiles, showplayer->playedSample/44100/60, (showplayer->playedSample/44100)%60,
					showplayer->estimatedSeconds/60, showplayer->estimatedSeconds%60, showplayer->bitRate/1000, (int)(showplayer->samplingRate/1000),
					(playerMode==PM_RANDOM)?"RANDOM":"      ", autoRepeat?"∽":"  ",
					(audioPlayMode==APM_PAUSED /*&& tval.tv_usec<500000*/?"PAUSED":""));
//					(audioPlayMode==APM_PAUSED && tval.tv_usec<500000?"PAUSED":""));
				textTool.PutStringMByteCC(0, 1, playtextColor, buf);
			}
		}

		if( drawFrame && frameModified )
		{
			display.FillBackgroundPtr(workBuffer, 0, 0, 0, 512, 272);
			textTool.SetBaseAddr((char*)workBuffer);
			textTool16.SetBaseAddr((char*)workBuffer);

			if(showMode==0)
			{
				ShowMp3();
			}
			else if(showMode==1)
			{
				ShowDirList();
			}
			else if(showMode==2)
			{
				ShowTextFile();
			}
			else if(showMode==4)
			{
				ShowOption();
			}
			else if(showMode==5)
			{
				ShowUsbOption();
			}

			sceKernelDcacheWritebackAll();
		}

		frameModified=0;
#endif
	}

	scePowerTick(0);
	psplEnableDisplay();
	scePowerSetClockFrequency(222, 222, 111);

	if(usbInitialized)
	{
		FinalizeUSBDriver();
	}

	if(showMode!=4)
        SaveState();

	int waitdcount=100;
	while(0<=decodeThreadOn && 0<waitdcount--)
	{
		sceKernelDelayThread(20*1000);
	}

	if(waitdcount<=0)
	{
		sceKernelExitGame();
		return 0;
	}

	// send stop command.
	// wait.
	SceUInt timeout=50*1000;
//	sceKernelWaitThreadEnd(usbThread, &timeout);
	sceKernelWaitThreadEnd(id3Thread, &timeout);
	sceKernelWaitThreadEnd(decoderThread, &timeout);

	// kill thread.
//	sceKernelTerminateDeleteThread(usbThread);
	sceKernelTerminateDeleteThread(hprmThread);
	sceKernelTerminateDeleteThread(id3Thread);
	sceKernelTerminateDeleteThread(decoderThread);

	pspAudioEnd();

	textTool.Finalize();
	textTool16.Finalize();
	display.Finalize();

	sceKernelExitGame();

	return 0;
}

int power_callback(int unknown, int pwrflags, void *common)
{
    /* check for power switch and suspending as one is manual and the other automatic */
    if (pwrflags & PSP_POWER_CB_POWER_SWITCH || pwrflags & PSP_POWER_CB_SUSPENDING)
	{
/*		if(curplayer->isPlaying)
		{
			audioRestartPos=-1;
		}*/

		MP3StopTrack();

		ClearBuffers();
		sceKernelDelayThread(1000*100);

		curplayer->_uc.fdValid=0;
		txtFile.fdValid=0;

//	sprintf(powerCBMessage,
//		"first arg: 0x%08X, flags: 0x%08X: suspending\n", unknown, pwrflags);
    }
	else if (pwrflags & PSP_POWER_CB_RESUMING)
	{
		scePowerTick(0);
//	sprintf(powerCBMessage,
//		"first arg: 0x%08X, flags: 0x%08X: resuming from suspend mode\n",
//		unknown, pwrflags);
    }
	else if (pwrflags & PSP_POWER_CB_RESUME_COMPLETE)
	{
		sceKernelWakeupThread(mainThreadId);
/*		idleSec=0;
		if(audioRestartPos==-1)
		{
			audioRestartPos=-2;
		}*/
//	sprintf(powerCBMessage,
//		"first arg: 0x%08X, flags: 0x%08X: resume complete\n", unknown, pwrflags);
    }
	else if (pwrflags & PSP_POWER_CB_STANDBY)
	{
//	sprintf(powerCBMessage,
//		"first arg: 0x%08X, flags: 0x%08X: entering standby mode\n", unknown, pwrflags);
    }
	else
	{
//	sprintf(powerCBMessage, "first arg: 0x%08X, flags: 0x%08X: Unhandled power event\n", unknown, pwrflags);
    }
//    sceDisplayWaitVblankStart();

	return 0;
}

unsigned long DecodeColor(const char *data)
{
	unsigned long ret=0;
	int len=strlen(data);
	int fourbit;
	for(int i=0; i<len; i++)
	{
		ret=ret<<4;
		fourbit=0;
		if('0'<=*data && *data<='9')
			fourbit=*data-'0';
		else if('A'<=*data && *data<='F')
			fourbit=*data-'A'+0x0a;
		else if('a'<=*data && *data<='f')
			fourbit=*data-'a'+0x0a;
		ret = ret | fourbit;
		data++;
	}

	return ret;
}

void InitialLoad(int loadmp3, int loadtext)
{
	int dirLoaded=0;
	char buf[STRING_LENGTH];
	char tname[STRING_LENGTH]="";

	BuildPath(buf, init_path, "option.txt");
	FILE *fp=fopen(buf, "r");

	if(fp)
	{
		char token[STRING_LENGTH], data[STRING_LENGTH*2];

		while(!feof(fp))
		{
			fscanf(fp, "%s : %s\n", token, data);
			if(!strcmp(token, "TextFile"))
			{
				DecodeString(tname, STRING_LENGTH, data);
			}
			else if(!strcmp(token, "LastDir"))
			{
				DecodeString(token, STRING_LENGTH,  data);
				if(GetDirFileList(token)==0)
					GetDirFileList(init_path);
				dirLoaded=1;
			}
			else if(!strcmp(token, "Random"))
			{
				int rrr=atoi(data);
				if(rrr==0)
				{
					playerMode=PM_SEQUENTIAL;
				}
				else
					playerMode=PM_RANDOM;
			}
			else if(!strcmp(token, "autoRepeat"))
			{
				autoRepeat=atoi(data);
			}
			else if(!strcmp(token, "ShowBG"))
			{
				showBG=atoi(data);
				if(showBG)
					showBG=2;
			}
			else if(!strcmp(token, "useLargeFont"))
			{
				useLargeFont=atoi(data);
			}
			else if(!strcmp(token, "bgColor"))
			{
				bgColor=DecodeColor(data);
			}
			else if(!strcmp(token, "titleColor"))
			{
				titleColor=DecodeColor(data);
			}
			else if(!strcmp(token, "playtextColor"))
			{
				playtextColor=DecodeColor(data);
			}
			else if(!strcmp(token, "generalTextColor"))
			{
				generalTextColor=DecodeColor(data);
			}
			else if(!strcmp(token, "generalTextColor"))
			{
				generalTextColor=DecodeColor(data);
			}
			else if(!strcmp(token, "selectedTextColor"))
			{
				selectedTextColor=DecodeColor(data);
			}
			else if(!strcmp(token, "playingTextColor"))
			{
				playingTextColor=DecodeColor(data);
			}
			else if(!strcmp(token, "textViewerColor"))
			{
				textViewerColor=DecodeColor(data);
			}
			else if(!strcmp(token, "shadowColor"))
			{
				textTool.shadowColor=DecodeColor(data);
				textTool16.shadowColor=DecodeColor(data);
			}
			else if(!strcmp(token, "showTagOnly"))
			{
				showTagOnly=atoi(data);
			}
			else if(!strcmp(token, "filterMode"))
			{
				filterMode=(FilterMode)atoi(data);
			}
			else if(!strcmp(token, "showDateSize"))
			{
				showDateSize=atoi(data);
			}
			else if(!strcmp(token, "normalCPUClock"))
			{
				normalCPUClock=atoi(data);
			}
			else if(!strcmp(token, "normalBusClock"))
			{
				normalBusClock=atoi(data);
			}
			else if(!strcmp(token, "powerSaveCPUClock"))
			{
				powerSaveCPUClock=atoi(data);
			}
			else if(!strcmp(token, "powerSaveBusClock"))
			{
				powerSaveBusClock=atoi(data);
			}
			else if(!strcmp(token, "timeZoneOffset"))
			{
				timeZoneOffset=atoi(data);
				if(!(-12<=timeZoneOffset && timeZoneOffset<=12))
				{
					timeZoneOffset=9;
				}
			}
			else if(!strcmp(token, "remoteTextControl"))
			{
				remoteTextControl=atoi(data);
			}
			else if(!strcmp(token, "lineGap"))
			{
				lineGap=atoi(data);
			}
			else if(!strcmp(token, "fullScreenText"))
			{
				fullScreenText=atoi(data);
			}
			else if(!strcmp(token, "autoSaveMode"))
			{
				autoSaveMode=atoi(data);
			}
			else if(!strcmp(token, "powerSaveModeTime"))
			{
				powerSaveModeTime=atoi(data);
			}
			else if(!strcmp(token, "suspendTime"))
			{
				suspendTime=atoi(data);
			}
			else if(!strcmp(token, "displayOffWhenHold"))
			{
				displayOffWhenHold=atoi(data);
			}
			else if(!strcmp(token, "forceTrimLeft"))
			{
				forceTrimLeft=atoi(data);
			}
			else if(!strcmp(token, "libmadClockBoost"))
			{
				libmadClockBoost=atoi(data);
			}
			else if(!strcmp(token, "oggvorbisClockBoost"))
			{
				oggvorbisClockBoost=atoi(data);
			}
			else if(!strcmp(token, "meClockBoost"))
			{
				meClockBoost=atoi(data);
			}
			else if(!strcmp(token, "aviClockBoost"))
			{
				aviClockBoost=atoi(data);
			}
			else if(!strcmp(token, "useLibMAD"))
			{
				useLibMAD=atoi(data);
			}
			else if(!strcmp(token, "mpeg4vmecsc"))
			{
				mpeg4vmecsc=atoi(data);
			}
			else if(!strcmp(token, "scaleMode"))
			{
				extern int scaleMode;
				scaleMode=atoi(data);
			}
			else if(!strcmp(token, "Bookmark"))
			{
				int len=strlen(data);

				if(len==16)
				{
					int ftellPos=DecodeColor(data+8);
					data[8]=0;
					int fileSize=DecodeColor(data);
					bookmark.AddMark(fileSize, ftellPos);
				}
			}
		}
		fclose(fp);
	}

	// fix some param.
	normalCPUClock=max(1, min(333, normalCPUClock));
	normalBusClock=max(normalCPUClock/2, min(normalCPUClock, normalBusClock));

	powerSaveCPUClock=max(1, min(333, powerSaveCPUClock));
	powerSaveBusClock=max(powerSaveCPUClock/2, min(powerSaveCPUClock, powerSaveBusClock));

	musicClockBoost=max(1, min(333, musicClockBoost));

	if(dirLoaded==0)
        GetDirFileList(init_path);

	CalcTextLineParam();

	if(loadtext && strlen(tname)>0)
	{
		if(LoadTextFile(tname))
		{
			LoadText();
		}
	}

	if(loadmp3)
	{
		char buf[STRING_LENGTH];
		BuildPath(buf, init_path, "playlist.m3u");
		ParseM3UFile(buf, NULL);

		audioPlayMode=APM_PAUSED;

		curplayer->abortFlag=1;

		SetCmd(PC_PLAYNEWFILES, (void*)&mfdCallBuffer);
	}
}

void SaveState()
{
	char buf[STRING_LENGTH*2];
	FILE *fplist=NULL, *fpoption=NULL;

	if(numFiles)
	{
		BuildPath(buf, init_path, "playlist.m3u");

		fplist=fopen(buf, "w");

		if(fplist)
		{
			for(int i=0; i<numFiles; i++)
			{
				fprintf(fplist, "%s\n", fileList[i].c_str());
			}
		}
		
	}

	{
		BuildPath(buf, init_path, "option.txt");
		fpoption=fopen(buf, "w");

		if(fpoption)
		{
			EncodeString(buf, STRING_LENGTH*2,  curPath.c_str());
			fprintf(fpoption, "LastDir : %s\n", buf);
			fprintf(fpoption, "Random : %d\n", playerMode==PM_SEQUENTIAL?0:1);
			fprintf(fpoption, "autoRepeat : %d\n", autoRepeat);
			fprintf(fpoption, "ShowBG : %d\n", showBG);
			fprintf(fpoption, "useLargeFont : %d\n", useLargeFont);
			fprintf(fpoption, "bgColor : %08x\n", bgColor);
			fprintf(fpoption, "titleColor : %08x\n", titleColor);
			fprintf(fpoption, "playtextColor : %08x\n", playtextColor);
			fprintf(fpoption, "generalTextColor : %08x\n", generalTextColor);
			fprintf(fpoption, "selectedTextColor : %08x\n", selectedTextColor);
			fprintf(fpoption, "playingTextColor : %08x\n", playingTextColor);
			fprintf(fpoption, "textViewerColor : %08x\n", textViewerColor);
			fprintf(fpoption, "shadowColor : %08x\n", textTool.shadowColor);
			fprintf(fpoption, "showTagOnly : %d\n", showTagOnly);
			fprintf(fpoption, "filterMode : %d\n", filterMode);
			fprintf(fpoption, "showDateSize : %d\n", showDateSize);
			fprintf(fpoption, "normalCPUClock : %d\n", normalCPUClock);
			fprintf(fpoption, "normalBusClock : %d\n", normalBusClock);
			fprintf(fpoption, "powerSaveCPUClock : %d\n", powerSaveCPUClock);
			fprintf(fpoption, "powerSaveBusClock : %d\n", powerSaveBusClock);
			fprintf(fpoption, "autoSaveMode : %d\n", autoSaveMode);
			fprintf(fpoption, "timeZoneOffset : %d\n", timeZoneOffset);
			fprintf(fpoption, "powerSaveModeTime : %d\n", powerSaveModeTime);
			fprintf(fpoption, "remoteTextControl : %d\n", remoteTextControl);
			fprintf(fpoption, "lineGap : %d\n", lineGap);
			fprintf(fpoption, "suspendTime : %d\n", suspendTime);
			fprintf(fpoption, "displayOffWhenHold : %d\n", displayOffWhenHold);
			fprintf(fpoption, "forceTrimLeft : %d\n", forceTrimLeft);
			fprintf(fpoption, "fullScreenText : %d\n", fullScreenText);

			fprintf(fpoption, "libmadClockBoost : %d\n", libmadClockBoost);
			fprintf(fpoption, "oggvorbisClockBoost : %d\n", oggvorbisClockBoost);
			fprintf(fpoption, "meClockBoost : %d\n", meClockBoost);
			fprintf(fpoption, "aviClockBoost : %d\n", aviClockBoost);
			fprintf(fpoption, "useLibMAD : %d\n", useLibMAD);
			fprintf(fpoption, "mpeg4vmecsc : %d\n", mpeg4vmecsc);

			extern int scaleMode;
			fprintf(fpoption, "scaleMode : %d\n", scaleMode);

			for(int i=0; i<NUM_BOOKMARK; i++)
			{
				if(bookmark.data[i].fileSize>0)
					fprintf(fpoption, "Bookmark : %08x%08x\n", bookmark.data[i].fileSize, bookmark.data[i].ftellPos);
			}
			
			if(totalLines)
			{
				EncodeString(buf, STRING_LENGTH*2,  textFileName.c_str());
				fprintf(fpoption, "TextFile : %s\n", buf);
			}

		}
	}
	if(fplist)
		fclose(fplist);
	if(fpoption)
		fclose(fpoption);

}

void PrintErrMsgWait(const char *msg)
{
	sceDisplayWaitVblankStart();

	//pspDebugScreenClear();
	//pspDebugScreenSetXY(0,10);

	printf("\n");
	printf(msg);

	while(MainThreadOn)
	{
		sceDisplayWaitVblankStart();
	}

	sceKernelExitGame();
}

int exit_callback(int arg1, int arg2, void *common)
{
	decodeThreadOn=0;
	id3ThreadOn=0;
	HPRMThreadOn=0;
	MainThreadOn=0;

	sceKernelDelayThread(1000);

	while(0<=decodeThreadOn)
	{
		sceKernelDelayThread(1000*20);

		MP3StopTrack();
	}

	pspAudioEndPre();

	//sceKernelExitGame();

	return 0;
}

//#define USE_FFMPEG_DIGEST
#ifdef  USE_FFMPEG_DIGEST

extern "C" {
#include "avformat.h"
}

#include "Queue.h"

AVFormatContext *pFormatCtx;

AVCodecContext  *vCodecCtx;
AVStream		*vStream;
AVCodec         *vCodec;

AVCodecContext  *aCodecCtx;
AVStream		*aStream;
AVCodec         *aCodec;

ImageQueue iq;

AVFrame *vFrame;
void *vFrameMemPtr;
int vFrameImgSize=0;

int (*packet_reader)(AVFormatContext *s, AVPacket *pkt);

//AVPacket _packet, *_pkt=&_packet;

int vstreamindex=-1;
int astreamindex=-1;

static int debug = 0;
static int debug_mv = 0;
static int workaround_bugs = 1;

static int fast = 0;
static int genpts = 1;
static int lowres = 0;
static int idct_v = FF_IDCT_AUTO;
static int idct_a = FF_IDCT_AUTO;
static enum AVDiscard skip_frame= AVDISCARD_DEFAULT;
static enum AVDiscard skip_idct= AVDISCARD_DEFAULT;
static enum AVDiscard skip_loop_filter= AVDISCARD_DEFAULT;
static int error_resilience_v = FF_ER_CAREFUL;
static int error_resilience_a = FF_ER_CAREFUL;
static int error_concealment = 3;

static int my_mp3_decoder_getEDRAM=0;
static int my_mp3_close(AVCodecContext * avctx)
{
	if ( my_mp3_decoder_getEDRAM )
	{ 
		sceAudiocodecReleaseEDRAM(mp3_codec_buffer); 
		my_mp3_decoder_getEDRAM=0;
	} 

	return 0;
}
static int my_mp3_decode_init(AVCodecContext * avctx)
{
	myInitAVModules();

	memset(mp3_codec_buffer, 0, sizeof(mp3_codec_buffer)); 

	if ( sceAudiocodecCheckNeedMem(mp3_codec_buffer, 0x1002) < 0 ) 
		goto wait; 
	if ( sceAudiocodecGetEDRAM(mp3_codec_buffer, 0x1002) < 0 ) 
		goto wait; 
	my_mp3_decoder_getEDRAM = 1; 

	if ( sceAudiocodecInit(mp3_codec_buffer, 0x1002) < 0 ) { 
		goto wait; 
	}

	return 0;

wait:
	my_mp3_close(avctx);
	return -1;
}
static int my_mp3_decode_frame(AVCodecContext * avctx, void *outdata, int *outdata_size, const uint8_t * inbuf, int inbuf_size)
{
	//count3+=inbuf_size;

	if(	inbuf_size<4 )
	{
		return 0;
	}

	unsigned char mp3_header[4];
	int mp3HeaderSize=4;
	memcpy(mp3_header, inbuf, mp3HeaderSize);

	if( !(mp3_header[0]==0xff && mp3_header[1]&0xf0==0xf0 ) )
	{
		fprintf(stderr, "my_mp3_decode_frame header error\n");
		return -1;
	}

	int bitratetag = (mp3_header[2] & 0xf0) >> 4; 
	int padding = (mp3_header[2] & 0x2) >> 1; 
	int version = (mp3_header[1] & 0x18) >> 3;
	int samplerate = samplerates[version][ (mp3_header[2] & 0xC) >> 2 ];
	int channeltag = (mp3_header[3] & 0xC0)>>6;
	int nCh=(channeltag==3)?1:2;		// bit 11 is 1 channel(mono), else 2 channels (stereo, joint stereo, dual)
	int instantbitrate;//=bitrates[bitratetag]*1000;

	if ((bitratetag > 14) || (version == 1) || (samplerate == 0) || (bitratetag == 0) )
	{
		// invalid frame. stop.
		fprintf(stderr, "my_mp3_decode_frame bitratetag error\n");
		return -1;
	}

	int frame_size, sample_per_frame;
	if (version == 3) //mpeg-1
	{
		sample_per_frame = 1152;
		instantbitrate=bitrates[bitratetag]*1000;
		frame_size = 144*instantbitrate/samplerate + padding;
	}
	else
	{
		sample_per_frame = 576;
		instantbitrate=bitrates_v2[bitratetag]*1000;
		frame_size = 72*instantbitrate/samplerate + padding;
	}

	if(inbuf_size<frame_size)
	{
		return 0;
	}

	memcpy(audio_input_buffer, inbuf, frame_size);

	mp3_codec_buffer[6] = (unsigned long)audio_input_buffer; 
	mp3_codec_buffer[8] = (unsigned long)audio_out_buffer; 

	mp3_codec_buffer[7] = mp3_codec_buffer[10] = frame_size; 
	mp3_codec_buffer[9] = sample_per_frame * 4; 

	int res = sceAudiocodecDecode(mp3_codec_buffer, 0x1002); 
	if ( res < 0 )
	{ 
		memset(audio_out_buffer, 0, sample_per_frame * 4);
	}

	curplayer->bitRate=instantbitrate;

	*outdata_size=sample_per_frame * 4;

	return frame_size;
}

AVCodec my_mp3_decoder =
{
    "mp3",
    CODEC_TYPE_AUDIO,
    CODEC_ID_MP3,
    0,
    my_mp3_decode_init,
    NULL,
    my_mp3_close,
    my_mp3_decode_frame,
//    CODEC_CAP_PARSE_ONLY,
};

static int my_aac_decoder_getEDRAM=0;
static int my_aac_close(AVCodecContext * avctx)
{
	if ( my_aac_decoder_getEDRAM )
	{ 
		sceAudiocodecReleaseEDRAM(aac_codec_buffer); 
		my_aac_decoder_getEDRAM=0;
	} 

	return 0;
}
static int my_aac_decode_init(AVCodecContext * avctx)
{
	myInitAVModules();

	if(avctx->frame_size==0)
	{
		avctx->frame_size=1024;
	}

	//if(avctx->extradata && avctx->extradata_size)
	//{
	//	fprintf(stderr, "my_aac_decode_init codec extradata\n");
	//	unsigned char *rawData=(unsigned char*)avctx->extradata;
	//	int i;
	//	for(i=0; i<avctx->extradata_size; i++)
	//	{
	//		fprintf(stderr, "%02x ", rawData[i]);
	//		if(i%8==7)
	//			fprintf(stderr, "  ");
	//		if(i%16==15)
	//			fprintf(stderr, "\n");
	//	}
	//	if(i&0xf) fprintf(stderr, "\n");
	//}

	memset(aac_codec_buffer, 0, sizeof(aac_codec_buffer)); 

	if ( sceAudiocodecCheckNeedMem(aac_codec_buffer, 0x1003) < 0 )
	{
		goto wait; 
	}
	if ( sceAudiocodecGetEDRAM(aac_codec_buffer, 0x1003) < 0 ) 
	{
		goto wait; 
	}
	my_aac_decoder_getEDRAM = 1; 

	aac_codec_buffer[10] = avctx->sample_rate; 
	if(sceAudiocodecInit(aac_codec_buffer, 0x1003) < 0 )
	{
		goto wait; 
	}

	return 0;

wait:
	my_aac_close(avctx);
	return -1;
}
static int my_aac_decode_frame(AVCodecContext * avctx, void *outdata, int *outdata_size, const uint8_t *inbuf, int inbuf_size)
{
	int frame_size=inbuf_size;//avctx->frame_size;
	int sample_per_frame=1024;

	//count3+=inbuf_size;

	if( inbuf_size<frame_size )
	{
		return 0;
	}

	memcpy(audio_input_buffer, inbuf, frame_size);

	aac_codec_buffer[6] = (unsigned long)audio_input_buffer; 
	aac_codec_buffer[8] = (unsigned long)audio_out_buffer; 

	aac_codec_buffer[7] = frame_size; 
	aac_codec_buffer[9] = sample_per_frame * 4; 

	int res = sceAudiocodecDecode(aac_codec_buffer, 0x1003); 
	if ( res < 0 )
	{ 
		memset(audio_out_buffer, 0, sample_per_frame * 4);
	}

	int averagebitrate=(inbuf_size*avctx->sample_rate)>>7;
	curplayer->bitRate=averagebitrate;

	*outdata_size=sample_per_frame * 4;
	return frame_size;
}

#define AAC_CODEC(id, name)     \
AVCodec name ## _decoder = {    \
    #name,                      \
    CODEC_TYPE_AUDIO,           \
    id,                         \
    0,        \
    my_aac_decode_init,           \
    NULL,                       \
    my_aac_close,            \
    my_aac_decode_frame,          \
}

// FIXME - raw AAC files - maybe just one entry will be enough
AAC_CODEC(CODEC_ID_AAC, aac);
// If it's mp4 file - usually embeded into Qt Mov
AAC_CODEC(CODEC_ID_MPEG4AAC, mpeg4aac);

int IsSupportedPixelFormat(enum PixelFormat fmt)
{
	if(fmt==PIX_FMT_YUV420P)
		return 1;
	if(fmt==PIX_FMT_YUV422P)
		return 1;
	if(fmt==PIX_FMT_YUV444P)
		return 1;

	return 0;
}

// videocodec from cooleyes's ppa.

#include "pspmpeg.h"
#include "pspmpegbase.h"
#include "pspvideocodec.h"

extern "C" {
int sceVideocodecStop(unsigned long *Buffer, int Type);
int sceVideocodecDelete(unsigned long *Buffer, int Type);
};

int mpegInit=-1;
unsigned long my_mp4v_codec_buffer[96] __attribute__((aligned(64)));

unsigned char my_vidframe_input_buffer[256*1024] __attribute__((aligned(64)));

SceMpegYCrCbBuffer cscBuffer __attribute__((aligned(64)));
unsigned long dest_buffer[24] __attribute__((aligned(64)));
unsigned long _src_buffer[24] __attribute__((aligned(64)));

static int my_mp4v_decoder_getEDRAM=0;
static int my_mp4v_close(AVCodecContext * avctx)
{
	if( my_mp4v_decoder_getEDRAM )
	{
		sceVideocodecStop(my_mp4v_codec_buffer, 0x1);
		sceVideocodecDelete(my_mp4v_codec_buffer, 0x1);
		sceVideocodecReleaseEDRAM(my_mp4v_codec_buffer);
		my_mp4v_decoder_getEDRAM=0;
	}
	
	if (!mpegInit)
	{
		sceMpegFinish();
	}

	mpegInit=-1;
	
	fprintf(stderr, "my_mp4v_close success.\n");

	return 0;
}

static int my_mp4v_decode_init(AVCodecContext * avctx)
{
	if( avctx->pix_fmt == PIX_FMT_NONE )
	{
		if(mpeg4vmecsc || (320<avctx->width && avctx->width&0x1f))
		{
			avctx->pix_fmt = PIX_FMT_RGB32_1;
		}
		else
		{
			avctx->pix_fmt = PIX_FMT_YUV420P;
		}

		return 0;
	}

	myInitAVModules();

	mpegInit = sceMpegInit();
	if (mpegInit)
	{
		fprintf(stderr, "my_mp4v_decode_init fail: sceMpegInit()\n");
		goto wait;
	}

	sceMpegBaseCscInit(512);

	memset(my_mp4v_codec_buffer, 0, sizeof(my_mp4v_codec_buffer));
	//memset(&cscBuffer, 0, sizeof(cscBuffer));
	memset(&dest_buffer, 0, sizeof(dest_buffer));
	memset(&_src_buffer, 0, sizeof(_src_buffer));

	my_mp4v_codec_buffer[4] = (unsigned long)((char*)(my_mp4v_codec_buffer) + 128 );
	my_mp4v_codec_buffer[11] = 512;
	my_mp4v_codec_buffer[12] = 512;
	my_mp4v_codec_buffer[13] = 512*512;
	//my_mp4v_codec_buffer[11] = width64;
	//my_mp4v_codec_buffer[12] = height64;
	//my_mp4v_codec_buffer[13] = width64*height64;

	if (sceVideocodecOpen(my_mp4v_codec_buffer, 0x1) < 0 )
	{
		fprintf(stderr, "my_mp4v_decode_init fail: sceVideocodecOpen()\n");
		goto wait;
	}

	my_mp4v_codec_buffer[7] = 16384;

	if ( sceVideocodecGetEDRAM(my_mp4v_codec_buffer, 0x1) < 0 )
	{
		fprintf(stderr, "my_mp4v_decode_init fail: sceVideocodecGetEDRAM()\n");
		goto wait;
	}

	if ( sceVideocodecInit(my_mp4v_codec_buffer, 0x1) < 0 )
	{
		fprintf(stderr, "my_mp4v_decode_init fail: sceVideocodecInit()\n");
		goto wait;
	}

	my_mp4v_codec_buffer[34] = 7;
	my_mp4v_codec_buffer[36] = 0;

	if ( sceVideocodecStop(my_mp4v_codec_buffer, 0x1) < 0 ) {
		fprintf(stderr, "my_mp4v_decode_init fail: sceVideocodecInit()\n");
		goto wait;
	}

	my_mp4v_decoder_getEDRAM=1;

	if(avctx->extradata && avctx->extradata_size)
	{
		memcpy(my_vidframe_input_buffer, avctx->extradata, avctx->extradata_size);
	}

	fprintf(stderr, "my_mp4v_decode_init success.\n");

	return 0;

wait:
	my_mp4v_close(avctx);
	return -1;
}

static int my_mp4v_decode_frame(AVCodecContext * avctx, void *outdata, int *outdata_size, const uint8_t *inbuf, int inbuf_size)
{
	AVFrame *frame=(AVFrame *)outdata;

	if(inbuf==NULL || inbuf_size==0 )
		return inbuf_size;

	if(0<avctx->extradata_size)
	{
		if(256*1024<avctx->extradata_size+inbuf_size)
		{
			fprintf(stderr, "Too big sized frame. %d\n", inbuf_size);
			return inbuf_size;
		}

		memcpy(my_vidframe_input_buffer+avctx->extradata_size, inbuf, inbuf_size+FF_INPUT_BUFFER_PADDING_SIZE);

		my_mp4v_codec_buffer[9]  = (unsigned long)my_vidframe_input_buffer;
		my_mp4v_codec_buffer[10] = avctx->extradata_size+inbuf_size;
	}
	else
	{
		my_mp4v_codec_buffer[9]  = (unsigned long)inbuf;
		my_mp4v_codec_buffer[10] = inbuf_size;
	}

	sceKernelDcacheWritebackInvalidateAll();

	my_mp4v_codec_buffer[14] = 7;
	int ret=sceVideocodecDecode(my_mp4v_codec_buffer, 0x1);
	if ( ret< 0 )
	{
		fprintf(stderr, "my_mp4v_decode_frame fail: sceVideocodecDecode()\n");
		return inbuf_size;
	}

	if( avctx->pix_fmt==PIX_FMT_YUV420P && frame->linesize[0]<my_mp4v_codec_buffer[56] )
	{
		fprintf(stderr, "buffer width mismatch. adjusting to (%d/%d)->%d\n", iq.width, (int)frame->linesize[0], my_mp4v_codec_buffer[56]);

		iq.QDestroy();
		iq.QInit(iq.pix_fmt, my_mp4v_codec_buffer[56], iq.height);
	}

	// codec_buffer : 44 : width
	// codec_buffer : 45 : height
	// codec_buffer : 53 : y buffer pointer
	// codec_buffer : 54 : cr buffer pointer
	// codec_buffer : 55 : cb buffer pointer
	// codec_buffer : 56 : y buffer width
	// codec_buffer : 57 : cr buffer width
	// codec_buffer : 58 : cb buffer width

	unsigned long height;
	height = (my_mp4v_codec_buffer[45]+15) & 0xFFFFFFF0;

	if(avctx->pix_fmt==PIX_FMT_RGB32_1)
	{
		cscBuffer.iFrameBufferHeight16=(my_mp4v_codec_buffer[45]+15) >> 4;
		cscBuffer.iFrameBufferWidth16=(my_mp4v_codec_buffer[56]+15) >> 4;
		cscBuffer.iUnknown=0;
		cscBuffer.iUnknown2=1;
		cscBuffer.pYBuffer=(void*)my_mp4v_codec_buffer[53];
		cscBuffer.pYBuffer2=(char*)cscBuffer.pYBuffer + (my_mp4v_codec_buffer[56] * (height >> 1));
		cscBuffer.pCrBuffer=(void*)my_mp4v_codec_buffer[54];
		cscBuffer.pCbBuffer=(void*)my_mp4v_codec_buffer[55];
		cscBuffer.pCrBuffer2=(char*)cscBuffer.pCrBuffer + (my_mp4v_codec_buffer[57] * (height >> 2));
		cscBuffer.pCbBuffer2=(char*)cscBuffer.pCbBuffer + (my_mp4v_codec_buffer[58] * (height >> 2));
		cscBuffer.iFrameHeight=my_mp4v_codec_buffer[45];
		cscBuffer.iFrameWidth=my_mp4v_codec_buffer[44];
		cscBuffer.iFrameBufferWidth=512;
		
		unsigned char *dest1=frame->data[0];
		unsigned char *dest2=frame->data[0]+frame->linesize[0]*cscBuffer.iFrameBufferHeight16*8;

		if ( sceMpegBaseCscVme(dest1, dest2, 512, &cscBuffer) < 0 )
		{
			fprintf(stderr, "my_mp4v_decode_frame fail: sceMpegBaseCscVme()\n");
		}
	}
	else if( avctx->pix_fmt==PIX_FMT_YUV420P )
	{
		_src_buffer[0] = (my_mp4v_codec_buffer[45]+15) & 0xFFFFFFF0;
		_src_buffer[1] = (my_mp4v_codec_buffer[44]+15) & 0xFFFFFFF0;
		_src_buffer[2] = 0;
		_src_buffer[3] = 1;
		_src_buffer[4] = my_mp4v_codec_buffer[53];
		_src_buffer[5] = _src_buffer[4] + (my_mp4v_codec_buffer[56] * (_src_buffer[0] >> 1)); 
		_src_buffer[6] = my_mp4v_codec_buffer[54];
		_src_buffer[7] = my_mp4v_codec_buffer[55];
		_src_buffer[8] = _src_buffer[6] + (my_mp4v_codec_buffer[57] * (_src_buffer[0] >> 2));
		_src_buffer[9] = _src_buffer[7] + (my_mp4v_codec_buffer[58] * (_src_buffer[0] >> 2));
		_src_buffer[10] = 0;
		_src_buffer[11] = 0;

		//dest_buffer[0] = (my_mp4v_codec_buffer[45]+15) >> 4;
		//dest_buffer[1] = (my_mp4v_codec_buffer[56]+15) >> 4;
		////dest_buffer[2] = 0;
		////dest_buffer[3] = 1;
		//dest_buffer[4] = (unsigned long)ybuffer;
		//dest_buffer[5] = dest_buffer[4] + (my_mp4v_codec_buffer[56] * (height >> 1)); 
		//dest_buffer[6] = (unsigned long)crbuffer;
		//dest_buffer[7] = (unsigned long)cbbuffer;
		//dest_buffer[8] = dest_buffer[6] + (my_mp4v_codec_buffer[57] * (height >> 2));
		//dest_buffer[9] = dest_buffer[7] + (my_mp4v_codec_buffer[58] * (height >> 2));
		//dest_buffer[10] = my_mp4v_codec_buffer[45];
		//dest_buffer[11] = my_mp4v_codec_buffer[44];
		//dest_buffer[12] = 512;

		dest_buffer[0] = (my_mp4v_codec_buffer[45]+15) >> 4;
		dest_buffer[1] = frame->linesize[0]>>4;
		dest_buffer[2] = 0;
		dest_buffer[3] = 1;
		dest_buffer[4] = (unsigned long)frame->data[0];
		dest_buffer[5] = dest_buffer[4] + (frame->linesize[0] * (height >> 1)); 
		dest_buffer[6] = (unsigned long)frame->data[1];
		dest_buffer[7] = (unsigned long)frame->data[2];
		dest_buffer[8] = dest_buffer[6] + (frame->linesize[0]/2 * (height >> 2));
		dest_buffer[9] = dest_buffer[7] + (frame->linesize[0]/2 * (height >> 2));
		dest_buffer[10] = my_mp4v_codec_buffer[45];
		dest_buffer[11] = my_mp4v_codec_buffer[44];
		dest_buffer[12] = frame->linesize[0];

		if( sceMpegBaseYCrCbCopyVme(dest_buffer, (SceInt32*)&_src_buffer, 3) < 0 )
		{
			fprintf(stderr, "my_mp4v_decode_frame fail: sceMpegBaseYCrCbCopyVme()\n");
		}

		if( avctx->width&0x1f )
		{
			unsigned char *dm1, *dm2, *sm1, *sm2;
			int cheight=avctx->height/2;
			int cwidth=avctx->width/2;
			for(int i=0; i<cheight-1; i++)
			{
				sm1=(unsigned char*)frame->data[1]+frame->linesize[0]/2*(cheight-i-1);
				sm2=(unsigned char*)frame->data[2]+frame->linesize[0]/2*(cheight-i-1);
				dm1=(unsigned char*)frame->data[1]+frame->linesize[1]*(cheight-i-1);
				dm2=(unsigned char*)frame->data[2]+frame->linesize[2]*(cheight-i-1);

				if(dm1-sm1<cwidth)
				{
					unsigned char tmpbuf[256];
					memcpy(tmpbuf, sm1, cwidth);
					memcpy(dm1, tmpbuf, cwidth);

					memcpy(tmpbuf, sm2, cwidth);
					memcpy(dm2, tmpbuf, cwidth);
				}
				else
				{
					memcpy(dm1, sm1, cwidth);
					memcpy(dm2, sm2, cwidth);
				}
			}
		}
	}

	*outdata_size=1;

	return inbuf_size;
}

AVCodec my_mpeg4_decoder = {
    "mpeg4",
    CODEC_TYPE_VIDEO,
    CODEC_ID_MPEG4,
    0,
    my_mp4v_decode_init,
    NULL,
    my_mp4v_close,
    my_mp4v_decode_frame,
    //CODEC_CAP_DRAW_HORIZ_BAND | CODEC_CAP_DR1 | CODEC_CAP_TRUNCATED | CODEC_CAP_DELAY,
    //.flush= ff_mpeg_flush,
    //.long_name= NULL_IF_CONFIG_SMALL("MPEG-4 part 2"),
};

int avc_sps_size=0, avc_pps_size=0;
unsigned char avc_sps[1024]  __attribute__((aligned(64)));
unsigned char avc_pps[1024]  __attribute__((aligned(64)));

#pragma pack(1)

struct MySceMpegLLI
{
	void *src;
	void *dest;
	void *next;
	int size;
};

#pragma pack()

#define DMABLOCK 4095
#define MEAVCBUF 0x4a000

#define MAX_BLOCK_SIZE (512*1024)
#define NUM_LLI (MAX_BLOCK_SIZE/DMABLOCK+1)

#if _PSP_FW_VERSION <= 150

#define LIB_MEMSIZE 93184

#else

#define LIB_MEMSIZE 65536

#endif

int mpegCreateRet=-1;
SceMpegRingbuffer rb;
SceMpeg mpeg={0,};
ScePVoid esbuf=0;
SceMpegAu au={0,};

unsigned char mp4srcdata[MAX_BLOCK_SIZE] __attribute__((aligned(64)));
unsigned char libData[LIB_MEMSIZE] __attribute__((aligned(64)));
MySceMpegLLI lli[NUM_LLI] __attribute__((aligned(64)));;

int isMP4=0;
int isAVC=0;
int isMP4AVC=0;
int isFirstFrame=1;
static int my_h264_close(AVCodecContext * avctx)
{
#if _PSP_FW_VERSION > 150

	SceInt32 status=0;
	SceInt32 ret=sceMpegAvcDecodeStop(&mpeg, 512, workBuffer, &status);

#endif

	if(esbuf)
	{
		sceMpegFreeAvcEsBuf(&mpeg, esbuf);
	}

	if(!mpegCreateRet)
	{
		sceMpegDelete(&mpeg);
	}

	if(!mpegInit)
	{
		sceMpegFinish();
		fprintf(stderr, "my_avc_close success.\n");
	}

	esbuf=0;
	mpegCreateRet=-1;
	mpegInit=-1;

	return 0;
}

static int my_h264_decode_init(AVCodecContext * avctx)
{
	int memsize;

	if( avctx->pix_fmt == PIX_FMT_NONE )
	{
		avctx->pix_fmt = PIX_FMT_RGB32_1;

		return 0;
	}

	myInitAVModules();

	mpegInit = sceMpegInit();
	if (mpegInit != 0)
	{
		fprintf(stderr, "my_avc_decode_init fail. sceMpegInit().\n");
		goto wait;
	}

	memsize = sceMpegQueryMemSize(0);
	if ( LIB_MEMSIZE < memsize )
	{
		fprintf(stderr, "my_avc_decode_init fail. sceMpegQueryMemSize(). %d %d\n", memsize, LIB_MEMSIZE);
		goto wait;
	}

	mpegCreateRet = sceMpegCreate(&mpeg, libData, memsize, &rb, 512, 0, 0);
	if (mpegCreateRet != 0)
	{
		fprintf(stderr, "my_avc_decode_init fail. sceMpegCreate()\n");
		goto wait;
	}

	sceKernelDcacheWritebackInvalidateAll();

	esbuf = sceMpegMallocAvcEsBuf(&mpeg);
	if (esbuf == 0)
	{
		fprintf(stderr, "my_avc_decode_init fail. sceMpegMallocAvcEsBuf().\n");
		goto wait;
	}

	sceKernelDcacheWritebackInvalidateAll();

	if(sceMpegInitAu(&mpeg, esbuf, &au)<0)
	{
		fprintf(stderr, "my_avc_decode_init fail: sceMpegInitAu().\n");
		goto wait;
	}

#if _PSP_FW_VERSION > 150

	{
		SceInt32 status=0;
		SceInt32 ret=sceMpegAvcDecodeStop(&mpeg, 512, workBuffer, &status);
	}

#endif

	fprintf(stderr, "my_avc_decode_init success\n");

	//if(avctx->extradata && avctx->extradata_size)
	//{
	//	fprintf(stderr, "my_avc_decode_init codec extradata\n");
	//	unsigned char *rawData=(unsigned char*)avctx->extradata;
	//	int i;
	//	for(i=0; i<avctx->extradata_size; i++)
	//	{
	//		fprintf(stderr, "%02x ", rawData[i]);
	//		if(i%8==7)
	//			fprintf(stderr, "  ");
	//		if(i%16==15)
	//			fprintf(stderr, "\n");
	//	}
	//	if(i&0xf) fprintf(stderr, "\n");
	//}

	// from cooleyes' atom.c
	if(7<avctx->extradata_size && avctx->extradata)
	{
		unsigned char *data=(unsigned char *)avctx->extradata;

		data+=6;

		avc_sps_size=*data;
		avc_sps_size=avc_sps_size*256+*(data+1);
		data+=2;

		memcpy(avc_sps, data, avc_sps_size);
		data+=avc_sps_size;
		//fprintf(stderr, "sps %d %02x %02x\n", avc_sps_size, avc_sps[0], avc_sps[1]);

		data++;

		avc_pps_size=*data;
		avc_pps_size=avc_pps_size*256+*(data+1);
		data+=2;


		memcpy(avc_pps, data, avc_pps_size);
		data+=avc_pps_size;
		//fprintf(stderr, "pps %d %02x %02x\n", avc_pps_size, avc_pps[0], avc_pps[1]);

		//if(avctx->extradata_size<data-avctx->extradata)
		//{
		//	fprintf(stderr, "avcC error.\n");
		//	goto wait;
		//}
	}

	isFirstFrame=1;

	return 0;

wait:
	my_h264_close(avctx);
	return -1;
}

int numlli;
static void MyCopyAu2Me(const unsigned char *inbuf, int inbuf_size)
{
	unsigned char *destbuf=(unsigned char *) MEAVCBUF;

	numlli=((inbuf_size-1)/DMABLOCK)+1;

	if( NUM_LLI<numlli )
	{
		numlli=NUM_LLI;
	}

	for(int i=0; i<numlli; i++)
	{
		lli[i].src=(void*)(inbuf+i*DMABLOCK);
		lli[i].dest=(void*)(destbuf+i*DMABLOCK);
		lli[i].next=&lli[i+1];
		lli[i].size=DMABLOCK;
	}

	lli[numlli-1].next=NULL;
	lli[numlli-1].size=inbuf_size%DMABLOCK;
	if( lli[numlli-1].size==0 && inbuf_size )
	{
		lli[numlli-1].size=DMABLOCK;
	}

	sceKernelDcacheWritebackInvalidateAll();

	sceMpegbase_BEA18F91((SceMpegLLI*)lli);
}

unsigned int magic=0x00010000;
unsigned char *WriteCompatibleH264FrameData(unsigned char *destPtr, unsigned char *srcPtr, int srcSize)
{
	int offset=0;
	
	memcpy(destPtr, &magic, 3);
	memcpy(destPtr+3, srcPtr, srcSize);
	*(destPtr+3+srcSize)=0;

	return destPtr+srcSize+4;
}

static int my_h264_decode_frame(AVCodecContext * avctx, void *outdata, int *outdata_size, const uint8_t *inbuf, int inbuf_size)
{
	AVFrame *frame=(AVFrame*)outdata;

	if( isMP4AVC )
	{
		unsigned char *destPtr=my_vidframe_input_buffer;

		if(isFirstFrame==1)
		{
			destPtr=WriteCompatibleH264FrameData(destPtr, avc_sps, avc_sps_size);
			destPtr=WriteCompatibleH264FrameData(destPtr, avc_pps, avc_pps_size);
		}

		unsigned char *srcPtr=(unsigned char *)inbuf;
		int srcsize=inbuf_size;

		while(0<srcsize)
		{
			// read size.
			unsigned int size=((int)*(srcPtr+0)<<24)|((int)*(srcPtr+0)<<16)|((int)*(srcPtr+2)<<8)|((int)*(srcPtr+3));

#if _PSP_FW_VERSION <= 150
			if( !(srcPtr[4]==0x06 && srcPtr[5]==0x05 ) )
#endif
			{
				destPtr=WriteCompatibleH264FrameData(destPtr, srcPtr+4, size);
			}

			srcPtr+=size+4;
			srcsize-=size+4;
		}

		memset(destPtr, 0, FF_INPUT_BUFFER_PADDING_SIZE);

		int newsize=destPtr-my_vidframe_input_buffer;

		MyCopyAu2Me(my_vidframe_input_buffer, newsize+FF_INPUT_BUFFER_PADDING_SIZE);
		au.iAuSize = newsize;

		//fprintf(stderr, "newsize %d(%02x %02x) lli %d\n", newsize, newsize%256, newsize/256, numlli);
		//int i;
		//for(i=0; i<newsize+FF_INPUT_BUFFER_PADDING_SIZE; i++)
		//{
		//	fprintf(stderr, "%02x ", my_vidframe_input_buffer[i]);
		//	if(i%8==7)
		//		fprintf(stderr, "  ");
		//	if(i%16==15)
		//		fprintf(stderr, "\n");
		//}
		//if(i&0xf) fprintf(stderr, "\n");
	}
	else
	{
#if _PSP_FW_VERSION <= 150
		// remove comment data?
		if( inbuf[4]==0x06 && inbuf[5]==0x05 /*&& inbuf[6]==0xff && inbuf[7]==0xe2*/ )
		{
			int foundzero=0, i;
			for(i=8; i<inbuf_size; i++)
			{
				if(inbuf[i]==0)
				{
					i++;
					break;
				}
			}
			if(inbuf_size<=i)
			{
				*outdata_size=0;
				return inbuf_size;
			}
			MyCopyAu2Me(inbuf+i, inbuf_size-i+FF_INPUT_BUFFER_PADDING_SIZE);
			au.iAuSize = inbuf_size-i;
		}
		else
#endif
		{
			MyCopyAu2Me(inbuf, inbuf_size+FF_INPUT_BUFFER_PADDING_SIZE);
			au.iAuSize = inbuf_size;
		}
	}
	au.iPts=au.iDts=frame->pts;

	// 0x80628001 why?
	// 0x80628002 why?
	// 0x806101fe copy frame error?
	int ret=sceMpegAvcDecode(&mpeg, &au, 512, &frame->data[0], (SceInt32*)outdata_size);

	//if( ret==(SceInt32)0x80628002 )
	//{
	//	memset(frame->data[0], 0, frame->linesize[0]*avctx->height);
	//	*outdata_size=1;
	//}
	if(isAVC)
	{
		if(isFirstFrame==1 && ret==0)
		{
			fprintf(stderr, "avc first frame ok\n");
			isFirstFrame=0;
		}
	}
	if ( ret!=0 && ret!=(SceInt32)0x80628002 && ret!=(SceInt32)0x806101fe)
	{
		fprintf(stderr, "my_h264_decode_frame fail. sceMpegAvcDecode() ret %08x %d\n", ret, *outdata_size);
		//fprintf(stderr, "ret %08x   ", ret);

//		fprintf(stderr, "inbuf_size %d(%02x %02x)\n", inbuf_size, inbuf_size%256, inbuf_size/256);
//		int i;
//		for(i=0; i<512; i++)
////		for(i=0; i<inbuf_size+FF_INPUT_BUFFER_PADDING_SIZE; i++)
//		{
//			fprintf(stderr, "%02x ", inbuf[i]);
//			if(i%8==7)
//				fprintf(stderr, "  ");
//			if(i%16==15)
//				fprintf(stderr, "\n");
//		}
//		if(i&0xf) fprintf(stderr, "\n");
	}

	return inbuf_size;
}

AVCodec my_h264_decoder = {
    "h264",
    CODEC_TYPE_VIDEO,
    CODEC_ID_H264,
    0,
    my_h264_decode_init,
    NULL,
    my_h264_close,
    my_h264_decode_frame,
    //CODEC_CAP_DRAW_HORIZ_BAND | CODEC_CAP_DR1 | CODEC_CAP_TRUNCATED | CODEC_CAP_DELAY,
    //.flush= ff_mpeg_flush,
    //.long_name= NULL_IF_CONFIG_SMALL("MPEG-4 part 2"),
};

void AVFileClose()
{
	if(pFormatCtx)
	{
		av_close_input_file(pFormatCtx);
	}

	pFormatCtx=NULL;
}
void VideoCodecCleanup()
{
	if( vFrameMemPtr )
	{
		av_free(vFrameMemPtr);
		vFrameMemPtr=NULL;
	}

	if( vFrame )
	{
		av_free(vFrame);
		vFrame=NULL;
	}

	if( vstreamindex<0 )
		return;

	if(vCodecCtx)
	{
		vCodecCtx->extradata_size=0;
		avcodec_close(vCodecCtx);
	}

	vCodecCtx=NULL;
	vStream=NULL;
	vCodec=NULL;

	vstreamindex=-1;
}
void AudioCodecCleanup()
{
	if( astreamindex<0 )
		return;

	if(aCodecCtx)
	{
		avcodec_close(aCodecCtx);
	}

	aCodecCtx=NULL;
	aStream=NULL;
	aCodec=NULL;

	my_resample_close(&rstate);

	astreamindex=-1;
}

int InitVideoStream()
{
	// Find the first video stream
	int i, videoStream=-1;

	for(i=0; i<pFormatCtx->nb_streams; i++)
	{
		if(pFormatCtx->streams[i]->codec->codec_type==CODEC_TYPE_VIDEO)
		{
			videoStream=i;

			break;
		}
	}

	if(videoStream>=0)
	{
		vStream=pFormatCtx->streams[videoStream];
		vCodecCtx=pFormatCtx->streams[videoStream]->codec;

		vCodec=avcodec_find_decoder(vCodecCtx->codec_id);
	    vCodecCtx->debug_mv = debug_mv;
	    vCodecCtx->debug = debug;
		vCodecCtx->workaround_bugs = workaround_bugs;
		vCodecCtx->lowres = lowres;
		if(lowres) vCodecCtx->flags |= CODEC_FLAG_EMU_EDGE;
		vCodecCtx->idct_algo= idct_v;
		if(fast) vCodecCtx->flags2 |= CODEC_FLAG2_FAST;
		vCodecCtx->skip_frame= skip_frame;
		vCodecCtx->skip_idct= skip_idct;
		vCodecCtx->skip_loop_filter= skip_loop_filter;
		vCodecCtx->error_resilience= error_resilience_v;
		vCodecCtx->error_concealment= error_concealment;

		if(vCodec)
		{
			if(avcodec_open(vCodecCtx, vCodec)<0 || (vCodecCtx->width>480 || vCodecCtx->height>272))// || IsSupportedPixelFormat(vCodecCtx->pix_fmt)==0)
			{
				fprintf(stderr, "Fail vcodec open. wh(%d/%d)\n", vCodecCtx->width, vCodecCtx->height);
				goto error;
			}
		}
		else
		{
			fprintf(stderr, "vc not found\n");
			goto error;
		}

#if 0		// bus error prevention?
		vFrame=avcodec_alloc_frame();
		if( vFrame )
		{
			vFrameImgSize=avpicture_get_size(vCodecCtx->pix_fmt, vCodecCtx->width, vCodecCtx->height);

			vFrameMemPtr=av_malloc(vFrameImgSize+64);

			unsigned long ptr=(unsigned long)vFrameMemPtr;
			ptr = (ptr+63)&0xffffffc0;

			avpicture_fill((AVPicture *)vFrame, (unsigned char*)ptr, vCodecCtx->pix_fmt, vCodecCtx->width, vCodecCtx->height);
		}

		if( !vFrame || !vFrameMemPtr)
		{
			snprintf(errorString, 256, "frame alloc failed");
			goto error;
		}
#endif

		return videoStream;
	}

	fprintf(stderr, "video stream not found\n");
error:
	VideoCodecCleanup();

	return -1;
}

int InitAudioStream()
{
	int i, audioStream=-1;

	for(i=0; i<pFormatCtx->nb_streams; i++)
	{
		if(pFormatCtx->streams[i]->codec->codec_type==CODEC_TYPE_AUDIO)
		{
			if( pFormatCtx->streams[i]->codec->channels>2 && pFormatCtx->streams[i]->codec->codec_id==CODEC_ID_AAC )
			{
				fprintf(stderr, "cannot decode aac multichannel track. numch:%d\n", pFormatCtx->streams[i]->codec->channels);
				continue;
			}

			audioStream=i;

			break;
		}
	}

	if(audioStream>=0)
	{
		aStream=pFormatCtx->streams[audioStream];
		aCodecCtx=pFormatCtx->streams[audioStream]->codec;

		aCodecCtx->channels=2;

		aCodec=avcodec_find_decoder(aCodecCtx->codec_id);
	    aCodecCtx->debug_mv = debug_mv;
	    aCodecCtx->debug = debug;
		aCodecCtx->workaround_bugs = workaround_bugs;
		aCodecCtx->lowres = lowres;
		if(lowres) aCodecCtx->flags |= CODEC_FLAG_EMU_EDGE;
		aCodecCtx->idct_algo= idct_a;
		if(fast) aCodecCtx->flags2 |= CODEC_FLAG2_FAST;
		aCodecCtx->skip_frame= skip_frame;
		aCodecCtx->skip_idct= skip_idct;
		aCodecCtx->skip_loop_filter= skip_loop_filter;
		aCodecCtx->error_resilience= error_resilience_a;
		aCodecCtx->error_concealment= error_concealment;

		if(aCodec)
		{
			if(avcodec_open(aCodecCtx, aCodec)<0 || aCodecCtx->channels>2)
			{
				fprintf(stderr, "Fail acodec open. ch:%d\n", aCodecCtx?aCodecCtx->channels:0);
				goto error;
			}
		}
		else
		{
			fprintf(stderr, "ac not found\n");
			goto error;
		}

		return audioStream;
	}

	fprintf(stderr, "audio stream not found\n");
error:
	AudioCodecCleanup();

	return -1;
}

#define PQSIZE 1024
int pqsize=0;
AVPacket *pdeque[PQSIZE]={NULL,};
int destbuf=0;

AVPacket *_pkt=NULL;
int firstVideoUp=0;

void MovieSeek(double t, int dir)
{
	//int ret=-1;

	//int64_t fn=(int64_t)(t*vStream->time_base.den/vStream->time_base.num);
	//av_seek_frame(pFormatCtx, vstreamindex, fn, 0);

	//fprintf(stderr, "ms %.1f %d %d %d\n", (float)t, (int)fn, vStream->time_base.den, vStream->time_base.num);

	//int64_t fn=(int64_t)(t*aStream->time_base.den/aStream->time_base.num);
	//av_seek_frame(pFormatCtx, astreamindex, fn, 0);

	//fprintf(stderr, "ms %.1f %d %d %d\n", (float)t, (int)fn, aStream->time_base.den, aStream->time_base.num);

	//if(0<=vstreamindex)
	//{
	//	avcodec_flush_buffers(vCodecCtx);
	//}
	//if(0<=astreamindex)
	//{
	//	avcodec_flush_buffers(aCodecCtx);
	//}

	int flags=0;
	if(dir<0)
	{
		flags=AVSEEK_FLAG_BACKWARD;
	}

	av_seek_frame(pFormatCtx, -1, (int64_t)(t*AV_TIME_BASE), flags);
}
	int len1, bytesRemaining, sbSize, cpSize, cSample;
	unsigned char *rawData;

int ResyncAC3Header(unsigned char *rawData, int bytesRemaining)
{
	int cpos=0;
	unsigned char a, b;
	while(1)
	{
		if(bytesRemaining<=cpos)
		{
			return -1;
		}

		a=rawData[cpos++];

		if(a==0x0b)
		{
			if(bytesRemaining<=cpos)
			{
				return -1;
			}

			b=rawData[cpos++];

			if( b==0x77 )
			{
				return cpos-2;
			}
		}
	}

	return -1;
}

int dropOneFrame=0;
int DecodeVideo(AVPacket *_pkt, int enqueue);
void DecodeAudio(AVPacket *_pkt);
void CheckVideo(int waitsound);
int InputFunction(int doQueuing)
{
	if(!_pkt)
	{
		_pkt=(AVPacket *)av_malloc(sizeof(AVPacket));
	}

	if(packet_reader(pFormatCtx, _pkt)<0)
	{
		fprintf(stderr, "packet reader returned -1\n");
		return -1;
	}

	if(_pkt->stream_index==vstreamindex)
	{
	//int64_t pts=1000*_pkt->dts;
	//int64_t frametime=pts*(int64_t)vStream->time_base.num/(int64_t)vStream->time_base.den;
	//curplayer->playedSample=frametime*44100/1000;

		//DecodeVideo(_pkt);
		//av_free_packet(_pkt);

		pdeque[pqsize++]=_pkt;
		_pkt=NULL;

		if(pqsize==PQSIZE)
		{
			fprintf(stderr, "pqsize full\n");
			curplayer->abortFlag=1;
		}
	}
	else if(_pkt->stream_index==astreamindex)
	{
		DecodeAudio(_pkt);
		av_free_packet(_pkt);
	}
	else
	{
		av_free_packet(_pkt);
	}

	CheckVideo(1);

	if(5<iq.GetAvailSize())
	{
		sceKernelDelayThread(3000);
	}

	return 1;
}
//FILE *tp=NULL;
void DecodeAudio(AVPacket *_pkt)
{
	int len1, bytesRemaining, sbSize, cpSize, cSample;
	unsigned char *rawData;
	short *buffer;

	rawData=_pkt->data;
	bytesRemaining=_pkt->size;

	//if(tp)
	//	fwrite(rawData, bytesRemaining, 1, tp);
	//return;

	while(bytesRemaining>0)
	{
		sbSize=sizeof(audio_out_buffer)<<2;
		len1 = avcodec_decode_audio2(aCodecCtx, audio_out_buffer, &sbSize, rawData, bytesRemaining);
		rawData+=len1;
		bytesRemaining-=len1;

		if(len1<0)
		{
			fprintf(stderr, "avcodec_decode_audio2() returned -1 at %d\n", _pkt->dts);
			if( CODEC_ID_AC3!=aCodec->id )
			{
				curplayer->abortFlag=1;
			}
			return;
		}
		if(CODEC_ID_MP3==aCodec->id && len1==0)
			break;

		if(curplayer->seekStat==2 || curplayer->seekSample==-1)
		{
			int samplepos=(int)(44100*_pkt->dts*av_q2d(aStream->time_base));

			curplayer->copiedSample=curplayer->playedSample=samplepos;
			curplayer->seekSample=0;
			curplayer->seekStat=0;

			//fprintf(stderr, "sao2 %d %d %d %d\n", (int)_pkt->dts, (int)_pkt->pts, aStream->time_base.den, aStream->time_base.num);
			//fprintf(stderr, "set audio offset %d\n", curplayer->playedSample*10/441);
		}

		int sample_per_frame=sbSize/4;
		int rsRet=my_resample(&rstate, audio_resample_buffer, audio_out_buffer, sample_per_frame);
		int iSamples=rsRet<0?-rsRet:rsRet;

		while(GetFreeSize()<iSamples)
		{
			sceKernelDelayThread(1000);

			if(curplayer->abortFlag)
				break;
		}

		if(iSamples)
		{
			sceKernelWaitSema(semaid, 0, 0);

			sceKernelSignalSema(semaid, 1);
			PushSampleData(rsRet<0?audio_out_buffer:audio_resample_buffer, iSamples);

			sceKernelSignalSema(semaid, -1);

			curplayer->copiedSample+=iSamples;
		}
	}

	return;
}

inline void WaitFrameBuffer()
{
	while(iq.GetFreeSize()<1)
	{
		sceKernelDelayThread(1000);

		if(curplayer->abortFlag)
			break;
	}
}

typedef unsigned short WORD;
typedef unsigned long DWORD;
typedef long LONG;

#pragma pack(1)

typedef struct tagBITMAPFILEHEADER
{ 
WORD bfType; //"BM" 
DWORD bfSize; //이미지의 크기 
WORD bfReserved1; //사용하지 않음 
WORD bfReserved2; //사용하지 않음 
DWORD bfOffBits; //이미지 데이터가 있는 파일 포인터 
} BITMAPFILEHEADER; 
typedef struct tagBITMAPINFOHEADER{ 
DWORD biSize; //현 구조체의 크기 
LONG biWidth; //가로 크기 
LONG biHeight; //세로 크기 
WORD biPlanes; //플레인수 
WORD biBitCount; //비트카운트 
DWORD biCompression; //압축 유무 
DWORD biSizeImage; //이미지 사이즈 
LONG biXPelsPerMeter; //미터당 가로 픽셀 
LONG biYPelsPerMeter; //미터당 세로 픽셀 
DWORD biClrUsed; //컬러 사용 유무 
DWORD biClrImportant; //중요하게 사용하는 색 
} BITMAPINFOHEADER; 
#pragma pack()

#define DIB_HEADER_MARKER ((WORD)('M'<<8) | 'B') 

void WriteBitMapFile(const char *fileName, int width, int height, int linesize, void *ptr)
{
	BITMAPFILEHEADER fileHeader;
	BITMAPINFOHEADER bi;

	memset(&fileHeader, 0, sizeof(fileHeader));
	fileHeader.bfType=DIB_HEADER_MARKER;
	fileHeader.bfSize=sizeof(fileHeader)+sizeof(bi)+width*height*3;
	fileHeader.bfOffBits=sizeof(fileHeader)+sizeof(bi);

	memset(&bi, 0, sizeof(bi));
	bi.biSize=sizeof(bi);

	bi.biWidth=width;
	bi.biHeight=height;
	bi.biPlanes=1;
	bi.biBitCount=24;
	bi.biSizeImage=width*height*3;

	FILE *fp=fopen(fileName, "wb");

	if(fp)
	{
		fwrite(&fileHeader, 1, sizeof(fileHeader), fp);
		fwrite(&bi, 1, sizeof(bi), fp);

		unsigned long *t=(unsigned long *)ptr;
		unsigned char *c;
		for(int y=0; y<height; y++)
		{
			for(int x=0; x<width; x++)
			{
				c=(unsigned char*)(t+(linesize*(height-y-1)+x));
				fwrite(c+2, 1, 1, fp);
				fwrite(c+1, 1, 1, fp);
				fwrite(c, 1, 1, fp);
			}
		}

		fclose(fp);
	}
}

int failCount=0;
int logcount=0;
int enableRetry=1;
int dtsoffset=0;	// for b frame?
int DecodeVideo(AVPacket *_pkt, int enqueue)
{
	int len1, bytesRemaining, sbSize, cpSize, cSample;
	unsigned char *rawData;
	short *buffer;
	int frameFinished=0;

	ImageQueue::FrameHolder *destFrame=NULL;

	rawData=_pkt->data;
	bytesRemaining=_pkt->size;

	//if(firstVideoUp==0)
	//{
	//	int64_t pts=1000*_pkt->dts;
	//	pts=pts*(int64_t)vStream->time_base.num/(int64_t)vStream->time_base.den;
	//	fprintf(stderr, "video start %d\n", (int)pts);
	//}

	//fprintf(stderr, "dts %d(%d) %05d   ", (int)_pkt->dts, (int)(_pkt->dts+dtsoffset), bytesRemaining);
	//if(_pkt->dts==2347)
	//{
	//	int i;
	//	for(i=0; i<bytesRemaining; i++)
	//	{
	//		if(rawData[i]==0 && rawData[i+1]==0 && rawData[i+2]==0 && rawData[i+3]==1)
	//		{
	//			fprintf(stderr, "found0001 at %d\n", i);
	//		}
	//	}
	//	for(i=0; i<2048; i++)
	//	{
	//		fprintf(stderr, "%02x ", rawData[i]);
	//		if(i%8==7)
	//			fprintf(stderr, "  ");
	//		if(i%16==15)
	//			fprintf(stderr, "\n");
	//	}
	//	if(i&0xf) fprintf(stderr, "\n");
	//}

	if(vFrame)
	{
		len1=avcodec_decode_video(vCodecCtx, vFrame, &frameFinished, rawData, bytesRemaining);
	}
	else
	{
		WaitFrameBuffer();

		_pkt->pts=_pkt->dts=_pkt->dts+dtsoffset;
		
		destFrame = iq.GetFreeSample();
		destFrame->dts=_pkt->dts;
		int64_t pts=1000*_pkt->dts;
		destFrame->frametime=pts*(int64_t)vStream->time_base.num/(int64_t)vStream->time_base.den;

		AVFrame *frame=destFrame->pict;
		frame->pts=_pkt->dts;

		len1=avcodec_decode_video(vCodecCtx, frame, &frameFinished, rawData, bytesRemaining);

		if(frameFinished)
		{
			failCount=0;
			//fprintf(stderr, "dts %d succ\n", _pkt->dts);
		}
		else
		{
			failCount++;
			//fprintf(stderr, "dts %d fail\n", _pkt->dts);
		}

		//if(isAVC && isFirstFrame==1)
		//{
		//	for(int i=0; i<10; i++)
		//	{
		//		fprintf(stderr, "retry %d %08x\n", i, frame->data[0]);
		//		my_h264_close(vCodecCtx);
		//		my_h264_decode_init(vCodecCtx);
		//		len1=avcodec_decode_video(vCodecCtx, frame, &frameFinished, rawData, bytesRemaining);
		//		if(isFirstFrame==0)
		//		{
		//			break;
		//		}
		//	}
		//	isFirstFrame=0;
		//}

		//static int count=0;
		//if(frameFinished && 2300<_pkt->dts && _pkt->dts<2500)
		//{
		//	char fname[256];
		//	sprintf(fname, "host0:/out/out%04d-%d.bmp", (int)count++, (int)_pkt->dts);
		//	fprintf(stderr, "%s\n", fname);
		//	WriteBitMapFile(fname, 480, 272, 512, ((AVFrame*)destFrame->pict)->data[0]);
		//}
	}

	//rawData+=len1;
	//bytesRemaining-=len1;

	if(len1<0)
	{
		fprintf(stderr, "avcodec_decode_video() returned -1\n");
		curplayer->abortFlag=1;

		return 0;
	}

	if(!frameFinished)
	{
		dtsoffset--;
		if(dtsoffset<0)
			dtsoffset=0;

		return 0;
	}

	if(vFrame && destFrame)
	{
		WaitFrameBuffer();

		// copy frame. no copying, bus error?
		memcpy( destFrame->pict->data[0], vFrame->data[0], vFrameImgSize);
	}

	// due to mov(mp4) seek bug?
	if(dropOneFrame)
	{
		dropOneFrame=0;
		return 0;
	}

	if(enqueue)
	{
		iq.WriteAdvance(1);
	}

	if(!enableRetry)
	{
		return 1;
	}

	enableRetry=0;
	int ret=1;

	while(1<frameFinished--)
	{
		if(audioPlayMode!=APM_NORMAL && iq.GetFreeSize()<1 )
			break;

		//fprintf(stderr, "decode one more time. %d %d\n", _pkt->dts, dtsoffset);

		dtsoffset++;

		////// decode one more time.
		ret+=DecodeVideo(_pkt, 1);
	}

	enableRetry=1;
	return ret;
}
void CheckVideo(int waitsound)
{
	if(pqsize<= 0)
	{
		return;
	}
	if( iq.GetFreeSize()<1 )
	{
		return;
	}

	AVPacket *cur=pdeque[0];

	//int64_t pts=1000*cur->dts;
	//int64_t frametime=pts*(int64_t)vStream->time_base.num/(int64_t)vStream->time_base.den;

	//int64_t spos=(int64_t)(curplayer->playedSample)*1000/44100; if(spos<0) spos=0;
	//spos += 44100/4;

	//if( waitsound && spos<frametime )
	//{
	//	return;
	//}

	DecodeVideo(cur, 1);

	pqsize--;

	int i;
	for(i=0; i<pqsize; i++)
	{
		pdeque[i]=pdeque[i+1];
	}
	pdeque[i]=0;

	av_free_packet(cur);
	av_free(cur);
}

void PQClear()
{
	while(pqsize)
	{
		pqsize--;
		av_free_packet(pdeque[pqsize]);
		av_free(pdeque[pqsize]);
		pdeque[pqsize]=NULL;
	}
}

void CheckPointOut(char *des)
{
	fprintf(stderr, "%s\n", des);
	strcpy(errorString, des);
}

void DecodeH264FirstFrame()
{
	if(!_pkt)
	{
		_pkt=(AVPacket *)av_malloc(sizeof(AVPacket));
	}

	int count=100, fc=0, retry=0;
	int videoRendered=0, rf;
	while(0<count-- && retry<10)
	{
		if(packet_reader(pFormatCtx, _pkt)<0)
		{
			return;
		}

		if(_pkt->stream_index==vstreamindex)
		{
			rf=DecodeVideo(_pkt, 0);
			av_free_packet(_pkt);
			if(rf==0)
			{
				fc++;
			}
			else
			{
				fc-=(rf-1);
			}

//			fprintf(stderr, "rf %d count %d vr %d fc %d\n", rf, count, videoRendered, fc);

			if(20<fc)
			{
				retry++;

				fprintf(stderr, "first frame retry %d\n", retry);

				my_h264_close(vCodecCtx);
				my_h264_decode_init(vCodecCtx);

				isFirstFrame=1;

				fc=0;
				videoRendered=0;
				count=100;
				MovieSeek(0, -1);
				continue;
			}

			videoRendered+=rf;

			if(isFirstFrame==0 && 10<videoRendered && rf==1)
			{
				fprintf(stderr, "codec verified.\n");
				return;
			}
		}
		else
		{
			av_free_packet(_pkt);
		}
	}
	fprintf(stderr, "codec fail countover. %d %d\n", count, retry);
}

int isMovieOpening=0;
int nermsg=0;
char strbuf[2048][40];
int PlayMovieFile(std::string2 &fileName, int _isMP4)
{
	int err;
	int inputended=0;
	int seekingPacketCount=0;

	showBG=0;
	LoadBackground();

	isMovieOpening=1;
	isSeekable=1;
	dropOneFrame=0;
	dtsoffset=0;
	count1=0;count2=1;

	musicClockBoost=aviClockBoost;
	BoostCPU(60.0f);

	sceKernelChangeThreadPriority(decoderThread, mainThreadPriority+4);

	CheckPointOut("mvopen 1");

	//my_mp3_reserve_buffer_size=0;

	vstreamindex=astreamindex=-1;
	isMP4=_isMP4;

	if(curplayer->InitFileData(fileName, 0)==0)
	{
		curplayer->isPlaying=0;

		sceKernelDelayThread(1000*1000);

		return 0;
	}

	void InitIndexVars();
	InitIndexVars();

	CheckPointOut("mvopen 2");

	// Open video file
	if((err=av_open_input_file(&pFormatCtx, fileName.c_str(), NULL, 0, NULL))!=0)
	{
		snprintf(errorString, 256, "Open error. %s", fileName.c_str());

		pFormatCtx=NULL;
		goto avclose;
	}

	fprintf(stderr, "file size %d\n", curplayer->fileLength);

	// subtitle open.
	{
		char subName[STRING_LENGTH];
		strcpy(subName, fileName.c_str());

		char *dot=strrchr(subName, '.');
		if(dot)
		{
			dot++;

			int numExt=1;
			char *ext[]={"smi"};

			for(int i=0; i<numExt; i++)
			{
				strcpy(dot, ext[i]);

				//fprintf(stderr, "%s %s\n", subName, dot);

				// try open sub.
				int OpenSubFile(char *fileName);
				int ret=OpenSubFile(subName);
				if(ret)
				{
					curplayer->haveSubtitle=1;
					break;
				}
			}
		}
	}

	//if(genpts)
	//{
	//	pFormatCtx->flags |= AVFMT_FLAG_GENPTS;
	//}

	CheckPointOut("mvopen 3");

	packet_reader=av_read_frame;

	if(av_find_stream_info(pFormatCtx)<0)
	{
		strcpy(errorString, "Stream info error.");

		goto avclose;
	}

	CheckPointOut("mvopen 4");

	vstreamindex=InitVideoStream();

	CheckPointOut("mvopen 5");

	astreamindex=InitAudioStream();

	CheckPointOut("mvopen 6");

	if( vstreamindex==-1 && astreamindex==-1 )
	{
		CheckPointOut("No decodable stream");
		goto avclose;
	}

	if( vstreamindex==-1 && 0<=astreamindex)
	{
		if( aCodecCtx->codec_id != CODEC_ID_AC3 )
		{
			musicClockBoost=meClockBoost+10;
		}
		else
		{
			musicClockBoost=meClockBoost+30;
		}
	}

	isAVC=0;
	isMP4AVC=0;

	if(vstreamindex!=-1)
	{
		curplayer->haveVideo=1;

		void AdjustVideoRenderingFactors();
		AdjustVideoRenderingFactors();

		iq.QInit(vCodecCtx->pix_fmt, vCodecCtx->width, vCodecCtx->height);

		if( isMP4 )
		{
			// this is mp4.
			if(CODEC_ID_H264==vCodec->id)
			{
				isAVC=1;
				isMP4AVC=1;
			}
		}
		else
		{
			// this is avi.
			if(CODEC_ID_H264==vCodec->id)
			{
				isAVC=1;
			}
			musicClockBoost+=10;
		}
		if(astreamindex!=-1 && CODEC_ID_AC3==aCodec->id)
		{
			musicClockBoost+=50;
		}
	}

	BoostCPU(2.5f);

	//if(tp==NULL)
	//	tp=fopen("ms0:/hoho.aac", "wb");

	memset(audio_out_buffer, 0, sizeof(audio_out_buffer));

	audioPlayMode=APM_INITIALBUFFERING;

	curplayer->isPlaying=1;
	curplayer->playedSample=curplayer->copiedSample=0;

	curplayer->estimatedSeconds=pFormatCtx->duration / AV_TIME_BASE;
	if( 0<=astreamindex )
	{
		curplayer->numChannel=aCodecCtx->channels;
		curplayer->bitRate=aCodecCtx->bit_rate;
		curplayer->samplingRate=aCodecCtx->sample_rate;

		if(rstate.inited==0)
		{
			my_resample_init(&rstate, 44100, curplayer->samplingRate, 2);
		}
	}
	else
	{
		isSeekable=0;
	}

	frameModified=1;

	CheckPointOut("mvopen 7");

	if(isAVC)
	{
		fprintf(stderr, "Decode h264 first frames...\n");
		DecodeH264FirstFrame();
	}

	CheckPointOut("mvopen 8");

	// seek.
	{
		int playpos=bookmark.GetMark(curplayer->fileLength);

		if(0<playpos||isAVC)
		{
			MovieSeek(playpos, -1);
		}
	}

	if(vstreamindex<0)
	{
		sceKernelChangeThreadPriority(decoderThread, decoderThreadPriority);
	}

	CheckPointOut("open succ");

	firstVideoUp=0;
	isMovieOpening=0;
	count2=0;

	//u64 t1, t2;
	//sceRtcGetCurrentTick(&t1);
	while(curplayer->abortFlag==0)
	{
		if( 0<=vstreamindex )
		{
			if( curplayer->seekStat==1 )
			{
				double seektime=curplayer->seekSample/44100.0;

				//fprintf(stderr, "movie seek to %.1f\n", (float)seektime);
				iq.QFlush();
				PQClear();
				ClearBuffers();

				logcount=5;

				if(_pkt)
				{
					av_free_packet(_pkt);
				}

				extern int64_t waitingFrameTime;
				waitingFrameTime=(int64_t)seektime;
				MovieSeek(seektime, seekDir);

				if(isMP4)
				{
					dropOneFrame=1;
				}

				dtsoffset=0;
				curplayer->seekStat=2;

				if(audioPlayMode != APM_PAUSED || curplayer->haveVideo==0)
				{
					audioPlayMode=APM_INITIALBUFFERING;
				}

				firstVideoUp=0;

				rstate.ptime=0;
				rstate.last.l=rstate.last.r=0;
			}
		}
		else
		{
			if( curplayer->seekStat==1 )
			{
				sceKernelDelayThread(1000);
				continue;
			}
			else if( curplayer->seekStat==2 )
			{
				double seektime=curplayer->seekSample/44100.0;
				ClearBuffers();

				MovieSeek(seektime, -1);

				curplayer->seekSample=-1;
				curplayer->seekStat=0;

				audioPlayMode=APM_INITIALBUFFERING;

				rstate.ptime=0;
				rstate.last.l=rstate.last.r=0;

				curplayer->seekStat=0;
			}
		}

		if(audioPlayMode==APM_INITIALBUFFERING ||
			(0<=astreamindex && GetAvailSize()<ONE_SECOND_SAMPLE_COUNT) ||
			(0<=vstreamindex && iq.GetAvailSize()<3) )
		{
			BoostCPU(5.0f);
		}
		if(audioPlayMode==APM_INITIALBUFFERING)
		{
			if(0<=vstreamindex && firstVideoUp==2 && isVideoSeeking==0)
			{
				if(GetAvailSize()>ONE_SECOND_SAMPLE_COUNT/4)
				{
					audioPlayMode=APM_NORMAL;
				}
			}
			if(vstreamindex<0)
			{
				if(GetAvailSize()>ONE_SECOND_SAMPLE_COUNT)
				{
					audioPlayMode=APM_NORMAL;
				}
			}
		}

		if(isVideoSeeking)
		{
			if(GetAvailSize()>ONE_SECOND_SAMPLE_COUNT*2)
			{
				//fprintf(stderr, "continue!!!! %d %d %d %d\n", (int)audioPlayMode, (int)GetAvailSize(), firstVideoUp, isVideoSeeking);
				continue;
			}
		}

		//fprintf(stderr, "haha %d %d\n", isVideoSeeking, firstVideoUp);
		//if(isVideoSeeking)
		//{
		//	if(firstVideoUp==2)
		//	{
		//		continue;
		//	}
		//}
		//else
		//{
		//	seekingPacketCount=0;
		//}

//		BoostCPU(5.0f);
		if( !inputended && InputFunction(0) < 0 )
		{
			inputended=1;
			isSeekable=0;	// for stability.
		}
		while( inputended && pqsize && !curplayer->abortFlag )
		{
			CheckVideo(0);
			sceKernelDelayThread(1000);
		}

		if( inputended && pqsize==0)
			break;

		idleSec=0;
	}

	//sceRtcGetCurrentTick(&t2);
	//{
	//int ccc=scePowerGetCpuClockFrequency();
	//int ddd=scePowerGetBusClockFrequency();
	//fprintf(stderr, "decode time %d at %d,%d\n", (int)((t2-t1)/1e6), ccc, ddd);
	//}

	if(audioPlayMode==APM_INITIALBUFFERING)
	{
		ClearBuffers();
		audioPlayMode==APM_NORMAL;
	}
	curplayer->isPlaying=2;

	my_resample_close(&rstate);

	curplayer->CloseFileData();

	if(curplayer->abortFlag==0)
	{
		while(GetAvailSize()>0 && curplayer->abortFlag==0)
		{
			sceKernelDelayThread(25000);
		}

		audioPlayMode=APM_PAUSED;
		curplayer->isPlaying=0;
		
		if(vstreamindex!=-1)
		{
			bookmark.AddMark(curplayer->fileLength, 0);
		}
	}
	else
	{
		if(vstreamindex!=-1)
		{
			bookmark.AddMark(curplayer->fileLength, curplayer->playedSample/44100);
		}

		audioPlayMode=APM_PAUSED;
		curplayer->isPlaying=0;
		ClearBuffers();

		curplayer->playedSample=curplayer->copiedSample=0;
	}

	// restore thread priority.
	sceKernelChangeThreadPriority(decoderThread, decoderThreadPriority);

	curplayer->haveVideo=0;

	BoostCPU(5);

	//extern int64_t waitingFrameTime;
	//waitingFrameTime=0;

	//for(int i=nermsg; i<nermsg+2000; i++)
	//{
	//	if( strbuf[i-nermsg][0] )
	//		fprintf(stderr, "%s\n", strbuf[i-nermsg]);
	//	strbuf[i-nermsg][0]=0;
	//}
	//nermsg=0;

	sceKernelDelayThread(300*1000);

avclose:

	void CloseSubFile();
	CloseSubFile();

	isMovieOpening=0;

	if(_pkt)
	{
		av_free(_pkt);
		_pkt=NULL;
	}

	PQClear();

	iq.QDestroy();

	//if(tp)
	//{	fclose(tp); tp=NULL; }

	VideoCodecCleanup();
	AudioCodecCleanup();
	AVFileClose();

	return 1;
}

int64_t waitingFrameTime=0;
void WaitForFrameSwap()
{
	//if(vCodecCtx->pix_fmt!=PIX_FMT_YUV420P)
	//{
	//	return;
	//}
	if(isVideoSeeking)
	{
		sceKernelDelayThread(5000);
		return;
	}
	if(astreamindex<0 || audioPlayMode==APM_PAUSED || curplayer->abortFlag || curplayer->isPlaying==0 || curplayer->seekStat || firstVideoUp<2 )
	{
		return;
	}

	//static int64_t prevwtime=0;
	//int64_t difft=waitingFrameTime-prevwtime;
	//fprintf(stderr, "waiting frame... %d %d\n", (int)waitingFrameTime, (int)difft);
	//prevwtime=waitingFrameTime;

	int ccc=1000;
	while (curplayer->abortFlag==0 && curplayer->isPlaying )
	{
		ccc--;
		u64 etime;
		sceRtcGetCurrentTick(&etime);

		int64_t tdiff=(etime-bftick)/1000;

		if( audioPlayMode!=APM_NORMAL )
		{
			tdiff=0;
		}

		//int64_t spos=(int64_t)bufferStartSampleIndex*1000/44100+tdiff;
		int64_t spos=(int64_t)(curplayer->playedSample-2048)*1000/44100+tdiff; if(spos<0) spos=0;

		if(waitingFrameTime<spos)
		{
			break;
		}

		sceKernelDelayThread(1000);
		if(ccc==0)
		{
			fprintf(stderr, "wait!!! %d %d %d %d\n", (int)waitingFrameTime, (int)spos, curplayer->playedSample, (int)tdiff);
		}
		if(ccc<=0)
		{
			SceCtrlData pad;
			sceCtrlReadBufferPositive(&pad, 1);
			if(pad.Buttons & PSP_CTRL_CROSS)
			{
				curplayer->abortFlag=1;
			}
			if(pad.Buttons & PSP_CTRL_CIRCLE)
			{
				fprintf(stderr, "apm %d fvu %d ivs %d av %d\n", audioPlayMode, firstVideoUp, isVideoSeeking, GetAvailSize());
			}
		}
	}
}

#define RGB_MUL_CONVERT(r, g, b, mul) (0xff000000|(b&0xff)<<16|(g&0xff)<<8|(r&0xff))

static unsigned long __attribute__((aligned(16))) texWhite[64];
static unsigned long __attribute__((aligned(16))) palWhite[256];

//unsigned char cmb[2048+256], *cm=&cmb[1024];

static unsigned long __attribute__((aligned(16))) pal_Y_1[256];
static unsigned long __attribute__((aligned(16))) pal_Y_2[256];
static unsigned long __attribute__((aligned(16))) pal_R_Cr_1[256];
static unsigned long __attribute__((aligned(16))) pal_R_Cr_2[256];
static unsigned long __attribute__((aligned(16))) pal_B_Cb_1[256];
static unsigned long __attribute__((aligned(16))) pal_B_Cb_2[256];
static unsigned long __attribute__((aligned(16))) pal_G_Cb_1[256];
static unsigned long __attribute__((aligned(16))) pal_G_Cb_2[256];
static unsigned long __attribute__((aligned(16))) pal_G_Cr_1[256];
static unsigned long __attribute__((aligned(16))) pal_G_Cr_2[256];
unsigned long g_fill_1;
unsigned long g_fill_2;//=(0.34414*255.0/224.0+0.71414*255.0/224.0)*128;
unsigned long rgb_sub_1;
unsigned long rgb_sub_2;
#define SCALER (24.0f)
void InitPallete3()
{
	unsigned int i, c, c1, c2;
	float f;
	unsigned int f1, f2;
	for(i=0; i<256; i++)
	{
		f=(i>16?i-16:0)*(255.0/219.0);
		f1=((int)f)/2;
		f2=(int)((f-f1*2)*SCALER);
		pal_Y_1[i]=RGB_MUL_CONVERT(f1, f1, f1, 1);
		pal_Y_2[i]=RGB_MUL_CONVERT((int)(f2+(i>=135?SCALER:0)), (int)(f2+(i>=178?SCALER:0)), (int)(f2+(i>=92?SCALER:0)), 1);

		f=i*1.40200*255.0/224.0;
		f1=((int)f)/2;
		f2=(int)((f-f1*2)*SCALER);
		pal_R_Cr_1[i]=RGB_MUL_CONVERT(f1, 0, 0, 1);
		pal_R_Cr_2[i]=RGB_MUL_CONVERT(f2, 0, 0, 1);

		f=i*1.77200*255.0/224.0;
		f1=((int)f)/2; f1=(f1<2)?0:f1-2;
		f2=(int)((f-f1*2)*SCALER);
		pal_B_Cb_1[i]=RGB_MUL_CONVERT(0, 0, f1, 1);
		pal_B_Cb_2[i]=RGB_MUL_CONVERT(0, 0, f2, 1);

		f=i*0.34414*255.0/224.0;
		f1=((int)f)/2;
		f2=(int)((f-f1*2)*SCALER);
		pal_G_Cb_1[i]=RGB_MUL_CONVERT(0, f1, 0, 1);
		pal_G_Cb_2[i]=RGB_MUL_CONVERT(0, f2, 0, 1);

		f=i*0.71414*255.0/224.0;
		f1=((int)f)/2;
		f2=(int)((f-f1*2)*SCALER);
		pal_G_Cr_1[i]=RGB_MUL_CONVERT(0, f1, 0, 1);
		pal_G_Cr_2[i]=RGB_MUL_CONVERT(0, f2, 0, 1);

		palWhite[i]=0xffffffff;
	}
	for(i=0; i<64; i++)
	{
		texWhite[i]=0;
	}

	unsigned long rsub_1, gsub_1, bsub_1;
	unsigned long rsub_2, gsub_2, bsub_2;

	f=(0.34414*255.0/224.0+0.71414*255.0/224.0)*128.0f;
	f1=(int)f;
	f2=(int)((f-f1)*SCALER);
	gsub_1=RGB_MUL_CONVERT(0, (f1+30), 0, 1);
	g_fill_1=RGB_MUL_CONVERT(0, (f1+15), 0, 1);
	g_fill_2=RGB_MUL_CONVERT(0, f2, 0, 1);
	g_fill_2=RGB_MUL_CONVERT(12, (f2+96+12), 12, 1);


	f=(1.40200*255.0/224.0)*128.0f;
	f1=(int)f;
	f2=(int)((f-f1)*SCALER);
	rsub_1=RGB_MUL_CONVERT(f1, 0, 0, 1);
	rsub_2=RGB_MUL_CONVERT(f2, 0, 0, 1);

	f=(1.77200*255.0/224.0)*128.0f;
	f1=((int)f)/2; f1=(f1<2)?0:f1-2; f1*=2;
	f2=(int)((f-f1)*SCALER);
	bsub_1=RGB_MUL_CONVERT(0, 0, f1, 1);
	bsub_2=RGB_MUL_CONVERT(0, 0, f2, 1);

	gsub_2=RGB_MUL_CONVERT(0, 96, 0, 1);

	rgb_sub_1=rsub_1|gsub_1|bsub_1;
	rgb_sub_2=rsub_2|gsub_2|bsub_2;

	//for(int i=0; i<2048+256; i++)
	//{
	//	if(i<1024)
	//		cmb[i]=0;
	//	else if(i<1024+256)
 //           cmb[i]=i-1024;
	//	else
	//		cmb[i]=255;
	//}

}
float factorX=1, factorY=1, chromaDeblock=1;
int tfx, tfy, xm, ym, sampleMode, scaleMode=1;
void AdjustVideoRenderingFactors()
{
	if(vstreamindex<0)
		return;

	factorX=480.0f/vCodecCtx->width;
	factorY=272.0f/vCodecCtx->height;

	switch(scaleMode)
	{
	case 0:
	default:
		// no scale.
		factorX=factorY=1.0f;
		break;
	case 1:
		// fit. keep aspect.
		if(factorX<factorY)
		{
			factorY=factorX;
		}
		else
		{
			factorX=(int)(100.0f*factorY)/100.0f;
		}
		//if(fabs(1.0f-factorX)<0.01f)
		//{
		//	factorX=480.0f/vCodecCtx->width;
		//	if(vCodecCtx->width==364)
		//	{
		//		factorX=480.0f/368;
		//	}
		//	factorY=272.0f/vCodecCtx->height;
		//	scaleMode=2;
		//}
		break;
	case 2:
		//// full fit.
		//if( fabs(factorX-factorY)<1e-2 ) 
		//{
		//	scaleMode=0;
		//	factorX=factorY=1;
		//}
		if(fabs(1.0f-factorX)<0.05f)
		{
			factorX=1;
		}
		break;
	case 3:
		// 4:3
		factorX=(int)(36400.f/vCodecCtx->width)/100.0f;
		//factorX=362.67f/vCodecCtx->width;
		break;
	case 4:
		// 2.35:1
		if(fabs(1.0f-factorX)<0.05f)
		{
			factorX=1;
		}
		factorY = factorY/1.321875f;
		break;
	case 5:
		// 2x zoommode. disabled.
		factorX=factorY=2;
		break;
	}

	float xmx=480-vCodecCtx->width*factorX;
	float ymy=272-vCodecCtx->height*factorY;
	xmx/=2.0f; xm=(int)xmx;
	ymy/=2.0f; ym=(int)ymy;
	tfx=2;
	tfy=2;
	if(fabs(factorX-1.0f)<0.01f && fabs(factorY-1.0f)<0.01f)
	{
		sampleMode=0;
	}
	else
	{
		sampleMode=1;
	}
	if(vCodecCtx->pix_fmt==PIX_FMT_YUV422P)
	{
		tfx=2;
		tfy=1;
	}
	if(vCodecCtx->pix_fmt==PIX_FMT_YUV444P)
	{
		tfx=1;
		tfy=1;
	}
}

#undef SLICE_SIZE

unsigned char *ybase, *ubase, *vbase;
int ysize, usize, vsize;
const int SLICE_SIZE=128;
float offsetX1, offsetY1, offsetX2, offsetY2;

void MakeIQAdvance()
{
	if(isVideoSeeking && iq.GetAvailSize()<2) return;

	if( audioPlayMode!=APM_PAUSED )
	{
		iq.ReadAdvance(1);
	}
}

void RenderRGB32Image(AVFrame *frame)
{
	unsigned char *texbase, *base=frame->data[0];
	int j,rm=0;
	int width=frame->linesize[0];
	struct Vertex* vertices;

	sceGuClearColor(0);
	sceGuClear(GU_COLOR_BUFFER_BIT);

	sceGuDisable(GU_BLEND);

	if(chromaDeblock || sampleMode)
	{
		offsetX1=0.01f;
		offsetY1=0.01f;
		offsetX2=0.001f;
		offsetY2=0.001f;
		sceGuTexFilter(GU_LINEAR,GU_LINEAR);
	}
	else
	{
		offsetX1=0.05f;
		offsetY1=0.05f;
		offsetX2=-0.1f;
		offsetY2=-0.1f;
		sceGuTexFilter(GU_NEAREST,GU_NEAREST);
	}

	sceGuTexMode(GU_PSM_8888,0,0,0);
	sceGuTexFunc(GU_TFX_REPLACE,GU_TCC_RGB);

	for (j = 0; j < vCodecCtx->width; j = j+SLICE_SIZE)
	{
		rm=vCodecCtx->width-j; if(rm>SLICE_SIZE) rm=SLICE_SIZE;

		texbase=base+j*4;
		sceGuTexImage(0, SLICE_SIZE, 512, 512, texbase); // width, height, buffer width, tbp
		vertices = (struct Vertex*)sceGuGetMemory(2 * sizeof(struct Vertex));

		vertices[0].u = offsetX1; vertices[0].v = offsetY1;
		vertices[0].color = 0;
		vertices[0].x = xm+j*factorX; vertices[0].y = ym; vertices[0].z = 0;
		vertices[1].u = rm+offsetX1; vertices[1].v = vCodecCtx->height+offsetY1;
		vertices[1].color = 0;
		vertices[1].x = xm+(j+rm)*factorX; vertices[1].y = ym+vCodecCtx->height*factorY; vertices[1].z = 0;

		sceGuDrawArray(GU_SPRITES,GU_TEXTURE_32BITF|GU_COLOR_8888|GU_VERTEX_32BITF|GU_TRANSFORM_2D, 2, 0, vertices);
	}

	MakeIQAdvance();
	return;
}

void RenderVideoImage()
{
	//if(vCodecCtx->pix_fmt!=PIX_FMT_YUV420P)
	//{
	//	return;
	//}
	if(!vCodecCtx || !iq.num_iq)
	{
		return;
	}

	int ccc=1000;
	while( !iq.GetAvailSize() )
	{
		ccc--;
		sceKernelDelayThread(1000);
		if(curplayer->abortFlag || curplayer->isPlaying==0)
		{
			return;
		}
		if(ccc==0)
		{
			if(15<failCount)
				curplayer->abortFlag=1;
			fprintf(stderr, "wait!!! RenderVideoImage() \n");
		}
		if(ccc<=0)
		{
			SceCtrlData pad;
			sceCtrlReadBufferPositive(&pad, 1);
			if(pad.Buttons & PSP_CTRL_CROSS)
			{
				curplayer->abortFlag=1;
			}
		}
	}
	ImageQueue::FrameHolder *destFrame=iq.GetCurrentSample();
	AVFrame *frame=(AVFrame *)destFrame->pict;

	waitingFrameTime=destFrame->frametime;

	struct Vertex* vertices;
	unsigned char *texbase;
	int j,rm=0;

	if( !frame || !frame->data[0] )
	{
		fprintf(stderr, "wrong iq %d %08x %08x\n", iq.rpos, frame, frame->data[0]);

		curplayer->abortFlag=1;
		return;
	}

	if(firstVideoUp==0)
	{
		firstVideoUp=1;
	}

	if(vCodecCtx->pix_fmt!=PIX_FMT_YUV420P)
	{
		RenderRGB32Image(frame);
		return;
	}

	//ybase=ybuffer;
	//ubase=crbuffer;
	//vbase=cbbuffer;
	//ysize=512;
	//usize=256;
	//vsize=256;

	ybase=frame->data[0];
	ubase=frame->data[1];
	vbase=frame->data[2];
	ysize=frame->linesize[0];
	usize=frame->linesize[1];
	vsize=frame->linesize[2];

	//if( waitingFrameTime==0 )
	{
		//sceGuScissor(0,0,SCR_WIDTH,SCR_HEIGHT);

		sceGuClearColor(0);
		sceGuClear(GU_COLOR_BUFFER_BIT);

		//sceGuScissor((int)xm, (int)ym, (int)(vCodecCtx->width*factorX+1), (int)(vCodecCtx->height*factorY+1));
	}

//#define BW_ONLY
#ifdef  BW_ONLY

	sceGuClearColor(0);
	sceGuClear(GU_COLOR_BUFFER_BIT);

	sceGuDisable(GU_BLEND);

	if(chromaDeblock || sampleMode)
	{
		offsetX1=0.01f;
		offsetY1=0.01f;
		offsetX2=0.001f;
		offsetY2=0.001f;
		sceGuTexFilter(GU_LINEAR,GU_LINEAR);
	}
	else
	{
		offsetX1=0.05f;
		offsetY1=0.05f;
		offsetX2=-0.1f;
		offsetY2=-0.1f;
		sceGuTexFilter(GU_NEAREST,GU_NEAREST);
	}

	sceGuClutMode(GU_PSM_8888, 0, 0xff, 0);
	sceGuTexMode(GU_PSM_T8,0,0,0);
	sceGuTexFunc(GU_TFX_REPLACE,GU_TCC_RGBA);

	sceGuClutLoad(32, pal_Y_1);
	sceGuBlendFunc(GU_ADD, GU_FIX, GU_FIX, 0xffffffff, 0xffffffff);
	for (j = 0; j < vCodecCtx->width; j = j+SLICE_SIZE)
	{
		rm=vCodecCtx->width-j; if(rm>SLICE_SIZE) rm=SLICE_SIZE;

		texbase=ybase+j;
		sceGuTexImage(0, SLICE_SIZE, 512, ysize, texbase); // width, height, buffer width, tbp
		vertices = (struct Vertex*)sceGuGetMemory(2 * sizeof(struct Vertex));

		vertices[0].u = offsetX1; vertices[0].v = offsetY1;
		vertices[0].color = 0;
		vertices[0].x = xm+j*factorX; vertices[0].y = ym; vertices[0].z = 0;
		vertices[1].u = rm+offsetX1; vertices[1].v = vCodecCtx->height+offsetY1;
		vertices[1].color = 0;
		vertices[1].x = xm+(j+rm)*factorX; vertices[1].y = ym+vCodecCtx->height*factorY; vertices[1].z = 0;

		sceGuDrawArray(GU_SPRITES,GU_TEXTURE_32BITF|GU_COLOR_8888|GU_VERTEX_32BITF|GU_TRANSFORM_2D, 2, 0, vertices);
	}

	if( audioPlayMode!=APM_PAUSED && curplayer->seekStat==0 )
	{
		MakeIQAdvance();
	}
	return;

#endif//BW_ONLY

#define PRECISE_CONVERSION
#ifdef  PRECISE_CONVERSION

	if(chromaDeblock || sampleMode)
	{
		offsetX1=0.01f;
		offsetY1=0.01f;
		offsetX2=0.501f;//-(vCodecCtx->width%32)/32.0f;
		offsetY2=0.501f;//-(vCodecCtx->height%256)/256.0f;
		sceGuTexFilter(GU_LINEAR,GU_LINEAR);
	}
	else
	{
		offsetX1=0.05f;
		offsetY1=0.05f;
		offsetX2=-0.1f;
		offsetY2=-0.1f;
		sceGuTexFilter(GU_NEAREST,GU_NEAREST);
	}

	float rdfactor=1.002f;

	sceGuEnable(GU_BLEND);

	sceGuClutMode(GU_PSM_8888, 0, 0xff, 0);
	sceGuTexMode(GU_PSM_T8,0,0,0);
	sceGuTexFunc(GU_TFX_REPLACE,GU_TCC_RGBA);

	sceGuDrawBufferList(GU_PSM_8888,vramtex, BUF_WIDTH);

	sceGuClutLoad(1, palWhite);
	sceGuTexImage(0, 4, 4, 4, texWhite); // width, height, buffer width, tbp
	sceGuBlendFunc(GU_ADD, GU_FIX, GU_FIX, g_fill_2, 0);
	for (j = 0; j < vCodecCtx->width; j = j+SLICE_SIZE)
	{
		rm=vCodecCtx->width-j; if(rm>SLICE_SIZE) rm=SLICE_SIZE;

		vertices = (struct Vertex*)sceGuGetMemory(2 * sizeof(struct Vertex));

		vertices[0].u = offsetX1; vertices[0].v = offsetY1;
		vertices[0].color = 0;
		vertices[0].x = xm+j*factorX; vertices[0].y = ym; vertices[0].z = 0;
		vertices[1].u = rm+offsetX1; vertices[1].v = vCodecCtx->height+offsetY1;
		vertices[1].color = 0;
		vertices[1].x = xm+(j+rm)*factorX; vertices[1].y = ym+vCodecCtx->height*factorY; vertices[1].z = 0;

		sceGuDrawArray(GU_SPRITES,GU_TEXTURE_32BITF|GU_COLOR_8888|GU_VERTEX_32BITF|GU_TRANSFORM_2D, 2, 0, vertices);
	}

	sceGuClutLoad(32, pal_B_Cb_2);
	for (j = 0; j < vCodecCtx->width; j = j+SLICE_SIZE)
	{
		rm=vCodecCtx->width-j; if(rm>SLICE_SIZE) rm=SLICE_SIZE;

		vertices = (struct Vertex*)sceGuGetMemory(2 * sizeof(struct Vertex));

		vertices[0].u = offsetX2; vertices[0].v = offsetY2;
		vertices[0].color = 0;
		vertices[0].x = xm+(j)*factorX; vertices[0].y = ym; vertices[0].z = 0;
		vertices[1].u = rm/tfx+offsetX2; vertices[1].v =vCodecCtx->height/tfy+offsetY2-rdfactor;
		vertices[1].color = 0;
		vertices[1].x = xm+(j+rm)*factorX; vertices[1].y = ym+vCodecCtx->height*factorY; vertices[1].z = 0;

		texbase=ubase+j/tfx;
		sceGuTexImage(0, SLICE_SIZE/tfx, 512, usize, texbase); // width, height, buffer width, tbp

		sceGuBlendFunc(GU_ADD, GU_FIX, GU_FIX, 0xffffffff, 0xffffffff);
		sceGuDrawArray(GU_SPRITES,GU_TEXTURE_32BITF|GU_COLOR_8888|GU_VERTEX_32BITF|GU_TRANSFORM_2D, 2, 0, vertices);
	}

	sceGuClutLoad(32, pal_G_Cb_2);
	for (j = 0; j < vCodecCtx->width; j = j+SLICE_SIZE)
	{
		rm=vCodecCtx->width-j; if(rm>SLICE_SIZE) rm=SLICE_SIZE;

		vertices = (struct Vertex*)sceGuGetMemory(2 * sizeof(struct Vertex));

		vertices[0].u = offsetX2; vertices[0].v = offsetY2;
		vertices[0].color = 0;
		vertices[0].x = xm+(j)*factorX; vertices[0].y = ym; vertices[0].z = 0;
		vertices[1].u = rm/tfx+offsetX2; vertices[1].v =vCodecCtx->height/tfy+offsetY2-rdfactor;
		vertices[1].color = 0;
		vertices[1].x = xm+(j+rm)*factorX; vertices[1].y = ym+vCodecCtx->height*factorY; vertices[1].z = 0;

		texbase=ubase+j/tfx;
		sceGuTexImage(0, SLICE_SIZE/tfx, 512, usize, texbase); // width, height, buffer width, tbp

		sceGuBlendFunc(GU_REVERSE_SUBTRACT, GU_FIX, GU_FIX, 0xffffffff, 0xffffffff);
		sceGuDrawArray(GU_SPRITES,GU_TEXTURE_32BITF|GU_COLOR_8888|GU_VERTEX_32BITF|GU_TRANSFORM_2D, 2, 0, vertices);
	}

	sceGuClutLoad(32, pal_R_Cr_2);
	for (j = 0; j < vCodecCtx->width; j = j+SLICE_SIZE)
	{
		rm=vCodecCtx->width-j; if(rm>SLICE_SIZE) rm=SLICE_SIZE;

		vertices = (struct Vertex*)sceGuGetMemory(2 * sizeof(struct Vertex));

		vertices[0].u = offsetX2; vertices[0].v = offsetY2;
		vertices[0].color = 0;
		vertices[0].x = xm+(j)*factorX; vertices[0].y = ym; vertices[0].z = 0;
		vertices[1].u = rm/tfx+offsetX2; vertices[1].v =vCodecCtx->height/tfy+offsetY2-rdfactor;
		vertices[1].color = 0;
		vertices[1].x = xm+(j+rm)*factorX; vertices[1].y = ym+vCodecCtx->height*factorY; vertices[1].z = 0;

		texbase=vbase+j/tfx;
		sceGuTexImage(0, SLICE_SIZE/tfx, 512, vsize, texbase); // width, height, buffer width, tbp

		sceGuBlendFunc(GU_ADD, GU_FIX, GU_FIX, 0xffffffff, 0xffffffff);
		sceGuDrawArray(GU_SPRITES,GU_TEXTURE_32BITF|GU_COLOR_8888|GU_VERTEX_32BITF|GU_TRANSFORM_2D, 2, 0, vertices);
	}

	sceGuClutLoad(32, pal_G_Cr_2);
	for (j = 0; j < vCodecCtx->width; j = j+SLICE_SIZE)
	{
		rm=vCodecCtx->width-j; if(rm>SLICE_SIZE) rm=SLICE_SIZE;

		vertices = (struct Vertex*)sceGuGetMemory(2 * sizeof(struct Vertex));

		vertices[0].u = offsetX2; vertices[0].v = offsetY2;
		vertices[0].color = 0;
		vertices[0].x = xm+(j)*factorX; vertices[0].y = ym; vertices[0].z = 0;
		vertices[1].u = rm/tfx+offsetX2; vertices[1].v =vCodecCtx->height/tfy+offsetY2-rdfactor;
		vertices[1].color = 0;
		vertices[1].x = xm+(j+rm)*factorX; vertices[1].y = ym+vCodecCtx->height*factorY; vertices[1].z = 0;

		texbase=vbase+j/tfx;
		sceGuTexImage(0, SLICE_SIZE/tfx, 512, vsize, texbase); // width, height, buffer width, tbp

		sceGuBlendFunc(GU_REVERSE_SUBTRACT, GU_FIX, GU_FIX, 0xffffffff, 0xffffffff);
		sceGuDrawArray(GU_SPRITES,GU_TEXTURE_32BITF|GU_COLOR_8888|GU_VERTEX_32BITF|GU_TRANSFORM_2D, 2, 0, vertices);
	}

	sceGuClutLoad(32, pal_Y_2);
	sceGuBlendFunc(GU_ADD, GU_FIX, GU_FIX, 0xffffffff, 0xffffffff);
	for (j = 0; j < vCodecCtx->width; j = j+SLICE_SIZE)
	{
		rm=vCodecCtx->width-j; if(rm>SLICE_SIZE) rm=SLICE_SIZE;

		texbase=ybase+j;
		sceGuTexImage(0, SLICE_SIZE, 512, ysize, texbase); // width, height, buffer width, tbp
		vertices = (struct Vertex*)sceGuGetMemory(2 * sizeof(struct Vertex));

		vertices[0].u = offsetX1; vertices[0].v = offsetY1;
		vertices[0].color = 0;
		vertices[0].x = xm+j*factorX; vertices[0].y = ym; vertices[0].z = 0;
		vertices[1].u = rm+offsetX1; vertices[1].v = vCodecCtx->height+offsetY1;
		vertices[1].color = 0;
		vertices[1].x = xm+(j+rm)*factorX; vertices[1].y = ym+vCodecCtx->height*factorY; vertices[1].z = 0;

		sceGuDrawArray(GU_SPRITES,GU_TEXTURE_32BITF|GU_COLOR_8888|GU_VERTEX_32BITF|GU_TRANSFORM_2D, 2, 0, vertices);
	}

	sceGuClutLoad(1, palWhite);
	sceGuTexImage(0, 4, 4, 4, texWhite); // width, height, buffer width, tbp
	sceGuBlendFunc(GU_REVERSE_SUBTRACT, GU_FIX, GU_FIX, rgb_sub_2, 0xffffffff);
	for (j = 0; j < vCodecCtx->width; j = j+SLICE_SIZE)
	{
		rm=vCodecCtx->width-j; if(rm>SLICE_SIZE) rm=SLICE_SIZE; rm=(rm+63)&~63;

		vertices = (struct Vertex*)sceGuGetMemory(2 * sizeof(struct Vertex));

		vertices[0].u = offsetX1; vertices[0].v = offsetY1;
		vertices[0].color = 0;
		vertices[0].x = xm+j*factorX; vertices[0].y = ym; vertices[0].z = 0;
		vertices[1].u = rm+offsetX1; vertices[1].v = vCodecCtx->height+offsetY1;
		vertices[1].color = 0;
		vertices[1].x = xm+(j+rm)*factorX; vertices[1].y = ym+vCodecCtx->height*factorY; vertices[1].z = 0;

		sceGuDrawArray(GU_SPRITES,GU_TEXTURE_32BITF|GU_COLOR_8888|GU_VERTEX_32BITF|GU_TRANSFORM_2D, 2, 0, vertices);
	}

	sceGuDrawBufferList(GU_PSM_8888,(void*)display.GetDrawBufferBasePtr(), BUF_WIDTH);

	sceGuClutLoad(1, palWhite);
	sceGuTexImage(0, 4, 4, 4, texWhite); // width, height, buffer width, tbp
	sceGuBlendFunc(GU_ADD, GU_FIX, GU_FIX, g_fill_1, 0);
	for (j = 0; j < vCodecCtx->width; j = j+SLICE_SIZE)
	{
		rm=vCodecCtx->width-j; if(rm>SLICE_SIZE) rm=SLICE_SIZE;

		vertices = (struct Vertex*)sceGuGetMemory(2 * sizeof(struct Vertex));

		vertices[0].u = offsetX1; vertices[0].v = offsetY1;
		vertices[0].color = 0;
		vertices[0].x = xm+j*factorX; vertices[0].y = ym; vertices[0].z = 0;
		vertices[1].u = rm+offsetX1; vertices[1].v = vCodecCtx->height+offsetY1;
		vertices[1].color = 0;
		vertices[1].x = xm+(j+rm)*factorX; vertices[1].y = ym+vCodecCtx->height*factorY; vertices[1].z = 0;

		sceGuDrawArray(GU_SPRITES,GU_TEXTURE_32BITF|GU_COLOR_8888|GU_VERTEX_32BITF|GU_TRANSFORM_2D, 2, 0, vertices);
	}

	sceGuClutLoad(32, pal_B_Cb_1);
	for (j = 0; j < vCodecCtx->width; j = j+SLICE_SIZE)
	{
		rm=vCodecCtx->width-j; if(rm>SLICE_SIZE) rm=SLICE_SIZE;

		vertices = (struct Vertex*)sceGuGetMemory(2 * sizeof(struct Vertex));

		vertices[0].u = offsetX2; vertices[0].v = offsetY2;
		vertices[0].color = 0;
		vertices[0].x = xm+(j)*factorX; vertices[0].y = ym; vertices[0].z = 0;
		vertices[1].u = rm/tfx+offsetX2; vertices[1].v =vCodecCtx->height/tfy+offsetY2-rdfactor;
		vertices[1].color = 0;
		vertices[1].x = xm+(j+rm)*factorX; vertices[1].y = ym+vCodecCtx->height*factorY; vertices[1].z = 0;

		texbase=ubase+j/tfx;
		sceGuTexImage(0, SLICE_SIZE/tfx, 512, usize, texbase); // width, height, buffer width, tbp

		sceGuBlendFunc(GU_ADD, GU_FIX, GU_FIX, 0xffffffff, 0xffffffff);
		sceGuDrawArray(GU_SPRITES,GU_TEXTURE_32BITF|GU_COLOR_8888|GU_VERTEX_32BITF|GU_TRANSFORM_2D, 2, 0, vertices);
	}

	sceGuClutLoad(32, pal_G_Cb_1);
	for (j = 0; j < vCodecCtx->width; j = j+SLICE_SIZE)
	{
		rm=vCodecCtx->width-j; if(rm>SLICE_SIZE) rm=SLICE_SIZE;

		vertices = (struct Vertex*)sceGuGetMemory(2 * sizeof(struct Vertex));

		vertices[0].u = offsetX2; vertices[0].v = offsetY2;
		vertices[0].color = 0;
		vertices[0].x = xm+(j)*factorX; vertices[0].y = ym; vertices[0].z = 0;
		vertices[1].u = rm/tfx+offsetX2; vertices[1].v =vCodecCtx->height/tfy+offsetY2-rdfactor;
		vertices[1].color = 0;
		vertices[1].x = xm+(j+rm)*factorX; vertices[1].y = ym+vCodecCtx->height*factorY; vertices[1].z = 0;

		texbase=ubase+j/tfx;
		sceGuTexImage(0, SLICE_SIZE/tfx, 512, usize, texbase); // width, height, buffer width, tbp

		sceGuBlendFunc(GU_REVERSE_SUBTRACT, GU_FIX, GU_FIX, 0xffffffff, 0xffffffff);
		sceGuDrawArray(GU_SPRITES,GU_TEXTURE_32BITF|GU_COLOR_8888|GU_VERTEX_32BITF|GU_TRANSFORM_2D, 2, 0, vertices);
	}

	sceGuClutLoad(32, pal_R_Cr_1);
	for (j = 0; j < vCodecCtx->width; j = j+SLICE_SIZE)
	{
		rm=vCodecCtx->width-j; if(rm>SLICE_SIZE) rm=SLICE_SIZE;

		vertices = (struct Vertex*)sceGuGetMemory(2 * sizeof(struct Vertex));

		vertices[0].u = offsetX2; vertices[0].v = offsetY2;
		vertices[0].color = 0;
		vertices[0].x = xm+(j)*factorX; vertices[0].y = ym; vertices[0].z = 0;
		vertices[1].u = rm/tfx+offsetX2; vertices[1].v =vCodecCtx->height/tfy+offsetY2-rdfactor;
		vertices[1].color = 0;
		vertices[1].x = xm+(j+rm)*factorX; vertices[1].y = ym+vCodecCtx->height*factorY; vertices[1].z = 0;

		texbase=vbase+j/tfx;
		sceGuTexImage(0, SLICE_SIZE/tfx, 512, vsize, texbase); // width, height, buffer width, tbp

		sceGuBlendFunc(GU_ADD, GU_FIX, GU_FIX, 0xffffffff, 0xffffffff);
		sceGuDrawArray(GU_SPRITES,GU_TEXTURE_32BITF|GU_COLOR_8888|GU_VERTEX_32BITF|GU_TRANSFORM_2D, 2, 0, vertices);
	}

	sceGuClutLoad(32, pal_G_Cr_1);
	for (j = 0; j < vCodecCtx->width; j = j+SLICE_SIZE)
	{
		rm=vCodecCtx->width-j; if(rm>SLICE_SIZE) rm=SLICE_SIZE;

		vertices = (struct Vertex*)sceGuGetMemory(2 * sizeof(struct Vertex));

		vertices[0].u = offsetX2; vertices[0].v = offsetY2;
		vertices[0].color = 0;
		vertices[0].x = xm+(j)*factorX; vertices[0].y = ym; vertices[0].z = 0;
		vertices[1].u = rm/tfx+offsetX2; vertices[1].v =vCodecCtx->height/tfy+offsetY2-rdfactor;
		vertices[1].color = 0;
		vertices[1].x = xm+(j+rm)*factorX; vertices[1].y = ym+vCodecCtx->height*factorY; vertices[1].z = 0;

		texbase=vbase+j/tfx;
		sceGuTexImage(0, SLICE_SIZE/tfx, 512, vsize, texbase); // width, height, buffer width, tbp

		sceGuBlendFunc(GU_REVERSE_SUBTRACT, GU_FIX, GU_FIX, 0xffffffff, 0xffffffff);
		sceGuDrawArray(GU_SPRITES,GU_TEXTURE_32BITF|GU_COLOR_8888|GU_VERTEX_32BITF|GU_TRANSFORM_2D, 2, 0, vertices);
	}

	sceGuClutLoad(32, pal_Y_1);
	sceGuBlendFunc(GU_ADD, GU_FIX, GU_FIX, 0xffffffff, 0xffffffff);
	for (j = 0; j < vCodecCtx->width; j = j+SLICE_SIZE)
	{
		rm=vCodecCtx->width-j; if(rm>SLICE_SIZE) rm=SLICE_SIZE;

		texbase=ybase+j;
		sceGuTexImage(0, SLICE_SIZE, 512, ysize, texbase); // width, height, buffer width, tbp
		vertices = (struct Vertex*)sceGuGetMemory(2 * sizeof(struct Vertex));

		vertices[0].u = offsetX1; vertices[0].v = offsetY1;
		vertices[0].color = 0;
		vertices[0].x = xm+j*factorX; vertices[0].y = ym; vertices[0].z = 0;
		vertices[1].u = rm+offsetX1; vertices[1].v = vCodecCtx->height+offsetY1;
		vertices[1].color = 0;
		vertices[1].x = xm+(j+rm)*factorX; vertices[1].y = ym+vCodecCtx->height*factorY; vertices[1].z = 0;

		sceGuDrawArray(GU_SPRITES,GU_TEXTURE_32BITF|GU_COLOR_8888|GU_VERTEX_32BITF|GU_TRANSFORM_2D, 2, 0, vertices);
	}

	sceGuClutLoad(1, palWhite);
	sceGuTexImage(0, 4, 4, 4, texWhite); // width, height, buffer width, tbp
	sceGuBlendFunc(GU_REVERSE_SUBTRACT, GU_FIX, 6, rgb_sub_1, 0);
	for (j = 0; j < vCodecCtx->width; j = j+SLICE_SIZE)
	{
		rm=vCodecCtx->width-j; if(rm>SLICE_SIZE) rm=SLICE_SIZE; rm=(rm+63)&~63;

		vertices = (struct Vertex*)sceGuGetMemory(2 * sizeof(struct Vertex));

		vertices[0].u = offsetX1; vertices[0].v = offsetY1;
		vertices[0].color = 0;
		vertices[0].x = xm+j*factorX; vertices[0].y = ym; vertices[0].z = 0;
		vertices[1].u = rm+offsetX1; vertices[1].v = vCodecCtx->height+offsetY1;
		vertices[1].color = 0;
		vertices[1].x = xm+(j+rm)*factorX; vertices[1].y = ym+vCodecCtx->height*factorY; vertices[1].z = 0;

		sceGuDrawArray(GU_SPRITES,GU_TEXTURE_32BITF|GU_COLOR_8888|GU_VERTEX_32BITF|GU_TRANSFORM_2D, 2, 0, vertices);
	}

//	sceGuClutMode(GU_PSM_8888, 0, 0xff, 0);
	sceGuTexMode(GU_PSM_8888,0,0,0);
	sceGuTexFunc(GU_TFX_REPLACE,GU_TCC_RGB);

	int waitParam=1;
	if(waitParam)
	{
	unsigned long *obase=(unsigned long *)vramtex;
	if(waitParam==1)
		sceGuBlendFunc(GU_ADD, GU_FIX, GU_FIX, 0xff0a0a0a, 0xffffffff);
	else
		sceGuBlendFunc(GU_ADD, GU_FIX, GU_FIX, 0xffffffff, 0xff000000);
	for (j = 0; j < vCodecCtx->width; j = j+SLICE_SIZE)
	{
		rm=vCodecCtx->width-j; if(rm>SLICE_SIZE) rm=SLICE_SIZE;

		texbase=(unsigned char *)(obase+j);
		sceGuTexImage(0, SLICE_SIZE, 512, 512, texbase); // width, height, buffer width, tbp
		vertices = (struct Vertex*)sceGuGetMemory(2 * sizeof(struct Vertex));

		vertices[0].u = offsetX1; vertices[0].v = offsetY1;
		vertices[0].color = 0;
		vertices[0].x = xm+j*factorX; vertices[0].y = ym; vertices[0].z = 0;
		vertices[1].u = rm+offsetX1; vertices[1].v = vCodecCtx->height+offsetY1;
		vertices[1].color = 0;
		vertices[1].x = xm+(j+rm)*factorX; vertices[1].y = ym+vCodecCtx->height*factorY; vertices[1].z = 0;

		sceGuDrawArray(GU_SPRITES,GU_TEXTURE_32BITF|GU_COLOR_8888|GU_VERTEX_32BITF|GU_TRANSFORM_2D, 2, 0, vertices);
	}
	}
#endif//PRECISE_CONVERSION

	MakeIQAdvance();
	return;
}

int enableVideoTitle=0;
void DrawVideoTitle(float fps)
{
	char buf[STRING_LENGTH];

	//scaleMode;
	char *scalestring[]={"ori", "fit", "ful", "4:3", "mov", "2xz"};
	char vtag[5]="";
	char atag[4]="";
	char pixfmt[4]="   ";
	float vrate=0;
	if(0<=vstreamindex && vCodecCtx)
	{
		if( vCodecCtx->codec_id==CODEC_ID_MPEG4)
		{
			vtag[0]=(vCodecCtx->codec_tag>>0 )&0xff;
			vtag[1]=(vCodecCtx->codec_tag>>8 )&0xff;
			vtag[2]=(vCodecCtx->codec_tag>>16)&0xff;
			vtag[3]=(vCodecCtx->codec_tag>>24)&0xff;
		}
		else if( vCodecCtx->codec_id==CODEC_ID_H264)
		{
			strcpy(vtag, "h264");
		}

		if(vStream->time_base.num)
			vrate=(float)vStream->time_base.den/(float)vStream->time_base.num;
		if( vCodecCtx->pix_fmt==PIX_FMT_RGB32_1 )
		{
			strcpy(pixfmt, "RGB");
		}
		else if( vCodecCtx->pix_fmt==PIX_FMT_YUV420P )
		{
			strcpy(pixfmt, "YUV");
		}
	}
	if(0<=astreamindex && aCodecCtx)
	{
		if( aCodecCtx->codec_id==CODEC_ID_AC3)
		{
			strcpy(atag, "ac3");
		}
		else if( aCodecCtx->codec_id==CODEC_ID_MP3)
		{
			strcpy(atag, "mp3");
		}
		else if( aCodecCtx->codec_id==CODEC_ID_AAC)
		{
			strcpy(atag, "aac");
		}
	}
	int ccc=scePowerGetCpuClockFrequency();
	int ddd=scePowerGetBusClockFrequency();

	mp3TopClamper.StrSet(curplayer->fileName.c_str());

	int vol=imposeGetVolume();

	snprintf(buf, STRING_LENGTH, "%s (%d:%02d/%d:%02d) (%s/%.1fX) %s%s%s", mp3TopClamper.Get(),
		curplayer->playedSample/44100/60, (curplayer->playedSample/44100)%60, curplayer->estimatedSeconds/60, curplayer->estimatedSeconds%60,
		scalestring[scaleMode], volumeBoost,
		(audioPlayMode==APM_PAUSED)?"P":"-", autoRepeat?"∽":"-", (playerMode==PM_RANDOM)?"R":"-"
		);
	textTool.PutStringMByteCC(0, 0, playtextColor, buf);

	pspTime curTime;
	sceRtcGetCurrentClock(&curTime, timeZoneOffset*60);

	if(extMenu || sleepCounter)
	{
		snprintf(buf, STRING_LENGTH, "%s %02d:%02d Sleep:%dm",
			curTime.hour<12?"am":"pm", curTime.hour>12?curTime.hour-12:curTime.hour,
			curTime.minutes, (sleepCounter+59)/60);
		textTool.PutStringMByteCC(27, 0, titleColor, buf);
	}
	else
	{
		snprintf(buf, STRING_LENGTH, "%s %02d:%02d:%02d", 
			curTime.hour<12?"am":"pm", curTime.hour>12?curTime.hour-12:curTime.hour,
			curTime.minutes, curTime.seconds);
		textTool.PutStringMByteCC(27, 0, titleColor, buf);
	}
	if(scePowerIsBatteryExist())
	{
		snprintf(buf, STRING_LENGTH, "%s %02d%%", sceHprmIsRemoteExist()?"R":" ", scePowerGetBatteryLifePercent());
		textTool.PutStringMByteCC(37, 0, 0xffff80e0, buf);
	}
	else
	{
		snprintf(buf, STRING_LENGTH, "%s ≠♨", sceHprmIsRemoteExist()?"R":" ");
		textTool.PutStringMByteCC(37, 0, 0xffff80e0, buf);
	}

	if(enableVideoTitle<2)
	{
		return;
	}

	snprintf(buf, STRING_LENGTH, "[%d:%s:%.2f:%s][%d:%s:%dkbps:%d㎑] [%03d/%03d/%.1f/%d/%.1f] %d %d %d %s",
		vstreamindex, vtag, vrate, pixfmt,
		astreamindex, atag, curplayer->bitRate/1000, (int)(curplayer->samplingRate/1000),
		ccc, ddd, fps, iq.GetAvailSize(), (float)(curplayer->copiedSample-curplayer->playedSample)/44100.0f,
		count1, count2, count3, errorString
		);
	textTool.PutStringMByteCC(0, 1, playtextColor, buf);

	return;
}

void RegisterAVCodec()
{
	av_register_all();
	register_avcodec(&my_mp3_decoder);
    register_avcodec(&aac_decoder);
    register_avcodec(&mpeg4aac_decoder);
    register_avcodec(&my_mpeg4_decoder);
	register_avcodec(&my_h264_decoder);

	//av_log_set_level(AV_LOG_DEBUG);

	InitPallete3();

	//if( my_mp4v_decode_init(NULL) < 0)
	//	PrintErrMsgWait("merong");
	//my_mp4v_close(NULL);

	//my_avc_decode_init(NULL);
	//my_avc_close(NULL);

	//PrintErrMsgWait("hahaha");
}

#define MAX_INDEX_ENTRY (20*240) //(20 per min, 240 min, keyframe only)
int videoIndexEntry=0;
int64_t lastTimeStamp[10]={0,};
void InitIndexVars()
{
	videoIndexEntry=0;
	for(int i=0; i<10; i++)
	{
		lastTimeStamp[i]=-0x8000000000000000LL;
	}
}

extern "C" {

#if 1
// keyframe only.
// no realloc.
// no av_index_search_timestamp.
// assume input indices are sorted. (for performance)
// 1 video index then 1 audio index (for memory)
int av_add_index_entry(AVStream *st,
                            int64_t pos, int64_t timestamp, int size, int distance, int flags)
{
    AVIndexEntry *entries, *ie;
    int index;

	if(st->codec->codec_type==CODEC_TYPE_VIDEO)
	{
		if(timestamp<=0)
		{
			flags=AVINDEX_KEYFRAME;
		}
	}
	if( !isMP4 )
	{
		if(flags!=AVINDEX_KEYFRAME || MAX_INDEX_ENTRY<=st->nb_index_entries || !st->codec || 10<=st->index )
		{
			return 0;
		}
		if( timestamp<lastTimeStamp[st->index] )
		{
			return 0;
		}

		if(st->codec->codec_type==CODEC_TYPE_VIDEO)
		{
			if(timestamp>0)
			{
				int64_t td=timestamp-lastTimeStamp[st->index];
				if((int)td*av_q2d(st->time_base)<3)
				{
					return 0;
				}
			}

			lastTimeStamp[st->index]=timestamp;
			videoIndexEntry=1;
		}
		else if(st->codec->codec_type==CODEC_TYPE_AUDIO)
		{
			if(timestamp>0 && videoIndexEntry==0)
			{
				return 0;
			}

			videoIndexEntry=0;
		}
	}

	if((unsigned)st->nb_index_entries + 1 >= UINT_MAX / sizeof(AVIndexEntry))
		return -1;

	if( !isMP4 )
	{
		if(!st->index_entries)
		{
			int entrysize=sizeof(AVIndexEntry)*MAX_INDEX_ENTRY;
			st->index_entries=(AVIndexEntry *)av_malloc(entrysize);
			
			st->index_entries_allocated_size=entrysize;

	//		fprintf(stderr, "idx alloc %08x %d\n", entries, entrysize);
		}
		entries=st->index_entries;
	}
	else
	{
		entries = (AVIndexEntry *)av_fast_realloc(st->index_entries,
								&st->index_entries_allocated_size,
								(st->nb_index_entries + 1) *
								sizeof(AVIndexEntry));

		//static int prevallocsize=0;
		//if(prevallocsize+128*1024<st->index_entries_allocated_size)
		//{
		//	fprintf(stderr, "new alloc size %d\n", st->index_entries_allocated_size);
		//	prevallocsize=st->index_entries_allocated_size;
		//}
	}

    if(!entries)
        return -1;

    st->index_entries= entries;

	if( !isMP4)
	{
		index=-1;//av_index_search_timestamp(st, timestamp, AVSEEK_FLAG_ANY);
	}
	else
	{
		index=av_index_search_timestamp(st, timestamp, AVSEEK_FLAG_ANY);
	}

	//fprintf(stderr, "ai si %d ii %d st %d\n", st->index, st->nb_index_entries, (int)timestamp);

    if(index<0){
        index= st->nb_index_entries++;
        ie= &entries[index];
 	//fprintf(stderr, "index %d ie %08x\n", index, ie);
       //assert(index==0 || ie[-1].timestamp < timestamp);
    }else{
        ie= &entries[index];
        if(ie->timestamp != timestamp){
            if(ie->timestamp <= timestamp)
                return -1;
            memmove(entries + index + 1, entries + index, sizeof(AVIndexEntry)*(st->nb_index_entries - index));
            st->nb_index_entries++;
        }else if(ie->pos == pos && distance < ie->min_distance) //do not reduce the distance
            distance= ie->min_distance;
    }

    ie->pos = pos;
    ie->timestamp = timestamp;
    ie->min_distance= distance;
    ie->size= size;
    ie->flags = flags;

    return index;
}

#endif

u64 timerCheckStart[10];
int pp=0;
void StartTimer()
{
	if(pp==10)
		return;

	sceRtcGetCurrentTick(&timerCheckStart[pp++]);
}
void EndTimer(const char *reason, int inmsec=0)
{
	if(pp==0)
		return;

	u64 timerCheckEnd;
	sceRtcGetCurrentTick(&timerCheckEnd);

	u64 diff=timerCheckEnd-timerCheckStart[--pp];

	if(inmsec)
	{
		double mdiff = (double)diff/1e3;
		fprintf(stderr, "%s %.2fms\n", reason, (float)mdiff);
	}
	else
	{
		double tdiff = (double)diff/1e6;
		fprintf(stderr, "%s %.2fs\n", reason, (float)tdiff);
	}
}

}

extern "C" {
#include "subreader.h"
};

// subtitle rendering...
#define SUB_OFFSET (512*272*4*3)
#define SUB_BUFFER_HEIGHT 208	// if rgba 8888.

int subShowRefY=260;
float subSizeRatio=1.5f;
int subMaxCharInLine=(int)(480/(6*subSizeRatio));	// 1 char=6*sizeratio pixel

int subSrcSX=240, subSrcSY=32;
int subDestX=0, subDestY=160;
int subDestSX=360, subDestSY=48;

u32 *subtex=NULL;
sub_data *subData=NULL;

int subMaxLen=0;
int subStrlen[10]={0};
int subCurPos=-1;
int isSubActive=0;

void CalcSubDestRegion()
{
	subDestX = 0;//(int)((480-subSrcSX*subSizeRatio)/2.0f);
	subDestY = (int)(subShowRefY-subSrcSY*subSizeRatio);

	subDestSX=(int)(subSrcSX*subSizeRatio);
	subDestSY=(int)(subSrcSY*subSizeRatio);
}

void CalcSubSrcRegion()
{
	if(subCurPos<0 || subCurPos>=subData->sub_num)
	{
		subMaxLen=0;
		subSrcSX=0;
		subSrcSY=0;
		return;
	}

	subtitle *cur=&subData->subtitles[subCurPos];

	subMaxLen=0;
	int lines=0;
	for(int i=0; i<cur->lines; i++)
	{
		subStrlen[i]=strlen(cur->text[i]);
		lines +=subStrlen[i]/(subMaxCharInLine-1)+1;

		if(subMaxLen<subStrlen[i])
		{
			subMaxLen=subStrlen[i];
		}
	}

	//subSrcSX=subMaxLen*6;
	//subSrcSY=14*cur->lines+2*(cur->lines-1); if(subSrcSY<0) subSrcSY=0;
	//fprintf(stderr, "CalcSubSrcRegion. %d %d\n", subSrcSX, subSrcSY);
	subMaxCharInLine=(int)(480/(7*subSizeRatio));	// 1 char=6*sizeratio pixel
	subSrcSX=subMaxCharInLine*7;
	subSrcSY=14*lines+2*(lines-1); if(subSrcSY<0) subSrcSY=0;
}

void PrepareSubBuf()
{
	PrepareLargeFont();
	if(!subtex)
	{
		subtex=(u32*)(((u32)sceGeEdramGetAddr() + SUB_OFFSET));
	}
}

int OpenSubFile(char *fileName)
{
	if(subData)
	{
		return 0;
	}

	subCurPos=-1;
	sub_data* data=sub_read_file(fileName, 30);

	subData=data;

	//fprintf(stderr, "sub open %08x %s\n", subData, fileName);
	//for(int i=0; i<10; i++)
	//{
	//	subtitle *cur=&subData->subtitles[i];

	//	fprintf(stderr, "%d %d %s\n", cur->start, cur->end, cur->text[0]);
	//}
	return subData?1:0;
}

void CloseSubFile()
{
	subCurPos=-1;

	sub_data *data=subData;
	subData=NULL;

	if(data)
	{
		free(data);
	}
}

// return endtime.
int FindCurrentSub(int timeInMP)
{
	int findstart=subCurPos;
	subtitle *cur, *next;

	if(findstart<0 || findstart>=subData->sub_num)
	{
		findstart=0;
	}

	cur=&subData->subtitles[findstart];
	if(timeInMP<cur->start)
	{
		findstart=0;
	}

	for(int i=findstart; i<subData->sub_num-1; i++)
	{
		cur=&subData->subtitles[i];
		next=&subData->subtitles[i+1];
		if(timeInMP<cur->start)
		{
			return -1;
		}
		if(cur->start<=timeInMP && timeInMP<next->start)
		{
			subCurPos=i;
			return cur->end;
		}
	}

	subCurPos=subData->sub_num-1;
	return subData->subtitles[subCurPos].end;
}

void UpdateSubtitle(int timeInMP)
{
	if(!subData) return;

	int prevSub=subCurPos;
	int endtime=FindCurrentSub(timeInMP);

	if(endtime<timeInMP || endtime==-1)
	{
		isSubActive=0;
		return;
	}

	if(prevSub==subCurPos)
	{
		isSubActive=1;
		return;
	}

	//StartTimer();
	if(prevSub!=subCurPos)
	{
		CalcSubSrcRegion();
	}

	isSubActive=1;

	subtitle *cur=&subData->subtitles[subCurPos];

	DWORD s=textTool16.shadowColor;

	textTool16.shadowColor=0;

	textTool16.SetSpacing(0, 2);
	textTool16.SetTextRefCoord(0, 0);

	display.FillBackgroundPtr(subtex, 0, 0, 0, (int)subSrcSX+1, (int)subSrcSY+1);
	textTool16.SetBaseAddr((char*)subtex);

	int lmargin=(int)(480-subMaxCharInLine*7*subSizeRatio)/2;
	int left, ysize=0, ctextlen, iStartPos=0;
	char *ctext, *next;
	std::StringClamper textClamper(subMaxCharInLine);
	for(int i=0; i<cur->lines; i++)
	{
		int yline=subStrlen[i]/(subMaxCharInLine-2)+1;

		iStartPos=0;
		ctext=cur->text[i];
		for(int j=0; j<yline; j++)
		{
			next=textClamper.MyTcsOffset(ctext, subMaxCharInLine-2);
			ctextlen=next-ctext;

			left=(subMaxCharInLine-ctextlen)/2;
			textTool16.PutStringMByte(lmargin+left*7, ysize*16, 0xffffffff, ctext, ctextlen);
			ysize++;

			ctext=next;
			iStartPos += ctextlen;

			if(6<=ysize)
				break;
		}

		if(6<=ysize)
			break;
	}

	textTool16.shadowColor=s;

	sceKernelDcacheWritebackAll();

	subSizeRatio=1.5f;
	CalcSubDestRegion();

	//EndTimer("subtitle.", 1);
}

void RenderSubtitleTex1(float ox, float oy, int blendenable, int color, float tofx=0.0f, float tofy=0.0f)
{
	unsigned char *texbase, *base=(unsigned char*)subtex;
	int j,rm=0;
	int width=512;
	struct Vertex* vertices;
	float tox=0, toy=0;

	if(blendenable)
	{
		tox=0.01f;
		toy=0.01f;
		sceGuEnable(GU_BLEND);
		sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);

		sceGuTexFunc(GU_TFX_MODULATE,GU_TCC_RGBA);
		sceGuTexFilter(GU_LINEAR,GU_LINEAR);
	}
	else
	{
		tox=0.05f+tofx;
		toy=0.05f+tofy;
		sceGuDisable(GU_BLEND);

		sceGuTexFunc(GU_TFX_MODULATE,GU_TCC_RGBA);
		sceGuTexFilter(GU_LINEAR,GU_LINEAR);
//		sceGuTexFilter(GU_NEAREST,GU_NEAREST);
	}

	for (j = 0; j < subSrcSX; j = j+SLICE_SIZE)
	{
		rm=subSrcSX-j; if(rm>SLICE_SIZE) rm=SLICE_SIZE;

		texbase=base+j*4;
		sceGuTexImage(0, SLICE_SIZE, 512, 512, texbase); // width, height, buffer width, tbp
		vertices = (struct Vertex*)sceGuGetMemory(2 * sizeof(struct Vertex));

		vertices[0].u = tox; vertices[0].v = toy;
		vertices[0].color = color;
		vertices[0].x = ox+subDestX+j*subSizeRatio; vertices[0].y = oy+subDestY; vertices[0].z = 0;
		vertices[1].u = rm+tox; vertices[1].v = subSrcSY+toy;
		vertices[1].color = color;
		vertices[1].x = ox+subDestX+(j+rm)*subSizeRatio; vertices[1].y = oy+subDestY+subDestSY; vertices[1].z = 0;

		sceGuDrawArray(GU_SPRITES,GU_TEXTURE_32BITF|GU_COLOR_8888|GU_VERTEX_32BITF|GU_TRANSFORM_2D, 2, 0, vertices);
	}

}

void RenderSubtitle()
{
	if(isSubActive==0)
	{
		return;
	}

	sceGuTexMode(GU_PSM_8888,0,0,0);
	sceGuTexFunc(GU_TFX_MODULATE,GU_TCC_RGBA);
	sceGuTexFilter(GU_LINEAR,GU_LINEAR);

//	sceGuTexFilter(GU_NEAREST,GU_NEAREST);
//	sceGuTexFunc(GU_TFX_REPLACE,GU_TCC_RGBA);

	sceGuAlphaFunc(GU_GREATER,0,0xff);
	sceGuEnable(GU_ALPHA_TEST);

	unsigned long color=0;
	int btype=0;
	int ksum=0;
	for(int i=-2; i<=2; i++)
	{
		for(int j=-2; j<=2; j++)
		{
			int ai=(i<0)?-i:i;
			int aj=(j<0)?-j:j;
			if(ai==0 && aj==0)
			{
				continue;
			}
			else if(ai+aj<=2 && ai<2 && aj<2)
			{
				btype=0;
				color=0xff000000;
			}
			else if(!(ai==2 && aj==2))
			{
				btype=1;
				color=0x80000000;
			}
			else
			{
				btype=1;
				color=0x80000000;
			}
			RenderSubtitleTex1(i, j, btype, color);
		}
	}

	//RenderSubtitleTex1(-2, -2, 1, 0x80000000);
	//RenderSubtitleTex1( 2, -2, 1, 0x80000000);
	//RenderSubtitleTex1(-2,  2, 1, 0x80000000);
	//RenderSubtitleTex1( 2,  2, 1, 0x80000000);

	//RenderSubtitleTex1(-2, -1, 1, 0x80000000);
	//RenderSubtitleTex1( 1, -2, 1, 0x80000000);
	//RenderSubtitleTex1(-2,  1, 1, 0x80000000);
	//RenderSubtitleTex1( 1,  2, 1, 0x80000000);

	//RenderSubtitleTex1(-1, -2, 1, 0x80000000);
	//RenderSubtitleTex1( 2, -1, 1, 0x80000000);
	//RenderSubtitleTex1(-1,  2, 1, 0x80000000);
	//RenderSubtitleTex1( 2,  1, 1, 0x80000000);

	//RenderSubtitleTex1(-1,  1, 0, 0xff000000);
	//RenderSubtitleTex1( 1,  1, 0, 0xff000000);
	//RenderSubtitleTex1(-1, -1, 0, 0xff000000);
	//RenderSubtitleTex1( 1, -1, 0, 0xff000000);

	//RenderSubtitleTex1(-1,  0, 0, 0xff000000);
	//RenderSubtitleTex1( 1,  0, 0, 0xff000000);
	//RenderSubtitleTex1( 0, -1, 0, 0xff000000);
	//RenderSubtitleTex1( 0,  1, 0, 0xff000000);

	RenderSubtitleTex1(0, 0, 0, 0xffffffff);

	sceGuDisable(GU_ALPHA_TEST);

	return;
}

#endif//USE_FFMPEG_DIGEST
