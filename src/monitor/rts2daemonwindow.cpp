#include "rts2daemonwindow.h"
#include "nmonitor.h"
#include "../utils/timestamp.h"

#include <iostream>

Rts2NWindow::Rts2NWindow (WINDOW * master_window, int x, int y, int w, int h,
			  int border):
Rts2NLayout ()
{
  if (h <= 0)
    h = 0;
  if (w <= 0)
    w = 0;
  if (x < 0)
    x = 0;
  if (y < 0)
    y = 0;
  window = newwin (h, w, y, x);
  if (!window)
    errorMove ("newwin", y, x, h, w);
  switch (border)
    {
    case 0:
      _haveBox = false;
      break;
    case 1:
      box (window, 0, 0);
      _haveBox = true;
      break;
    }
}

Rts2NWindow::~Rts2NWindow (void)
{
  delwin (window);
}

void
Rts2NWindow::draw ()
{
  werase (window);
  if (haveBox ())
    {
      box (window, 0, 0);
    }
}

int
Rts2NWindow::getX ()
{
  int x, y;
  getbegyx (window, y, x);
  return x;
}

int
Rts2NWindow::getY ()
{
  int x, y;
  getbegyx (window, y, x);
  return y;
}

int
Rts2NWindow::getCurX ()
{
  int x, y;
  getyx (window, y, x);
  return x + getX ();
}

int
Rts2NWindow::getCurY ()
{
  int x, y;
  getyx (window, y, x);
  return y + getY ();
}


int
Rts2NWindow::getWidth ()
{
  int w, h;
  getmaxyx (window, h, w);
  return w;
}

int
Rts2NWindow::getHeight ()
{
  int w, h;
  getmaxyx (window, h, w);
  return h;
}

int
Rts2NWindow::getWriteWidth ()
{
  int ret = getHeight ();
  if (haveBox ())
    return ret - 2;
  return ret;
}

int
Rts2NWindow::getWriteHeight ()
{
  int ret = getHeight ();
  if (haveBox ())
    return ret - 2;
  return ret;
}

void
Rts2NWindow::errorMove (const char *op, int y, int x, int h, int w)
{
  endwin ();
  std::cout << "Cannot perform ncurses operation " << op
    << " y x h w "
    << y << " "
    << x << " "
    << h << " "
    << w << " " << "LINES COLS " << LINES << " " << COLS << std::endl;
  exit (EXIT_FAILURE);
}

void
Rts2NWindow::move (int x, int y)
{
  int _x, _y;
  getbegyx (window, _y, _x);
  if (x == _x && y == _y)
    return;
  if (mvwin (window, y, x) == ERR)
    errorMove ("mvwin", y, x, -1, -1);
}

void
Rts2NWindow::resize (int x, int y, int w, int h)
{
  if (wresize (window, h, w) == ERR)
    errorMove ("wresize", 0, 0, h, w);
  move (x, y);
}

void
Rts2NWindow::grow (int max_w, int h_dif)
{
  int x, y, w, h;
  getbegyx (window, y, x);
  getmaxyx (window, h, w);
  if (max_w > w)
    w = max_w;
  resize (x, y, w, h + h_dif);
}

void
Rts2NWindow::refresh ()
{
  wnoutrefresh (window);
}

void
Rts2NWindow::enter ()
{
}

void
Rts2NWindow::leave ()
{
}

void
Rts2NWindow::setCursor ()
{
  setsyx (getCurY (), getCurX ());
}

void
Rts2NWindow::printState (Rts2Conn * conn)
{
  if (conn->getErrorState ())
    wcolor_set (getWriteWindow (), CLR_FAILURE, NULL);
  else if (conn->havePriority ())
    wcolor_set (getWriteWindow (), CLR_OK, NULL);
  wprintw (getWriteWindow (), "%s %s (%i) priority: %s\n", conn->getName (),
	   conn->getStateString ().c_str (), conn->getState (),
	   conn->havePriority ()? "yes" : "no");
  wcolor_set (getWriteWindow (), CLR_DEFAULT, NULL);
}

Rts2NSelWindow::Rts2NSelWindow (WINDOW * master_window, int x, int y, int w,
				int h, int border, int sw, int sh):
Rts2NWindow (master_window, x, y, w, h, border)
{
  selrow = 0;
  maxrow = 0;
  padoff_x = 0;
  padoff_y = 0;
  scrolpad = newpad (sh, sw);
  lineOffset = 1;
}

Rts2NSelWindow::~Rts2NSelWindow (void)
{
  delwin (scrolpad);
}

int
Rts2NSelWindow::injectKey (int key)
{
  switch (key)
    {
    case KEY_HOME:
      selrow = 0;
      break;
    case KEY_END:
      selrow = -1;
      break;
    case KEY_DOWN:
      if (selrow == -1)
	selrow = 0;
      else if (selrow < (maxrow - 1))
	selrow++;
      else
	selrow = 0;
      break;
    case KEY_UP:
      if (selrow == -1)
	selrow = (maxrow - 1);
      else if (selrow > 0)
	selrow--;
      else
	selrow = (maxrow - 1);
      break;
    case KEY_LEFT:
      if (padoff_x > 0)
	{
	  padoff_x--;
	}
      else
	{
	  beep ();
	  flash ();
	}
      break;
    case KEY_RIGHT:
      if (padoff_x < 300)
	{
	  padoff_x++;
	}
      else
	{
	  beep ();
	  flash ();
	}
      break;
    }
  return -1;
}

