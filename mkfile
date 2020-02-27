</$objtype/mkfile

TARG=bung
all:V: bung dialr urlparser

dialr: dong.$O fns.$O 
	$LD -o dialr dong.$O fns.$O
urlparser: urlparser.$O fns.$O
	$LD -o urlparser urlparser.$O fns.$O

bung: main.$O fns.$O clientfns.$O
	$LD -o bung main.$O fns.$O clientfns.$O

main.$O: main.c 
	$CC $CFLAGS main.c 

dong.$O: dong.c
	$CC $CFLAGS dong.c

fns.$O: fns.c
	$CC $CFLAGS fns.c

clientfns.$O: clientfns.c
	$CC $CFLAGS clientfns.c

urlparser.$O: urlparser.c
	$CC $CFLAGS urlparser.c

clean:V:
	rm -f *.[$OS] [$OS].out bung dialr urlparser
