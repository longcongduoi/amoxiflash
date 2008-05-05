CFLAGS	= -g -O2 -Wall
LDFLAGS	= -g -lusb -lm

all: amoxiflash

amoxiflash: amoxiflash.c ecc.c
	gcc $(CFLAGS) -o amoxiflash amoxiflash.c ecc.c $(LDFLAGS)

clean:
	rm amoxiflash amoxiflash.o