CFLAGS	= -g -O2 -Wall
LDFLAGS	= -g -lusb

all: amoxiflash

amoxiflash: amoxiflash.c
	gcc $(CFLAGS) -o amoxiflash amoxiflash.c $(LDFLAGS)
