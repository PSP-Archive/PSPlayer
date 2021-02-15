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

#ifndef __STRING_H__
#define __STRING_H__

#include <stdlib.h>

// no malloc, string class immitation.

#define STRING_LENGTH 256

namespace std
{
	class string
	{
	public:
		char str[STRING_LENGTH];

		string() { str[0]=0; }
		string(const char *s) {strncpy(str, s, STRING_LENGTH); str[STRING_LENGTH]=0;}
		string &operator=(const string &s)
		{
			strncpy(str, s.str, STRING_LENGTH);
			str[STRING_LENGTH]=0;
			return *this;
		}
		string &operator=(const char *s)
		{
			strncpy(str, s, STRING_LENGTH);
			str[STRING_LENGTH]=0;
			return *this;
		}
		string &append(const string &s)
		{
			int len=strlen(str);
			strncpy(str+len, s.str, STRING_LENGTH-len);
			str[STRING_LENGTH]=0;
			return *this;
		}
		string &append(const char *s)
		{
			int len=strlen(str);
			strncpy(str+len, s, STRING_LENGTH-len);
			str[STRING_LENGTH]=0;
			return *this;
		}
/*		string &operator+(const string &s)
		{
			int len=strlen(str);
			strncpy(str+len, s.str, STRING_LENGTH-len);
			str[STRING_LENGTH]=0;
			return *this;
		}
		string &operator+(const char *s)
		{
			int len=strlen(str);
			strncpy(str+len, s, STRING_LENGTH-len);
			str[STRING_LENGTH]=0;
			return *this;
		}*/
		const char *c_str() { return str; }
		int len() { return strlen(str); }
	};
};

#endif//__STRING_H__
