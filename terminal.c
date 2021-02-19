#include "terminal.h"

#define CTRL_KEY(k) ((k) & 0x1f)	//CTRL + q to quit
#define TAB_SIZE 4
#define ABUF_INIT 	{NULL, 0}
#define VERSION "0.0.0"

//Special Keys, Starting at 1000 so to not intervere with other common keys 
enum editorKey
{
	ARROW_LEFT = 1000,	//Setting this one on 1000 so that the others are 1001, 10002...
	ARROW_RIGHT,
	ARROW_UP,
	ARROW_DOWN,
	DEL_KEY,
	HOME_KEY,
	END_KEY,
	PAGE_UP,
	PAGE_DOWN
};

struct editorConf Editor;

//Appends text to the appendBuffer
void appendBufferAppend(struct appendBuffer *ab, const char *s, int len) 
{
	char *new = realloc(ab->buffer, ab->len + len);

	if (new == NULL) 
		return;

	memcpy(&new[ab->len], s, len);
	ab->buffer = new;
	ab->len += len;
}

//Frees the allocated buffer
void appendBufferFree(struct appendBuffer *ab) 
{
	free(ab->buffer);
}

/////////////////////////////////////////////////////////////////////////////////////
//*************///
//***FILE IO **///
//*************///

//Opens a file with name from command line
void editorOpen(char *filename)
{
	FILE *fp = fopen(filename, "r");
	if(!fp)
		die("File");

	char *line = NULL;
	size_t lineCap = 0;
	ssize_t lineLen;

	while((lineLen = getline(&line, &lineCap, fp)) != -1)
	{
		while(lineLen > 0 && (line[lineLen - 1] == '\n' || line[lineLen - 1] == '\r'))		
			lineLen--;
		editorAppendRow(line, lineLen);
	}
	free(line);
	fclose(fp);
}



/////////////////////////////////////////////////////////////////////////////////////
//*************///
//*** Input	***///
//*************///

//Reads key and desides what to do
//\x1b is an escape code
int editorReadKey()
{
	int nRead;
	char c;

	while((nRead = read(STDIN_FILENO, &c, 1) != 1))
		if(nRead == -1 && errno != EAGAIN)
			die("read");

	if(c == '\x1b')
	{
		char seq[3];
		
		if(read(STDIN_FILENO, &seq[0], 1) != 1)
			return '\x1b';

		if(read(STDIN_FILENO, &seq [1], 1) != 1)
			return '\x1b';

		if(seq[0] == '[')
		{
			if(seq[1] >= '0' && seq[1] <= '9')
			{
				if(read(STDIN_FILENO, &seq[2], 1) != 1)
					return '\x1b';
				if(seq[2] == '~')
					switch(seq[1])
					{
						case '1': return HOME_KEY;	//[1~
						case '3': return DEL_KEY;	//[3~
						case '4': return END_KEY;	//[4~
						case '5': return PAGE_UP;	//[5~
						case '6': return PAGE_DOWN;	//[6~
						case '7': return HOME_KEY;	//numpad
						case '8': return END_KEY;	//numpad
					}
			}else
				switch(seq[1])
				{
					case 'A': return ARROW_UP;		//[A
					case 'B': return ARROW_DOWN;	//[B
					case 'C': return ARROW_RIGHT;	//[C
					case 'D': return ARROW_LEFT;	//[D
					case 'H': return HOME_KEY;		//[H
					case 'F': return END_KEY;		//[F
				}
		}
		else if(seq[0] == 'O')
		{
			switch(seq[1])
			{
				case 'H': return HOME_KEY;	//OH
				case 'F': return END_KEY;	//OF
			}
		}
		return '\x1b';
	}

	return c;
}

//Processes key
//Arrows move, Page UP and DOWN move Editor.screenRows times
void editorProcessKeyPress()
{
	int c = editorReadKey();

	switch(c)
	{
		case CTRL_KEY('q'):
			clearAndReposition();
			exit(0);
			break;	
		case HOME_KEY:
			Editor.cursorX = 0;
			break;
		case END_KEY:
			Editor.cursorX = Editor.screenColumns - 1;	
			break;
		case PAGE_UP:
		case PAGE_DOWN:
			{
				int times = Editor.screenRows;
				while(times--)
					editorMoveCursor(c == PAGE_UP ? ARROW_UP: ARROW_DOWN);
			}
			break;
		case ARROW_RIGHT:
		case ARROW_DOWN:
		case ARROW_LEFT:
		case ARROW_UP:
			editorMoveCursor(c);
			break;
		default:
			break;
	}
}

