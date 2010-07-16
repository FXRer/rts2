/* 
 * Ncurses message box.
 * Copyright (C) 2003-2007,2010 Petr Kubanek <petr@kubanek.net>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include "nmonitor.h"
#include "nmsgbox.h"

using namespace rts2ncurses;

NMsgBox::NMsgBox (const char *in_query, const char *in_buttons[],int in_butnum):NWindow (COLS / 2 - 25, LINES / 2 - 15, 50, 5)
{
	query = in_query;
	buttons = in_buttons;
	butnum = in_butnum;
	exitState = 0;
}


NMsgBox::~NMsgBox (void)
{
}

keyRet NMsgBox::injectKey (int key)
{
	switch (key)
	{
		case KEY_RIGHT:
			exitState++;
			break;
		case KEY_LEFT:
			exitState--;
			break;
		case KEY_EXIT:
		case K_ESC:
			exitState = -1;
			return RKEY_ESC;
		case KEY_ENTER:
		case K_ENTER:
			return RKEY_ENTER;
		default:
			return NWindow::injectKey (key);
	}
	if (exitState < 0)
		exitState = 0;
	if (exitState >= butnum)
		exitState = butnum - 1;
	return RKEY_HANDLED;
}

void NMsgBox::draw ()
{
	NWindow::draw ();
	mvwprintw (window, 1, 2, "%s", query);
	for (int i = 0; i < butnum; i++)
	{
		if (i == exitState)
			wattron (window, A_REVERSE);
		else
			wattroff (window, A_REVERSE);
		mvwprintw (window, 2, 2 + i * 30 / 2, buttons[i]);
	}
	wattroff (window, A_REVERSE);
	winrefresh ();
}
