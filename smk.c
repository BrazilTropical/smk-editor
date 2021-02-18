#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <unistd.h>
#include <termios.h>

struct termios orig_termios;

//Resets to original terminal
void disableRawMode()
{
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

//Doesn't print on terminal
void enableRawMode()
{
	tcgetattr(STDIN_FILENO, &orig_termios);

	atexit(disableRawMode);	//used to call disableRawMode() on exit.

	struct termios raw = orig_termios;
  	
  	raw.c_iflag &= ~(ICRNL | IXON); //input flags
	 
	raw.c_oflag &= ~(OPOST);	//disables post processing of output

	raw.c_cflag |= (CS8);	//char size 8

	raw.c_lflag &= ~(ECHO | ICANON |IEXTEN | ISIG);	//negates bits corresponding to ECHO | ICANON

	raw.c_cc[VMIN] = 0;	//cc-> control chars
	raw.c_cc[VTIME] = 1;

	tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

int main(int argc, char** argv)
{
	enableRawMode();
	char c;
	do
	{
		c = '\0';
		read(STDIN_FILENO, &c, 1);

		if(iscntrl(c))
			printf("%d\r\n", c);
		else	
			printf("%d ('%c')\r\n", c, c);

	}while(c != 'q');
	
	return 0;
}