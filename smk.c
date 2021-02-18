/*** includes ***/
#include <stdio.h>
#include <ctype.h>
#include <unistd.h>

#include "terminal.h"

/*** defines ***/

/*** init ***/
int main(void)
{
	enableRawMode();
	while(1)
	{
		editorRefreshScreen();
		editorProcessKeyPress();
	}

	return 0;
}