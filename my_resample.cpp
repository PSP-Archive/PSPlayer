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

# include "my_resample.h"

//inline void ClampCopyWithVol(stereo_sample *dest, const int &volShiftCount, register int l, register int r)
//{
//	if(volShiftCount<0)
//	{
//		l=l>>(-volShiftCount);
//		r=r>>(-volShiftCount);
//	}
//	else
//	{
//		l=l<<volShiftCount;
//		r=r<<volShiftCount;
//	}
//	dest->l=l>32767?32767:(l<-32767?-32767:l);
//	dest->r=r>32767?32767:(r<-32767?-32767:r);
//}
//
//inline void ClampCopyMonoWithVol(stereo_sample *dest, const int &volShiftCount, register int l)
//{
//	if(volShiftCount<0)
//	{
//		l=l>>(-volShiftCount);
//	}
//	else
//	{
//		l=l<<volShiftCount;
//	}
//	dest->r=dest->l=l>32767?32767:(l<-32767?-32767:l);
//}

inline void ClampCopy(stereo_sample *dest, register int l, register int r)
{
	dest->l=l>32767?32767:(l<-32767?-32767:l);
	dest->r=r>32767?32767:(r<-32767?-32767:r);
}

inline void ClampCopyMono(stereo_sample *dest, register int l)
{
	dest->r=dest->l=l>32767?32767:(l<-32767?-32767:l);
}

inline void ClampCopyWithVolFloat(stereo_sample *dest, register float volumeBoost, register int l, register int r)
{
	l *= volumeBoost;
	r *= volumeBoost;

	dest->l=l>32767?32767:(l<-32767?-32767:l);
	dest->r=r>32767?32767:(r<-32767?-32767:r);
}

inline void ClampCopyMonoWithVolFloat(stereo_sample *dest, register float volumeBoost, register int l)
{
	l *= volumeBoost;

	dest->r=dest->l=l>32767?32767:(l<-32767?-32767:l);
}

/*
inline int Interpolate(short a, short b, int f, int all)
{
	int aa=a, bb=b;
	aa*=all-f;
	bb*=f;

	return (aa+bb)/all;
}
*/

int convert_mono(my_resample_state *s, short *dd, short *src, int srcSize)
{
	int fract=0, intpart=0, cp=0;
	int wp=0;
	stereo_sample *dest=(stereo_sample*)dd;
	short *sl;

	if(srcSize==0)
		return 0;

	while(s->ptime<0)
	{
		ClampCopyMono(&dest[wp], (((int)(s->last.l)*(-s->ptime))+((int)(*src)*(s->iinc+s->ptime)))/s->iinc);;

		wp++;

		s->ptime+=s->oinc;
	}

	intpart=s->ptime/s->iinc;
	cp=fract=s->ptime%s->iinc;
	sl=src+intpart;
	while(1)
	{
		if( sl-src>=srcSize-1 )
			break;

		if(!fract)
		{
            dest[wp].r=dest[wp].l=*sl;
		}
		else
		{
			ClampCopyMono(&dest[wp],
				(((int)(*sl)*(s->iinc-fract))+((int)(*(sl+1))*fract))/s->iinc);
		}
		wp++;

		cp+=s->oinc;
		intpart=cp/s->iinc;
		fract=cp%s->iinc;
		if(intpart>=1)
		{
			sl+=intpart;
			cp=fract;
		}
	}

	s->ptime=fract+((int)(sl-src)-srcSize)*s->iinc;
	s->last.l=*(src+srcSize-1);

	return wp;
}

int convert_stereo(my_resample_state *s, short *dd, short *ss, int srcSize)
{
	int fract=0, intpart=0, cp=0;
	int wp=0;
	stereo_sample *dest=(stereo_sample*)dd;
	stereo_sample *src=(stereo_sample*)ss;
	stereo_sample *sl;

	if(srcSize==0)
		return 0;

	while(s->ptime<0)
	{
		ClampCopy(&dest[wp], 
			(((int)(s->last.l)*(-s->ptime))+((int)(src->l)*(s->iinc+s->ptime)))/s->iinc,
            (((int)(s->last.r)*(-s->ptime))+((int)(src->r)*(s->iinc+s->ptime)))/s->iinc );
/*		ClampCopy(&dest[wp],
			Interpolate(s->last.l, src->l, s->iinc+s->ptime, s->iinc),
			Interpolate(s->last.r, src->r, s->iinc+s->ptime, s->iinc) );*/

		wp++;

		s->ptime+=s->oinc;
	}

	intpart=s->ptime/s->iinc;
	cp=fract=s->ptime%s->iinc;
	sl=src+intpart;
	while(1)
	{
		if( sl-src>=srcSize-1 )
			break;

		if(!fract)
		{
            dest[wp].l=sl->l;
			dest[wp].r=sl->r;
		}
		else
		{
			ClampCopy(&dest[wp],
				(((int)(sl->l)*(s->iinc-fract))+((int)((sl+1)->l)*fract))/s->iinc,
				(((int)(sl->r)*(s->iinc-fract))+((int)((sl+1)->r)*fract))/s->iinc );
		}
/*		ClampCopy(&dest[wp],
			Interpolate(sl->l, (sl+1)->l, fract, s->iinc),
			Interpolate(sl->r, (sl+1)->r, fract, s->iinc) );*/

		wp++;

		cp+=s->oinc;
		intpart=cp/s->iinc;
		fract=cp%s->iinc;
		if(intpart>=1)
		{
			sl+=intpart;
			cp=fract;
		}
	}

	s->ptime=fract+((int)(sl-src)-srcSize)*s->iinc;
	s->last.l=(src+srcSize-1)->l;
	s->last.r=(src+srcSize-1)->r;

	return wp;
}

