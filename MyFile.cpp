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

#include <pspge.h>
#include <pspgu.h>
#include <ctype.h>

#include <time.h>

#include <errno.h>

#include "MyFile.h"

static int _strstart(const char *str, const char *val, const char **ptr)
{
    const char *p, *q;
    p = str;
    q = val;
    while (*q != '\0') {
        if (*p != *q)
            return 0;
        p++;
        q++;
    }
    if (ptr)
        *ptr = p;
    return 1;
}

void InitMyURLContext(MyURLContext *ptr)
{
	memset(ptr, 0, sizeof(MyURLContext));

	ptr->fd=-1;
}

//#define h->_FBUFSIZE (64*1024)
//#define h->_FBUFSIZE (2000000)
//#define h->_FBUFSIZE (128*1024)
////#define h->_FBUFSIZE (1*1024*1024)
////#define h->_FBUFSIZE (2*1024*1024)
//#define TOTALSIZE (2*h->_FBUFSIZE)
//static unsigned char __attribute__((aligned(64))) fbuf[TOTALSIZE];
//static unsigned char *fbuf2=fbuf+h->_FBUFSIZE;
//unsigned long bstart_a=0;
//unsigned long bstart_b=0;
//unsigned long fcpos=0;
//unsigned long fsize=0;
//int waitfora=0, waitforb=0;
enum AsyncWaitStat { AWS_NOWAIT=1, AWS_WAIT };
void LoadBlockA(MyURLContext *h, unsigned long filepos, AsyncWaitStat waitforcompletion)
{
	SceUID fd=h->fd;
	if(fd<0)
		return;

	SceInt64 asyncres=0;

	if(sceIoLseek(fd, filepos, SEEK_SET)<0)
	{
		h->fdValid=0;
	}

	if( sceIoReadAsync(fd, h->fbuf, h->_FBUFSIZE) <0 )
	{
		h->fdValid=0;
	}

	if(waitforcompletion==AWS_WAIT)
	{
		while( sceIoWaitAsync(fd, &asyncres)<0 /*|| asyncres<=0*/) ;

		h->waitfora=0;
	}
	else
	{
		h->waitfora=1;
	}
}

void LoadBlockB(MyURLContext *h, unsigned long filepos, AsyncWaitStat waitforcompletion)
{
	SceUID fd=h->fd;
	if(fd<0)
		return;

	SceInt64 asyncres=0;

	if( sceIoLseek(fd, filepos, SEEK_SET)<0 )
	{
		h->fdValid=0;
	}

	if( sceIoReadAsync(fd, h->fbuf2, h->_FBUFSIZE)<0 )
	{
		h->fdValid=0;
	}

	if(waitforcompletion==AWS_WAIT)
	{
		while( sceIoWaitAsync(fd, &asyncres)<0 /*|| asyncres<=0*/) ;

		h->waitforb=0;
	}
	else
	{
		h->waitforb=1;
	}
}

int file_open(MyURLContext *h, const char *filename, int flags)
{
	int res;
	SceUID fd;
	SceInt64 asyncres=0;
	SceIoStat stat;

	if (flags & (URL_RDWR | URL_WRONLY) )
		return -ENOENT;

	_strstart(filename, "file:", &filename);

	res=sceIoGetstat(filename, &stat);
	if(res<0)
		return -ENOENT;

	h->fsize=stat.st_size;

    fd = sceIoOpenAsync(filename, IOASSIGN_RDONLY, 0777);
    if (fd < 0)
        return -ENOENT;

	if(sceIoWaitAsync(fd, &asyncres) || asyncres<=0)
	{
		return -ENOENT;
	}

	if( strnicmp(filename, "nethost0:", 9)==0 )
	{
		sceIoChangeAsyncPriority(fd, 0x28);
	}
	else
	{
		sceIoChangeAsyncPriority(fd, 0x28);
	}

	h->fd = fd;

	//if( !h->fbuf && !h->fbuf2)
	//{
	//	h->fbuf=(unsigned char*)malloc(TOTALSIZE);
	//	h->fbuf2=h->fbuf+h->_FBUFSIZE;
	//}

	h->fcpos=0;
	h->bstart_a=0;
	h->bstart_b=h->_FBUFSIZE;

	h->fdValid=1;

	LoadBlockA(h, h->bstart_a, AWS_WAIT);
	LoadBlockB(h, h->bstart_b, AWS_NOWAIT);

	return 0;
}

