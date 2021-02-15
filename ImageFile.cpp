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
#include <pspgu.h>
#include <pspge.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>

extern "C"
{
#include "jpeglib.h"
#include "png.h"
#include "gif_lib.h"
}

#include "setjmp.h"

#include "ImageFile.h"

ImageFile ImageFile::bgImage1;
ImageFile ImageFile::bgImage2;
ImageFile ImageFile::bgImage3;
ImageFile ImageFile::bgImage4;
//unsigned char __attribute__((aligned(16))) ImageFile::bgImageBuffer[IBSIZE];

#pragma pack(1)
struct SimpleBitmapHeader
{
	 char skip1[18];
	 int sizeX;
	 int sizeY;
	 char skip2[2];
	 short bitCount;
	 int compression;
	 int size;
	 char skip[16];
};
#pragma pack()

ImageFile::ImageFile()
{
	blockid=0;
	imageData=NULL;
}
ImageFile::~ImageFile()
{
	Reset();
}

void ImageFile::Reset()
{
	sizeX=sizeY=bufferWidth=0;

	FreeImageBuffer();
}

unsigned char *ImageFile::AllocImageBuffer()
{
	if(blockid || imageData)
	{
		Reset();
	}

	int y=sizeY; if(y<272) y=272;
	int allocSize=bufferWidth*y*4;

	blockid=sceKernelAllocPartitionMemory(2, "im_block", PSP_SMEM_Low, allocSize, NULL);
	if (blockid > 0)
	{
		imageData = (unsigned char *)sceKernelGetBlockHeadAddr(blockid);
	}
	else
	{
		imageData=(unsigned char *)malloc(allocSize);
	}

	if(imageData)
	{
		memset(imageData, 0, allocSize);
	}

	return imageData;
}

void ImageFile::FreeImageBuffer()
{
	if(blockid>0)
	{
		sceKernelFreePartitionMemory(blockid);

		blockid=0;
		imageData=NULL;
	}
	else if(imageData)
	{
		free(imageData);
		imageData=NULL;
	}
}

int GetImageTypeIndex(const char *fileName)
{
	if(IsMatchExt(fileName, "bmp"))
	{
		return 1;
	}
	else if(IsMatchExt(fileName, "jpg"))
	{
		return 2;
	}
	else if(IsMatchExt(fileName, "jpeg"))
	{
		return 2;
	}
	else if(IsMatchExt(fileName, "png"))
	{
		return 3;
	}
	else if(IsMatchExt(fileName, "gif"))
	{
		return 4;
	}
	else
	{
		return 0;
	}
	
}
void PrintErrMsgWait(const char *msg);

int ImageFile::LoadImage(const char *fileName)
{
	int itype=GetImageTypeIndex(fileName);
	if(itype==0)
		return 0;

	Reset();

	int ret=1;
	switch(itype)
	{
	case 1:
		ret=LoadBMPFile(fileName);
		break;
	case 2:
		ret=LoadJPGFile(fileName);
		break;
	case 3:
		ret=LoadPNGFile(fileName);
		break;
	case 4:
		ret=LoadGIFFile(fileName);
		break;
	default:
		ret=0;
		break;
	}

	if(ret==0)
	{
		char tmpbuf[256];
		sprintf(tmpbuf, "Error image loading... %s", fileName);
		//PrintErrMsgWait(tmpbuf);
		Reset();
	}

	return ret;
}

inline int GetBufferWidth(int x)
{
	if(x<=512)
		return 512;
	else if(x<=1024)
		return 1024;
	else if(x<=1536)
		return 1536;
	else if(x<=2048)
		return 2048;
	return 204800;
}

void ImageFile::SetImageSize(int x, int y)
{
	sizeX=x;
	sizeY=y;
	bufferWidth=GetBufferWidth(sizeX);
}

int ImageFile::LoadBMPFile(const char *fileName)
{
	sizeX=sizeY=0;
	imageType=GU_PSM_8888;

	int pixelSize=3;

	SimpleBitmapHeader header;

	int file = sceIoOpen(fileName, PSP_O_RDONLY, 0777);

	if(file<0)
		return 0;
	
	sceIoRead(file, &header, 54);

	if(header.bitCount!=24)
	{
		sceIoClose(file);
		return 0;
	}

	SetImageSize(header.sizeX, (header.sizeY>0)?header.sizeY:-header.sizeY);

	if(AllocImageBuffer()==NULL)
	{
		sceIoClose(file);
		return 0;
	}

	unsigned char *loadBuf=(unsigned char *)malloc(sizeX*3);
	if(header.sizeY>0)
	{
		unsigned char *dest, *src;
		unsigned long color=0xff000000, r=0, g=0, b=0xff;
		for(int i=0; i<sizeY; i++)
		{
			sceIoRead(file, loadBuf, sizeX*3);
			src=loadBuf;
			for(int ig=0; ig<bufferWidth/512; ig++)
			{
				dest=imageData+ig*4*512*sizeY+4*(sizeY-i-1)*512;
				for(int x=ig*512; x<ig*512+512; x++)
				{
					color=0xff000000;
					if(x>=sizeX)
					{
						*(unsigned long*)dest=color;
						dest+=4;
						continue;
					}
					color=0xff000000;
					b=*src++;
					g=*src++;
					r=*src++;

					color=color|(b<<16)|(g<<8)|r;
					*(unsigned long*)dest=color;
					dest+=4;
				}
			}
		}
	}
	else
	{
		unsigned char *dest, *src;
		unsigned long color=0xff000000, r=0, g=0, b=0xff;
		for(int i=0; i<sizeY; i++)
		{
			sceIoRead(file, loadBuf, sizeX*3);
			src=loadBuf;
			for(int ig=0; ig<bufferWidth/512; ig++)
			{
				dest=imageData+ig*4*512*sizeY+4*i*512;
				for(int x=ig*512; x<ig*512+512; x++)
				{
					color=0xff000000;
					if(x>=sizeX)
					{
						*(unsigned long*)dest=color;
						dest+=4;
						continue;
					}
					color=0xff000000;
					b=*src++;
					g=*src++;
					r=*src++;

					color=color|(b<<16)|(g<<8)|r;
					*(unsigned long*)dest=color;
					dest+=4;
				}
			}
		}
	}
	free(loadBuf);

/*	int pitch=sizeX*2;
	unsigned char *base=imageData+(sizeY-1)*pitch, *dest;
	unsigned short color, r, g, b;
	unsigned char *src;
	for(int i=0; i<sizeY; i++)
	{
		dest=base;
		sceIoRead(file, loadBuf, sizeX*3);
		src=loadBuf;
		for(int x=0; x<sizeX; x++)
		{
			b=(*src++)>>3;
			g=(*src++)>>2;
			r=(*src++)>>3;

			color=(b<<11)|(g<<5)|r;
			memcpy(dest, &color,2);
			dest+=2;
		}
		base-=pitch;
	}
*/
	sceIoClose(file);

	return 1;
}

