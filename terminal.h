#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <termios.h>

#ifndef FUNCTIONS_H_INCLUDED
#define FUNCTIONS_H_INCLUDED

struct appendBuffer 
{
  char *buffer;
  int len;
};

void initEditor();
void enableRawMode();
char editorReadKey();
void disableRawMode();
void die(const char *s);
void clearAndReposition();
void editorRefreshScreen();
void editorProcessKeyPress();
void editorMoveCursor(char key);
void getWindowSize(int *rows, int *cols);
void editorDrawRows(struct appendBuffer *ab);
void appendBufferFree(struct appendBuffer *ab);
void appendBufferAppend(struct appendBuffer *ab, const char *s, int len);
#endif