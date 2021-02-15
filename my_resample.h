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

# ifndef __MY_RESAMPLE_H__
# define __MY_RESAMPLE_H__

typedef struct _stereo_sample
{
	short l, r;
} stereo_sample;

typedef struct _resample_state
{
	int inited;
	int irate;
	int orate;
	int iinc;
	int oinc;
	int srcChannels;

	float volumeBoost;
	//int volShiftCount;

	int ptime;
	stereo_sample last;
} my_resample_state;

#ifdef __cplusplus
//extern "C" {
#endif

int my_resample_init(my_resample_state *s, int destRate, int srcRate, int srcChannels);
void my_resample_close(my_resample_state *s);
float my_resample_n_sample_expand_factor(my_resample_state *s);
int my_resample(my_resample_state *s, short *dest, short *src, int srcSize);

int my_resample_dest_samplecount(my_resample_state *s, short *dest, int destSize, short *src, int *srcSize);

#ifdef __cplusplus
//};
#endif

# endif//__MY_RESAMPLE_H__
