CFLAGS	= -g -O2 -Wall -arch i386
LDFLAGS	= -g -lusb -lm

all: amoxiflash

amoxiflash: amoxiflash.c ecc.c amoxiflash.h getopt.c
	gcc $(CFLAGS) -o amoxiflash amoxiflash.c ecc.c getopt.c $(LDFLAGS)

clean:
	rm amoxiflash
