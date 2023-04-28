CC=g++
CFLAGS=-std=c++11 -Wall -Wextra
LDFLAGS=-lrt
DEPS = resource_descriptor.h

all: oss worker

oss: oss.cpp $(DEPS)
	$(CC) $(CFLAGS) -o oss oss.cpp $(LDFLAGS)

worker: worker.cpp $(DEPS)
	$(CC) $(CFLAGS) -o worker worker.cpp $(LDFLAGS)

clean:
	rm -f oss worker
