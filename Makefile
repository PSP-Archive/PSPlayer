PSPSDK = $(shell psp-config --pspsdk-path)
PSPLIBSDIR = $(PSPSDK)/..
TARGET = PSPlayer

M33SDK = $(PSPSDK)/m33

LIBAUDIOCODEC = -lpspaudiocodec -lpspvideocodec -lpspmpeg -lpspmpegbase

INCOGGVORBIS = ../libtremor
LIBDIROGGVORBIS = ../tremor
LIBOGGVORBIS = -ltremor

#FFMPEGCFLAGS = -DHAVE_LRINTF -DHAVE_AV_CONFIG_H -DUSE_FFMPEG_DIGEST
FFMPEGCFLAGS = -DUSE_FFMPEG_DIGEST

#INCFFMPEG = ../ffmpeg/libavcodec ../ffmpeg/libavformat ../ffmpeg/libavutil ../a52dec-0.7.4 ../faad2 ../libtheora-1.0alpha5 
#LIBFFMPEG = -lavformat -lavcodec -lavutil -la52 -lfaad -ltheora

INCFFMPEG = ../ffmpeg-svn ../ffmpeg-svn/libavcodec ../ffmpeg-svn/libavformat ../ffmpeg-svn/libavutil ../ffmpeg-svn/a52dec
LIBFFMPEG = -lavformat -lavcodec -lavutil -la52

#INCFFMPEG = ../ffmpeg-digest/libavcodec ../ffmpeg-digest/libavformat ../ffmpeg-digest/libavutil ../a52dec-0.7.4
#LIBFFMPEG = -lavformat -lavcodec -lavutil -la52

OBJS_MP4 = mp4ff/mp4atom.o mp4ff/mp4ff.o mp4ff/mp4sample.o mp4ff/mp4util.o 
# mp4ff/mp4tagupdate.o mp4ff/mp4meta.o mp4ff/drms.o
INCFAAD = ./mp4ff
LIBDIRFAAD = 
LIBFAAD = 

OBJS_DL = display_control/supportlib.o
OBJS_SUB = subreader.o

OBJS = main.o Display.o TextToolMByte.o ImageFile.o resample.o MyFile.o display_control.o my_pspaudiolib.o my_resample.o Queue.o $(OBJS_DL) $(OBJS_MP4) $(OBJS_SUB)
LIBS =  -lmad -lid3tag -ljpeg -lpng -lz -lungif $(LIBFFMPEG) $(LIBOGGVORBIS) $(LIBAUDIOCODEC) $(LIBFAAD) \
		-lpspvfpu -lpspgu -lpsppower -lpspaudio -lpspusb -lpspusbstor -lpsphprm -lpspkubridge -lpsprtc \
		-lm 


INCDIR = $(M33SDK)/include ../libid3tag-0.15.1b ../libmad-0.15.1b ../jpeg-6b ../zlib123 ../lpng128 ../libungif/lib \
			$(INCFFMPEG) $(INCOGGVORBIS) $(INCFAAD)
LIBDIR = . $(M33SDK)/lib $(INCDIR) $(LIBDIROGGVORBIS) $(LIBDIRFAAD)

CFLAGS = -O2 -G0 $(FFMPEGCFLAGS)
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS)

PSP_FW_VERSION = 401

ifeq ($(PSP_FW_VERSION), 401)

PSP_LARGE_MEMORY = 1

BUILD_PRX = 1
EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = Psplayer_FW4XX

else

PSP_EBOOT_TITLE = Psplayer_FW150

;BUILD_PRX = 1

endif

PSP_EBOOT_ICON = ICON0.PNG
;PSP_EBOOT_PIC1 = PIC1.PNG

include $(PSPSDK)/lib/build.mak
