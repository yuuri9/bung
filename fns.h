void
eresized(int New);
void
utimer(void* arg);
void
kbdfsio(void* arg);
void
drawpost(Post* post, Image* canvas, Image** colors, Font* font);
int
tlswrap(int fd, char *servername);
int
readhttp(Biobuf* net, long* resp );
int
recievejson(Biobuf* net, long cl, JSON** ret);
void
printjson(JSON* root);
int
recievehtml(Biobuf* net, long cl, JSON** ret);
int
recievews(Biobuf* net, int fd);

void
wsproc(void* arg);
void
consfn(void* arg);
void
jsondriver(void* arg);
