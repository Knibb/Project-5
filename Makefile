# Compiler
CXX = g++

# Compiler flags
CXXFLAGS = -Wall -std=c++11

# Executables
OSS = oss
WORKER = worker

all: $(OSS) $(WORKER)

$(OSS): oss.cpp resource_descriptor.h
	$(CXX) $(CXXFLAGS) -o $(OSS) oss.cpp

$(WORKER): worker.cpp resource_descriptor.h
	$(CXX) $(CXXFLAGS) -o $(WORKER) worker.cpp

clean:
	rm -f $(OSS) $(WORKER)
