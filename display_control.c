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

#include <pspsdk.h>
#include <pspkernel.h>

int getBrightness();
void setBrightness(int brightness);
int displayEnable(void);
int displayDisable(void);

static int lcdBrightness=24;
static int displayDisabled=0;

int GetLCDBrightness() { return lcdBrightness; }
int IsDisplayDisabled() { return displayDisabled; }

void psplEnableDisplay()
{
	displayDisabled=0;

	setBrightness(lcdBrightness);
	sceKernelDelayThread(1000*3);

	displayEnable();
}

void psplDisableDisplay()
{
	displayDisabled=1;

	lcdBrightness=getBrightness();
	sceKernelDelayThread(1000*3);

	setBrightness(0);
	displayDisable();
}