struct my_error_mgr {
  struct jpeg_error_mgr pub;	/* "public" fields */

  jmp_buf setjmp_buffer;	/* for return to caller */
};
typedef struct my_error_mgr * my_error_ptr;

/*
 * Here's the routine that will replace the standard error_exit method:
 */

//char kkkk[JMSG_LENGTH_MAX]="";
int xxxxx=0;
void//METHODDEF(void)
my_error_exit (j_common_ptr cinfo)
{
	/* cinfo->err really points to a my_error_mgr struct, so coerce pointer */
	my_error_ptr myerr = (my_error_ptr) cinfo->err;

	/* Always display the message. */
	/* We could postpone this until after returning, if we chose. */
	//(*cinfo->err->output_message) (cinfo);

	//xxxxx+=1024;
	//sprintf(kkkk, "%08x ", xxxxx);
	//(*cinfo->err->format_message) (cinfo, kkkk+9);

	/* Return control to the setjmp point */
	longjmp(myerr->setjmp_buffer, 1);
}

int ImageFile::LoadJPGFile(const char *fileName)
{
	int memThreshold;

	sizeX=sizeY=0;
	imageType=GU_PSM_8888;

	//kkkk[0]=0;

	/* This struct contains the JPEG decompression parameters and pointers to
	* working space (which is allocated as needed by the JPEG library).
	*/
	struct jpeg_decompress_struct cinfo;
	/* We use our private extension JPEG error handler.
	* Note that this struct must live as long as the main JPEG parameter
	* struct, to avoid dangling-pointer problems.
	*/
	struct my_error_mgr jerr;
	/* More stuff */
	FILE * infile;		/* source file */
	JSAMPARRAY buffer;		/* Output row buffer */
	int row_stride;		/* physical row width in output buffer */

	/* In this example we want to open the input file before doing anything else,
	* so that the setjmp() error recovery below can assume the file is open.
	* VERY IMPORTANT: use "b" option to fopen() if you are on a machine that
	* requires it in order to read binary files.
	*/

	if ((infile = fopen(fileName, "rb")) == NULL)
	{
//		fprintf(stderr, "can't open %s\n", fileName);
		return 0;
	}

	/* Step 1: allocate and initialize JPEG decompression object */

	/* We set up the normal JPEG error routines, then override error_exit. */
	cinfo.err = jpeg_std_error(&jerr.pub);
	jerr.pub.error_exit = my_error_exit;
	/* Establish the setjmp return context for my_error_exit to use. */
	if (setjmp(jerr.setjmp_buffer)) {
		/* If we get here, the JPEG code has signaled an error.
		* We need to clean up the JPEG object, close the input file, and return.
		*/
		jpeg_destroy_decompress(&cinfo);
		fclose(infile);
		return 0;
	}
	/* Now we can initialize the JPEG decompression object. */
	jpeg_create_decompress(&cinfo);

	xxxxx=1;
	/* Step 2: specify data source (eg, a file) */

	jpeg_stdio_src(&cinfo, infile);

	/* Step 3: read file parameters with jpeg_read_header() */

	(void) jpeg_read_header(&cinfo, TRUE);
	/* We can ignore the return value from jpeg_read_header since
	*   (a) suspension is not possible with the stdio data source, and
	*   (b) we passed TRUE to reject a tables-only JPEG file as an error.
	* See libjpeg.doc for more info.
	*/

	xxxxx=2;

	/* Step 4: set parameters for decompression */

	/* In this example, we don't need to change any of the defaults set by
	* jpeg_read_header(), so we do nothing here.
	*/

	//int pfactor=0;
	//if(jpeg_has_multiple_scans(&cinfo))
	//{
	//	cinfo.buffered_image = TRUE;
	//	cinfo.do_block_smoothing=FALSE;

	//	pfactor=3;
	//}

	//int count=0;
	//int freemem=sceKernelMaxFreeMemSize();
	//cinfo.scale_denom=1;
	//while(count<3)
	//{
	//	if((cinfo.image_width*cinfo.image_height*pfactor+
	//		cinfo.image_width*cinfo.image_height*4/cinfo.scale_denom/cinfo.scale_denom)<freemem)
	//		break;
	//	count++;
	//	cinfo.scale_denom*=2;
	//}
	//if(count==3)
	//{
	//	jpeg_destroy_decompress(&cinfo);
	//	fclose(infile);

	//	return 0;
	//}

	int longside=cinfo.image_width>cinfo.image_height?cinfo.image_width:cinfo.image_height;

	if(longside<1536)
	{
		cinfo.scale_denom=1;
	}
	else if(longside<2048)
	{
		cinfo.scale_denom=2;
	}
	else if(longside<4096)
	{
		cinfo.scale_denom=4;
	}
	else
	{
		jpeg_destroy_decompress(&cinfo);
		fclose(infile);

		return 0;
	}

	fprintf(stderr, "jpg scale down factor %d\n", cinfo.scale_denom);

	cinfo.out_color_space=JCS_RGB;

	//cinfo.scale_denom=2;
	xxxxx=4;

	/* Step 5: Start decompressor */

	(void) jpeg_start_decompress(&cinfo);
	/* We can ignore the return value since suspension is not possible
	* with the stdio data source.
	*/

	xxxxx=5;

	SetImageSize(cinfo.output_width, cinfo.output_height);
	AllocImageBuffer();
	if(cinfo.output_components!=3 || imageData==NULL)
	{
		jpeg_destroy_decompress(&cinfo);
		fclose(infile);

		return 0;
	}

	/* We may need to do some setup of our own at this point before reading
	* the data.  After jpeg_start_decompress() we have the correct scaled
	* output image dimensions available, as well as the output colormap
	* if we asked for color quantization.
	* In this example, we need to make an output work buffer of the right size.
	*/ 
	/* JSAMPLEs per row in output buffer */
	row_stride = cinfo.output_width * cinfo.output_components;
	/* Make a one-row-high sample array that will go away when done with image */
	buffer = (*cinfo.mem->alloc_sarray)
		((j_common_ptr) &cinfo, JPOOL_IMAGE, row_stride, 1);

	xxxxx=10;
	do
	{
		/* Step 6: while (scan lines remain to be read) */
		/*           jpeg_read_scanlines(...); */

		/* Here we use the library's state variable cinfo.output_scanline as the
		* loop counter, so that we don't have to keep track ourselves.
		*/

		if(jpeg_has_multiple_scans(&cinfo))
		{
			jpeg_start_output(&cinfo, cinfo.input_scan_number);
		}

		xxxxx++;
		unsigned char *dest;
		unsigned long color=0xff000000, r=0, g=0, b=0xff, scan;
		while (cinfo.output_scanline < cinfo.output_height)
		{
			/* jpeg_read_scanlines expects an array of pointers to scanlines.
			* Here the array is only one element long, but you could ask for
			* more than one scanline at a time if that's more convenient.
			*/
			scan=cinfo.output_scanline;
			(void) jpeg_read_scanlines(&cinfo, buffer, 1);
			const JSAMPLE *src=*buffer;
			/* Assume put_scanline_someplace wants a pointer and sample count. */
			for(int ig=0; ig<bufferWidth/512; ig++)
			{
				dest=imageData+ig*4*512*cinfo.output_height+4*scan*512;
				for(int x=ig*512; x<ig*512+512; x++)
				{
					color=0xff000000;
					if(x>=sizeX)
					{
						*(unsigned long*)dest=color;
						dest+=4;
						continue;
					}
					color=0xff000000;
					r=*src++;
					g=*src++;
					b=*src++;

					color=color|(b<<16)|(g<<8)|r;
					*(unsigned long*)dest=color;
					dest+=4;
				}
			}
//			put_scanline_someplace(buffer[0], row_stride);
		}

		xxxxx++;
		if(jpeg_has_multiple_scans(&cinfo))
		{
			jpeg_finish_output(&cinfo);
		}
	} while(jpeg_has_multiple_scans(&cinfo) && !jpeg_input_complete(&cinfo));	

	/* Step 7: Finish decompression */

	(void) jpeg_finish_decompress(&cinfo);
	/* We can ignore the return value since suspension is not possible
	* with the stdio data source.
	*/

	/* Step 8: Release JPEG decompression object */

	/* This is an important step since it will release a good deal of memory. */
	jpeg_destroy_decompress(&cinfo);

	/* After finish_decompress, we can close the input file.
	* Here we postpone it until after no more JPEG errors are possible,
	* so as to simplify the setjmp error logic above.  (Actually, I don't
	* think that jpeg_destroy can do an error exit, but why assume anything...)
	*/
	fclose(infile);

	/* At this point you may want to check to see whether any corrupt-data
	* warnings occurred (test whether jerr.pub.num_warnings is nonzero).
	*/

	xxxxx+=100;
	/* And we're done! */
	return 1;
}

