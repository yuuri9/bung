</$objtype/mkfile

TARG=bung
all:V: bung dialr

dialr: dong.$O fns.$O 
	$LD -o dialr dong.$O fns.$O

bung: main.$O fns.$O
	$LD -o bung main.$O fns.$O

main.$O: main.c
	$CC $CFLAGS main.c 
dong.$O: dong.c
	$CC $CFLAGS dong.c

fns.$O: fns.c
	$CC $CFLAGS fns.c
clean:V:
	rm -f *.[$OS] [$OS].out bung dialr
