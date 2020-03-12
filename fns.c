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
utimer(void* arg){
	Channel* c;
	c = arg;
	for(;;){
		sleep(10);
		sendul(c, 1);
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
			break;
		}
		if(Blinelen(net) > 9 && (strncmp(linestr, "HTTP", 4) == 0)){
			i = atoi(&linestr[9]);
			switch(i){
				case 404:
					return ENOTFOUND;
				case 101:
				case 200:
					break;
				case 300:
				case 301:
				case 302:
				case 303:
				case 307:
				case 308:
					return EMOVED;
				default:
					return EGENERIC;
			}
		}


		if(Blinelen(net) > 16 && (cistrncmp(linestr, "content-length:", 15) == 0)){
			cl = atoi(&linestr[16]);
		}
		if(Blinelen(net) > 26 && (cistrncmp(linestr, "Transfer-Encoding: chunked", 26) ==0))
			cl = -1;


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
			free(linestr);

			if(cl==0)
				break;

			linestr = Brdstr(net, '', 1);
			/*
			if(Blinelen(net) > 0 && linestr[0] == '\n')
				linestr = &linestr[1];
			*/


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
		lbuf = Brdstr(nnt, '<', 1);
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
	if(frame->len < 65536 && frame->len > 125){
		for(i=0;i<2;++i){
			pck = (frame->len >> ((1 - i) * 8)) & 0xFF;
			write(p[0], &pck, 1);

		}
	}
	if(frame->len > 65536){
		for(i=0;i<8;++i){
			pck = (frame->len >> ((7 - i) * 8)) & 0xFF;
			write(p[0], &pck, 1);

		}
	}
	if(frame->mask > 0){
		for(i=0;i<4;++i){
			pck = (frame->key >> ((3 - i) * 8)) & 0xFF;
			write(p[0], &pck, 1);

		}
	}
	if(frame->len > 0)
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
				ret->buf[i] = Bgetc(net) ^ ((ret->key >> ((3-(i%4))*8))&0xFF);
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

				case 301:
				default:
					free(nnl);
					return EHTMLINVALID;

			}
			
		}
		free(nnl);
	}
	Bgetc(net);
	return EOK;
}

