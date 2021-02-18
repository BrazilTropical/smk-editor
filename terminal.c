#include "terminal.h"

#define CTRL_KEY(k) ((k) & 0x1f)	//CTRL + q to quit

struct termios orig_termios;

//*************///
//*** Input	***///
//*************///
char editorReadKey()
{
	int nRead;
	char c;

	while((nRead = read(STDIN_FILENO, &c, 1) != 1))
		if(nRead == -1 && errno != EAGAIN)
			die("read");
	return c;
}

void editorProcessKeyPress()
{
	char c = editorReadKey();

	printf("%c", c);
	switch(c)
	{
		case CTRL_KEY('q'):
			clearAndReposition();
			exit(0);
			break;
		default:
			break;
	}
}

//kills on error
void die(const char *s)
{
	clearAndReposition();

	perror(s);
	exit(-1);
}

//Resets to original terminal
void disableRawMode()
{
	if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) == -1)
		die("tcsetattr");
}

//Doesn't print on terminal
void enableRawMode()
{
	if(tcgetattr(STDIN_FILENO, &orig_termios) == -1)	//error on tcget
		die("tcgetattr");

	atexit(disableRawMode);	//used to call disableRawMode() on exit.

	struct termios raw = orig_termios;

  	raw.c_iflag &= ~(ICRNL | IXON); //input flags
	raw.c_oflag &= ~(OPOST);	//disables post processing of output
	raw.c_cflag |= (CS8);	//char size 8
	raw.c_lflag &= ~(ECHO | ICANON |IEXTEN | ISIG);	//negates bits corresponding to ECHO | ICANON
	raw.c_cc[VMIN] = 0;	//cc-> control chars
	raw.c_cc[VTIME] = 1;

	if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)	//error on tcset
		die("tcsetattr");
}

//*************///
//** Output  **///
//*************///

//Clears screen and repositions cursor
//Used on die and as refresh
void editorRefreshScreen()
{
	clearAndReposition();

	editorDrawRows();

	write(STDOUT_FILENO, "\x1b[H", 3);
}

//Draws tilde chars on the first column, like vim
void editorDrawRows()
{
	int y;
	for(y = 0; y < 24; ++y)
    	write(STDOUT_FILENO, "~\r\n", 3);	//tilde char is default
}

//Clears screen and repositions cursor at the top;
void clearAndReposition()
{
	write(STDOUT_FILENO, "\x1b[2J", 4);
	write(STDOUT_FILENO, "\x1b[H", 3);
}