#define OK 1
#define ERROR 0
/* Read a PNG file.  You may want to return an error code if the read
 * fails (depending upon the failure).  There are two "prototypes" given
 * here - one where we are given the filename, and we need to open the
 * file, and the other where we are given an open file (possibly with
 * some or all of the magic bytes read - see comments above).
 */
int ImageFile::LoadPNGFile(const char *fileName)
{
	png_structp png_ptr;
	png_infop info_ptr;
	unsigned int sig_read = 0;
	png_uint_32 width, height;
	int bit_depth, color_type, interlace_type;
	FILE *fp;

	if ((fp = fopen(fileName, "rb")) == NULL)
		return (ERROR);

	/* Create and initialize the png_struct with the desired error handler
	* functions.  If you want to use the default stderr and longjump method,
	* you can supply NULL for the last three parameters.  We also supply the
	* the compiler header file version, so that we know if the application
	* was compiled with a compatible version of the library.  REQUIRED
	*/
	png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
//	png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING,
//		png_voidp user_error_ptr, user_error_fn, user_warning_fn);

	if (png_ptr == NULL)
	{
		fclose(fp);
		return (ERROR);
	}

	/* Allocate/initialize the memory for image information.  REQUIRED. */
	info_ptr = png_create_info_struct(png_ptr);
	if (info_ptr == NULL)
	{
		fclose(fp);
		png_destroy_read_struct(&png_ptr, png_infopp_NULL, png_infopp_NULL);
		return (ERROR);
	}

	/* Set error handling if you are using the setjmp/longjmp method (this is
	* the normal method of doing things with libpng).  REQUIRED unless you
	* set up your own error handlers in the png_create_read_struct() earlier.
	*/

	if (setjmp(png_jmpbuf(png_ptr)))
	{
		/* Free all of the memory associated with the png_ptr and info_ptr */
		png_destroy_read_struct(&png_ptr, &info_ptr, png_infopp_NULL);
		fclose(fp);
		/* If we get here, we had a problem reading the file */
		return (ERROR);
	}

	/* One of the following I/O initialization methods is REQUIRED */
//#ifdef streams /* PNG file I/O method 1 */
	/* Set up the input control if you are using standard C streams */
	png_init_io(png_ptr, fp);

//#else no_streams /* PNG file I/O method 2 */
	/* If you are using replacement read functions, instead of calling
	* png_init_io() here you would call:
	*/
//	png_set_read_fn(png_ptr, (void *)user_io_ptr, user_read_fn);
	/* where user_io_ptr is a structure you want available to the callbacks */
//#endif no_streams /* Use only one I/O method! */

	/* If we have already read some of the signature */
	png_set_sig_bytes(png_ptr, sig_read);

#ifdef hilevel
	/*
	* If you have enough memory to read in the entire image at once,
	* and you need to specify only transforms that can be controlled
	* with one of the PNG_TRANSFORM_* bits (this presently excludes
	* dithering, filling, setting background, and doing gamma
	* adjustment), then you can read the entire image (including
	* pixels) into the info structure with this call:
	*/
	png_read_png(png_ptr, info_ptr, png_transforms, png_voidp_NULL);
#else
	/* OK, you're doing it the hard way, with the lower-level functions */

	/* The call to png_read_info() gives us all of the information from the
	* PNG file before the first IDAT (image data chunk).  REQUIRED
	*/
	png_read_info(png_ptr, info_ptr);

	png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth, &color_type,
		&interlace_type, int_p_NULL, int_p_NULL);

	SetImageSize(width, height);
	if(AllocImageBuffer()==NULL)
	{
		png_destroy_read_struct(&png_ptr, &info_ptr, png_infopp_NULL);
		fclose(fp);

		return (ERROR);
	}

	/* Set up the data transformations you want.  Note that these are all
	* optional.  Only call them if you want/need them.  Many of the
	* transformations only work on specific types of images, and many
	* are mutually exclusive.
	*/

	/* tell libpng to strip 16 bit/color files down to 8 bits/color */
	png_set_strip_16(png_ptr);

	/* Strip alpha bytes from the input data without combining with the
	* background (not recommended).
	*/
	//png_set_strip_alpha(png_ptr);

	/* Extract multiple pixels with bit depths of 1, 2, and 4 from a single
	* byte into separate bytes (useful for paletted and grayscale images).
	*/
	png_set_packing(png_ptr);

	/* Change the order of packed pixels to least significant bit first
	* (not useful if you are using png_set_packing). */
	png_set_packswap(png_ptr);

	/* Expand paletted colors into true RGB triplets */
	if (color_type == PNG_COLOR_TYPE_PALETTE)
		png_set_palette_to_rgb(png_ptr);

	/* Expand grayscale images to the full 8 bits from 1, 2, or 4 bits/pixel */
	if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
		png_set_gray_1_2_4_to_8(png_ptr);

	// do not expand alpha... (hope)
	/* Expand paletted or RGB images with transparency to full alpha channels
	* so the data will be available as RGBA quartets.
	*/
	//if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS))
	//	png_set_tRNS_to_alpha(png_ptr);

	/* Set the background color to draw transparent and alpha images over.
	* It is possible to set the red, green, and blue components directly
	* for paletted images instead of supplying a palette index.  Note that
	* even if the PNG file supplies a background, you are not required to
	* use it - you should use the (solid) application background if it has one.
	*/

	png_color_16 my_background, *image_background;

	if (png_get_bKGD(png_ptr, info_ptr, &image_background))
		png_set_background(png_ptr, image_background,
		PNG_BACKGROUND_GAMMA_FILE, 1, 1.0);
	else
		png_set_background(png_ptr, &my_background,
		PNG_BACKGROUND_GAMMA_SCREEN, 0, 1.0);

	double screen_gamma=2.2;
	/* Some suggestions as to how to get a screen gamma value */

	/* Note that screen gamma is the display_exponent, which includes
	* the CRT_exponent and any correction for viewing conditions * /
	if (/* We have a user-defined screen gamma value * /)
	{
		screen_gamma = user-defined screen_gamma;
	}
	/* This is one way that applications share the same screen gamma value * /
	else if ((gamma_str = getenv("SCREEN_GAMMA")) != NULL)
	{
		screen_gamma = atof(gamma_str);
	}
	/* If we don't have another value * /
	else
	{
		screen_gamma = 2.2;  /* A good guess for a PC monitors in a dimly
							 lit room * /
		screen_gamma = 1.7 or 1.0;  /* A good guess for Mac systems * /
	}

	/* Tell libpng to handle the gamma conversion for you.  The final call
	* is a good guess for PC generated images, but it should be configurable
	* by the user at run time by the user.  It is strongly suggested that
	* your application support gamma correction.
	*/

	int intent;

	if (png_get_sRGB(png_ptr, info_ptr, &intent))
		png_set_gamma(png_ptr, screen_gamma, 0.45455);
	else
	{
		double image_gamma;
		if (png_get_gAMA(png_ptr, info_ptr, &image_gamma))
			png_set_gamma(png_ptr, screen_gamma, image_gamma);
		else
			png_set_gamma(png_ptr, screen_gamma, 0.45455);
	}

	/* Dither RGB files down to 8 bit palette or reduce palettes
	* to the number of colors available on your screen.
	*/
	//if (color_type & PNG_COLOR_MASK_COLOR)
	//{
	//	int num_palette;
	//	png_colorp palette;

	//	/* This reduces the image to the application supplied palette */
	//	if (/* we have our own palette */)
	//	{
	//		/* An array of colors to which the image should be dithered */
	//		png_color std_color_cube[MAX_SCREEN_COLORS];

	//		png_set_dither(png_ptr, std_color_cube, MAX_SCREEN_COLORS,
	//			MAX_SCREEN_COLORS, png_uint_16p_NULL, 0);
	//	}
	//	/* This reduces the image to the palette supplied in the file */
	//	else if (png_get_PLTE(png_ptr, info_ptr, &palette, &num_palette))
	//	{
	//		png_uint_16p histogram = NULL;

	//		png_get_hIST(png_ptr, info_ptr, &histogram);

	//		png_set_dither(png_ptr, palette, num_palette,
	//			max_screen_colors, histogram, 0);
	//	}
	//}

	/* invert monochrome files to have 0 as white and 1 as black */
	png_set_invert_mono(png_ptr);

	/* If you want to shift the pixel values from the range [0,255] or
	* [0,65535] to the original [0,7] or [0,31], or whatever range the
	* colors were originally in:
	*/
	//if (png_get_valid(png_ptr, info_ptr, PNG_INFO_sBIT))
	//{
	//	png_color_8p sig_bit;

	//	png_get_sBIT(png_ptr, info_ptr, &sig_bit);
	//	png_set_shift(png_ptr, sig_bit);
	//}

	/* flip the RGB pixels to BGR (or RGBA to BGRA) */
	//if (color_type & PNG_COLOR_MASK_COLOR)
	//	png_set_bgr(png_ptr);

	/* swap the RGBA or GA data to ARGB or AG (or BGRA to ABGR) */
	//png_set_swap_alpha(png_ptr);

	/* swap bytes of 16 bit files to least significant byte first */
	png_set_swap(png_ptr);

	/* Add filler (or alpha) byte (before/after each RGB triplet) */
	png_set_filler(png_ptr, 0xff, PNG_FILLER_AFTER);

	int number_passes, row, pass, y;
	/* Turn on interlace handling.  REQUIRED if you are not using
	* png_read_image().  To see how to handle interlacing passes,
	* see the png_read_row() method below:
	*/
	//number_passes = png_set_interlace_handling(png_ptr);

	/* Optional call to gamma correct and add the background to the palette
	* and update info structure.  REQUIRED if you are expecting libpng to
	* update the palette for you (ie you selected such a transform above).
	*/
	//png_read_update_info(png_ptr, info_ptr);

	/* Allocate the memory to hold the image using the fields of info_ptr. */

	/* The easiest way to read the image: */
	unsigned char *frame=NULL;
	SceUID blockid=sceKernelAllocPartitionMemory(2, "png_block", PSP_SMEM_Low, sizeX*sizeY*4, NULL);
	if(blockid>0)
	{
		frame = (unsigned char *)sceKernelGetBlockHeadAddr(blockid);
	}
	else
	{
		frame=(unsigned char *)malloc(sizeX*sizeY*4);
	}
	if(frame==NULL)
	{
		png_destroy_read_struct(&png_ptr, &info_ptr, png_infopp_NULL);
		fclose(fp);

		return (ERROR);
	}

	png_bytep row_pointers[height];

	for (row = 0; row < height; row++)
	{
		row_pointers[row] = (png_byte*)(frame+row*sizeX*4);
//		memset(row_pointers[row], 0, bufferWidth*4);
	}

