#include <u.h>
#include <libc.h>
#include <draw.h>
#include <bio.h>

#include <json.h>

#include <thread.h>
#include <keyboard.h>
#include <mouse.h>


#include "defs.h"
#include "fns.h"

void
usage(void){
	fprint(1, "dialr -s dialstring -d directory [-c] /srv/pipe [-w] [-t]\n");
	threadexitsall(nil);
}
/*1 MB*/
int mainstacksize = 1024000;

void
threadmain(int argc, char** argv){

	char* dialstring, *dirst, *addrstr,   *seckey, *session_id, *cons;
	char* buf, *consbuf;
	char cdir[40];
	Channel* c,*v, *conu, *condat;
	uchar* msg; 
	void** resp;
	JSONEl* jst;
	ulong wsr,csr;

	long* cl;

	Biobuf* net,*snd;
	int fd, server_id,t,n,cfd, conspid;
	uint i, w;

	dialstring = "net!daijoubu.cf!443";
	msg = (uchar*)"bunisgay";
	dirst = "";
	w = 1;
	dirst = "hana/";
	t = 1;
	cons = "/srv/dialr.cmd";

	ARGBEGIN{
		case 's':
			dialstring = EARGF(usage());
			break;
		case 'd':
			dirst = EARGF(usage());
			break;
		case 'w':
			w = 0;
			break;
		case 't':
			t = 0;
			break;
	 	case 'c':
			cons = EARGF(usage());
			break;
	}ARGEND

	JSONfmtinstall();
	srand(nsec());

	seckey = (char*)calloc(80, sizeof(char));
	enc64(seckey, 80, msg, 8);
	server_id = rand() % 1000;

	session_id = (char*)calloc(KEYL, sizeof(char));
	for(i=0;i<(KEYL - 2);++i){
		session_id[i] = (rand() % (0x7a - 0x61)) + 0x61;
	}
	session_id[(KEYL - 1)] = '\0';

	/*If argv doesn't nullterm we have kernel problems*/
 	addrstr = calloc(strlen(dialstring) + 1, sizeof(char));
	strcpy(addrstr, dialstring);
	for(i=0;i<strlen(addrstr);++i){
		if(addrstr[i] == '!'){
			addrstr = &addrstr[i+1];
			break;
		}
	}
	for(i=0;i<strlen(addrstr);++i){
		if(addrstr[i] == '!'){
			addrstr[i] = '\0';
		}
	}


	conu = chancreate(sizeof(ulong),0);
	condat = chancreate(sizeof(char*), 0);
	conspid = proccreate(consfn, condat, 8192);
	sendp(condat, conu);
	sendp(condat, cons);

	v = chancreate(sizeof(ulong), 0);
	c = chancreate(sizeof(char*),0);
	proccreate(jsondriver, c,1024000);

	sendp(c,v);
	Alt a[] = {
		{v, &wsr, CHANRCV},
		{conu, &csr, CHANRCV},
		{nil,nil,CHANEND},
	};
	for(;;){
	switch(alt(a)){
		









	}
	}


	fd = dial(dialstring,0,&cdir[0],&cfd);
	fprint(1, "DIR: %s\n", cdir);
	if (fd < 0)
		sysfatal("Couldn't dial: %r ");
	if(t == 1)
		fd = tlswrap(fd, addrstr);
	fprint(2, "DIR: %s\nKEY: %s\nADDR: %s\nSESSION: %s\nSERVER: %d\n",dirst, seckey, addrstr,session_id, server_id);


	if(w == 1){
		/*fprint(fd, "GET /%s/%d/%s/websocket HTTP/1.1\r\nHost: %s\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Key: %s\r\nSec-WebSocket-Protocol: chat,superchat\r\nSec-WebSocket-Version: 13\r\nOrigin:%s\r\n\r\n", dirst,server_id,session_id, addrstr,seckey,addrstr);*/
		
		fprint(fd, "GET / HTTP/1.1\r\nHost: %s\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Key: %s\r\nSec-WebSocket-Protocol: chat,superchat\r\nSec-WebSocket-Version: 13\r\nOrigin:%s\r\n\r\n", addrstr,seckey,addrstr);
		
		net = Bfdopen(fd, OREAD);
 		v = chancreate(sizeof(ulong), 0);
		c = chancreate(sizeof(WSFrame*),0);
		proccreate(wsproc, c,1024000);
		sendp(c,v);
		sendp(c, net);
 		conu = chancreate(sizeof(ulong),0);
		condat = chancreate(sizeof(char*), 0);
		conspid = proccreate(consfn, condat, 8192);
		sendp(condat, conu);
		sendp(condat, cons);

		Alt a[] = {
			{v, &wsr, CHANRCV},
			{conu, &csr, CHANRCV},
			{nil,nil,CHANEND},
		};
		for(;;){
			switch(alt(a)){
				/*They hung up on us! All men back to their stations, manuver restart*/
				case 0:
					if(wsr != 42069)
						break;
					Bterm(net);
					fd = dial(dialstring,0,&cdir[0],&cfd);
					fprint(1, "DIR: %s\n", cdir);
					if (fd < 0)
						sysfatal("Couldn't dial: %r ");
					if(t == 1)
						fd = tlswrap(fd, addrstr);
					fprint(2, "DIR: %s\nKEY: %s\nADDR: %s\nSESSION: %s\nSERVER: %d\n",dirst, seckey, addrstr,session_id, server_id);
					fprint(fd, "GET /%s/%d/%s/websocket HTTP/1.1\r\nHost: %s\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Key: %s\r\nSec-WebSocket-Protocol: chat,superchat\r\nSec-WebSocket-Version: 13\r\nOrigin:%s\r\n\r\n", dirst,server_id,session_id, addrstr,seckey,addrstr);

					net = Bfdopen(fd, OREAD);
					proccreate(wsproc, c,1024000);
					sendp(c,v);
					sendp(c, net);
					break;
				case 1:
					consbuf = recvp(condat);
					if(csr>7 && strncmp(consbuf, "sendtf ", 7) == 0){
						sendul(v, csr );

						/*Malloc's and copies*/
						chanprint(c,"%s",consbuf);

					}
					if(csr==4 && cistrncmp(consbuf, "halt", 4) == 0){
						/*Graceful shutdown code goes here*/


						threadexitsall(nil);
					}
					free(consbuf);
					break;
			}
		}
	 
		threadexitsall(nil);
	}



	/*Dial normal*/
	fprint(fd, "GET /%s HTTP/1.1\nHost: %s\nUser-Agent: Mozilla/4.0 (compatible; hjdicks; 9front 1.0)\n\n", dirst, addrstr);




	net = Bfdopen(fd, OREAD);

	cl = (long*)calloc(1, sizeof(long));
	i = readhttp(net, cl);

	fprint(1, "I JSON?? %s\n", (i == EJSON)?"yes":"no");
	fprint(1, "I HTML?? %s\n", (i == EHTML)?"yes":"no");

	switch(i){
		case EJSON:
			resp = (JSON**)calloc(1, sizeof(JSON*));
  			i = recievejson(net, cl[0], (JSON**)resp);
 
			break;
		case EHTML:
			resp = (JSON**)calloc(1, sizeof(JSON*));
			i = recievehtml(net, cl[0], (JSON**)resp);

			break;
		default:
			i = EGENERIC;
			break;
	}
	free(cl);


	switch(i){
		case EJSON:
			fprint(1, "J-TYPE: %d\n",((JSON*)resp[0])->t);

			printjson(resp[0]);
			jsonfree(resp[0]);
			free(resp);
			break;
		case EHTML:
			fprint(1, "FJSON: %J \n\n", ((JSON*)resp[0]));
			jsonfree(resp[0]);
			free(resp);
			break;
		case ESIZEWRONG:
			
			free(resp);
			fprint(2, "SIZEWRONG \n");

			break;
		case EJSONINVALID:
		case EHTMLINVALID:
			free(resp);
			fprint(2, "INVALID INPUT\n");
			break;
	}

	Bterm(net);
	threadexits(nil);
	
}
