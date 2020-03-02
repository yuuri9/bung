typedef struct Post Post;
typedef struct Chunk Chunk;
typedef struct WSFrame WSFrame;
typedef struct WSFramel WSFramel;
typedef struct Site Site;
typedef struct Url Url;

struct Post {
	uint error;
	uint comp;
	Image* Post;
	uint img;
	Image* pimg;
	char* date;
	char* name;
	char* pnum;
	char* msg;
	uint bl;
	char** blinks;
};
struct Site{
	char* dialstr;
	char* addrstr;

	char* seckey;

	uint server_id;
	char* session_id;

	char* dir;

	JSON* config;

	int pid;

};

struct Url
{
	char	*scheme;
	char	*host;
	char	*port;
	char	*path;
};

struct Chunk{
	char* str;
	long len;
	int n;
	Chunk* next;
};
struct WSFrame{
	int err;

	int fin;
	int op;
	int mask;
	int key;

	uvlong len;
	char* buf;
};
struct WSFramel{
	WSFrame* elm;
	WSFramel* next;
	int end;
};
enum {
	IBackground,
	ILink,
	IText,
	IPost,
	IPostactive,
	IName,
	IReference,
	IHeader,
	ITitle,
};
enum {
	KEYL = 10,
	MAXCONNS = 63,
};
enum {
	EGENERIC,
	ENOTFOUND,
	ESWITCHING,
	EOK,
	ETOOBIG,
	ESIZEWRONG,
	ESUCCESS,
	EJSON,
	ENOMETHOD,
	EJSONINVALID,
	EHTML,
	EHTMLINVALID,
	EDIALFAIL,
	EMOVED,
};
enum {
	WPING = 0x9,
	WPONG = 0xA,
	WTEXT = 0x1,
	WBIN = 0x2,
	WHUP = 0x8,  
	WBAD = 0x4,  
	WCONT = 0x0,
};

/*Imported*/
enum {
	MGINVALID = 0,
	MGINSERT_POST = 2,
	MGUPDATE_POST = 3,
	MGFINISH_POST = 4,
	// Legacy?
	MGCATCH_UP = 5,
	MGINSERT_IMAGE = 6,
	MGSPOILER_IMAGES = 7,
	MGDELETE_IMAGES = 8,
	MGDELETE_POSTS = 9,
	MGDELETE_THREAD = 10,
	MGLOCK_THREAD = 11,
	MGUNLOCK_THREAD = 12,
	MGREPORT_POST = 13,

	MGIMAGE_STATUS = 31,
	MGSYNCHRONIZE = 32,
	MGEXECUTE_JS = 33,
	MGUPDATE_BANNER = 35,
	MGONLINE_COUNT = 37,
	MGHOT_INJECTION = 38,
	MGNOTIFICATION = 39,
	MGRADIO = 40,
	MGRESYNC = 41,

	MGMODEL_SET = 50,
	MGCOLLECTION_RESET = 55,
	MGCOLLECTION_ADD = 56,
	MGSUBSCRIBE = 60,
	MGUNSUBSCRIBE = 61,
	MGGET_TIME = 62,

	MGINPUT_ROOM = 20,
	MGMAX_POST_LINES = 30,
	MGMAX_POST_CHARS = 2000,
	MGWORD_LENGTH_LIMIT = 300,

	MGS_NORMAL = 0,
	MGS_BOL = 1,
	MGS_QUOTE = 2,
	MGS_SPOIL = 3
};
