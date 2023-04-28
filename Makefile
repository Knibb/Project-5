CC=g++
CFLAGS=-std=c++11 -pthread
DEPS=
OBJ_OSS=oss.o resource_descriptor.o
OBJ_WORKER=worker.o resource_descriptor.o

%.o: %.cpp $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

oss: $(OBJ_OSS)
	$(CC) -o $@ $^ $(CFLAGS)

worker: $(OBJ_WORKER)
	$(CC) -o $@ $^ $(CFLAGS)

clean:
	rm -f oss worker *.o
