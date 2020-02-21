#include <u.h>
#include <libc.h>
#include <draw.h>
#include <bio.h>

#include <thread.h>
#include <keyboard.h>
#include <mouse.h>
//#include <control.h>

#include <mp.h>
#include <libsec.h>

#include <json.h>
#include "defs.h"
#include "fns.h"

void 
threadmain(int argc, char** argv){

	uint Key, Tick, Buf;
	Mouse Mouse;
	Mousectl* mc;
	Keyboardctl* kc;
	int diskbd, ifd;
	char* fontnam;

	Font* font;
	fontnam = "/lib/font/bit/vga/vga.font";

	Post test = {
		.error 0,
		.comp 0,
		.img 0,
		.date "46 quadrillion years ago",
		.name "INABUN",
		.pnum "42069",
		.msg "imgay"
	};

	if(initdraw(nil, nil, "bung") < 0)
		sysfatal("Initdraw Error: %r");

	if(getwindow(display, Refmesg) < 0)		
		sysfatal("Getwindow Error: %r");
	//initcontrols();

	Image* colors[] = {
		[IBackground]	 allocimage(display,Rect(0,0,1,1), RGBA32,1, 0xEEF2FFFF),
		[ILink]	allocimagemix(display, DBlue, DBlack),
		[IText]	allocimagemix(display, DBlack, DBlack),
		[IPost]	allocimage(display,Rect(0,0,1,1), RGBA32,1, 0xD6DAF0FF),
		[IPostactive]	allocimage(display,Rect(0,0,1,1), RGBA32,1, 0x0f0a06ff),
		[IName]	allocimage(display,Rect(0,0,1,1), RGBA32,1, 0x117743ff),
		[IReference]	allocimagemix(display, DRed, DRed),
		[IHeader]	allocimagemix(display, DPaleblue, DWhite),
		[ITitle]	allocimagemix(display, DRed, DRed) ,
	};


	mc = initmouse(nil, screen);
	if(diskbd == 1 || (ifd = open("/dev/kbd", OREAD)) <= 0 ){
		kc = initkeyboard(nil);
		diskbd = 1;
	}
	else 
		close(ifd);

	Alt a[] = {
		{mc->c, &Mouse, CHANRCV},
		{mc->resizec, nil, CHANRCV},
		{nil, &Buf, CHANRCV},
		{nil, &Tick, CHANRCV},
		{nil,nil,CHANEND},
	};

	if(diskbd == 1){
		a[2].c = kc->c;
	}
 	else{
		a[2].c = chancreate(sizeof(ulong), 0);
		proccreate(kbdfsio, a[2].c, 2048);
	}
	a[3].c = chancreate(sizeof(ulong), 0);
	proccreate(utimer,a[3].c,2048);

	font = openfont(display, fontnam);
	if(font == 0)
		sysfatal("Font could not be loaded");
	display->locking = 1;
	unlockdisplay(display);

	for(;;){

		Key = alt(a);
		switch(Key){
			case 0:
				fprint(2, "MOUES: %d\n", Mouse.buttons);
				break;
			case 1:
				eresized(1);
				break;
			case 2:
 				if(Buf == 'Q' || Buf == 'q' || Buf == 127)
					threadexitsall(nil);
				break;
			case 3:
				lockdisplay(display);

				drawpost(&test, screen, colors,font);
				flushimage(display, 1);
				unlockdisplay(display);
				break;

		}



	}
}
