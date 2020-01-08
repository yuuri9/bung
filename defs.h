typedef struct Post Post;
typedef struct Chunk Chunk;
typedef struct WSFrame WSFrame;
typedef struct WSFramel WSFramel;

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
	WSFrame;
	WSFramel* next;
	int fin;
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
};
enum{
	WPING = 0x9,
	WPONG = 0xA,
	WTEXT = 0x1,
	WBIN = 0x2,
	WHUP = 0x8, /*Technically wrong, but sockjs doesn't give a shit*/
	WBAD = 0x4, /*What even does this signal, fucking sockjs*/
	WCONT = 0x0,
};
