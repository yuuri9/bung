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
	fprint(1, "urlparser -u url\n");
	threadexitsall(nil);
}
void
threadmain(int argc, char** argv){
	Url* URL;
	char* src;
	int set;
	set =0;
	ARGBEGIN{
		case 'u':
			src = EARGF(usage());
			set = 1;
			continue;
		default:
			src = EARGF(usage());
			continue;
	}ARGEND
	if(set==0){
		if(argc > 0)
			src = argv[0];
		else
			usage();
	}

	if(src == nil)
		usage();

	URL = url(src, strlen(src));
	fprint(1, "Scheme: %s\nUser: %s\nPass: %s\nHost: %s\nPort: %s\nPath: %s\nQuery: %s\nFragment: %s\n", URL->scheme, URL->user, URL->pass, URL->host, URL->port, URL->path, URL->query, URL->fragment);	
	freeurl(URL);
}
