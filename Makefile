NAME=upd72020x-load
CC ?= gcc

.PHONY: all clean

all: $(NAME)

clean:
	rm $(NAME)

$(NAME): upd72020x-load.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^
