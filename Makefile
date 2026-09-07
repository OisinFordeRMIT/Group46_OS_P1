# COSC1114 Project 1 - Group 46
# make all   -> builds mmcopier and mscopier
# make clean -> removes the executables and object files

CC = g++
# -std=c++11 is required: the sources use nullptr, to_string and <deque>,
# and the teaching servers' g++ does not default to C++11.
CFLAGS = -Wall -Werror -std=c++11 -pthread
LDFLAGS = -lpthread

TARGETS = mmcopier mscopier

all: $(TARGETS)

mmcopier: mmcopier.cpp
	$(CC) $(CFLAGS) -o mmcopier mmcopier.cpp $(LDFLAGS)

mscopier: mscopier.cpp
	$(CC) $(CFLAGS) -o mscopier mscopier.cpp $(LDFLAGS)

clean:
	rm -f $(TARGETS) *.o

.PHONY: all clean