#define entire
#define single
#undef sparkle

	/* Now it's time to read the image.  One of these methods is REQUIRED */
#ifdef entire /* Read the entire image in one go */
	png_read_image(png_ptr, row_pointers);
	unsigned char *dest, *src;
	unsigned long color=0xff000000, r=0, g=0, b=0xff;
	for(int i=0; i<sizeY; i++)
	{
		src=row_pointers[i];
		for(int ig=0; ig<bufferWidth/512; ig++)
		{
			dest=imageData+ig*4*512*sizeY+4*i*512;
			for(int x=ig*512; x<ig*512+512; x++)
			{
				color=0xff000000;
				if(x>=sizeX)
				{
					*(unsigned long*)dest=color;
					dest+=4;
					continue;
				}
				color=0xff000000;
				r=*src++;
				g=*src++;
				b=*src++;
				src++;

				color=color|(b<<16)|(g<<8)|r;
				*(unsigned long*)dest=color;
				dest+=4;
			}
		}
	}

#else //no_entire /* Read the image one or more scanlines at a time */
	/* The other way to read images - deal with interlacing: */

	for (pass = 0; pass < number_passes; pass++)
	{
#ifdef single /* Read the image a single row at a time */
		for (y = 0; y < height; y++)
		{
			png_read_rows(png_ptr, row_pointers, png_bytepp_NULL, 1);
		}

#else //no_single /* Read the image several rows at a time */
		for (y = 0; y < height; y += number_of_rows)
		{
#ifdef sparkle /* Read the image using the "sparkle" effect. */
			png_read_rows(png_ptr, &row_pointers[y], png_bytepp_NULL,
				number_of_rows);
#else //no_sparkle /* Read the image using the "rectangle" effect */
			png_read_rows(png_ptr, png_bytepp_NULL, &row_pointers[y],
				number_of_rows);
#endif //no_sparkle /* use only one of these two methods */
		}

		/* if you want to display the image after every pass, do
		so here */
#endif //no_single /* use only one of these two methods */
	}
