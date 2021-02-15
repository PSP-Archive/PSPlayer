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

#ifndef __MY_UTIL_H__
#define __MY_UTIL_H__

#define printf	pspDebugScreenPrintf

inline int IsMatchExt(const char *fileName, const char *ext)
{
	char *point=strrchr(fileName, '.');

	if(point==NULL)
		return 0;

	point++;

	return (stricmp(point, ext)==0);

	return 1;
}

inline void BuildPath(char *path, const char *basePath, const char *fileName)
{
	snprintf(path, STRING_LENGTH, "%s/%s", basePath, fileName);
}

#endif//__MY_UTIL_H__
