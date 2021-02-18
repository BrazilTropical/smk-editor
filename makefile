smk: smk.c terminal.c terminal.h
	$(CC) smk.c terminal.c -o smk -Wall -Wextra -pedantic -std=c99