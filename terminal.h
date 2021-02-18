#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <termios.h>

#ifndef FUNCTIONS_H_INCLUDED
#define FUNCTIONS_H_INCLUDED

void die(const char *s);
void disableRawMode();
void enableRawMode();
char editorReadKey();
void editorProcessKeyPress();
void editorRefreshScreen();
void clearAndReposition();
void editorDrawRows();
#endif