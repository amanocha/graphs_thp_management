CXX=g++
CXXFLAGS=-std=c++11 -lnuma

TARGETS= main simple

all: $(TARGETS)

$(TARGETS):
	$(CXX) $(CXXFLAGS) -o $@ $@.cpp

clean:
	rm $(TARGETS)
