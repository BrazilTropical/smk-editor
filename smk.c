/*** includes ***/
#include "terminal.h"

int main(void)
{
	enableRawMode();
	initEditor();
	while(1)
	{
		editorRefreshScreen();
		editorProcessKeyPress();
	}

	return 0;
}