int file_read(MyURLContext *h, unsigned char *buf, int size)
{
	SceUID fd=h->fd;
	if(fd<0 || !h->fdValid)
		return 0;

	SceInt64 asyncres=0;

	int readsize=0;

	if(h->fcpos>=h->fsize)
		return 0;

	if(h->fsize-h->fcpos<size)
		size=h->fsize-h->fcpos;

	if(h->bstart_a<=h->fcpos && h->fcpos<h->bstart_a+h->_FBUFSIZE)
	{
		if(h->waitfora)
		{
			h->waitfora=0;
			if( sceIoWaitAsync(fd, &asyncres)<0 /*|| asyncres==0*/)
			{
				LoadBlockA(h, h->bstart_a, AWS_WAIT);
			}
		}
		readsize=h->_FBUFSIZE-(h->fcpos-h->bstart_a); if(size<readsize) readsize=size;

		memcpy(buf, h->fbuf+(h->fcpos-h->bstart_a), readsize);
		h->fcpos+=readsize;

		if(h->bstart_a+h->_FBUFSIZE!=h->bstart_b && h->bstart_a+h->_FBUFSIZE/2<=h->fcpos)
		{
			h->bstart_b=h->bstart_a+h->_FBUFSIZE;
			LoadBlockB(h, h->bstart_b, AWS_NOWAIT);
		}
	}
	else if(h->bstart_b<=h->fcpos && h->fcpos<h->bstart_b+h->_FBUFSIZE)
	{
		if(h->waitforb)
		{
			h->waitforb=0;
			if( sceIoWaitAsync(fd, &asyncres)<0 /*|| asyncres==0*/)
			{
				LoadBlockB(h, h->bstart_b, AWS_WAIT);
			}
		}
		readsize=h->_FBUFSIZE-(h->fcpos-h->bstart_b); if(size<readsize) readsize=size;

		memcpy(buf, h->fbuf2+(h->fcpos-h->bstart_b), readsize);
		h->fcpos+=readsize;

		if(h->bstart_b+h->_FBUFSIZE!=h->bstart_a && h->bstart_b+h->_FBUFSIZE/2<=h->fcpos)
		{
			h->bstart_a=h->bstart_b+h->_FBUFSIZE;
			LoadBlockA(h, h->bstart_a, AWS_NOWAIT);
		}
	}

	if(readsize<size)
	{
		readsize+=file_read(h, buf+readsize, size-readsize);
	}

	return readsize;
}

int file_write(MyURLContext *h, unsigned char *buf, int size)
{
	// no write operation...
	return -1;
}

