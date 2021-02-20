#include "terminal.h"

#define CTRL_KEY(k) ((k) & 0x1f) //CTRL + q to quit
#define TAB_SIZE 8
#define ABUF_INIT 	{NULL, 0}
#define QUIT_TIMES 3
#define VERSION "0.0.0"
#define ABSOLUTE 1
#define RELATIVE 0
#define OFF -1
#define SIZE(x) sizeof(x) / sizeof(x[0])

//Special Keys, Starting at 1000 so to not intervere with other common keys 
enum editorKey
{
	BACKSPACE = 127,
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
	free(Editor.filename);
	Editor.filename = strdup(filename);

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
		editorInsertRow(Editor.numRows, line, lineLen);
	}
	free(line);
	fclose(fp);
	Editor.dirty = 0;
}

void editorOpenPromptFile()
{
	char *filename = editorPrompt("File name: %s", NULL);
	if(filename)
	{
		initEditor();
		editorOpen(filename);
		editorRefreshScreen();
		free(filename);
	}
}

void editorSetLineNumber()
{
	char* type = editorPrompt("Type (absolute/relative/off): %s", NULL);

	if(type)
	{
		if(strcmp(type, "abs") == 0)
			Editor.typeLineNumber = ABSOLUTE;
		if(strcmp(type, "rel") == 0)
			Editor.typeLineNumber = RELATIVE;
		if(strcmp(type, "off") == 0)
			Editor.typeLineNumber = OFF;
		free(type);
	}
}

char *editorRowsToString(int *bufferLen)
{
	int totLen = 0;
	int j;

	for(j = 0; j < Editor.numRows; ++j)
		totLen += Editor.row[j].size + 1;
	*bufferLen = totLen;

	char *buf = malloc(totLen);
	char *p = buf;
	for(j = 0; j < Editor.numRows; ++j)
	{
		memcpy(p, Editor.row[j].chars, Editor.row[j].size);
		p += Editor.row[j].size;
		*p = '\n';
		p++;
	}

	return buf;
}

void editorSave()
{
	if(Editor.filename == NULL)
	{
		Editor.filename = editorPrompt("Save as: %s (ESC to cancel)", NULL);
		if(Editor.filename == NULL)
		{
			editorSetStatusMessage("Save aborted");
			return;
		}
	}

	int len;
	char *buf = editorRowsToString(&len);
	int fd = open(Editor.filename, O_RDWR | O_CREAT, 0644);
	if(fd != -1)
	{
		if(ftruncate(fd, len) != -1)
			if(write(fd, buf, len) != -1)
			{
				close(fd);
				free(buf);
				Editor.dirty = 0;
				editorSetStatusMessage("%d bytes written to disk", len);
				return;
			}
		close(fd);
	}
	free(buf);
	editorSetStatusMessage("Can't save I/0 error: %s", strerror(errno));
}

/////////////////////////////////////////////////////////////////////////////////////
//*************///
//*** Input	***///
//*************///

char *editorPrompt(char* prompt, void(*callback)(char*, int))
{
	size_t bufferSize = 128;
	char *buf = malloc(bufferSize);

	size_t bufferLen = 0;
	buf[0] = '\0';

	while(1)
	{
		editorSetStatusMessage(prompt, buf);
		editorRefreshScreen();
		int c = editorReadKey();

		if(c == DEL_KEY || c == CTRL_KEY('h') || c == BACKSPACE)
		{
			if(bufferLen != 0)
				buf[--bufferLen] = '\0';
		}
		else if(c == '\x1b')
		{
			editorSetStatusMessage("");
			if(callback)
				callback(buf, c);
			free(buf);
			return NULL;
		}
		else if(c == '\r')
		{
			if(bufferLen != 0)
			{
				editorSetStatusMessage("");
				if(callback)
					callback(buf, c);
				return buf;
			}
		}
		else if (!iscntrl(c) && c < 128)
		{
			if(bufferLen == bufferSize - 1)
			{
				bufferSize *= 2;
				buf = realloc(buf, bufferSize);
			}
			buf[bufferLen++] = c;
			buf[bufferLen] = '\0';
		}
		if(callback)
			callback(buf, c);
	}
}

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
	static int quit_times = QUIT_TIMES;

	switch(c)
	{
		case '\r':
			editorInsertNewLine();
			break;
		case CTRL_KEY('q'):
			{
				
				if(Editor.dirty && quit_times > 0)
				{
					editorSetStatusMessage("File unchanged! Press CTRL Q %d more times to quit.", quit_times);
					quit_times--;
					return;
				}
				clearAndReposition();
				exit(0);
			}
			break;	
		case CTRL_KEY('s'):
			editorSave();
			break;
		case CTRL_KEY('o'):
			editorOpenPromptFile();
			break;
		case CTRL_KEY('k'):
			editorSetLineNumber();
			break;
		case CTRL_KEY('f'):
			editorFind();
			break;
		case HOME_KEY:
			Editor.cursorX = 0;
			break;
		case END_KEY:
			if(Editor.cursorY < Editor.numRows)
				Editor.cursorX = Editor.row[Editor.cursorY].size;	
			break;
		case BACKSPACE:
		case DEL_KEY:
			if(c == DEL_KEY)
				editorMoveCursor(ARROW_RIGHT);
			editorDelChar();
			break;
		case PAGE_UP:
		case PAGE_DOWN:
			pageUpDown(c);
			break;
		case ARROW_RIGHT:
		case ARROW_DOWN:
		case ARROW_LEFT:
		case ARROW_UP:
			editorMoveCursor(c);
			break;
		case CTRL_KEY('l'):
		case '\x1b':
			break;
		default:
			editorInsertChar(c);
			break;
	}

	quit_times = QUIT_TIMES;
}

