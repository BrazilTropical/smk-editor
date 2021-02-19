#define _DEFAULT_SOURCE
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <termios.h>
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
	int rowX;
	int rowOffset;
	int columnOffset;
	int screenRows;
	int screenColumns;
	int numRows;
	erow *row;
	struct termios orig_termios;
};

void initEditor();
int editorReadKey();
void editorScroll();
void enableRawMode();
void disableRawMode();
void die(const char *s);
void clearAndReposition();
void editorRefreshScreen();
void editorProcessKeyPress();
void editorMoveCursor(int key);
void editorUpdateRow(erow *row);
void editorOpen(char *filename);
void getWindowSize(int *rows, int *cols);
void editorDrawRows(struct appendBuffer *ab);
void appendBufferFree(struct appendBuffer *ab);
void editorAppendRow(char *string, size_t len);
int editorRowCursorXToRowX(erow *row, int cursorX);
void appendBufferAppend(struct appendBuffer *ab, const char *s, int len);
#endif