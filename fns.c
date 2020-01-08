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
utimer(void* arg){
	Channel* c;
	c = arg;
	for(;;){
		sleep(10);
		sendul(c, 1);
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

/*Taken from webfs/http.c */
int
tlswrap(int fd, char *servername)
{
	TLSconn conn;

	memset(&conn, 0, sizeof(conn));

	/*
	if(debug)
		conn.trace = tlstrace;
	*/

	if(servername != nil)
		conn.serverName = smprint("%s", servername);
	if((fd = tlsClient(fd, &conn)) < 0){
		fprint(2, "tlsClient: %r\n"); 
	}
	free(conn.cert);
	free(conn.sessionID);
	free(conn.serverName);
	return fd;
}
int
readhttp(Biobuf* net, long* resp ){
 	int i, ct;
	long cl;
	cl = 0;
	ct =0;
	char* ctyp, *nnl, *linestr;
	for(;;){
		linestr = Brdstr(net, '', 1);
		if(Blinelen(net) > 0 && linestr[0] == '\n'){
			nnl = linestr;
			linestr = &linestr[1];
		}
		else
			nnl = linestr;

		
		if(Blinelen(net) < 2){
			fprint(1, "WE ALL DIE \n");
			break;
		}

		if(Blinelen(net) > 9 && (strncmp(linestr, "HTTP", 4) == 0)){
			i = atoi(&linestr[9]);
			switch(i){
				case 404:
					fprint(1, "FOR OH FOR \n");
					return ENOTFOUND;
				case 101:
				case 200:
					break;
				default:
					fprint(1, "ERROR CODE %d \n", i);
					return EGENERIC;
			}
		}



		if(Blinelen(net) > 16 && (cistrncmp(linestr, "content-length:", 15) == 0)){
			cl = atoi(&linestr[16]);
			fprint(1, "CL: %ld\n", cl);
		}
		if(Blinelen(net) > 26 && (cistrncmp(linestr, "Transfer-Encoding: chunked", 26) ==0))
			cl = -1;


		fprint(1, "%d ||| %s\n", Blinelen(net), linestr);
		if(Blinelen(net) > 13 && (cistrncmp(linestr, "content-type:",13) == 0)){
			ct = Blinelen(net);
			ctyp = nnl;
		}
		else
			free(nnl);
	}

	if(ct != 0){
		for(i=0;i<(ct - 5);++i){
			if(ctyp[i] == '/'){
				if(cistrncmp(&ctyp[i+1], "json", 4) == 0){
					free(ctyp);

					/*Chunked or length*/
					if( cl != 0){
						resp[0] = cl;
						return EJSON;
					}
					else
						return ENOMETHOD;
				}
				if(cistrncmp(&ctyp[i+1], "html", 4) == 0){
					free(ctyp);

					/*Chunked or length*/
					if( cl != 0){
						resp[0] = cl;
						return EHTML;
					}
					else
						return ENOMETHOD;
				}
			}
		}

		free(ctyp);
	}
	return ESUCCESS;

}


void
chunkdata(Chunk* root, long len, char* data){
 	if(root->n == -1){
		root->len = len;
		root->str = data;
		root->n = 0;
	}
	else{
		for(;root->n !=0;root = root->next);
 		root->next = (Chunk*)calloc(1, sizeof(Chunk));
		root->n = 1;
		root = root->next;
		root->len = len;
		root->str = data;
		root->n = 0;
	}
}
char*
sumchunks(Chunk* root){
	Chunk* tail;
	long len;
	char* ret, *rpos;

	for(tail = root, len = 0;;){
		len += tail->len;
		if(tail->n > 0)
			tail = tail->next;
		else
			break;
	}
	fprint(1, "CHUNK SIZE: %ld\n", len);
	ret = (char*)calloc(len + 1, sizeof(char));
	ret[len] = '\0';
	for(tail = root, rpos = ret;;){
		strncpy(rpos, &tail->str[1], tail->len);
		if(tail->n > 0){
			rpos = &rpos[tail->len];
			tail = tail->next;
		}
		else
			break;

	}

	return ret;
}
void
rcfr(Chunk* root){
	if(root->n != 0)
		rcfr(root->next);
	free(root->str);
	free(root);
}
char*
recvcontent(Biobuf* net, long cl){
	long i;
	char* buf, *linestr;
	Chunk* root;
	/* -1 = chunked*/
	if(cl > 0){
		buf = (char*)calloc(cl + 2, sizeof(buf));

		fprint(1, "CONTENTLENGTH: %ld\n", cl);

		Bgetc(net);
		for(i=0;i<cl;++i){
			buf[i] = Bgetc(net);
		}

		buf[cl+1] = '\0';
	}
	else{
		root = (Chunk*)calloc(1, sizeof(Chunk));
		root->n = -1;
		for(;;){
			linestr = Brdstr(net, '', 1);

			/*TODO: make these actually free() able*/
			/*if(Blinelen(net) > 0 && linestr[0] == '\n')
				linestr = &linestr[1];
			*/
			cl = strtol(&linestr[1],nil,16);
			fprint(2, "%s\n", &linestr[1]);
			fprint(2, "LINELEN: %ld\n", cl);
			free(linestr);

			if(cl==0)
				break;

			linestr = Brdstr(net, '', 1);
			/*
			if(Blinelen(net) > 0 && linestr[0] == '\n')
				linestr = &linestr[1];
			*/

			fprint(2, "LINE: %d\n", Blinelen(net));

			if(Blinelen(net) != (cl + 1))
				return nil;

			chunkdata(root, cl, linestr); 
 		}
		buf = sumchunks(root);
		rcfr(root);

	}
	return buf;
}
int
recievejson(Biobuf* net, long cl, JSON** ret){
	char* buf;

 	buf = recvcontent(net, cl);
	if(buf == nil)
		return ESIZEWRONG;
  	ret[0] = jsonparse(buf);
	free(buf);

	/*Generally bad, but json.h's only error routine is returning nil*/
	if(ret[0] == nil)
		return EJSONINVALID;
	return EJSON;
}
int
recievehtml(Biobuf* net, long cl, JSON** ret){
	char* buf, *lbuf, *jstr;
	Biobuf* nnt;
	int p[2], i;
 	buf = recvcontent(net, cl);
	if(buf == nil)
		return ESIZEWRONG;
  
	pipe(p);
	nnt = Bfdopen(p[1], OREAD);
	fprint(p[0], "%s", buf);
	for(;;){
		lbuf = Brdstr(nnt,'<',1);
		if(Blinelen(nnt) > 21 && strncmp(lbuf,"script>var imports =",20) == 0){
			jstr = (char*)calloc(Blinelen(nnt), sizeof(char));
			strncpy(jstr, &lbuf[21], Blinelen(nnt) - 21);
			
			ret[0] = jsonparse(jstr);
			free(jstr);
			free(lbuf);
			break;
		}
		
		free(lbuf);
	}
	Bterm(nnt);
	close(p[1]);
	close(p[0]);
	free(buf);
 	if(ret[0] == nil)
		return EHTMLINVALID;

	return EHTML;
	
}
void
printjson(JSON* root){
	JSONEl* tail;
	JSON* jtm;

	switch(root->t){
		case JSONNull:
			fprint(1, "null \n");
			break;
		case JSONBool:
			fprint(1, "bool\n");
			break;
		case JSONNumber:
			fprint(1, "2dbl? %.0f\n", root->n);
			break;
		case JSONString:
			fprint(1,"3str? %s\n",root->s);
			break;
		case JSONArray:
			for(tail=root->first;;){
				printjson(tail->val);
				if(tail->next == nil)
					break;
				tail = tail->next;
			}
				
			break;
		case JSONObject:
			jtm = jsonbyname(root, "num");
			/*<nil> or errstr*/
			if(jtm == nil)
				fprint(1, "NOPOSTNUMBER\n");
			else
				printjson(jtm);

			jtm = jsonbyname(root, "name");
			if(jtm == nil)
				fprint(1, "3str? Anonymous\n");
			else
				printjson(jtm);

			jtm = jsonbyname(root, "body");
			if(jtm == nil)
				fprint(1, "JISAKUJIENS\n");
			else
				printjson(jtm);
			jtm = jsonbyname(root, "SOCKET_PATH");
			if(jtm == nil)
				fprint(1, "noconf\n");
			else
				printjson(jtm);

			jtm = jsonbyname(root, "config");
			if(jtm == nil)
				break;
			else
				printjson(jtm);

			break;
	}
}
void
freewsf(WSFrame* frame){
	if(frame->len > 0)
		free(frame->buf);
	free(frame);
}
void
sendws(int fd, WSFrame* frame){
	int p[2], *buf, pck,i;
	Biobuf* net;
	uvlong n;

	pipe(p);
	net = Bfdopen(p[1], OREAD);


	n= 2 + (frame->mask>0?4:0) + (frame->len>65535?8:frame->len>125?2:0) + frame->len;
	buf = calloc(n + (4 - (n%4)),1);

	pck = frame->fin<<7;
	pck |= frame->op;
	write(p[0], &pck, 1);
	pck = frame->len<126?(frame->len):(frame->len<65536?126:127);
	pck |= (frame->mask << 7);
	write(p[0], &pck, 1);
	if(frame->len < 65536 && frame->len > 125)
		write(p[0], &frame->len, 2);
	if(frame->len > 65536)
		write(p[0], &frame->len, 8);
	if(frame->mask > 0)
		write(p[0], &frame->key, 4);
	write(p[0], frame->buf, frame->len);

	for(i=0;i<n;++i){
		buf[i/4] |= Bgetc(net)<<((i%4)*8);
	}
	write(fd, buf, n);

	close(p[0]);
	Bterm(net);
	free(buf);
}
WSFrame* 
recvframe(Biobuf* net){
	int i;
	WSFrame* ret;

	ret = (WSFrame*)calloc(1, sizeof(WSFrame));
	i = Bgetc(net);
	if(i<0){
		ret->err = -1;
		return ret;
	}
 	ret->fin = (i&0x80)>>7;
	ret->op = i&0xF;
	i = Bgetc(net);
	ret->mask = (i&0x80)>>7;

	if((i&0x7F)<126){
		ret->len = i&0x7F;
 
	}
	else if((i&0x7F)<127){
		ret->len = Bgetc(net)<<8;
 		ret->len |= Bgetc(net);
 	}
	else{
		ret->len = 0;
		for(i=0;i<8;++i){
			ret->len |= Bgetc(net);
			ret->len <<= 8;
		}
	}

	if(ret->mask > 0){
		ret->key = 0;
		for(i=0;i<4;++i){
			ret->key |= Bgetc(net);
			ret->key <<= 8;
		}
	}

	if(ret->len > 0){
		ret->buf = (char*)calloc(ret->len + 1, sizeof(char));
		if(ret->mask > 0){
			for(i=0;i<ret->len;++i)
				ret->buf[i] = Bgetc(net) ^ ((ret->key >> ((i%4)*8))&0xFF);
		}
		else{
			for(i=0;i<ret->len;++i)
				ret->buf[i] = Bgetc(net);
		}
		ret->buf[ret->len] = '\0';

	}
	
	return ret;
}
int
recvheaders(Biobuf* net){
	char* nnl, *buf;
	int i;

	for(;;){
		buf = Brdstr(net, '', 1);
		if(Blinelen(net) > 1 && buf[0] == '\n')
			nnl = &buf[1];
		else
			nnl = buf;

		if(Blinelen(net) > 1){
			fprint(1,"%d ||| %s\n",Blinelen(net), buf[0]=='\n'?&buf[1]:buf);
		}
		else{
			free(nnl);
			break;
		}
		if(Blinelen(net) > 9 && (strncmp(buf, "HTTP", 4) == 0)){
			i = atoi(&buf[9]);
			switch(i){
				case 101:
					break;
				default:
					return EHTMLINVALID;

			}
			
		}
		free(nnl);
	}
	Bgetc(net);
	return EOK;
}
void
rcvproc(void* arg){
	Channel* v,*c;
	Biobuf* net;
	WSFrame* frame;

	c = arg;
	v = recvp(c);
	net = recvp(c);

	for(;;){
		frame = recvframe(net);
		if(frame->err == 0)
			sendul(v, 1);
		else
			sendul(v, 2);
		sendp(c, frame);
	}
}
void
sendproc(void* arg){
	Channel* v, *c;
	int* fd;
	WSFrame* frame;
 	c = arg;
	v = recvp(c);
 	for(;;){
		frame = recvp(c);
 		fd = recvp(c);
  
		sendws(fd[0],frame);
 		freewsf(frame);

	}
}
void
consfn(void* arg){
	Channel* v, *c;
	int p[2];
	int consfd;
	char* consfil;
	char* consinbuf;
	Biobuf* cons;

	c = arg;
	v = recvp(c);
	consfil = recvp(c);


	pipe(p);
	if((consfd = create(consfil, ORCLOSE|OWRITE, 0660)) < 0){
		remove(consfil);
		if((consfd = create(consfil, ORCLOSE|OWRITE, 0660)) < 0)
			threadexitsall("create failed");
	}
	
	fprint(consfd, "%d", p[0]);
	/*These Close commands are recommended by the srv (3) manpage, but seem to break*/
	/*
	close(consfd);
	close(p[0]);
	*/

	fprint(p[1], "Console initialized %ulld\n", nsec());
	cons = Bfdopen(p[1], OREAD);
	for(;;){
		//ADD HELP FN DICKBAG
		consinbuf = Brdstr(cons, '\n', 1);
		if(Blinelen(cons) > 3 && cistrncmp(consinbuf,"help",4) == 0 )
			fprint(p[1], "halt\nsendtf fin op len buf\n");
		
		else if(Blinelen(cons)){
			sendul(v, Blinelen(cons));
			sendp(c,consinbuf);
		}
	}
}
WSFrame*
parsecmd(char* cmd, int len){
	WSFrame* ret;
	int i,j;
	char* tok;

	ret = calloc(1, sizeof(WSFrame));

	i = 0;
	tok = strtok(cmd, " "); /*sendtf*/
	i += strlen(tok);
	tok = strtok(0, " ");
	if(tok == 0){
		ret->err = 1;
		return ret;
	}
	i+=strlen(tok);
	ret->fin = atoi(tok);
	tok = strtok(0, " ");
	if(tok == 0){
		ret->err = 1;
		return ret;
	}
	i+=strlen(tok);
	ret->op = atoi(tok);
	tok = strtok(0, " ");
	if(tok == 0){
		ret->err = 1;
		return ret;
	}
	ret->len = atoi(tok);
	i += strlen(tok);
	for(j=0;tok[j] != '\0';++j);
	if(j+i + 1>len){
		ret->err = 1;
		return ret;
	}

	tok = &tok[j+1];
	if(tok == 0 || ret->len > (len - i)){
		ret->err = 1;
		return ret;
	}

	/*One could send a ret->len bigger than len - preceeding fields, creating a buffer of len instead of ret->len ensures it will be 0ed*/
	if(ret->len > 0){
		ret->buf = calloc(len, sizeof(char));
		ret->mask = 1;
		ret->key = lrand();
		for(i=0;i<ret->len;++i)
			ret->buf[i] = tok[i] ^ ((ret->key >> (8*( i%4)))& 0xFF);
 

	}
	ret->err = 0;
	return ret;
}
void
wsproc(void* arg){
	Channel* rv, *sv, *c,*q, *rcvc,*sndc;
	ulong r,s,h;
	int Key,fd,i, chl[2];
	Biobuf* net;
	WSFrame* frame, *sndfrm;
	WSFramel* stack, top;

	char* cmd;

	c = arg;
	q = recvp(c);
	rv = chancreate(sizeof(ulong),0);
	sv = chancreate(sizeof(ulong),0);

	Alt a[] = {
		{rv,&r,CHANRCV},
		{sv,&s,CHANRCV},
		{q,&h, CHANRCV },
		{nil,nil,CHANEND},
	};

	net = recvp(c);
	fd = Bfildes(net);

	i = recvheaders(net);
	if(i!=EOK){
		sendul(q,42069);
		threadexits(nil);
	}
	rcvc = chancreate(sizeof(WSFrame*),0);
	chl[0] = proccreate(rcvproc,rcvc,1024000);
	sendp(rcvc,rv);
	sendp(rcvc, net);

	sndc = chancreate(sizeof(WSFrame*),0);
	chl[1] = proccreate(sendproc, sndc,1024000);
	sendp(sndc, sv);

	for(;;){
		Key = alt(a);
 		switch(Key){
			case 0:
 				switch(r){
					case 1:
						frame = recvp(rcvc);
						fprint(1, "FRAME RECIEVED: FIN: %d OP: %ulx MASK: %d LEN: %ulld KEY: %ulx \n", frame->fin, frame->op, frame->mask, frame->len, frame->key);
							if(frame->len > 0){
								fprint(1, "RECIEVED FRAME: ");
								write(1, frame->buf, frame->len);
								fprint(1, "\n");

							}

						switch(frame->op){
							case WPONG:
								break;
							case WPING:
								sndfrm=(WSFrame*)calloc(1, sizeof(WSFrame));
								sndfrm->fin = 1;
								sndfrm->op=WPONG;
								sndfrm->mask = 1;
								sndfrm->key = lrand();
								sndfrm->len = frame->len;
								sndfrm->buf = (char*)calloc(sndfrm->len + 1, sizeof(char));
								for(i=0;i<sndfrm->len;++i)
									sndfrm->buf[i] = frame->buf[i] ^ (sndfrm->key >> (( (i%4)*8)&0xFF));
								sndfrm->buf[sndfrm->len] = '\0';
								fprint(1, "SENDING FRAME: FIN: %d OP: %ulx MASK: %d LEN: %ulld KEY: %ulx \n", sndfrm->fin, sndfrm->op, sndfrm->mask, sndfrm->len, sndfrm->key);
 
								sendp(sndc, sndfrm);
								sendp(sndc, &fd);

								break;
							case WTEXT:
 								fprint(1,"TEXT RECIEVED: %s\n",frame->buf);
 								break;
							case WHUP:
							case WBAD:
								fprint(1, "HUNG UP: %s\n", frame->buf);
								threadkill(chl[0]);
								threadkill(chl[1]);
								sendul(q, 42069);
		 						threadexits(nil);
								break;
							}
							freewsf(frame);
							break;
					case 2:
						threadkill(chl[0]);
						threadkill(chl[1]);
						sendul(q, 42069);
	 					threadexits(nil);
						break;
				}
				break;

			case 2:
				cmd = recvp(c);
				fprint(1, "LEN: %d, STRN: %s\n", h, cmd);
				if(h>7 && strncmp(cmd, "sendtf ",7) == 0){
					sndfrm = parsecmd(cmd, h);
					if(sndfrm->err == 0){
						fprint(1, "SENDING FRAME: FIN: %d OP: %ulx MASK: %d LEN: %ulld KEY: %ulx \n", sndfrm->fin, sndfrm->op, sndfrm->mask, sndfrm->len, sndfrm->key);
						sendp(sndc, sndfrm);
						sendp(sndc, &fd);
					}
					else
						freewsf(sndfrm);

				}
				free(cmd);

				break;
		}
	}
}