void pageUpDown(int c)
{

	if(c == PAGE_UP)
		Editor.cursorY = Editor.rowOffset;
	else if(c == PAGE_DOWN)
	{
		Editor.cursorY = Editor.rowOffset + Editor.screenRows - 1;
		if (Editor.cursorY > Editor.numRows)
			Editor.cursorY = Editor.numRows;
	}
	int times = Editor.screenRows;
	while(times--)
		editorMoveCursor(c == PAGE_UP ? ARROW_UP: ARROW_DOWN);
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
	raw.c_cc[VTIME] = 100;

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

int addLineNumber(struct appendBuffer *ab, int posY)
{
	char lineNumber[Editor.numRows];
	char* numRowsToASCII = itoa(Editor.numRows, 10);
	int padding = strlen(numRowsToASCII);
	int p = padding;
	Editor.cursorX = p;

	while(padding-- != (int)strlen(lineNumber))
		appendBufferAppend(ab, " ", 1);

	if(Editor.typeLineNumber == ABSOLUTE)
		sprintf(lineNumber, "%d", posY);
	else if(Editor.typeLineNumber == RELATIVE)
		sprintf(lineNumber, "%d", abs(Editor.cursorY - posY));

	//lineNumber[Editor.numRows] = '\0';

	appendBufferAppend(ab, lineNumber, 4);
	appendBufferAppend(ab, " ", 1);
	return p;
}

void editorDrawRows(struct appendBuffer *ab) {
	int y;
	int printTilde;

	for (y = 0; y < Editor.screenRows; ++y) 
	{
		int fileRow = y + Editor.rowOffset;
		printTilde = 1;

		if(Editor.typeLineNumber != -1 && fileRow < Editor.numRows)
		{
			printTilde = 0;
			int z = addLineNumber(ab, fileRow);
			printf("%d, ", z);
		}

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
			} 
			else if(printTilde)
				appendBufferAppend(ab, "~", 1);
		}	
		else
		{
			int len = Editor.row[fileRow].rowSize - Editor.columnOffset;
			if(len < 0)
				len = 0;
			if(len > Editor.screenColumns)
				len = Editor.screenColumns;

			//colors
			char *c = &Editor.row[fileRow].render[Editor.columnOffset];
			int j;
			for(j = 0; j < len; ++j)
			{
				if(isdigit(c[j]))
				{
					appendBufferAppend(ab, "\x1b[31m", 5);	//red
					appendBufferAppend(ab, &c[j], 1);		//digit
					appendBufferAppend(ab, "\x1b[39m", 5);	//white
				}
				else
					appendBufferAppend(ab, &c[j], 1);
			}
		}

		appendBufferAppend(ab, "\x1b[K", 3);
		appendBufferAppend(ab, "\r\n", 2);
	}
}

void editorScroll()
{
	Editor.rowX = 0;

	if(Editor.cursorY < Editor.numRows)
		Editor.rowX = editorRowCursorXToRowX(&Editor.row[Editor.cursorY], Editor.cursorX);

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
	editorDrawStatusBar(&ab);
	editorDrawMessageBar(&ab);
	char buf[32];

	snprintf(buf, sizeof(buf), "\x1b[%d;%dH", (Editor.cursorY - Editor.rowOffset) + 1, 
											  (Editor.rowX -  Editor.columnOffset) + 1);
	
	appendBufferAppend(&ab, buf, strlen(buf));
	
	appendBufferAppend(&ab, "\x1b[?25h", 6);

	write(STDOUT_FILENO, ab.buffer, ab.len);
	appendBufferFree(&ab);
}

