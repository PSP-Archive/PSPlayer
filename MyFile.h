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

#ifndef __MYFILE_H__
#define __MYFILE_H__

struct MyURLContext
{
	int _FBUFSIZE;
	unsigned char *fbuf;
	unsigned char *fbuf2;
	unsigned long bstart_a;
	unsigned long bstart_b;
	unsigned long fcpos;
	unsigned long fsize;
	int waitfora;
	int waitforb;
	SceUID fd;
	int fdValid;

	void (*free_func)(void *ptr);
};

void InitMyURLContext(MyURLContext*ptr);

#ifndef offset_t

typedef int64_t offset_t;

#endif

#define ENOENT 2
#define EINVAL 22

#define URL_RDONLY 0
#define URL_WRONLY 1
#define URL_RDWR   2

int file_open(MyURLContext *h, const char *filename, int flags);
int file_read(MyURLContext *h, unsigned char *buf, int size);
offset_t file_seek(MyURLContext *h, offset_t pos, int whence);
offset_t file_tell(MyURLContext *h);
int file_close(MyURLContext *h);

#endif//__MYFILE_H__
