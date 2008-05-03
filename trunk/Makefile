CFLAGS	= -g -O2 -Wall
LDFLAGS	= -g -lusb -lm

all: amoxiflash

amoxiflash: amoxiflash.c
	gcc $(CFLAGS) -o amoxiflash amoxiflash.c $(LDFLAGS)

clean:
	rm amoxiflash amoxiflash.o