void editorSetStatusMessage(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(Editor.statusmsg, sizeof(Editor.statusmsg), fmt, ap);
	va_end(ap);
	Editor.statusmsg_time = time(NULL);
}

//Inserts text to a row
void editorInsertRow(int at, char *string, size_t len)
{
	if(at < 0 || at > Editor.numRows)
		return;


	Editor.row = realloc(Editor.row, sizeof(erow) * (Editor.numRows + 1));
	memmove(&Editor.row[at + 1], &Editor.row[at], sizeof(erow) * (Editor.numRows - at));

	Editor.row[at].size = len;
	Editor.row[at].chars = malloc(len + 1);
	memcpy(Editor.row[at].chars, string, len);
	Editor.row[at].chars[len] = '\0';

	Editor.row[at].rowSize = 0;
	Editor.row[at].render = NULL;
	editorUpdateRow(&Editor.row[at]);

	Editor.numRows++;
	Editor.dirty = 1;
}

void editorRowInsertChar(erow *row, int at, int c)
{
	if(at < 0 || at > row->size)
		at = row->size;
	row->chars = realloc(row->chars, row->size + 2);
	memmove(&row->chars[at + 1], &row->chars[at], row->size - at + 1);
	row->size++;
	row->chars[at] = c;
	editorUpdateRow(row);
	Editor.dirty = 1;
}

void editorInsertChar(int c)
{
	if(Editor.cursorY == Editor.numRows)	//cursor is on the tilde line after the end of the file
		editorInsertRow(Editor.numRows, "", 0);
	editorRowInsertChar(&Editor.row[Editor.cursorY], Editor.cursorX, c);
	Editor.cursorX++;
}

void editorInsertNewLine()
{
	if(Editor.cursorX == 0)
	{
		editorInsertRow(Editor.cursorY, "", 0);
	}
	else
	{
		erow *row = &Editor.row[Editor.cursorY];
		editorInsertRow(Editor.cursorY + 1, &row->chars[Editor.cursorX], row->size - Editor.cursorX);
		row = &Editor.row[Editor.cursorY];
		row->size = Editor.cursorX;
		row->chars[row->size] = '\0';
		editorUpdateRow(row);
	}
	Editor.cursorY++;
	Editor.cursorX = 0;
}

void editorFreeRow(erow *row)
{
	if(row->render != NULL)
		free(row->render);
	free(row->chars);
}

//Deletes row using memmove
void editorDelRow(int at)
{
	if(at < 0 || at >= Editor.numRows)
		return;
	editorFreeRow(&Editor.row[at]);
	memmove(&Editor.row[at], &Editor.row[at + 1], sizeof(erow) * (Editor.numRows - at - 1));
	Editor.numRows--;
	Editor.dirty = 1;
}

//Deletes a char from a row. A row is a erow* and char located at "at"
void editorRowDelChar(erow *row, int at)
{
	if(at < 0 || at >= row->size)
		return;
	memmove(&row->chars[at], &row->chars[at + 1], row->size - at);
	row->size--;
	editorUpdateRow(row);
	Editor.dirty = 1;
}

//called to delete a char
void editorDelChar()
{
	if(Editor.cursorY == Editor.numRows)
		return;
	if(Editor.cursorX == 0 && Editor.cursorY == 0)
		return;

	erow *row = &Editor.row[Editor.cursorY];
	if(Editor.cursorX > 0)
	{
		editorRowDelChar(row, Editor.cursorX - 1);
		Editor.cursorX--;
	} 
	else
	{
		Editor.cursorX = Editor.row[Editor.cursorY - 1].size;
		editorRowAppendString(&Editor.row[Editor.cursorY - 1], row->chars, row->size);
		editorDelRow(Editor.cursorY);
		Editor.cursorY--;
	}
}

