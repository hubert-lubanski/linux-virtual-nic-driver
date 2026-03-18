CC = gcc
CFLAGS := -O2 -Wall -Werror


transmitter: transmitter.c
	$(CC) $(CFLAGS) $< -o $@ -luring 

debug: transmitter.c
	$(CC) $(CFLAGS) -DDEBUG=1 $< -o transmitter -luring 

clean:
	rm -rf transmitter
	rm -rf debug.out

.PHONY: clean