#endif //no_entire /* use only one of these two methods */

	if(blockid>0)
	{
		sceKernelFreePartitionMemory(blockid);
		frame=NULL;
	}
	else if(frame)
	{
		free(frame);
		frame=NULL;
	}

	/* read rest of file, and get additional chunks in info_ptr - REQUIRED */
	png_read_end(png_ptr, info_ptr);
#endif //hilevel

	/* At this point you have read the entire image */

	/* clean up after the read, and free any memory allocated - REQUIRED */
	png_destroy_read_struct(&png_ptr, &info_ptr, png_infopp_NULL);

	/* close the file */
	fclose(fp);

	/* that's it */
	return (OK);

}

int ImageFile::LoadGIFFile(const char *fileName)
{
#define GIF_MAX_WIDTH 4096
	int	i, j, Error, NumFiles, Size, Row, Col, Width, Height, ExtCode, Count, OutFileFlag = FALSE;
	GifRecordType RecordType;
	GifByteType *Extension;
	GifPixelType ScreenBuffer[GIF_MAX_WIDTH];
    GifColorType *ColorMapEntry;

	GifFileType *GifFile;

	int
    ImageNum = 0,
    BackGround = 0,
    OneFileFlag = FALSE,
    HelpFlag = FALSE,
    ColorMapSize = 0,
    InterlacedOffset[] = { 0, 4, 2, 1 }, /* The way Interlaced image should. */
    InterlacedJumps[] = { 8, 8, 4, 2 };    /* be read - offsets and jumps... */
	ColorMapObject *ColorMap;

	GifFile = DGifOpenFileName(fileName);
	if(GifFile==NULL)
		return 0;

	do
	{
		if (DGifGetRecordType(GifFile, &RecordType) == GIF_ERROR)
		{
			goto fail;
		}
		switch (RecordType)
		{
		case IMAGE_DESC_RECORD_TYPE:
			if (DGifGetImageDesc(GifFile) == GIF_ERROR)
			{
				goto fail;
			}
			Row = GifFile->Image.Top;
			Col = GifFile->Image.Left;
			Width = GifFile->Image.Width;
			Height = GifFile->Image.Height;

			SetImageSize(Width, Height);
			if(AllocImageBuffer()==NULL)
			{
				goto fail;
			}

			Row=0; Col=0;	// i dont want screen!!

			BackGround = GifFile->SBackGroundColor;
			ColorMap = (GifFile->Image.ColorMap
				? GifFile->Image.ColorMap
				: GifFile->SColorMap);
			ColorMapSize = ColorMap->ColorCount;

			if (GifFile->Image.Interlace)
			{
				/* Need to perform 4 passes on the images: */
				unsigned char *dest, *src;
				for (Count = i = 0; i < 4; i++)
				{
					for (j = Row + InterlacedOffset[i]; j < Row + Height;
						j += InterlacedJumps[i])
					{
//						if (DGifGetLine(GifFile, &ScreenBuffer[j][Col], Width) == GIF_ERROR)
						if (DGifGetLine(GifFile, ScreenBuffer, Width) == GIF_ERROR)
						{
							goto fail;
						}
						for(int ig=0; ig<bufferWidth/512; ig++)
						{
							dest=imageData+ig*4*512*sizeY+4*j*512;
							for(int x=ig*512; x<ig*512+512; x++)
							{
								if(x>=sizeX)
								{
									*(unsigned long*)dest=0;
									dest+=4;
									continue;
								}
								ColorMapEntry = &ColorMap->Colors[ScreenBuffer[x]];
								*dest++ = ColorMapEntry->Red;
								*dest++ = ColorMapEntry->Green;
								*dest++ = ColorMapEntry->Blue;
								dest++;
							}
						}
/*						else
						{
							// copy pixels. to row j
							unsigned char *dest=imageData+j*bufferWidth*4;
							for(int z=0; z<Width; z++)
							{
								ColorMapEntry = &ColorMap->Colors[ScreenBuffer[z]];
								*dest++ = ColorMapEntry->Red;
								*dest++ = ColorMapEntry->Green;
								*dest++ = ColorMapEntry->Blue;
								dest++;
							}
						}*/
					}
				}
			}
			else
			{
				unsigned char *dest, *src;
				for(int i=0; i<sizeY; i++)
				{
					if (DGifGetLine(GifFile, ScreenBuffer, Width) == GIF_ERROR)
					{
						goto fail;
					}
					for(int ig=0; ig<bufferWidth/512; ig++)
					{
						dest=imageData+ig*4*512*sizeY+4*i*512;
						for(int x=ig*512; x<ig*512+512; x++)
						{
							if(x>=sizeX)
							{
								*(unsigned long*)dest=0;
								dest+=4;
								continue;
							}
							ColorMapEntry = &ColorMap->Colors[ScreenBuffer[x]];
							*dest++ = ColorMapEntry->Red;
							*dest++ = ColorMapEntry->Green;
							*dest++ = ColorMapEntry->Blue;
							dest++;
						}
					}
				}
/*				for (i = 0; i < Height; i++)
				{
					if (DGifGetLine(GifFile, ScreenBuffer, Width) == GIF_ERROR)
					{
						goto fail;
					}
					else
					{
						// copy pixels. to row i
						unsigned char *dest=imageData+i*bufferWidth*4;

						for(int z=0; z<Width; z++)
						{
							ColorMapEntry = &ColorMap->Colors[ScreenBuffer[z]];
							*dest++ = ColorMapEntry->Red;
							*dest++ = ColorMapEntry->Green;
							*dest++ = ColorMapEntry->Blue;
							dest++;
						}
					}
				}*/
			}
			goto success;
			break;
		case EXTENSION_RECORD_TYPE:
			/* Skip any extension blocks in file: */
			if (DGifGetExtension(GifFile, &ExtCode, &Extension) == GIF_ERROR)
			{
				goto fail;
				//				PrintGifError();
				//				exit(EXIT_FAILURE);
			}
			while (Extension != NULL)
			{
				if (DGifGetExtensionNext(GifFile, &Extension) == GIF_ERROR)
				{
					goto fail;
					//					PrintGifError();
					//					exit(EXIT_FAILURE);
				}
			}
			break;
		case TERMINATE_RECORD_TYPE:
			break;
		default:		    /* Should be traps by DGifGetRecordType. */
			break;
		}
	}
	while (RecordType != TERMINATE_RECORD_TYPE);

success:
	DGifCloseFile(GifFile);

	return 1;

fail:

	DGifCloseFile(GifFile);

	return 0;
#undef GIF_MAX_WIDTH
}

