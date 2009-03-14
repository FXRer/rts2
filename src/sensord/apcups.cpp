/* 
 * System sensor, displaying free disk space and more.
 * Copyright (C) 2008 Petr Kubanek <petr@kubanek.net>
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

#include "sensord.h"

#include "../utils/rts2connnosend.h"

#include <iomanip>
#include <netdb.h>
#include <fcntl.h>

#include <map>

namespace rts2sensor
{

	/**
	 * Class for communication with APC UPS.
	 *
	 * @author Petr Kubanek <petr@kubanek.net>
	 */
	class ConnApcUps: public Rts2ConnNoSend
	{
		private:
			const char *hostname;
			int port;
			std::map <std::string, std::string> values;

		public:
			/**
			 * Create new connection to APC UPS daemon.
			 *
			 * @param _master   Reference to master holding this connection.
			 *
			 * @param _hostname APC UPSD IP address or hostname.
			 * @param _port     Portnumber of APC UPSD daemon (default to 3551).
			 */
			ConnApcUps (Rts2Block *_master, const char *_hostname, int _port);

			/**
			 * Init TCP/IP connection to given host.
			 *
			 * @return -1 on error, 0 on success.
			 */
			virtual int init ();

			/**
			 * Call command, get reply.
			 *
			 * @param cmd       Command.
			 * @param _buf       Buffer holding reply data.
			 * @param _buf_size  Size of buffer for reply data.
			 */
			int command (const char *cmd, char *_buf, int _buf_size);

			const char *getString (const char *val);
			float getPercents (const char *val);
			float getTemp (const char *val);
			int getTime (const char *val);
	};

	/**
	 * Sensor for accessing APC UPS informations.
	 *
	 * @author Petr Kubanek <petr@kubanek.net>
	 */
	class ApcUps:public SensorWeather
	{
		private:
			HostString *host;
			ConnApcUps *connApc;

			Rts2ValueString *model;

			Rts2ValueFloat *loadpct;
			Rts2ValueFloat *bcharge;
			Rts2ValueInteger *timeleft;
			Rts2ValueFloat *itemp;
			Rts2ValueInteger *tonbatt;
			Rts2ValueString *status;

		protected:
			virtual int processOption (int opt);
			virtual int info ();

			virtual int init ();

		public:
			ApcUps (int argc, char **argv);
			virtual ~ApcUps (void);
	};

}


using namespace rts2sensor;


ConnApcUps::ConnApcUps (Rts2Block *_master, const char *_hostname, int _port)
:Rts2ConnNoSend (_master)
{
	hostname = _hostname;
	port = _port;
}


int
ConnApcUps::init ()
{
	int ret;
	struct sockaddr_in apc_addr;
	struct hostent *hp;

	sock = socket (AF_INET, SOCK_STREAM, 0);
        if (sock == -1)
        {
                logStream (MESSAGE_ERROR) << "Cannot create socket for an APC UPS TCP/IP connection, error: " << strerror (errno) << sendLog;
                return -1;
        }

        apc_addr.sin_family = AF_INET;
        hp = gethostbyname(hostname);
        bcopy ( hp->h_addr, &(apc_addr.sin_addr.s_addr), hp->h_length);
        apc_addr.sin_port = htons(port);

        ret = connect (sock, (struct sockaddr *) &apc_addr, sizeof(apc_addr));
        if (ret == -1)
        {
                logStream (MESSAGE_ERROR) << "Cannot connect socket, error: " << strerror (errno) << sendLog;
                return -1;
        }

        ret = fcntl (sock, F_SETFL, O_NONBLOCK);
        if (ret)
                return -1;
        return 0;
}


