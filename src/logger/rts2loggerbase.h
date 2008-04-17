/* 
 * Logger base.
 * Copyright (C) 2007-2008 Petr Kubanek <petr@kubanek.net>
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

#include "../utils/rts2devclient.h"
#include "../utils/rts2displayvalue.h"
#include "../utils/rts2command.h"
#include "../utils/rts2expander.h"

#define EVENT_SET_LOGFILE RTS2_LOCAL_EVENT+800

/**
 * This class logs given values to given file.
 *
 * @author Petr Kubanek <petr@kubanek.net>
 *
 * @ingroup RTS2Logger
 */
class Rts2DevClientLogger:public Rts2DevClient
{
	private:
		std::list < Rts2Value * >logValues;
		std::list < std::string > logNames;

		std::ostream * outputStream;

		Rts2Expander * exp;
		std::string expandPattern;
		std::string expandedFilename;

		/**
		 * Next time info command will be called.
		 */
		struct timeval nextInfoCall;

		/**
		 * Info command will be send every numberSec seconds.
		 */
		struct timeval numberSec;
	
		/**
		 * Fill to log file values which are in logValues array.
		 */
		void fillLogValues ();

		/**
		 * Change output stream according to new expansion.
		 */
		void changeOutputStream ();
	protected:
		/**
		 * Change expansion pattern to new filename.
		 *
		 * @param pattern Expanding pattern.
		 */
		void setOutputFile (const char *pattern);
	public:
		/**
		 * Construct client for logging device.
		 *
		 * @param in_conn Connection.
		 * @param in_numberSec Number of seconds when the info command will be send.
		 * @param in_logNames String with space separated names of values which will be logged.
		 */
		Rts2DevClientLogger (Rts2Conn * in_conn, double in_numberSec, std::list < std::string > &in_logNames);

		virtual ~ Rts2DevClientLogger (void);
		virtual void infoOK ();
		virtual void infoFailed ();

		virtual void idle ();

		virtual void postEvent (Rts2Event * event);
};

/**
 * Holds informations about which device should log which values at which time interval.
 *
 * @author Petr Kubanek <petr@kubanek.net>
 */
class Rts2LogValName
{
	public:
		std::string devName;
		double timeout;
		std::list < std::string > valueList;

		Rts2LogValName (std::string in_devName, double in_timeout,
			std::list < std::string > in_valueList)
		{
			devName = in_devName;
			timeout = in_timeout;
			valueList = in_valueList;
		}
};

/**
 * This class is base class for logging.
 */
class Rts2LoggerBase
{
	private:
		std::list < Rts2LogValName > devicesNames;

	protected:
		int readDevices (std::istream & is);

		Rts2LogValName *getLogVal (const char *name);
		int willConnect (Rts2Address * in_addr);

	public:
		Rts2LoggerBase ();
		Rts2DevClient *createOtherType (Rts2Conn * conn, int other_device_type);
};
