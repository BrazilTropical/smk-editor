#include "terminal.h"

#define CTRL_KEY(k) ((k) & 0x1f)	//CTRL + q to quit
#define ABUF_INIT 	{NULL, 0}
#define VERSION "0.0.0"

struct editorConf
{
	int cursorX, cursorY;
	int screenRows;
	int screenColumns;
	struct termios orig_termios;
};

struct editorConf Editor;

void appendBufferAppend(struct appendBuffer *ab, const char *s, int len) 
{
	char *new = realloc(ab->buffer, ab->len + len);

	if (new == NULL) 
		return;

	memcpy(&new[ab->len], s, len);
	ab->buffer = new;
	ab->len += len;
}

void appendBufferFree(struct appendBuffer *ab) 
{
	free(ab->buffer);
}

/////////////////////////////////////////////////////////////////////////////////////
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
		case 'a':
		case 'w':
		case 's':
		case 'd':
		case 'A':
		case 'W':
		case 'S':
		case 'D':
		case 'j':
		case 'J':
		case 'k':
		case 'K':
		case 'h':
		case 'H':
		case 'l':
		case 'L':
			editorMoveCursor(c);
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
	if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &Editor.orig_termios) == -1)
		die("tcsetattr");
}

//Doesn't print on terminal
void enableRawMode()
{
	if(tcgetattr(STDIN_FILENO, &Editor.orig_termios) == -1)	//error on tcget
		die("tcgetattr");

	atexit(disableRawMode);	//used to call disableRawMode() on exit.

	struct termios raw = Editor.orig_termios;

  	raw.c_iflag &= ~(ICRNL | IXON); //input flags
	raw.c_oflag &= ~(OPOST);	//disables post processing of output
	raw.c_cflag |= (CS8);	//char size 8
	raw.c_lflag &= ~(ECHO | ICANON |IEXTEN | ISIG);	//negates bits corresponding to ECHO | ICANON
	raw.c_cc[VMIN] = 0;	//cc-> control chars
	raw.c_cc[VTIME] = 1;

	if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)	//error on tcset
		die("tcsetattr");
}

void getWindowSize(int *rows, int *cols)
{
	struct winsize ws;

	if(ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0)
		die("window size");

	*cols = ws.ws_col;
	*rows = ws.ws_row;
}

/////////////////////////////////////////////////////////////////////////////////////
//*************///
//** Output  **///
//*************///

//Clears screen and repositions cursor
//Used on die and as refresh
void editorDrawRows(struct appendBuffer *ab) {
	int y;
	for (y = 0; y < Editor.screenRows; ++y) 
	{
		if(y == Editor.screenRows / 3)
		{
			char welcome[80];
			int welcomelen = snprintf(welcome, sizeof(welcome),
				"smk editor based on Kilo -- version %s", VERSION);
			if(welcomelen > Editor.screenColumns)
				welcomelen = Editor.screenColumns;

			int padding = (Editor.screenColumns - welcomelen) / 2;

			if(padding)
			{
				appendBufferAppend(ab, "~", 1);
				padding--;
			}

			while(padding--)
				appendBufferAppend(ab, " ", 1);

			appendBufferAppend(ab, welcome, welcomelen);
		}
		else
			appendBufferAppend(ab, "~", 1);

		appendBufferAppend(ab, "\x1b[K", 3);
		
		if (y < Editor.screenRows - 1) 
			appendBufferAppend(ab, "\r\n", 2);
	}
}
void editorRefreshScreen() 
{
	struct appendBuffer ab = ABUF_INIT;

	appendBufferAppend(&ab, "\x1b[?25l", 6);
	appendBufferAppend(&ab, "\x1b[H", 3);

	editorDrawRows(&ab);
	
	char buf[32];

	if(Editor.cursorY + 1 < 0)
		Editor.cursorY = 0;
	
	if(Editor.cursorX + 1 < 0)
		Editor.cursorX = 0;

	snprintf(buf, sizeof(buf), "\x1b[%d;%dH", Editor.cursorY + 1, Editor.cursorX + 1);
	appendBufferAppend(&ab, buf, strlen(buf));
	
	appendBufferAppend(&ab, "\x1b[?25h", 6);

	write(STDOUT_FILENO, ab.buffer, ab.len);
	appendBufferFree(&ab);
}

//Clears screen and repositions cursor at the top;
void clearAndReposition()
{
	write(STDOUT_FILENO, "\x1b[2J", 4);
	write(STDOUT_FILENO, "\x1b[H", 3);
}

void editorMoveCursor(char key)
{
	switch(key)
	{
		case 'A':
		case 'a':
		case 'h':
		case 'H':
			Editor.cursorX--;
			break;
		case 'W':
		case 'w':
		case 'k':
		case 'K':
			Editor.cursorY--;
			break;
		case 'S':
		case 's':
		case 'j':
		case 'J':
			Editor.cursorY++;
			break;
		case 'D':
		case 'd':
		case 'l':
		case 'L':
			Editor.cursorX++;
			break;
	}
}


/////////////////////////////////////////////////////////////////////////////////////
//*************///
//*** Init	***///
//*************///

void initEditor()
{
	Editor.cursorX = 0;
	Editor.cursorY = 0;
	getWindowSize(&Editor.screenRows, &Editor.screenColumns);
}