int
ConnApcUps::command (const char *cmd, char *_buf, int _buf_size)
{
	int ret;
	uint16_t len = htons (strlen (cmd));
	ret = send (sock, &len, 2, 0);
	if (ret != 2)
	{
		logStream (MESSAGE_DEBUG) << "Cannot send data to TCP/IP port" << sendLog;
		connectionError (-1);
		return -1;
	}
	ret = send (sock, cmd, strlen (cmd), 0);
	if (ret != strlen (cmd))
	{
		logStream (MESSAGE_DEBUG) << "Cannot send data to TCP/IP port" << sendLog;
		connectionError (-1);
		return -1;
	}

	// receive data..
	int left = 2;
	bool data = false;
        char reply_data[502];
	int rsize = 0;

	while (left > 0)
	{
		fd_set read_set;

	        struct timeval read_tout;
	        read_tout.tv_sec = 20;
	        read_tout.tv_usec = 0;
	
	        FD_ZERO (&read_set);
	
	        FD_SET (sock, &read_set);
	
	        ret = select (FD_SETSIZE, &read_set, NULL, NULL, &read_tout);
	        if (ret < 0)
	        {
	                logStream (MESSAGE_ERROR) << "error calling select function for apc ups, error: " << strerror (errno)
	                        << sendLog;
	                connectionError (-1);
	                return -1;
	        }
	        else if (ret == 0)
	        {
	                logStream (MESSAGE_ERROR) << "Timeout during call to select function. Calling connection error." << sendLog;
	                connectionError (-1);
	                return -1;
	        }
	
	        ret = recv (sock, reply_data + rsize, left, 0);
	        if (ret < 0)
	        {
	                logStream (MESSAGE_ERROR) << "Cannot read from APC UPS socket, error " << strerror (errno) << sendLog;
	                connectionError (-1);
	                return -1;
	        }
		if (!data)
		{
		  	std::cout << "data" << std::endl;
			left = ntohs (*((uint16_t *) reply_data));
			data = true;
			if (left == 0)
				return 0;
		}
		else
		{
			left -= ret;
			rsize += ret;
		}
		if (left == 0)
		{
			reply_data[rsize] = '\0';
			// try to parse reply
			if (reply_data[9] != ':')
			{
				logStream (MESSAGE_ERROR) << "Invalid reply data" << " " << ret << " " << reply_data << sendLog;
				return -1;
			}
			reply_data[9] = '\0';
			std::cout << "val '" << reply_data << "'" << std::endl;
			if (strcmp (reply_data, "END APC  ") == 0)
			{
			  	std::cout << "END" << std::endl;
			}
			else
			{
				// eat any spaces..
				char *pchr = reply_data + 8;
				while (isspace (*pchr) && pchr > reply_data)
				{
					pchr--;
				}
				pchr[1] = '\0';
				char *dat = reply_data + rsize - 1;
				while (isspace (*dat))
					dat--;
				dat[1] = '\0';
				dat = reply_data + 10;
				while (isspace (*dat))
				  	dat++;
				values[std::string (reply_data)] = std::string (dat);
			}
			left = 2;
			data = false;
			rsize = 0;
		}
	}
	return 0;
}


const char*
ConnApcUps::getString (const char *val)
{
	std::map <std::string, std::string>::iterator iter = values.find (val);
	if (values.find (val) == values.end ())
		return "";
	return (*iter).second.c_str ();
}


float
ConnApcUps::getPercents (const char *val)
{
	std::map <std::string, std::string>::iterator iter = values.find (val);
	if (values.find (val) == values.end ())
		return nan("f");
	return atof ((*iter).second.c_str());
}


float
ConnApcUps::getTemp (const char *val)
{
	const char *v = getString (val);
	if (strchr (v, 'C'))
		return nan("f");
	return atof (v);
}


int
ConnApcUps::getTime (const char *val)
{
	const char *v = getString (val);
	if (strcasestr (v, "hours") != NULL)
	  	return atof (v) * 3600;
	if (strcasestr (v, "minutes") != NULL)
		return atof (v) * 60;
	if (strcasestr (v, "seconds") != NULL)
	  	return atof (v);
	return 0;
}




int
ApcUps::processOption (int opt)
{
	switch (opt)
	{
		case 'a':
			host = new HostString (optarg, "3551");
			break;
		default:
			return SensorWeather::processOption (opt);
	}
	return 0;
}


int
ApcUps::init ()
{
  	int ret;
	ret = SensorWeather::init ();
	if (ret)
		return ret;
	
	connApc = new ConnApcUps (this, host->getHostname (), host->getPort ());
	ret = connApc->init ();
	if (ret)
		return ret;
	ret = info ();
	if (ret)
		return ret;
	setIdleInfoInterval (10);
	return 0;
}


int
ApcUps::info ()
{
	int ret;
	char reply[500];
	ret = connApc->command ("status", reply, 500);
	if (ret)
		return ret;
	model->setValueString (connApc->getString ("MODEL"));
	loadpct->setValueFloat (connApc->getPercents ("LOADPCT"));
	bcharge->setValueFloat (connApc->getPercents ("BCHARGE"));
	timeleft->setValueInteger (connApc->getTime ("TONBATT"));
	itemp->setValueFloat (connApc->getTemp ("ITEMP"));
	tonbatt->setValueInteger (connApc->getTime ("TIMELEFT"));
	status->setValueString (connApc->getString ("STATUS"));
	
	return SensorWeather::info ();
}


ApcUps::ApcUps (int argc, char **argv):SensorWeather (argc, argv)
{
  	createValue (model, "model", "UPS mode", false);
	createValue (loadpct, "load", "UPS load", false);
	createValue (bcharge, "bcharge", "battery charge", false);
	createValue (timeleft, "timeleft", "time left for on-UPS operations", false);
	createValue (itemp, "temperature", "internal UPS temperature", false);
	createValue (tonbatt, "tonbatt", "time on battery", false);
	createValue (status, "status", "UPS status", false);

	addOption ('a', NULL, 1, "hostname[:port] of apcupds");
}


ApcUps::~ApcUps (void)
{
	delete connApc;
}


int
main (int argc, char **argv)
{
	ApcUps device = ApcUps (argc, argv);
	return device.run ();
}
