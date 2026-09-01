CC = g++
CFLAGS = -Wall -Werror
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