void ImageFile::BitBltF32T32(void *dest, int dpitch, int px, int py, int sx, int sy, int ix, int iy, int spitch)
{
	if(imageData==NULL)
		return;

	spitch=bufferWidth;

	char *destXBase=(char *)dest+2*(py*dpitch+px);
	char *destAddr;
	unsigned char *srcXBase=imageData+4*(iy*spitch+ix);
	unsigned char *srcAddr;
	unsigned short color, b, g, r;

	for(int y=0; y<sy; y++)
	{
		destAddr=destXBase;
		srcAddr=srcXBase;

		if( !(iy+y<sizeY))
			return;

		for(int x=0; x<sx; x++)
		{
			if( !(ix+x<sizeX))
				continue;

			r=(*srcAddr++)>>3;
			g=(*srcAddr++)>>2;
			b=(*srcAddr++)>>3;
			srcAddr++;

			color=(b<<11)|(g<<5)|r;
			*(unsigned short*)destAddr=color;
			destAddr+=2;
		}
		destXBase+=dpitch*2;
		srcXBase+=spitch*4;
	}

}

void ImageFile::BitBltF32T16(void *dest, int dpitch, int px, int py, int sx, int sy, int ix, int iy, int spitch)
{
	if(imageData==NULL)
		return;

	//memcpy(dest, imageData, 480*272*4);
	//return;

	spitch=bufferWidth;

	char *destXBase=(char *)dest+4*(py*dpitch+px);
	char *destAddr;
	unsigned char *srcXBase=imageData+4*(iy*spitch+ix);
	unsigned char *srcAddr;
	unsigned long color, b, g, r;

	for(int y=0; y<sy; y++)
	{
		destAddr=destXBase;
		srcAddr=srcXBase;

		if( !(iy+y<sizeY))
			return;

		for(int x=0; x<sx; x++)
		{
			if( !(ix+x<sizeX))
				continue;

			r=*srcAddr++;
			g=*srcAddr++;
			b=*srcAddr++;
			srcAddr++;

			*(unsigned long *)destAddr=0xff000000|(b<<16)|(g<<8)|(r);
			destAddr+=4;
		}
		destXBase+=dpitch*4;
		srcXBase+=spitch*4;
	}

}

