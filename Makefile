NAME=upd72020x-load

.PHONY: all clean

all: $(NAME)

clean:
	rm $(NAME)

$(NAME): upd72020x-load.c
	gcc -static -o $@ $^