void
Rts2NSelWindow::draw ()
{
  Rts2NWindow::draw ();
}

void
Rts2NSelWindow::refresh ()
{
  int x, y;
  int w, h;
  getmaxyx (scrolpad, h, w);
  if (maxrow > 0)
    {
      if (selrow >= 0)
	{
	  mvwchgat (scrolpad, selrow, 0, w, A_REVERSE, 0, NULL);
	}
      else if (selrow == -1)
	{
	  mvwchgat (scrolpad, maxrow - 1, 0, w, A_REVERSE, 0, NULL);
	}
    }
  Rts2NWindow::refresh ();
  getbegyx (window, y, x);
  getmaxyx (window, h, w);
  // normalize selrow
  if (selrow == -1)
    padoff_y = (maxrow - 1) - getWriteHeight () + 1 + lineOffset;
  else if ((selrow - padoff_y + lineOffset) >= getWriteHeight ())
    padoff_y = selrow - getWriteHeight () + 1 + lineOffset;
  else if ((selrow - padoff_y) < 0)
    padoff_y = selrow;
  if (haveBox ())
    pnoutrefresh (scrolpad, padoff_y, padoff_x, y + 1, x + 1, y + h - 2,
		  x + w - 2);
  else
    pnoutrefresh (scrolpad, padoff_y, padoff_x, y, x, y + h - 1, x + w - 1);
}

Rts2NDevListWindow::Rts2NDevListWindow (WINDOW * master_window, Rts2Block * in_block):Rts2NSelWindow (master_window, 0, 1, 10,
		LINES -
		20)
{
  block = in_block;
}

Rts2NDevListWindow::~Rts2NDevListWindow (void)
{
}

void
Rts2NDevListWindow::draw ()
{
  Rts2NWindow::draw ();
  werase (scrolpad);
  maxrow = 0;
  for (connections_t::iterator iter = block->connectionBegin ();
       iter != block->connectionEnd (); iter++)
    {
      Rts2Conn *conn = *iter;
      const char *name = conn->getName ();
      if (*name == '\0')
	wprintw (scrolpad, "status\n");
      else
	wprintw (scrolpad, "%s\n", conn->getName ());
      maxrow++;
    }
  refresh ();
}

Rts2NDeviceWindow::Rts2NDeviceWindow (WINDOW * master_window, Rts2Conn * in_connection):Rts2NSelWindow
  (master_window, 10, 1, COLS - 10,
   LINES - 25)
{
  connection = in_connection;
  draw ();
}

Rts2NDeviceWindow::~Rts2NDeviceWindow ()
{
}

void
Rts2NDeviceWindow::drawValuesList ()
{
  Rts2DevClient *client = connection->getOtherDevClient ();
  if (client)
    drawValuesList (client);
}

void
Rts2NDeviceWindow::drawValuesList (Rts2DevClient * client)
{
  struct timeval tv;
  gettimeofday (&tv, NULL);
  double now = tv.tv_sec + tv.tv_usec / USEC_SEC;
  for (std::vector < Rts2Value * >::iterator iter = client->valueBegin ();
       iter != client->valueEnd (); iter++)
    {
      Rts2Value *val = *iter;
      // customize value display
      std::ostringstream _os;
      switch (val->getValueType ())
	{
	case RTS2_VALUE_TIME:
	  _os << LibnovaDateDouble (val->
				    getValueDouble ()) << " (" <<
	    TimeDiff (now, val->getValueDouble ()) << ")";
	  wprintw (getWriteWindow (), "%-20s|%30s\n",
		   val->getName ().c_str (), _os.str ().c_str ());
	  break;
	default:
	  wprintw (getWriteWindow (), "%-20s|%30s\n",
		   val->getName ().c_str (), val->getValue ());
	}
      maxrow++;
    }
}

void
Rts2NDeviceWindow::draw ()
{
  Rts2NWindow::draw ();
  werase (getWriteWindow ());
  maxrow = 1;
  printState (connection);
  drawValuesList ();
  refresh ();
}

void
Rts2NCentraldWindow::drawDevice (Rts2Conn * conn)
{
  printState (conn);
  maxrow++;
}

Rts2NCentraldWindow::Rts2NCentraldWindow (WINDOW * master_window, Rts2Client * in_client):Rts2NSelWindow
  (master_window, 10, 1, COLS - 10,
   LINES - 25)
{
  client = in_client;
  draw ();
}

Rts2NCentraldWindow::~Rts2NCentraldWindow (void)
{
}

void
Rts2NCentraldWindow::draw ()
{
  Rts2NSelWindow::draw ();
  werase (getWriteWindow ());
  maxrow = 0;
  for (connections_t::iterator iter = client->connectionBegin ();
       iter != client->connectionEnd (); iter++)
    {
      Rts2Conn *conn = *iter;
      drawDevice (conn);
    }
  refresh ();
}
