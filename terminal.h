#define _GNU_SOURCE
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <termios.h>
#include <time.h>
#include <sys/types.h>

#ifndef FUNCTIONS_H_INCLUDED
#define FUNCTIONS_H_INCLUDED

struct appendBuffer 
{
  char *buffer;
  int len;
};

//Struct for a row
typedef struct erow
{
	int size;
	int rowSize;
	char *chars;
	char *render;	//rendering tabs
} erow;

struct editorConf
{
	int cursorX, cursorY;
	int typeLineNumber;
	int lineNumberSize;
	int cursorStartingColumn;
	int rowX;
	int rowOffset;
	int columnOffset;
	int screenRows;
	int screenColumns;
	int numRows;
	char* filename;
	int dirty;
	char statusmsg[80];
	time_t statusmsg_time;
	erow *row;
	struct termios orig_termios;
};

void editorSave();
void initEditor();
void editorFind();
int editorReadKey();
void editorScroll();
void editorDelChar();
void enableRawMode();
void disableRawMode();
void pageUpDown(int c);
void die(const char *s);
void clearAndReposition();
void editorDelRow(int at);
void editorSetLineNumber();
void editorInsertNewLine();
void editorRefreshScreen();
void editorOpenPromptFile();
void editorProcessKeyPress();
void editorInsertChar(int c);
void editorFreeRow(erow *row);
char* itoa(int val, int base);
void editorMoveCursor(int key);
void editorUpdateRow(erow *row);
void editorOpen(char *filename);
void editorRowDelChar(erow *row, int at);
char *editorRowsToString(int *bufferLen);
void getWindowSize(int *rows, int *cols);
void editorDrawRows(struct appendBuffer *ab);
void editorFindCallback(char *query, int key);
void appendBufferFree(struct appendBuffer *ab);
void editorSetStatusMessage(const char *fmt, ...);
void editorDrawStatusBar(struct appendBuffer *ab);
void editorRowInsertChar(erow *row, int at, int c);
int editorRowCursorXToRowX(erow *row, int cursorX);
void editorDrawMessageBar(struct appendBuffer *ab);
int editorRowRenderXToCursorX(erow *row, int renderX);
int addLineNumber(struct appendBuffer *ab, int posY);
void editorInsertRow(int at, char *string, size_t len);
void editorRowAppendString(erow *row, char *s, size_t len);
char *editorPrompt(char* prompt, void(*callback)(char*, int));
void appendBufferAppend(struct appendBuffer *ab, const char *s, int len);
#endif