/*Procs which do I/O must be procs and not threads*/
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
	Biobuf* cons,*conp;

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
	conp = Bfdopen(p[1], OWRITE);
	sendp(c, conp);
	for(;;){
		consinbuf = Brdstr(cons, '\n', 1);
		if(Blinelen(cons) > 3 && cistrncmp(consinbuf,"help",4) == 0 ){
			Bprint(conp, "\thalt\n\tdump [id]\n\tsearch {\"site\":\"net!website.org!https\", \"board\" : \"/liveboard/\"}\n\tconnect { \"site\" : \"net!website.org!https\" }\n\tconfig [siteid]\n\tmconfig [siteid] {\"\" : \"\"} \n\t threads [siteid] [boardindex]\n\topen [siteid]\n");
			free(consinbuf);
			Bflush(conp);
		}
		else if(Blinelen(cons) > 0){
			sendul(v, Blinelen(cons));
			sendp(c,consinbuf);
		}
		else{
			free(consinbuf);
		}
	}
}
WSFrame*
parsecmd(char* cmd, int len){
	WSFrame* ret;
	int i,j;
	char* tok;

	ret = calloc(1, sizeof(WSFrame));

	ret->mask = 1;
	ret->key = lrand();

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
	if(ret->len == 0)
		return ret;

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
		for(i=0;i<ret->len;++i)
			ret->buf[i] = tok[i] ^ ((ret->key >> (8*(3-( i%4))))& 0xFF);
 

	}
	ret->err = 0;
	return ret;
}
void
wsproc(void* arg){
	Channel* rv, *sv, *c,*q, *rcvc,*sndc;
	ulong r,s,h;
	int Key,fd,i, chl[2], pid;
	Biobuf* net;
	WSFrame* frame, *sndfrm;
	uint wsfdep;
	/*WSFrame stack untested, no testbeds that could be found did discontinuous websocket frames*/
	WSFramel* stack, *cur;


	wsfdep = 0;

	char* cmd;

	c = arg;
	q = recvp(c);
	pid = recvul(q);

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
		sendul(q,pid);
		threadexits(nil);
	}
	rcvc = chancreate(sizeof(WSFrame*),0);
	chl[0] = proccreate(rcvproc,rcvc,1024000);
	sendp(rcvc,rv);
	sendp(rcvc, net);

	sndc = chancreate(sizeof(WSFrame*),0);
	chl[1] = proccreate(sendproc, sndc,1024000);
	sendp(sndc, sv);

	for(;;)
 		switch(alt(a)){
			case 0:
 				switch(r){
					case 1:
						frame = recvp(rcvc);

						/*WARNING: UNTESTED, COULD NOT FIND DISCONTINUOUS REPEATER TEST*/
						if(frame->fin != 1){
							cur = calloc(1, sizeof(WSFramel));
							cur->elm = frame;
							if(wsfdep++ == 0){
								cur->end = 1;
								stack = cur;

							}
							else{
								cur->end = 0;
								cur->next = stack;
								stack = cur;

							}
							break;
						}
						if(wsfdep != 0){
							wsfdep = 0;

							cur = calloc(1, sizeof(WSFramel));
							cur->elm = frame;
							cur->end = 0;
							cur->next = stack;
							stack = cur;

							for(i=0, cur = stack;cur->end !=1;cur = cur->next){
								i += cur->elm->len;
								if(cur->elm->op != cur->next->elm->op){
									i = -1;
									break;

								}

							}
							if(i==-1){
								for(i=0, cur = stack;cur->end !=1;){
									freewsf(cur->elm);
									stack = cur;
									cur = cur->next;
									free(stack);
								}
								freewsf(cur->elm);
								free(cur);
								break;
							}
							i += cur->elm->len;
							frame = calloc(1, sizeof(WSFrame));
							frame->len = i;
							frame->buf = calloc(i+1, sizeof(char));
							for(cur = stack;cur->end !=1;){
								i -= cur->elm->len;
								strncpy(&frame->buf[i],cur->elm->buf, cur->elm->len);
								freewsf(cur->elm);
								stack = cur;
								cur = cur->next;
								free(stack);

							}
							i -= cur->elm->len;
							strncpy(&frame->buf[i],cur->elm->buf, cur->elm->len);
							freewsf(cur->elm);
							free(cur);
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
									sndfrm->buf[i] = frame->buf[i] ^ (sndfrm->key >> (( (3-(i%4))*8)&0xFF));
								sndfrm->buf[sndfrm->len] = '\0';
 
								sendp(sndc, sndfrm);
								sendp(sndc, &fd);
								chanprint(c,"RECV PING\n");
								break;
							case WTEXT:
								sendul(q, pid);
								chanprint(c, "recv %lud %s",frame->len, frame->buf);
 								break;
							case WHUP:
							case WBAD:
								threadkill(chl[0]);
								threadkill(chl[1]);
								sendul(q, pid);
								chanprint(c, "ending");
		 						threadexits(nil);
								break;
							}
							freewsf(frame);
							break;
					case 2:
						threadkill(chl[0]);
						threadkill(chl[1]);
						sendul(q, pid);
						chanprint(c, "hangup");

	 					threadexits(nil);

				}
				break;

			case 2:
				cmd = recvp(c);
				if(h>7 && strncmp(cmd, "sendtf ",7) == 0){
					sndfrm = parsecmd(cmd, h);
					if(sndfrm->err == 0){
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
void
freesite(Site* site){
	int i;
	free(site->dialstr);
	free(site->addrstr);
	free(site->seckey);
	free(site->session_id);
	if(site->config != nil)
		jsonfree(site->config);
	if(site->nboards > 0)
		for(i=0;i<site->nboards;++i)
			if(site->threads[i] != nil)
				jsonfree(site->threads[i]);
		
		free(site->threads);

	free(site);

}
Biobuf*
dialurl(Url* dst, Site* site){
	int fd;
	char* addr, *dialstr, *scheme, *port;
	if(dst->host != nil && dst->host[0] != '\0')
		addr = dst->host;
	else
		addr=site->addrstr;

	if(dst->port != nil)
		port = dst->port;
	else if(dst->scheme !=nil)
		port = dst->scheme;
	else 
		port = "https";


	dialstr = smprint("net!%s!%s", addr, port);
	fd = dial(dialstr,0,0,0);
	if(fd<0){
		free(dialstr);
		return nil;
	}
	fd = tlswrap(fd, addr);
	if(fd<0){
		free(dialstr);
		return nil;
	}


	free(dialstr);
	return Bfdopen(fd, OREAD);
}
Biobuf*
dialsite(Site* site){
	int fd;
	if(site->dialstr == nil || site->addrstr == nil)
		return nil;

	fd = dial(site->dialstr,nil,nil,nil);
	if(fd<0)
		return nil;

	/*We could check for !https/!443 for this,
	 but in that it does not close on failure we should be fine assuming
	whatever port we're connecting to is TLS enabled by default*/
	fd = tlswrap(fd, site->addrstr);
	if(fd<0)
		return nil;
	return Bfdopen(fd, OREAD);
	
}

void
jsondriver(void* arg){
	Channel* c, *v, *cons, *consp, *wsp, *wspp;

	uchar* msg;
	char* cmd, *tmpst[2], *line;

	int i,j,rcod;
	uint rcv, ccv, wcv;
	long cl;
	uvlong nconns, niter;

	JSON* tmp[2];
	JSONEl* jltmp;
 	Site* conns[MAXCONNS], *stmp;
	Biobuf* conb, *dialb;
	Url* urlrdr;

	c = arg;
	v = recvp(c);
	consp = recvp(c);
	cons = recvp(c);

	nconns = 0;
	msg = (uchar*)"bunisgay";

	wsp = chancreate(sizeof(ulong), 0);
	wspp = chancreate(sizeof(char*), 0);

	conb = recvp(cons);

	Alt a[] = {
		{v, &rcv, CHANRCV},
		{consp, &ccv, CHANRCV},
		{wspp, &wcv, CHANRCV},
		{nil,nil,CHANEND},

	};
	for(;;)
	switch(alt(a)){

		case 1:
			cmd = recvp(cons);
			if(ccv > 3 && cistrncmp("halt", cmd, 4) == 0)
				threadexitsall(nil);
			if(ccv > 7 && cistrncmp("connect", cmd, 7) == 0){
				if(nconns >= MAXCONNS){
					free(cmd);
					break;
				}
				tmp[0] = jsonparse(&cmd[7]); 
				if(tmp[0] == nil){
					free(cmd);
					break;
				}
				stmp = calloc(1, sizeof(Site));
				tmp[1] = jsonbyname(tmp[0], "site");
				if(tmp[1] == nil){
					jsonfree( tmp[0]);
					free(cmd);
					break;
				}
				if(tmp[1]->t != JSONString){
					jsonfree(tmp[0]);
					free(cmd);
					break;
				}
				stmp->dialstr = calloc(strlen(tmp[1]->s) + 1, sizeof(char));
				strcpy(stmp->dialstr,tmp[1]->s);
				stmp->dialstr[strlen(tmp[1]->s)] = '\0';
				tmp[1] = jsonbyname(tmp[0], "dir");
				if(tmp[1] == nil || tmp[1]->t != JSONString){
					stmp->dir = calloc(1, sizeof(char));
					stmp->dir[0] = '\0';
				}
				else{
					stmp->dir = calloc(1 + strlen(tmp[1]->s), sizeof(char));
					strcpy(stmp->dir, tmp[1]->s);
					stmp->dir[strlen(tmp[1]->s)] = '\0';
				}

				jsonfree(tmp[0]);

				tmpst[0] = calloc(strlen(stmp->dialstr) + 1, sizeof(char));
				strcpy(tmpst[0], stmp->dialstr);
				tmpst[0][strlen(stmp->dialstr)] = '\0';
				strtok(tmpst[0], "!");
				tmpst[1] = strtok(0, "!");
				if(tmpst[1] == nil){
					free(tmpst[0]);
					free(cmd);
					break;

				}

				stmp->addrstr = calloc(strlen(tmpst[1]) + 1, sizeof(char));
				strcpy(stmp->addrstr, tmpst[1]);
				stmp->addrstr[strlen(tmpst[1])] = '\0';
				free(tmpst[0]);

				stmp->server_id = rand() % 1000;

				stmp->session_id = (char*)calloc(KEYL, sizeof(char));
				for(i=0;i<(KEYL - 2);++i){
					stmp->session_id[i] = (rand() % (0x7a - 0x61)) + 0x61;
				}
				stmp->session_id[(KEYL - 1)] = '\0';
	
				stmp->seckey = (char*)calloc(80, sizeof(char));
				enc64(stmp->seckey, 80, msg, 8);
				stmp->pid = 0;
				stmp->apidir = calloc(5,sizeof(char));
				strncpy(stmp->apidir, "/api",4);
				stmp->nboards = 0;

				conns[nconns++] = stmp;
			}
			if(ccv > 7 && cistrncmp("config", cmd, 6) == 0 && (niter=strtoull(&cmd[6],nil,0)) < nconns){
				cl = 0;
 
				dialb = dialsite(conns[niter]);

				if(dialb == nil){
					free(cmd);
					break;
				}
				/*Dialsite checks addr/dial str validity*/
			 	/*Bprint does not seem to work: if we Bflush then we've flushed
				out all the return data from the site, if we don't Bflush then
				the reads to the buffer consume the string meant to be sent to the
				site. A more proper solution would be two buffers (OREAD, OWRITE) but
				fprinting to the FD works too*/
 
				if(conns[niter]->dir !=nil)
					fprint(Bfildes(dialb),"GET /%s HTTP/1.1\r\nHost: %s\r\nUser-Agent: Mozilla/69.0 (compatible; hjdicks; 9front 1.0)\r\n\r\n",conns[niter]->dir, conns[niter]->addrstr);
				else
					fprint(Bfildes(dialb),"GET / HTTP/1.1\r\nHost: %s\r\nUser-Agent: Mozilla/69.0 (compatible; hjdicks; 9front 1.0)\r\n\r\n",conns[niter]->addrstr);

				rcod = readhttp(dialb, &cl);
				if(rcod == ENOTFOUND){
					free(cmd);
					Bterm(dialb);
					break;
				}
				if(rcod == EMOVED){
					urlrdr = nil;
					for(;;){
						line = Brdstr(dialb,'',1);
						if(Blinelen(dialb) < 2){
							break;
						}

						if(Blinelen(dialb) > 11 && cistrncmp("Location: ", &line[1], 10) == 0)
							urlrdr = url(&line[11], Blinelen(dialb) - 10);
						
 
		

						free(line);
					}

					if(urlrdr == nil){
						free(cmd);
						break;
					}
					Bterm(dialb);

					dialb = dialurl(urlrdr, conns[niter]);
					if(dialb == nil){
						freeurl(urlrdr);
						free(cmd);
					
	break;
					}
					fprint(Bfildes(dialb), "GET /%s HTTP/1.1\r\nHost: %s\r\n\r\n", urlrdr->path, ((urlrdr->host == nil || urlrdr->host[0] == '\0')?conns[niter]->addrstr:urlrdr->host));
					freeurl(urlrdr);
					rcod = readhttp(dialb, &cl);
				}

				switch(rcod){
					case EJSON:
						rcod = recievejson(dialb, cl, tmp);
						break;
					case EHTML:
						rcod = recievehtml(dialb, cl, tmp);
						break;
					default:
						rcod = EGENERIC;
						break;
				}

				switch(rcod){
					case EJSON:
					case EHTML:
						if(conns[niter]->config != nil)
							jsonfree(conns[niter]->config);

						conns[niter]->config = tmp[0];
						break;
					default:
						if(tmp[0] != nil)
							jsonfree(tmp[0]);
						break;
				}

				
				Bterm(dialb);
				if(conns[niter]->config != nil && (tmp[0] = jsonbyname(conns[niter]->config, "config")) != nil && (tmp[1] = jsonbyname(tmp[0], "SOCKET_PATH")) != nil && tmp[1]->t == JSONString)
					conns[niter]->dir = tmp[1]->s;
				
				if(conns[niter]->config != nil && (tmp[0] = jsonbyname(conns[niter]->config, "config")) != nil && (tmp[1] = jsonbyname(tmp[0], "BOARDS")) != nil && tmp[1]->t == JSONArray){
					for(i=0,jltmp=tmp[1]->first;jltmp != nil;++i)
						jltmp = jltmp->next;
					conns[niter]->nboards = i;
					conns[niter]->threads = calloc(i, sizeof(JSON*));

				}

 			}
			if(ccv > 7 && cistrncmp("mconfig", cmd, 7) == 0&& (niter=strtoull(&cmd[7],&tmpst[0],0)) < nconns){
				tmp[0] = jsonparse(tmpst[0]);
				if(tmp[0] == nil){
					free(cmd);
					break;
				}
				if(conns[niter]->config != nil)
					jsonfree(conns[niter]->config);
				conns[niter]->config = tmp[0];
				if(conns[niter]->config != nil && (tmp[0] = jsonbyname(conns[niter]->config, "config")) != nil && (tmp[1] = jsonbyname(tmp[0], "SOCKET_PATH")) != nil && tmp[1]->t == JSONString){
					conns[niter]->dir = tmp[1]->s;
				}
				if(conns[niter]->config != nil && (tmp[0] = jsonbyname(conns[niter]->config, "config")) != nil && (tmp[1] = jsonbyname(tmp[0], "BOARDS")) != nil && tmp[1]->t == JSONArray){
					for(i=0,jltmp=tmp[1]->first;jltmp != nil;++i)
						jltmp = jltmp->next;
					conns[niter]->nboards = i;
					conns[niter]->threads = calloc(i, sizeof(JSON*));

				}
			}
			if(ccv > 5 && cistrncmp("search", cmd,6) == 0){
				tmp[0] = jsonparse(&cmd[7]);
				if(tmp[0] == nil){
					free(cmd);
					break;
				}
				tmp[1] = jsonbyname(tmp[0], "site");
				if(tmp[1] != nil && tmp[1]->t==JSONString){
					for(niter=0;niter<nconns;++niter){
						if(cistrcmp(conns[niter]->dialstr, tmp[1]->s) == 0){
							Bprint(conb, "%uld\n",niter);
						}
					}
				}
				


				free(tmp[0]);
			}
			if(ccv > 3 && cistrncmp("dump", cmd, 4) == 0){

				if(ccv > 5 && (niter = strtoull(&cmd[4], nil, 0)) < nconns){
					if(conns[niter]->config != nil)
						Bprint(conb,"SNUM: %uld \n\tDSTR: %s\n\tASTR: %s\n\tSEC: %s\n\tSER: %ud\n\tSES: %s\n\tPID: %d\nCONF: %J\n",niter,conns[niter]->dialstr, conns[niter]->addrstr, conns[niter]->seckey, conns[niter]->server_id, conns[niter]->session_id, conns[niter]->pid, conns[niter]->config);
					else
						Bprint(conb,"SNUM: %uld \n\tDSTR: %s\n\tASTR: %s\n\tSEC: %s\n\tSER: %ud\n\tSES: %s\n\tPID: %d \n",niter,conns[niter]->dialstr, conns[niter]->addrstr, conns[niter]->seckey, conns[niter]->server_id, conns[niter]->session_id, conns[niter]->pid);

					for(i=0;i<conns[niter]->nboards;++i)
						if(conns[niter]->threads[i] != nil)
							Bprint(conb, "THREADS: %J\n", conns[niter]->threads[i]);
					Bprint(conb, "\n\n");
				}
				else{
					Bprint(conb, "NCONNS: %uld\n", nconns);
					for(niter=0;niter<nconns;++niter){
						if(conns[niter]->config != nil)
							Bprint(conb,"SNUM: %uld \n\tDSTR: %s\n\tASTR: %s\n\tSEC: %s\n\tSER: %ud\n\tSES: %s\n\tPID: %d\nCONF: %J\n\n",niter,conns[niter]->dialstr, conns[niter]->addrstr, conns[niter]->seckey, conns[niter]->server_id, conns[niter]->session_id, conns[niter]->pid, conns[niter]->config);
						else
							Bprint(conb,"SNUM: %uld \n\tDSTR: %s\n\tASTR: %s\n\tSEC: %s\n\tSER: %ud\n\tSES: %s\n\tPID: %d \n",niter,conns[niter]->dialstr, conns[niter]->addrstr, conns[niter]->seckey, conns[niter]->server_id, conns[niter]->session_id, conns[niter]->pid);


						for(i=0;i<conns[niter]->nboards;++i)
							if(conns[niter]->threads[i] != nil)
								Bprint(conb, "THREADS: %J\n", conns[niter]->threads[i]);
						Bprint(conb, "\n\n");

					}
				}

			}
			if(ccv > 4 && (strncmp("open", cmd, 4)) == 0 && (niter=strtoull(&cmd[4], &tmpst[0],0)) < nconns){
				if(conns[niter]->config == nil){
					free(cmd);
					break;
				}
				tmp[0] = jsonbyname(conns[niter]->config, "SOCKET_URL");
				tmp[1] = jsonbyname(conns[niter]->config, "SOCKET_PATH");
				if((tmp[0] == nil || tmp[0]->t != JSONString) && (tmp[1] == nil || tmp[1]->t != JSONString)){
					
					free(cmd);
					break;
				}
				if((tmp[0] == nil || tmp[0]->t != JSONString) && tmp[1]->t == JSONString){
					dialb = dialsite(conns[niter]);
					tmpst[0] = smprint("%s/%d/%s/websocket",tmp[1]->s,conns[niter]->server_id,conns[niter]->session_id);

				}
				else if((tmp[1] == nil || tmp[1]->t != JSONString) && tmp[0]->t == JSONString){
					urlrdr = url(tmp[1]->s,strlen(tmp[1]->s));
					dialb = dialurl(urlrdr, conns[niter]);
					freeurl(urlrdr);

					/*Untested: No sites seem to support this, contact if in error*/
					tmpst[0] = smprint("%s",tmp[0]->s);
				}
				else{
					free(cmd);
					break;
				}
				if(dialb == nil){
					free(cmd);
					break;
				}
				fprint(Bfildes(dialb),"GET %s HTTP/1.1\r\nHost: %s\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Key: %s\r\nSec-WebSocket-Protocol: chat,superchat\r\nSec-WebSocket-Version: 13\r\nOrigin:%s\r\n\r\n",tmpst[0],conns[niter]->addrstr,conns[niter]->seckey,conns[niter]->addrstr);
				conns[niter]->pid = proccreate(wsproc,wspp,1024000);
				sendp(wspp,wsp);
				sendul(wsp,conns[niter]->pid);
				sendp(wspp, dialb);
				free(tmpst);

			}
			if(ccv > 6 && strncmp("threads", cmd, 7) == 0&& (niter=strtoull(&cmd[7],&tmpst[0],0)) < nconns){
				if(conns[niter]->config == nil){
					free(cmd);
					break;
				}
				if(conns[niter]->apidir == nil){
					free(cmd);
					break;
				}
				dialb = dialsite(conns[niter]);
				if(dialb == nil){
					free(cmd);
					break;
				}
				i = atoi(tmpst[0]);
				if(i >= conns[niter]->nboards){
					free(cmd);
					break;
				}
				tmp[0] = jsonbyname(conns[niter]->config, "config");
				if(tmp[0] == nil){
					free(cmd);
					break;
				}
				tmp[1] = jsonbyname(tmp[0], "BOARDS");
				if(tmp[1] == nil || tmp[1]->first == nil){
					free(cmd);
					break;
				}
				jltmp = tmp[1]->first;
				for(j=0,jltmp=tmp[1]->first;j<i;++j,jltmp=jltmp->next)
					if(jltmp == nil || jltmp->val->t != JSONString){
						free(cmd);
						break;
					}


				/*Need board from JSON*/
				fprint(Bfildes(dialb), "GET %s/board/%s HTTP/1.1\r\nHost: %s\r\nUser-Agent: Mozilla/69.0 (compatible; hjdicks; 9front 1.0)\r\n\r\n",conns[niter]->apidir, jltmp->val->s );
				rcod = readhttp(dialb, &cl);
				if(rcod != EJSON){
					free(cmd);
					break;
				}
				rcod = recievejson(dialb, cl, &(conns[niter]->threads[i]));
				if(rcod != EJSON || conns[niter]->threads[i] == nil){
					free(cmd);
					break;
				}

				
			}

			Bflush(conb);
			free(cmd);
			break;
	
		case 2:
			cmd = recvp(wspp);
			Bprint(conb, "%s", cmd);
			Bflush(conb);
			free(cmd);
			break;
	}
	

}

/*This differs from Url in mothra/url.c in that 
it supports only a subset of valid URLs excluding
basic auth, queries, and fragmentation*/
void
freeurl(Url* URL){
	if(URL->scheme != nil)
		free(URL->scheme);
	if(URL->host != nil)
		free(URL->host);
	if(URL->port != nil)
		free(URL->port);
	if(URL->path != nil)
		free(URL->path);
	free(URL);
}
Url*
url(char* src, int len){
	int i,j;
	Url* ret;
	ret = calloc(1, sizeof(Url));
	for(i=0;i<len;++i){
		if(i<(len-3) && strncmp(&src[i],"://",3) == 0){
			ret->scheme = calloc(i+1, sizeof(char));
			strncpy(ret->scheme,src,i);
			ret->scheme[i] = '\0';
			i+=3;
			break;
		}

	}
	if(ret->scheme == nil){
		i=0;
	}
	for(j=i;j<len;++j){
		if(src[j] == ':' || src[j] == '/'){
			ret->host = calloc(1 + (j-i), sizeof(char));
			strncpy(ret->host, &src[i], (j-i));
			ret->host[j-i] = '\0';
			break;
		}
	}
	if(j==len && ret->host == nil){
		ret->host = calloc(len - i + 1, sizeof(char));
		strncpy(ret->host, &src[i], len - i);
		ret->host[len-i] = '\0';
		return ret;
	}

	i=j;
	if(src[i] == ':' && i++<(len-1)){
		for(j=i;j<len;++j){
			if(src[j] == '/'){
				ret->port = calloc(1+(j-i), sizeof(char));
				strncpy(ret->port, &src[i], j-i);
				ret->port[j-i] = '\0';
				break;
			}
		}
		if(j==len && ret->port == nil){
			ret->port = calloc(len-i + 1, sizeof(char));
			strncpy(ret->port, &src[i], len-i);
			ret->port[len-i] = '\0';
			return ret;
		}
	}

	i=j;
	if(src[i] == '/' && i++<(len-1)){
		ret->path = calloc(len-i + 1, sizeof(char));
		strncpy(ret->path, &src[i], len-i);
		ret->path[len-i] = '\0';	
	}
	return ret;
}
