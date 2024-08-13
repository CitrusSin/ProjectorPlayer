CC := riscv64-unknown-linux-musl-gcc
CFLAGS := -O2 -Wall

objects = main.o framebuffer.o
target = ppl

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(target): $(objects)
	$(CC) $^ -o $@

.PHONY: all
all: $(target)

.PHONY: clean
clean:
	- rm $(objects)
	- rm $(target)