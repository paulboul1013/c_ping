flags = -O2 -Wall -std=c2x

.PHONY:all clean

all: clean ping

ping: ping.o utils.o
	cc $(flags) $^ -o $@

ping.o: ping.c ping.h
	cc $(flags) -c $<

utils.o: utils.c utils.h
	cc $(flags) -c $<

clean:
	rm -f *.o ping