//Appends text to a row
void editorAppendRow(char *string, size_t len)
{
	Editor.row = realloc(Editor.row, sizeof(erow) * (Editor.numRows + 1));

	int at = Editor.numRows;
	Editor.row[at].size = len;
	Editor.row[at].chars = malloc(len + 1);
	memcpy(Editor.row[at].chars, string, len);
	Editor.row[at].chars[len] = '\0';

	Editor.row[at].rowSize = 0;
	Editor.row[at].render = NULL;
	editorUpdateRow(&Editor.row[at]);

	Editor.numRows++;
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

void editorDrawRows(struct appendBuffer *ab) {
	int y;
	for (y = 0; y < Editor.screenRows; ++y) 
	{
		int fileRow = y + Editor.rowOffset;
		if(fileRow >= Editor.numRows)
		{
			if(Editor.numRows == 0 && y == Editor.screenRows / 3)
			{
				char welcome[80];
				int welcomelen = snprintf(welcome, sizeof(welcome),
					"smk editor -- version %s", VERSION);
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
			} else
				appendBufferAppend(ab, "~", 1);

			appendBufferAppend(ab, "\x1b[K", 3);
			
			if (y < Editor.screenRows - 1) 
				appendBufferAppend(ab, "\r\n", 2);
		}	
		else
		{
			int len = Editor.row[fileRow].rowSize - Editor.columnOffset;
			if(len < 0)
				len = 0;
			if(len > Editor.screenColumns)
				len = Editor.screenColumns;

			appendBufferAppend(ab, &Editor.row[fileRow].render[Editor.columnOffset], len);
		}

		appendBufferAppend(ab, "\x1b[K", 3);
		if(y < Editor.screenRows - 1)
			appendBufferAppend(ab, "\r\n", 2);
	}
}

void editorScroll()
{
	Editor.rowX = Editor.cursorX;

	if(Editor.cursorY < Editor.numRows)
		Editor.rowX = editorRowCursorXToRowX(&Editor.row[Editor.cursorX], Editor.cursorX);

	if(Editor.cursorY < Editor.rowOffset)
		Editor.rowOffset = Editor.cursorY;
	if(Editor.cursorY >= Editor.rowOffset + Editor.screenRows)
		Editor.rowOffset = Editor.cursorY - Editor.screenRows + 1; 
	if(Editor.rowX < Editor.columnOffset)
		Editor.columnOffset = Editor.rowX;
	if(Editor.rowX >= Editor.columnOffset + Editor.screenColumns)
		Editor.columnOffset = Editor.rowX - Editor.screenColumns + 1;
}

void editorRefreshScreen() 
{
	editorScroll();

	struct appendBuffer ab = ABUF_INIT;

	appendBufferAppend(&ab, "\x1b[?25l", 6);
	appendBufferAppend(&ab, "\x1b[H", 3);

	editorDrawRows(&ab);
	
	char buf[32];

	snprintf(buf, sizeof(buf), "\x1b[%d;%dH", (Editor.cursorY - Editor.rowOffset) + 1, 
											  (Editor.rowX -  Editor.columnOffset) + 1);
	
	appendBufferAppend(&ab, buf, strlen(buf));
	
	appendBufferAppend(&ab, "\x1b[?25h", 6);

	write(STDOUT_FILENO, ab.buffer, ab.len);
	appendBufferFree(&ab);
}

void editorUpdateRow(erow *row)
{
	int tabs = 0;
	int j;

	for(j = 0; j < row->size; ++j)
		if(row->chars[j] == '\t')
			tabs++;

	free(row->render);
	row->render = malloc(row->size + tabs * (TAB_SIZE - 1) + 1);
	int idx = 0;
	for(j = 0; j < row->size; ++j)
	{
		if(row->chars[j] == '\t')
		{
			row->render[idx++] = ' ';
			while(idx % TAB_SIZE != 0)
				row->render[idx++] = ' ';
		}
		else
		{
			row->render[idx++] = row->chars[j];
		}
	}
	row->render[idx] = '\0';
	row->rowSize = idx; 
}

int editorRowCursorXToRowX(erow *row, int cursorX)
{
	int rowX = 0;
	int j;

	for(j = 0; j < cursorX; ++j)
	{
		if(row->chars[j] == '\t')
			rowX += (TAB_SIZE - 1) - (rowX % TAB_SIZE);
		rowX++;
	}
	return rowX;
}

//Clears screen and repositions cursor at the top;
void clearAndReposition()
{
	write(STDOUT_FILENO, "\x1b[2J", 4);
	write(STDOUT_FILENO, "\x1b[H", 3);
}

void editorMoveCursor(int key)
{
	erow *row = (Editor.cursorY >= Editor.numRows) ? NULL : &Editor.row[Editor.cursorY];
	switch(key)
	{
		case ARROW_LEFT:
			if(Editor.cursorX != 0)
				Editor.cursorX--;
			else if(Editor.cursorY > 0)	//Go end of a line
			{
				Editor.cursorY--;
				Editor.cursorX = Editor.row[Editor.cursorY].size;
			}
			break;
		case ARROW_RIGHT:
			if(row && Editor.cursorX < row -> size)
				Editor.cursorX++;
			else if(row && Editor.cursorX == row->size)		//Go to start of a line
			{
				Editor.cursorY++;
				Editor.cursorX = 0;
			}
			break;
		case ARROW_UP:
			if(Editor.cursorY != 0)
				Editor.cursorY--;
			break;
		case ARROW_DOWN:
			if(Editor.cursorY < Editor.numRows)
				Editor.cursorY++;
			break;
	}

	//Move cursor to end of a line
	row = (Editor.cursorY >= Editor.numRows) ? NULL : &Editor.row[Editor.cursorY];
	int rowLen = row ? row->size : 0;
	if(Editor.cursorX > rowLen)
		Editor.cursorX = rowLen;
}


/////////////////////////////////////////////////////////////////////////////////////
//*************///
//*** Init	***///
//*************///

void initEditor()
{
	Editor.cursorX = 0;
	Editor.cursorY = 0;
	Editor.numRows = 0;
	Editor.rowOffset = 0;
	Editor.rowX = 0;
	Editor.columnOffset = 0;
	Editor.row = NULL;
	getWindowSize(&Editor.screenRows, &Editor.screenColumns);
}