void ImageFile::BitBlt8888(Display &display, int px, int py, int sx, int sy, int ix, int iy)
{
	if(imageData==NULL)
		return;

	if( !(px>=0 && py>=0 && px<SCR_WIDTH && py<SCR_HEIGHT) )
		return;

	char *destXBase=(char *)display.GetDrawBufferBasePtr()+display.pixelSize*(py*BUF_WIDTH+px);
	char *destAddr;
	unsigned char *srcXBase=imageData+4*(iy*sizeX+ix);
	unsigned char *srcAddr;
//	unsigned long b, g, r;

	for(int y=0; y<sy; y++)
	{
		destAddr=destXBase;
		srcAddr=srcXBase;
		if( !(py+y<SCR_HEIGHT))
			return;
		if( !(iy+y<sizeY))
			return;

		memcpy(destXBase, srcXBase, 4*sx);
/*		for(int x=0; x<sx; x++)
		{
			b=*srcAddr++;
			g=*srcAddr++;
			r=*srcAddr++;

			*(unsigned long *)destAddr=0xff000000|(b<<16)|(g<<8)|(r);

			destAddr+=display.pixelSize;
		}*/
		destXBase+=BUF_WIDTH*display.pixelSize;
		srcXBase+=sizeX*4;
	}
}

void ImageFile::BitBlt565(unsigned short *dest, int px, int py, int sx, int sy, int ix, int iy)
{
	if(imageData==NULL)
		return;

	if( !(px>=0 && py>=0 && px<SCR_WIDTH && py<SCR_HEIGHT) )
		return;

	char *destXBase=(char *)dest+2*(py*BUF_WIDTH+px);
	char *destAddr;
	unsigned char *srcXBase=imageData+2*(iy*sizeX+ix);
	unsigned char *srcAddr;
//	unsigned long b, g, r;

	for(int y=0; y<sy; y++)
	{
		destAddr=destXBase;
		srcAddr=srcXBase;
		if( !(py+y<SCR_HEIGHT))
			return;
		if( !(iy+y<sizeY))
			return;

//		memcpy(destXBase, srcXBase, 2*sx);
/*		for(int x=0; x<sx; x++)
		{
			b=*srcAddr++;
			g=*srcAddr++;
			r=*srcAddr++;

			*(unsigned long *)destAddr=0xff000000|(b<<16)|(g<<8)|(r);

			destAddr+=display.pixelSize;
		}*/
		destXBase+=BUF_WIDTH*2;
		srcXBase+=sizeX*2;
	}
}