/* XXX: use llseek */
offset_t file_seek(MyURLContext *h, offset_t pos, int whence)
{
	SceUID fd=h->fd;
	if(fd<0)
	{
		errno=EINVAL;
		return -1L;
	}

	SceInt64 asyncres=0;
	offset_t newpos=-1;

	if(whence==SEEK_CUR)
	{
		newpos+=pos;
	}
	else if(whence==SEEK_END)
	{
		newpos=h->fsize+pos;
	}
	else if(whence==SEEK_SET)
	{
		newpos=pos;
	}
	if(newpos<0)
	{
		errno=EINVAL;
		return (offset_t)-1;
	}
	if(newpos>h->fsize)
	{
		errno=EINVAL;
		return (offset_t)-1;
	}
	h->fcpos=(unsigned long)newpos;

	if(h->bstart_a<=h->fcpos && h->fcpos<h->bstart_a+h->_FBUFSIZE)
	{
		if(h->waitfora)
		{
			h->waitfora=0;
			if( sceIoWaitAsync(fd, &asyncres)<0 /*|| asyncres==0*/)
			{
				LoadBlockA(h, h->bstart_a, AWS_WAIT);
			}
		}
	}
	else if(h->bstart_b<=h->fcpos && h->fcpos<h->bstart_b+h->_FBUFSIZE)
	{
		if(h->waitforb)
		{
			h->waitforb=0;
			if( sceIoWaitAsync(fd, &asyncres)<0 /*|| asyncres==0*/)
			{
				LoadBlockB(h, h->bstart_b, AWS_WAIT);
			}
		}
	}
	else
	{
		if(h->waitfora || h->waitforb)
		{
			sceIoWaitAsync(fd, &asyncres);

			h->waitfora=h->waitforb=0;
		}

//		h->bstart_a=h->fcpos & ~(h->_FBUFSIZE-1);
		h->bstart_a=h->fcpos/h->_FBUFSIZE*h->_FBUFSIZE;
		LoadBlockA(h, h->bstart_a, AWS_WAIT);

		h->bstart_b=-1;
	}

	return h->fcpos;
}

offset_t file_tell(MyURLContext *h)
{
	SceUID fd=h->fd;
	if(fd<0)
	{
		errno=EINVAL;
		return -1L;
	}

	return h->fcpos;
}

int file_close(MyURLContext *h)
{
	SceUID fd=h->fd;

	SceInt64 asyncres=-1;
	if( !(fd<0) )
	{
		sceIoWaitAsync(fd, &asyncres);
		
		sceIoCloseAsync(fd);
		sceIoWaitAsync(fd, &asyncres);
	}

	if( h->free_func!=NULL )
		(*h->free_func)(h->fbuf);

	InitMyURLContext(h);

	return (int)asyncres;
}

//#if 0
#ifdef USE_FFMPEG_DIGEST

extern "C" {

#include "avformat.h"

};

extern int count1, count2, count3;
extern char errorString[];
int my_ffmpeg_file_open(URLContext *h, const char *filename, int flags)
{
	h->priv_data=av_malloc(sizeof(MyURLContext));
	MyURLContext *muc=(MyURLContext *)h->priv_data;

	InitMyURLContext(muc);

	muc->_FBUFSIZE=128*1024;

	if( strnicmp(filename, "nethost0:", 9)==0 )
	{
		muc->_FBUFSIZE=1024*1024;
	}

	muc->fbuf=(unsigned char *)av_malloc(muc->_FBUFSIZE*2);
	muc->fbuf2=muc->fbuf+muc->_FBUFSIZE;

	muc->free_func=av_free;

	int ret=file_open((MyURLContext*)h->priv_data, filename, flags);
	if(ret<0)
	{
		av_free(h->priv_data);
		h->priv_data=0;
	}

	return ret;
}

int my_ffmpeg_file_read(URLContext *h, unsigned char *buf, int size)
{
	return file_read((MyURLContext*)h->priv_data, buf, size);
}
int my_ffmpeg_file_write(URLContext *h, unsigned char *buf, int size)
{
	return -1;
}
offset_t my_ffmpeg_file_seek(URLContext *h, offset_t pos, int whence)
{
	return file_seek((MyURLContext*)h->priv_data, pos, whence);
}
int my_ffmpeg_file_close(URLContext *h)
{
	int ret=file_close((MyURLContext*)h->priv_data);

	av_free(h->priv_data);

	return ret;
}

URLProtocol file_protocol = {
	"file",
	my_ffmpeg_file_open,
	my_ffmpeg_file_read,
	my_ffmpeg_file_write,
	my_ffmpeg_file_seek,
	my_ffmpeg_file_close,
};

#endif
