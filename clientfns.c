#include <u.h>
#include <libc.h>
#include <draw.h>
#include <bio.h>

#include <thread.h>
#include <keyboard.h>
#include <mouse.h>
#include <control.h>

#include <mp.h>
#include <libsec.h>

#include <json.h>


#include "defs.h"
#include "fns.h"


void
drawpost(Post* post, Image* canvas, Image** colors, Font* font){
	Point sp;
	draw(canvas, Rect(canvas->r.min.x,canvas->r.min.y,canvas->r.max.x,canvas->r.max.y),colors[IBackground],nil,ZP);
	draw(canvas, Rect(canvas->r.min.x + 100, canvas->r.min.y + 100 , canvas->r.max.x - 100, canvas->r.max.y-100), colors[IPost], nil, ZP);
	sp = string( canvas, Pt(canvas->r.min.x+100, canvas->r.min.y + 100)  , colors[IName] , colors[IName]->r.min , font , post->name);
	sp = string(canvas, sp, colors[IText], colors[IText]->r.min, font, " ");
	sp = string(canvas, sp, colors[IText], colors[IText]->r.min, font, post->date);
	sp = string(canvas, sp, colors[IText], colors[IText]->r.min, font, " No. ");
	sp = string(canvas, sp, colors[IText], colors[IText]->r.min, font, post->pnum);

	sp = string(canvas, Pt(canvas->r.min.x+120, canvas->r.min.y+190), colors[IText], colors[IText]->r.min, font, post->msg);



}

void
eresized(int New){
	if(New && getwindow(display, Refmesg) < 0){
		
		threadexitsall("Getwindow Error");
	}
}

void
kbdfsio(void* arg){
	Channel* c;
	Biobuf* buf;
	char* strn;
	char* strpt;
	uint strptn;
	uint i;
	uint t;
	c = arg;

 	buf = Bopen("/dev/kbd", OREAD);

	/*The dance t does here is required since the first keydown sends both hit and repeat signal*/
	/*This is sendul instead of chanprint because each down key needs to trigger the alt*/

	for(strptn=0,t=0;;strn = Brdstr(buf, '\0', 1)){
		if(Blinelen(buf) > 0 ){
			if(strn[0] == 'k' || strn[0] == 'K'){
				strpt = strn;
				strptn = Blinelen(buf);
				t = 0;
			}
			if(t==0 && strn[0] == 'c')
				t = 1;
			if(t == 1){
				for(i=1;i<strptn;++i){
					sendul(c, strpt[i]);
				}
			}
		}
	}

}
