/* 
 * Basic RTS2 devices and clients building block.
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

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <iostream>
#include <netinet/in.h>
#include <signal.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#include "block.h"
#include "rts2command.h"
#include "rts2client.h"
#include "rts2centralstate.h"

#include "imghdr.h"

//* Null terminated list of names for different device types.
const char *type_names[] = 
{
  "UNKNOWN", "SERVERD", "MOUNT", "CCD", "DOME", "WEATHER", "ARCH", "PHOT", "PLAN", "GRB", "FOCUS", // 10
  "MIRROR", "CUPOLA", "FILTER", "AUGERSH", "SENSOR", "UNKNOWN", "UNKNOWN", "UNKNOWN", "UNKNOWN" // 20
  "EXEC", "IMGP", "SELECTOR", "XMLRPC", "INDI", "LOGD", "SCRIPTOR", NULL
};

using namespace rts2core;

Block::Block (int in_argc, char **in_argv):rts2core::App (in_argc, in_argv)
{
	idle_timeout = USEC_SEC * 10;

	signal (SIGPIPE, SIG_IGN);

	masterState = SERVERD_HARD_OFF;
	stateMasterConn = NULL;
	// allocate ports dynamically
	port = 0;
}


Block::~Block (void)
{
	connections_t::iterator iter;
	for (iter = connections.begin (); iter != connections.end ();)
	{
		Rts2Conn *conn = *iter;
		iter = connections.erase (iter);
		delete conn;
	}
	for (iter = centraldConns.begin (); iter != centraldConns.end ();)
	{
		Rts2Conn *conn = *iter;
		iter = centraldConns.erase (iter);
		delete conn;
	}
	blockAddress.clear ();
	blockUsers.clear ();
}


void
Block::setPort (int in_port)
{
	port = in_port;
}

int Block::getPort (void)
{
	return port;
}

bool Block::commandQueEmpty ()
{
	connections_t::iterator iter;
	for (iter = connections.begin (); iter != connections.end (); iter++)
	{
		if (!(*iter)->queEmpty ())
			return false;
	}
	for (iter = centraldConns.begin (); iter != centraldConns.end (); iter++)
	{
		if (!(*iter)->queEmpty ())
			return false;
	}
	return true;
}

void Block::postEvent (Rts2Event * event)
{
	// send to all connections
	connections_t::iterator iter;
	for (iter = connections.begin (); iter != connections.end (); iter++)
		(*iter)->postEvent (new Rts2Event (event));
	for (iter = centraldConns.begin (); iter != centraldConns.end (); iter++)
		(*iter)->postEvent (new Rts2Event (event));
	return rts2core::App::postEvent (event);
}

Rts2Conn * Block::createConnection (int in_sock)
{
	return new Rts2Conn (in_sock, this);
}

void Block::addConnection (Rts2Conn *_conn)
{
	connections_added.push_back (_conn);
}

void Block::removeConnection (Rts2Conn *_conn)
{
	connections_t::iterator iter;
	for (iter = connections.begin (); iter != connections.end ();)
	{
		if (*iter == _conn)
			iter = connections.erase (iter);
		else
			iter++;
	}

	for (iter = connections_added.begin (); iter != connections_added.end ();)
	{
		if (*iter == _conn)
			iter = connections_added.erase (iter);
		else
			iter++;
	}
}

void Block::addCentraldConnection (Rts2Conn *_conn, bool added)
{
	if (added)
	  	centraldConns.push_back (_conn);
	else
		centraldConns_added.push_back (_conn);
}

Rts2Conn * Block::findName (const char *in_name)
{
	connections_t::iterator iter;
	for (iter = connections.begin (); iter != connections.end (); iter++)
	{
		Rts2Conn *conn = *iter;
		if (!strcmp (conn->getName (), in_name))
			return conn;
	}
	// if connection not found, try to look to list of available
	// connections
	return NULL;
}

Rts2Conn * Block::findCentralId (int in_id)
{
	connections_t::iterator iter;
	for (iter = connections.begin (); iter != connections.end (); iter++)
	{
		Rts2Conn *conn = *iter;
		if (conn->getCentraldId () == in_id)
			return conn;
	}
	return NULL;
}

int Block::sendAll (const char *msg)
{
	connections_t::iterator iter;
	for (iter = connections.begin (); iter != connections.end (); iter++)
		(*iter)->sendMsg (msg);
	for (iter = centraldConns.begin (); iter != centraldConns.end (); iter++)
		(*iter)->sendMsg (msg);
	return 0;
}

int Block::sendAllExcept (const char *msg, Rts2Conn *exceptConn)
{
	connections_t::iterator iter;
	for (iter = connections.begin (); iter != connections.end (); iter++)
		if ((*iter) != exceptConn)
			(*iter)->sendMsg (msg);
	for (iter = centraldConns.begin (); iter != centraldConns.end (); iter++)
	  	if ((*iter) != exceptConn)
			(*iter)->sendMsg (msg);
	return 0;
}

void Block::sendValueAll (char *val_name, char *value)
{
	connections_t::iterator iter;
	for (iter = connections.begin (); iter != connections.end (); iter++)
		(*iter)->sendValue (val_name, value);
	for (iter = centraldConns.begin (); iter != centraldConns.end (); iter++)
		(*iter)->sendValue (val_name, value);
}

void Block::sendMessageAll (Rts2Message & msg)
{
	connections_t::iterator iter;
	for (iter = connections.begin (); iter != connections.end (); iter++)
		(*iter)->sendMessage (msg);
	for (iter = centraldConns.begin (); iter != centraldConns.end (); iter++)
		(*iter)->sendMessage (msg);
}

void Block::sendStatusMessage (int state, const char * msg, Rts2Conn *commandedConn)
{
	std::ostringstream _os;
	_os << PROTO_STATUS << " " << state;
	if (msg != NULL)
		_os << " \"" << msg << "\"";
	if (commandedConn)
	{
		sendStatusMessageConn (state | DEVICE_SC_CURR, commandedConn);
		sendAllExcept (_os, commandedConn);
	}
	else
	{
		sendAll (_os);
	}
}

void Block::sendStatusMessageConn (int state, Rts2Conn * conn)
{
 	std::ostringstream _os;
	_os << PROTO_STATUS << " " << state;
	conn->sendMsg (_os);
}

void Block::sendBopMessage (int state, int bopState)
{
	std::ostringstream _os;
	_os << PROTO_BOP_STATE << " " << state << " " << bopState;
	sendAll (_os);
}

void Block::sendBopMessage (int state, int bopState, Rts2Conn * conn)
{
	std::ostringstream _os;
	_os << PROTO_BOP_STATE << " " << state << " " << bopState;
	conn->sendMsg (_os);
}

int Block::idle ()
{
	int ret;
	ret = waitpid (-1, NULL, WNOHANG);
	if (ret > 0)
	{
		childReturned (ret);
	}
	connections_t::iterator iter;
	for (iter = connections.begin (); iter != connections.end (); iter++)
		(*iter)->idle ();
	for (iter = centraldConns.begin (); iter != centraldConns.end (); iter++)
		(*iter)->idle ();

	// add from connection queue..
	for (iter = connections_added.begin (); iter != connections_added.end (); iter = connections_added.erase (iter))
	{
		connections.push_back (*iter);
	}

	for (iter = centraldConns_added.begin (); iter != centraldConns_added.end (); iter = centraldConns_added.erase (iter))
	{
		centraldConns.push_back (*iter);
	}

	// test for any pending timers..
	std::map <double, Rts2Event *>::iterator iter_t = timers.begin ();
	while (iter_t != timers.end () && iter_t->first < getNow ())
	{
		Rts2Event *sec = iter_t->second;
		timers.erase (iter_t++);
	 	if (sec->getArg () != NULL)
		  	((Rts2Object *)sec->getArg ())->postEvent (sec);
		else
			postEvent (sec);
	}

	return 0;
}

void Block::addSelectSocks ()
{
	connections_t::iterator iter;
	for (iter = connections.begin (); iter != connections.end (); iter++)
		(*iter)->add (&read_set, &write_set, &exp_set);
	for (iter = centraldConns.begin (); iter != centraldConns.end (); iter++)
		(*iter)->add (&read_set, &write_set, &exp_set);
}

void Block::selectSuccess ()
{
	Rts2Conn *conn;
	int ret;

	connections_t::iterator iter;

	for (iter = connections.begin (); iter != connections.end ();)
	{
		conn = *iter;
		if (conn->receive (&read_set) == -1 || conn->writable (&write_set) == -1)
		{
			ret = deleteConnection (conn);
			// delete connection only when it really requested to be deleted..
			if (!ret)
			{
				iter = connections.erase (iter);
				connectionRemoved (conn);
				delete conn;
			}
			else
			{
				iter++;
			}
		}
		else
		{
			iter++;
		}
	}
	for (iter = centraldConns.begin (); iter != centraldConns.end ();)
	{
		conn = *iter;
		if (conn->receive (&read_set) == -1 || conn->writable (&write_set) == -1)
		{
			#ifdef DEBUG_EXTRA
			logStream (MESSAGE_DEBUG) << "Will delete connection " << " name: " << conn->getName () << sendLog;
			#endif
			ret = deleteConnection (conn);
			// delete connection only when it really requested to be deleted..
			if (!ret)
			{
				iter = centraldConns.erase (iter);
				connectionRemoved (conn);
				delete conn;
			}
			else
			{
				iter++;
			}
		}
		else
		{
			iter++;
		}
	}
}

void Block::setMessageMask (int new_mask)
{
	connections_t::iterator iter;
	for (iter = centraldConns.begin (); iter != centraldConns.end (); iter++)
		(*iter)->queCommand (new Rts2CommandMessageMask (this, new_mask));
}

void Block::oneRunLoop ()
{
	int ret;
	struct timeval read_tout;
	double t_diff;

	if (timers.begin () != timers.end () && (USEC_SEC * (t_diff = timers.begin ()->first - getNow ())) < idle_timeout)
	{
		read_tout.tv_sec = t_diff;
		read_tout.tv_usec = (t_diff - floor (t_diff)) * USEC_SEC;
	}
	else
	{
		read_tout.tv_sec = idle_timeout / USEC_SEC;
		read_tout.tv_usec = idle_timeout % USEC_SEC;
	}

	FD_ZERO (&read_set);
	FD_ZERO (&write_set);
	FD_ZERO (&exp_set);

	addSelectSocks ();
	ret = select (FD_SETSIZE, &read_set, &write_set, &exp_set, &read_tout);
	if (ret > 0)
		selectSuccess ();
	ret = idle ();
	if (ret == -1)
		endRunLoop ();
}

int Block::deleteConnection (Rts2Conn * conn)
{
	if (conn->isConnState (CONN_DELETE))
	{
		connections_t::iterator iter;
		// try to look if there are any references to connection in other connections
		for (iter = connections.begin (); iter != connections.end (); iter++)
		{
			if (conn != *iter)
				(*iter)->deleteConnection (conn);
		}
		for (iter = centraldConns.begin (); iter != centraldConns.end (); iter++)
		{
			if (conn != *iter)
				(*iter)->deleteConnection (conn);
		}
		return 0;
	}
	// don't delete us when we are in incorrect state
	return -1;
}

void Block::connectionRemoved (Rts2Conn * conn)
{
}

void Block::deviceReady (Rts2Conn * conn)
{
}

void Block::deviceIdle (Rts2Conn * conn)
{
}

void Block::changeMasterState (int old_state, int new_state)
{
	connections_t::iterator iter;
	// send message to all connections that they can possibly continue executing..
	for (iter = connections.begin (); iter != connections.end (); iter++)
		(*iter)->masterStateChanged ();
	for (iter = centraldConns.begin (); iter != centraldConns.end (); iter++)
		(*iter)->masterStateChanged ();
}

void Block::bopStateChanged ()
{
	connections_t::iterator iter;
	// send message to all connections that they can possibly continue executing..
	for (iter = connections.begin (); iter != connections.end (); iter++)
		(*iter)->masterStateChanged ();
	for (iter = centraldConns.begin (); iter != centraldConns.end (); iter++)
		(*iter)->masterStateChanged ();
}

void Block::updateMetaInformations (Value *value)
{
	connections_t::iterator iter;
	for (iter = connections.begin (); iter != connections.end (); iter++)
		value->sendMetaInfo (*iter);
	for (iter = centraldConns.begin (); iter != centraldConns.end (); iter++)
		value->sendMetaInfo (*iter);
}

std::map <Rts2Conn *, std::vector <Value *> > Block::failedValues ()
{
	std::map <Rts2Conn *, std::vector <Value *> > ret;
	ValueVector::iterator viter;
	for (connections_t::iterator iter = connections.begin (); iter != connections.end (); iter++)
	{
		viter = (*iter)->valueBegin ();
		for (viter = (*iter)->getFailedValue (viter); viter != (*iter)->valueEnd (); viter = (*iter)->getFailedValue (viter))
		{
			ret[*iter].push_back (*viter);
			viter++;
		}
	}
	for (connections_t::iterator iter = centraldConns.begin (); iter != centraldConns.end (); iter++)
	{
		viter = (*iter)->valueBegin ();
		for (viter = (*iter)->getFailedValue (viter); viter != (*iter)->valueEnd (); viter = (*iter)->getFailedValue (viter))
		{
			ret[*iter].push_back (*viter);
			viter++;
		}
	}
	return ret;
}

bool Block::centralServerInState (int state)
{
	for (connections_t::iterator iter = centraldConns.begin (); iter != centraldConns.end (); iter++)
	{
		if ((*iter)->getState () & state)
			return true;
	}
	return false;
}

int Block::setMasterState (Rts2Conn *_conn, int new_state)
{
	int old_state = masterState;
	// ignore connections from wrong master..
	if (stateMasterConn != NULL && _conn != stateMasterConn)
	{
		if ((new_state & SERVERD_STATUS_MASK) != SERVERD_HARD_OFF && (new_state & WEATHER_MASK) != BAD_WEATHER && (new_state & SERVERD_STANDBY_MASK) != SERVERD_STANDBY)
		{
			if ((old_state & ~BOP_MASK) != (new_state & ~BOP_MASK))
				logStream (MESSAGE_DEBUG) << "ignoring state change, as it does not arrive from master connection" << sendLog;
			return 0;
		}
		// ignore request from non-master server asking us to switch to standby, when we are in hard off
		if ((masterState & SERVERD_STATUS_MASK) == SERVERD_HARD_OFF && (new_state & SERVERD_STANDBY_MASK) == SERVERD_STANDBY)
			return 0;
	}
	// change state NOW, before it will mess in processing routines
	masterState = new_state;

	if ((old_state & ~BOP_MASK) != (new_state & ~BOP_MASK))
	{
		// call changeMasterState only if something except BOP_MASK changed
		changeMasterState (old_state, new_state);
	}
	if ((old_state & BOP_MASK) != (new_state & BOP_MASK))
	{
		bopStateChanged ();
	}
	return 0;
}

std::string Block::getMasterStateString ()
{
	return Rts2CentralState (getMasterStateFull ()).getString ();
}

void Block::childReturned (pid_t child_pid)
{
	#ifdef DEBUG_EXTRA
	logStream (MESSAGE_DEBUG) << "child returned: " << child_pid << sendLog;
	#endif
	connections_t::iterator iter;
	for (iter = connections.begin (); iter != connections.end (); iter++)
		(*iter)->childReturned (child_pid);
	for (iter = centraldConns.begin (); iter != centraldConns.end (); iter++)
		(*iter)->childReturned (child_pid);
}

int Block::willConnect (Rts2Address * in_addr)
{
	return 0;
}

bool Block::isGoodWeather ()
{
	if (getCentraldConns ()->size () <= 0)
		return false;
	// check if all masters are up and running and think it is good idea to open
	// the roof
	connections_t::iterator iter;
	for (iter = getCentraldConns ()->begin (); iter != getCentraldConns ()->end (); iter++)
	{
		if (!((*iter)->isConnState (CONN_CONNECTED) || (*iter)->isConnState (CONN_AUTH_OK)))
			return false;
		if ((*iter)->isGoodWeather () == false)
			return false;
	}
	return true;
}

bool Block::canMove ()
{
	if (getCentraldConns ()->size () <= 0)
		return false;
	// check if all masters are up and running and think it is good idea to open
	// the roof
	connections_t::iterator iter;
	for (iter = getCentraldConns ()->begin (); iter != getCentraldConns ()->end (); iter++)
	{
		if (!((*iter)->isConnState (CONN_CONNECTED) || (*iter)->isConnState (CONN_AUTH_OK)))
			return false;
		if ((*iter)->canMove () == false)
			return false;
	}
	return true;
}

bool Block::allCentraldRunning ()
{
	if (getCentraldConns ()->size () <= 0)
		return false;
	// check if all masters are up and running and think it is good idea to open
	// the roof
	connections_t::iterator iter;
	for (iter = getCentraldConns ()->begin (); iter != getCentraldConns ()->end (); iter++)
	{
		if (!((*iter)->isConnState (CONN_CONNECTED) || (*iter)->isConnState (CONN_AUTH_OK)))
			return false;
	}
	return true;
}

bool Block::someCentraldRunning ()
{
	if (getCentraldConns ()->size () <= 0)
		return true;
	// check if all masters are up and running and think it is good idea to open
	// the roof
	connections_t::iterator iter;
	for (iter = getCentraldConns ()->begin (); iter != getCentraldConns ()->end (); iter++)
	{
		if ((*iter)->isConnState (CONN_CONNECTED) || (*iter)->isConnState (CONN_AUTH_OK))
			return true;
	}
	return false;
}

Rts2Address * Block::findAddress (const char *blockName)
{
	std::list < Rts2Address * >::iterator addr_iter;

	for (addr_iter = blockAddress.begin (); addr_iter != blockAddress.end (); addr_iter++)
	{
		Rts2Address *addr = (*addr_iter);
		if (addr->isAddress (blockName))
		{
			return addr;
		}
	}
	return NULL;
}

Rts2Address * Block::findAddress (int centraldNum, const char *blockName)
{
	std::list < Rts2Address * >::iterator addr_iter;

	for (addr_iter = blockAddress.begin (); addr_iter != blockAddress.end (); addr_iter++)
	{
		Rts2Address *addr = (*addr_iter);
		if (addr->isAddress (centraldNum, blockName))
		{
			return addr;
		}
	}
	return NULL;
}

void Block::addAddress (int p_host_num, int p_centrald_num, int p_centrald_id, const char *p_name, const char *p_host, int p_port,
int p_device_type)
{
	int ret;
	Rts2Address *an_addr;
	an_addr = findAddress (p_centrald_num, p_name);
	if (an_addr)
	{
		ret = an_addr->update (p_centrald_num, p_name, p_host, p_port, p_device_type);
		if (!ret)
		{
			addAddress (an_addr);
			return;
		}
	}
	an_addr = new Rts2Address (p_host_num, p_centrald_num, p_centrald_id, p_name, p_host, p_port, p_device_type);
	blockAddress.push_back (an_addr);
	addAddress (an_addr);
}

int Block::addAddress (Rts2Address * in_addr)
{
	Rts2Conn *conn;
	// recheck all connections waiting for our address
	conn = getOpenConnection (in_addr->getName ());
	if (conn)
		conn->addressUpdated (in_addr);
	else if (willConnect (in_addr))
	{
		conn = createClientConnection (in_addr);
		if (conn)
			addConnection (conn);
	}
	return 0;
}

void Block::deleteAddress (int p_centrald_num, const char *p_name)
{
	std::list < Rts2Address * >::iterator addr_iter;

	for (addr_iter = blockAddress.begin (); addr_iter != blockAddress.end (); addr_iter++)
	{
		Rts2Address *addr = (*addr_iter);
		if (addr->isAddress (p_centrald_num, p_name))
		{
			blockAddress.erase (addr_iter);
			delete addr;
			return;
		}
	}
}

Rts2DevClient * Block::createOtherType (Rts2Conn * conn, int other_device_type)
{
	switch (other_device_type)
	{
		case DEVICE_TYPE_MOUNT:
			return new Rts2DevClientTelescope (conn);
		case DEVICE_TYPE_CCD:
			return new Rts2DevClientCamera (conn);
		case DEVICE_TYPE_DOME:
			return new Rts2DevClientDome (conn);
		case DEVICE_TYPE_CUPOLA:
			return new Rts2DevClientCupola (conn);
		case DEVICE_TYPE_PHOT:
			return new Rts2DevClientPhot (conn);
		case DEVICE_TYPE_FW:
			return new Rts2DevClientFilter (conn);
		case DEVICE_TYPE_EXECUTOR:
			return new Rts2DevClientExecutor (conn);
		case DEVICE_TYPE_IMGPROC:
			return new Rts2DevClientImgproc (conn);
		case DEVICE_TYPE_SELECTOR:
			return new Rts2DevClientSelector (conn);
		case DEVICE_TYPE_GRB:
			return new Rts2DevClientGrb (conn);

		default:
			return new Rts2DevClient (conn);
	}
}

void Block::addUser (int p_centraldId, const char *p_login)
{
	int ret;
	std::list < Rts2ConnUser * >::iterator user_iter;
	for (user_iter = blockUsers.begin (); user_iter != blockUsers.end (); user_iter++)
	{
		ret =
			(*user_iter)->update (p_centraldId, p_login);
		if (!ret)
			return;
	}
	addUser (new Rts2ConnUser (p_centraldId, p_login));
}

int Block::addUser (Rts2ConnUser * in_user)
{
	blockUsers.push_back (in_user);
	return 0;
}

Rts2Conn * Block::getOpenConnection (const char *deviceName)
{
	connections_t::iterator iter;

	// try to find active connection..
	for (iter = connections.begin (); iter != connections.end (); iter++)
	{
		Rts2Conn *conn = *iter;
		if (conn->isName (deviceName))
			return conn;
	}
	return NULL;
}

void Block::getOpenConnectionType (int deviceType, connections_t::iterator &current)
{
	for (; current != connections.end (); current++)
	{
		if ((*current)->getOtherType () == deviceType)
			return;
	}
}

Rts2Conn * Block::getOpenConnection (int device_type)
{
	connections_t::iterator iter;

	for (iter = connections.begin (); iter != connections.end (); iter++)
	{
		Rts2Conn *conn = *iter;
		if (conn->getOtherType () == device_type)
			return conn;
	}
	return NULL;
}

Rts2Conn * Block::getConnection (char *deviceName)
{
	Rts2Conn *conn;
	Rts2Address *devAddr;

	conn = getOpenConnection (deviceName);
	if (conn)
		return conn;

	devAddr = findAddress (deviceName);

	if (!devAddr)
	{
		logStream (MESSAGE_ERROR)
			<< "Cannot find device with name " << deviceName
			<< sendLog;
		return NULL;
	}

	// open connection to given address..
	conn = createClientConnection (devAddr);
	if (conn)
		addConnection (conn);
	return conn;
}

int Block::getCentraldIdAtNum (int centrald_num)
{
	connections_t::iterator iter;
	for (iter = getCentraldConns ()->begin (); iter != getCentraldConns ()->end (); iter++)
	{
		if ((*iter)->getCentraldNum () == centrald_num)
			return (*iter)->getCentraldId ();
	}
	return -1;
}

void Block::message (Rts2Message & msg)
{
}

void Block::clearAll ()
{
	connections_t::iterator iter;
	for (iter = connections.begin (); iter != connections.end (); iter++)
		(*iter)->queClear ();
	for (iter = centraldConns.begin (); iter != centraldConns.end (); iter++)
		(*iter)->queClear ();
}

int Block::queAll (Rts2Command * command)
{
	// go throught all adresses
	std::list < Rts2Address * >::iterator addr_iter;
	Rts2Conn *conn;

	for (addr_iter = blockAddress.begin (); addr_iter != blockAddress.end ();
		addr_iter++)
	{
		conn = getConnection ((*addr_iter)->getName ());
		if (conn)
		{
			Rts2Command *newCommand = new Rts2Command (command);
			conn->queCommand (newCommand);
		}
		else
		{
			#ifdef DEBUG_EXTRA
			logStream (MESSAGE_DEBUG) << "Block::queAll no connection for "
				<< (*addr_iter)->getName () << sendLog;
			#endif
		}
	}
	delete command;
	return 0;
}

int Block::queAll (const char *text)
{
	return queAll (new Rts2Command (this, text));
}

void Block::queAllCentralds (const char *command)
{
	for (connections_t::iterator iter = centraldConns.begin (); iter != centraldConns.end (); iter++)
		(*iter)->queCommand (new Rts2Command (this, command));
}

Rts2Conn * Block::getMinConn (const char *valueName)
{
	int lovestValue = INT_MAX;
	Rts2Conn *minConn = NULL;
	connections_t::iterator iter;
	for (iter = connections.begin (); iter != connections.end (); iter++)
	{
		Value *que_size;
		Rts2Conn *conn = *iter;
		que_size = conn->getValue (valueName);
		if (que_size)
		{
			if (que_size->getValueInteger () >= 0
				&& que_size->getValueInteger () < lovestValue)
			{
				minConn = conn;
				lovestValue = que_size->getValueInteger ();
			}
		}
	}
	return minConn;
}

Value * Block::getValue (const char *device_name, const char *value_name)
{
	Rts2Conn *conn = getOpenConnection (device_name);
	if (!conn)
		return NULL;
	return conn->getValue (value_name);
}

int Block::statusInfo (Rts2Conn * conn)
{
	return 0;
}

bool Block::commandOriginatorPending (Rts2Object * object, Rts2Conn * exclude_conn)
{
	connections_t::iterator iter;
	for (iter = connections.begin (); iter != connections.end (); iter++)
	{
		if ((*iter) != exclude_conn && (*iter)->commandOriginatorPending (object))
		{
			#ifdef DEBUG_EXTRA
			std::cout << "command originator pending on " << (*iter)->getName () << std::endl;
			#endif				 /* DEBUG_EXTRA */
			return true;
		}
	}
	for (iter = centraldConns.begin (); iter != centraldConns.end (); iter++)
	{
		if ((*iter) != exclude_conn && (*iter)->commandOriginatorPending (object))
		{
			#ifdef DEBUG_EXTRA
			std::cout << "command originator pending on " << (*iter)->getName () << std::endl;
			#endif				 /* DEBUG_EXTRA */
			return true;
		}
	}
	return false;
}

void Block::deleteTimers (int event_type)
{
	for (std::map <double, Rts2Event *>::iterator iter = timers.begin (); iter != timers.end (); )
	{
		if (iter->second->getType () == event_type)
		{
			delete (iter->second);
			timers.erase (iter++);
		}
		else
		{
			iter++;
		}
	}
}

void Block::valueMaskError (Value *val, int32_t err)
{
  	if ((val->getFlags () & RTS2_VALUE_ERRORMASK) != err)
	{
		val->maskError (err);
		updateMetaInformations (val);
	}
}

bool isCentraldName (const char *_name)
{
	return !strcmp (_name, "..") || !strcmp (_name, "centrald");
}