//appends remaining text from a row to the row above when deleting the first char of a line
void editorRowAppendString(erow *row, char *s, size_t len)
{
	row->chars = realloc(row->chars, row->size + len + 1);
	memcpy(&row->chars[row->size], s, len);
	row->size += len;
	row->chars[row->size] = '\0';
	editorUpdateRow(row);
	Editor.dirty = 1;
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

//converts a chars index to a render index
//used to indent tabs
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

int editorRowRenderXToCursorX(erow *row, int renderX)
{
	int cur_rx = 0;
	int cx;
	for(cx = 0; cx < row->size; ++cx)
	{
		if(row->chars[cx] == '\t')
		{
			cur_rx += (TAB_SIZE - 1) - (cur_rx & TAB_SIZE);
		}
		cur_rx++;
		
		if(cur_rx > renderX)
			return cx;
	}
	return cx;
}

//draws status bar, when a file is modified the dirty flag is 1 and thus showing (modified) on status bar
void editorDrawStatusBar(struct appendBuffer *ab)
{
	appendBufferAppend(ab, "\x1b[7m", 4);
	char status[80], rstatus[80];
	int len = snprintf(status, sizeof(status), "%.20s %s - %d,%d",
				Editor.filename ? Editor.filename : "[No Name]",
				Editor.dirty ? "(modified)" : "", Editor.cursorX, Editor.cursorY);
	
	int rlen = snprintf(rstatus, sizeof(rstatus), "%d/%d lines", Editor.cursorY + 1, Editor.numRows);
	if(len > Editor.screenColumns)
		len = Editor.screenColumns;

	appendBufferAppend(ab, status, len);
	while(len < Editor.screenColumns)
	{
		if(Editor.screenColumns - len == rlen)
		{
			appendBufferAppend(ab, rstatus, rlen);
			break;
		}
		appendBufferAppend(ab, " ", 1);
		len++;
	}
	appendBufferAppend(ab, "\x1b[m", 3);
	appendBufferAppend(ab, "\r\n", 2);	

}

void editorDrawMessageBar(struct appendBuffer *ab)
{
	appendBufferAppend(ab, "\x1b[K", 3);		//clear message bar
	int msglen = strlen(Editor.statusmsg);
	if(msglen > Editor.screenColumns)
		msglen = Editor.screenColumns;
	if(msglen && time(NULL) - Editor.statusmsg_time < 5)
		appendBufferAppend(ab, Editor.statusmsg, msglen);	
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

void editorFindCallback(char *query, int key)
{
	static int lastMatch = -1;
	static int direction = 1;

	if(key == '\r' || key == '\x1b')
	{
		lastMatch = -1;
		direction = 1;
	}
	else if(key == ARROW_RIGHT || key == ARROW_DOWN)	//go next, down
	{
		direction = 1;
	}
	else if(key == ARROW_LEFT || key == ARROW_UP)		//go prev, up
	{
		direction = -1;
	}
	else
	{
		lastMatch = -1;
		direction = 1;
	}

	if(lastMatch == -1)
		direction = 1;

	int current = lastMatch;
	int i;
	for(i = 0; i < Editor.numRows; ++i)
	{
		current += direction;

		if(current == -1)
			current = Editor.numRows - 1;
		else if(current == Editor.numRows)
			current = 0;

		erow *row = &Editor.row[current];		
		char *match = strstr(row->render, query);
		if(match)
		{
			lastMatch = current;
			Editor.cursorY = current;
			Editor.cursorX = editorRowRenderXToCursorX(row, match - row->render);
			Editor.rowOffset = Editor.numRows;
			break;
		}
	}
}

void editorFind()
{
	int savedCursorX = Editor.cursorX;		//save pos, before moving to find
	int savedCursorY = Editor.cursorY;
	int savedColumnOff = Editor.columnOffset;
	int savedRowOff = Editor.rowOffset;

	char* query = editorPrompt("Search: %s (ESC to cancel)", editorFindCallback);

	if(query)
	{
		free(query);
	}
	else
	{
		Editor.cursorX = savedCursorX;
		Editor.cursorY = savedCursorY;
		Editor.columnOffset = savedColumnOff;
		Editor.rowOffset = savedRowOff;
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
	Editor.numRows = 0;
	Editor.typeLineNumber = -1;
	Editor.rowOffset = 0;
	Editor.rowX = 0;
	Editor.columnOffset = 0;
	Editor.row = NULL;
	Editor.filename = NULL;
	Editor.statusmsg[0] = '\0';
	Editor.statusmsg_time = 0;
	Editor.dirty = 0;
	Editor.lineNumberSize = 0;
	getWindowSize(&Editor.screenRows, &Editor.screenColumns);
	Editor.screenRows -= 2;
}


char* itoa(int val, int base)
{
	
	static char buf[32] = {0};
	
	int i = 30;
	
	for(; val && i ; --i, val /= base)
	
		buf[i] = "0123456789abcdef"[val % base];
	
	return &buf[i+1];
	
}