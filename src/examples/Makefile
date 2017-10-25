CC=g++
CFLAGS=-Wall -g -ggdb -I/data/sdr/limesdr/LimeSuite/src -I/data/sdr/limesdr/LimeSuite/src/lms7002m 

LDFLAGS=-L/data/sdr/limesdr/LimeSuite/build/src -llimesuite -lsqlite3 -lpthread -lusb-1.0

OBJS = basicRX.o 

hackrf_tcp: $(OBJS)
	$(CC) $(OBJS) $(CFLAGS) $(LDFLAGS) -o basicRX