int convert_mono_vol(my_resample_state *s, short *dd, short *src, int srcSize)
{
	int fract=0, intpart=0, cp=0;
	int wp=0;
	stereo_sample *dest=(stereo_sample*)dd;
	short *sl;

	if(srcSize==0)
		return 0;

	while(s->ptime<0)
	{
		ClampCopyMonoWithVolFloat(&dest[wp], s->volumeBoost, (((int)(s->last.l)*(-s->ptime))+((int)(*src)*(s->iinc+s->ptime)))/s->iinc);;
		//ClampCopyMonoWithVol(&dest[wp], s->volShiftCount, (((int)(s->last.l)*(-s->ptime))+((int)(*src)*(s->iinc+s->ptime)))/s->iinc);;

		wp++;

		s->ptime+=s->oinc;
	}

	intpart=s->ptime/s->iinc;
	cp=fract=s->ptime%s->iinc;
	sl=src+intpart;
	while(1)
	{
		if( sl-src>=srcSize-1 )
			break;

		if(!fract)
		{
			ClampCopyMonoWithVolFloat(&dest[wp], s->volumeBoost, *sl);
//			ClampCopyMonoWithVol(&dest[wp], s->volShiftCount, *sl);
		}
		else
		{
			ClampCopyMonoWithVolFloat(&dest[wp], s->volumeBoost, (((int)(*sl)*(s->iinc-fract))+((int)(*(sl+1))*fract))/s->iinc);
			//ClampCopyMonoWithVol(&dest[wp], s->volShiftCount, (((int)(*sl)*(s->iinc-fract))+((int)(*(sl+1))*fract))/s->iinc);
		}
		wp++;

		cp+=s->oinc;
		intpart=cp/s->iinc;
		fract=cp%s->iinc;
		if(intpart>=1)
		{
			sl+=intpart;
			cp=fract;
		}
	}

	s->ptime=fract+((int)(sl-src)-srcSize)*s->iinc;
	s->last.l=*(src+srcSize-1);

	return wp;
}

int convert_stereo_vol(my_resample_state *s, short *dd, short *ss, int srcSize)
{
	int fract=0, intpart=0, cp=0;
	int wp=0;
	stereo_sample *dest=(stereo_sample*)dd;
	stereo_sample *src=(stereo_sample*)ss;
	stereo_sample *sl;

	if(srcSize==0)
		return 0;

	while(s->ptime<0)
	{
		ClampCopyWithVolFloat(&dest[wp], s->volumeBoost, 
			(((int)(s->last.l)*(-s->ptime))+((int)(src->l)*(s->iinc+s->ptime)))/s->iinc,
            (((int)(s->last.r)*(-s->ptime))+((int)(src->r)*(s->iinc+s->ptime)))/s->iinc );
		//ClampCopyWithVol(&dest[wp], s->volShiftCount, 
		//	(((int)(s->last.l)*(-s->ptime))+((int)(src->l)*(s->iinc+s->ptime)))/s->iinc,
  //          (((int)(s->last.r)*(-s->ptime))+((int)(src->r)*(s->iinc+s->ptime)))/s->iinc );

		wp++;

		s->ptime+=s->oinc;
	}

	intpart=s->ptime/s->iinc;
	cp=fract=s->ptime%s->iinc;
	sl=src+intpart;
	while(1)
	{
		if( sl-src>=srcSize-1 )
			break;

		if(!fract)
		{
			ClampCopyWithVolFloat(&dest[wp], s->volumeBoost, sl->l, sl->r);
			//ClampCopyWithVol(&dest[wp], s->volShiftCount, sl->l, sl->r);
		}
		else
		{
			ClampCopyWithVolFloat(&dest[wp], s->volumeBoost,
				(((int)(sl->l)*(s->iinc-fract))+((int)((sl+1)->l)*fract))/s->iinc,
				(((int)(sl->r)*(s->iinc-fract))+((int)((sl+1)->r)*fract))/s->iinc );
			//ClampCopyWithVol(&dest[wp], s->volShiftCount,
			//	(((int)(sl->l)*(s->iinc-fract))+((int)((sl+1)->l)*fract))/s->iinc,
			//	(((int)(sl->r)*(s->iinc-fract))+((int)((sl+1)->r)*fract))/s->iinc );
		}

		wp++;

		cp+=s->oinc;
		intpart=cp/s->iinc;
		fract=cp%s->iinc;
		if(intpart>=1)
		{
			sl+=intpart;
			cp=fract;
		}
	}

	s->ptime=fract+((int)(sl-src)-srcSize)*s->iinc;
	s->last.l=(src+srcSize-1)->l;
	s->last.r=(src+srcSize-1)->r;

	return wp;
}

