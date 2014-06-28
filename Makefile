CFLAGS=-c -Wall
LDFLAGS=-L. -ldl
LEGACYSOURCES=
SOURCES=TestApp.cpp
OBJECTS=$(SOURCES:.cpp=.o) $(LEGACYSOURCES:.c=.o)
EXECUTABLE=TestApplication
G++=g++
all: $(SOURCES) $(EXECUTABLE)
TestApp.cpp: MemoryAbstraction.h
	touch TestApp.cpp
$(EXECUTABLE): $(OBJECTS)
	g++ --std=c++0x -g $(OBJECTS) $(LDFLAGS) -Wl,-rpath . -o $@
.c.o:
	gcc -fPIC -g $(CFLAGS) $< -o $@
.cpp.o:
	g++ -std=c++0x -g $(CFLAGS) $< -o $@