int ImageFile::GetImageInfo(const char *fileName, int &sx, int &sy, int &bit)
{
	int itype=GetImageTypeIndex(fileName);

	switch(itype)
	{
	case 1:
		return GetBMPInfo(fileName, sx, sy, bit);
	case 2:
		return GetJPGInfo(fileName, sx, sy, bit);
	case 3:
		return GetPNGInfo(fileName, sx, sy, bit);
	case 4:
		return GetGIFInfo(fileName, sx, sy, bit);
	default:
		return 0;
	}

	return 0;
}

int ImageFile::GetBMPInfo(const char *fileName, int &sx, int &sy, int &bit)
{
	SimpleBitmapHeader header;

	int file = sceIoOpen(fileName, PSP_O_RDONLY, 0777);

	if(file<0)
		return 0;
	
	sceIoRead(file, &header, 54);

	sx=header.sizeX;
	sy=header.sizeY;
	bit=header.bitCount;

	sceIoClose(file);

	return 1;
}

int ImageFile::GetJPGInfo(const char *fileName, int &sx, int &sy, int &bit)
{
	struct jpeg_decompress_struct cinfo;
	struct my_error_mgr jerr;
	FILE * infile;		/* source file */
	if ((infile = fopen(fileName, "rb")) == NULL)
	{
		return 0;
	}
	cinfo.err = jpeg_std_error(&jerr.pub);
	jerr.pub.error_exit = my_error_exit;
	if (setjmp(jerr.setjmp_buffer)) {
		jpeg_destroy_decompress(&cinfo);
		fclose(infile);
		return 0;
	}

	jpeg_create_decompress(&cinfo);
	jpeg_stdio_src(&cinfo, infile);
	(void) jpeg_read_header(&cinfo, TRUE);
	sx=cinfo.image_width;
	sy=cinfo.image_height;
	bit=cinfo.num_components*8;

	jpeg_destroy_decompress(&cinfo);
	fclose(infile);

	return 1;
}

int ImageFile::GetPNGInfo(const char *fileName, int &sx, int &sy, int &bit)
{
	png_structp png_ptr;
	unsigned int sig_read = 0;
	png_infop info_ptr;
	FILE *fp;

	if ((fp = fopen(fileName, "rb")) == NULL)
		return 0;

	png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

	if (png_ptr == NULL)
	{
		fclose(fp);
		return 0;
	}

	info_ptr = png_create_info_struct(png_ptr);
	if (info_ptr == NULL)
	{
		fclose(fp);
		png_destroy_read_struct(&png_ptr, png_infopp_NULL, png_infopp_NULL);
		return 0;
	}

	if (setjmp(png_jmpbuf(png_ptr)))
	{
		png_destroy_read_struct(&png_ptr, &info_ptr, png_infopp_NULL);
		fclose(fp);
		return 0;
	}

	png_init_io(png_ptr, fp);
	png_set_sig_bytes(png_ptr, sig_read);

	png_read_info(png_ptr, info_ptr);

	sx=png_get_image_width(png_ptr,info_ptr);
	sy=png_get_image_height(png_ptr,info_ptr);
	bit=png_get_bit_depth(png_ptr,info_ptr) * png_get_channels(png_ptr, info_ptr);;

	//png_read_end(png_ptr, info_ptr);
	png_destroy_read_struct(&png_ptr, &info_ptr, png_infopp_NULL);

	fclose(fp);

	return 1;
}

int ImageFile::GetGIFInfo(const char *fileName, int &sx, int &sy, int &bit)
{
#define GIF_MAX_WIDTH 4096
	int	i, j, Error, NumFiles, Size, Row, Col, Width, Height, ExtCode, Count, OutFileFlag = FALSE;
	GifRecordType RecordType;
	GifByteType *Extension;
	GifPixelType ScreenBuffer[GIF_MAX_WIDTH];
    GifColorType *ColorMapEntry;

	GifFileType *GifFile;

	int
    ImageNum = 0,
    BackGround = 0,
    OneFileFlag = FALSE,
    HelpFlag = FALSE,
    ColorMapSize = 0,
    InterlacedOffset[] = { 0, 4, 2, 1 }, /* The way Interlaced image should. */
    InterlacedJumps[] = { 8, 8, 4, 2 };    /* be read - offsets and jumps... */
	ColorMapObject *ColorMap;

	GifFile = DGifOpenFileName(fileName);
	if(GifFile==NULL)
		return 0;
/*	else
	{
		sx=sy=bit=(int)GifFile;
		return 1;
	}*/
	do
	{
		if (DGifGetRecordType(GifFile, &RecordType) == GIF_ERROR)
		{
			goto fail;
		}
		switch (RecordType)
		{
		case IMAGE_DESC_RECORD_TYPE:
			if (DGifGetImageDesc(GifFile) == GIF_ERROR)
			{
				goto fail;
			}
			sx=GifFile->Image.Width;
			sy=GifFile->Image.Height;

			ColorMap = (GifFile->Image.ColorMap
				? GifFile->Image.ColorMap
				: GifFile->SColorMap);
			bit=ColorMap->ColorCount;

			goto success;
//			break;
		case EXTENSION_RECORD_TYPE:
			if (DGifGetExtension(GifFile, &ExtCode, &Extension) == GIF_ERROR)
			{
				goto fail;
			}
			while (Extension != NULL)
			{
				if (DGifGetExtensionNext(GifFile, &Extension) == GIF_ERROR)
				{
					goto fail;
				}
			}
			break;
		case TERMINATE_RECORD_TYPE:
			break;
		default:		    /* Should be traps by DGifGetRecordType. */
			break;
		}
	}
	while (RecordType != TERMINATE_RECORD_TYPE);

success:
	DGifCloseFile(GifFile);

	return 1;

fail:
	DGifCloseFile(GifFile);

	return 0;
#undef GIF_MAX_WIDTH

}