float my_resample_n_sample_expand_factor(my_resample_state *s)
{
	float samplebase;

	samplebase=44100.0f/s->irate;

	return samplebase;
}

int my_resample_init(my_resample_state *s, int destRate, int srcRate, int srcChannels)
{
	s->irate=srcRate;
	s->orate=destRate;
	s->iinc=14112000/s->irate;
	s->oinc=14112000/s->orate;
	s->last.l=s->last.r=0;
	s->ptime=0;
	s->srcChannels=srcChannels;
	//s->volShiftCount=0;
	s->volumeBoost=1.0f;

	s->inited=1;
	return 1;
}

void my_resample_close(my_resample_state *s)
{
	s->inited=0;
}

int my_resample(struct _resample_state *s, short *dest, short *src, int srcSize)
{
	if(s->irate==44100 && s->srcChannels==2)
	{
		if(s->volumeBoost!=1.0f)
//		if(s->volShiftCount)
		{
			return convert_stereo_vol(s, dest, src, srcSize);
		}
		else
		{
			return -srcSize;
		}
	}
	else
	{
		if(s->volumeBoost!=1.0f)
//		if(s->volShiftCount)
		{
			if(s->srcChannels==2)
			{
				return convert_stereo_vol(s, dest, src, srcSize);
			}
			else if(s->srcChannels==1)
			{
				return convert_mono_vol(s, dest, src, srcSize);
			}
		}
		else
		{
			if(s->srcChannels==2)
			{
				return convert_stereo(s, dest, src, srcSize);
			}
			else if(s->srcChannels==1)
			{
				return convert_mono(s, dest, src, srcSize);
			}
		}
	}
}

int my_resample_dest_samplecount(struct _resample_state *s, short *dd, int destSize, short *ss, int *sSize)
{
/*	memset(dd, 0, destSize*4);
	*sSize=destSize/my_resample_n_sample_expand_factor(s);

	return destSize;*/
	int srcSize=*sSize;
	int fract=0, intpart=0, cp=0;
	int wp=0;
	stereo_sample *dest=(stereo_sample*)dd;
	stereo_sample *src=(stereo_sample*)ss;
	stereo_sample *sl;

	if(srcSize==0)
		return 0;

	while(s->ptime<0)
	{
		ClampCopy(&dest[wp], 
			(((int)(s->last.l)*(-s->ptime))+((int)(src->l)*(s->iinc+s->ptime)))/s->iinc,
            (((int)(s->last.r)*(-s->ptime))+((int)(src->r)*(s->iinc+s->ptime)))/s->iinc );

		wp++;

		s->ptime+=s->oinc;
	}

	intpart=s->ptime/s->iinc;
	cp=fract=s->ptime%s->iinc;
	sl=src+intpart;
	while(1)
	{
		if( sl-src>=srcSize-1 )
			break;

		if(!fract)
		{
            dest[wp].l=sl->l;
			dest[wp].r=sl->r;
		}
		else
		{
			ClampCopy(&dest[wp],
				(((int)(sl->l)*(s->iinc-fract))+((int)((sl+1)->l)*fract))/s->iinc,
				(((int)(sl->r)*(s->iinc-fract))+((int)((sl+1)->r)*fract))/s->iinc );
		}

		cp+=s->oinc;
		intpart=cp/s->iinc;
		fract=cp%s->iinc;
		if(intpart>=1)
		{
			sl+=intpart;
			cp=fract;
		}

		wp++;

		if(destSize<=wp)
			break;
	}

	*sSize=srcSize=(int)(sl-src+1);

	s->ptime=fract+((int)(sl-src)-srcSize)*s->iinc;
	s->last.l=(src+srcSize-1)->l;
	s->last.r=(src+srcSize-1)->r;

	return wp;
}

