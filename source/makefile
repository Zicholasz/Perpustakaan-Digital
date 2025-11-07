CC=gcc
CFLAGS=-Wall

SRCS = main.c admin.c peminjam.c library.c ui.c view.c
OBJS = $(SRCS:.c=.o)

all: main

main: $(OBJS)
    $(CC) $(OBJS) -o main

clean:
    rm -f *.o main
