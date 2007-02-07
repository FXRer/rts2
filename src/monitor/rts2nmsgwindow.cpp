#include "rts2nmsgwindow.h"

#include "nmonitor.h"

Rts2NMsgWindow::Rts2NMsgWindow (WINDOW * master_window):Rts2NSelWindow (master_window, 0, LINES - 19, COLS,
		18, 1, 300,
		500)
{
  msgMask = 0x07;
  setLineOffset (0);
}

Rts2NMsgWindow::~Rts2NMsgWindow (void)
{
}

void
Rts2NMsgWindow::draw ()
{
  Rts2NSelWindow::draw ();
  werase (getWriteWindow ());
  maxrow = 0;
  int i = 0;
  for (std::list < Rts2Message >::iterator iter = messages.begin ();
       iter != messages.end () && maxrow < (padoff_y + getScrollHeight ());
       iter++, i++)
    {
      Rts2Message msg = *iter;
      if (!msg.passMask (msgMask))
	continue;
      if (maxrow < padoff_y)
	{
	  maxrow++;
	  continue;
	}
      switch (msg.getType ())
	{
	case MESSAGE_ERROR:
	  wcolor_set (getWriteWindow (), CLR_FAILURE, NULL);
	  break;
	case MESSAGE_WARNING:
	  wcolor_set (getWriteWindow (), CLR_WARNING, NULL);
	  break;
	case MESSAGE_INFO:
	  wcolor_set (getWriteWindow (), CLR_OK, NULL);
	  break;
	case MESSAGE_DEBUG:
	  wcolor_set (getWriteWindow (), CLR_TEXT, NULL);
	  break;
	}
      mvwprintw (getWriteWindow (), maxrow, 0, "%s",
		 msg.toString ().c_str ());
      wcolor_set (getWriteWindow (), CLR_DEFAULT, NULL);
      maxrow++;
    }
  refresh ();
}

void
Rts2NMsgWindow::add (Rts2Message & msg)
{
  for (int tr = 0; tr < ((int) messages.size () - getScrollHeight ()); tr++)
    {
      messages.pop_front ();
    }
  messages.push_back (msg);
  if (getSelRow () == maxrow - 1)
    setSelRow (messages.size () - 1);

}

Rts2NMsgWindow & operator << (Rts2NMsgWindow & msgwin, Rts2Message & msg)
{
  msgwin.add (msg);
  return msgwin;
}
