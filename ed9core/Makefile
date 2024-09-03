.PHONY: all clean run

all: ed9t

ed9t: ed9t.c
	$(CC) ed9t.c -o ed9t -Wall -Wextra -pedantic -std=c99

run: ed9t
	./ed9t

clean:
	rm -f ed9t.exe ed9t