/**
 * Executor client for telescope and camera.
 * Copyright (C) 2005-2007 Petr Kubanek <petr@kubanek.net>
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

#include <limits.h>
#include <iostream>

#include "connimgprocess.h"
#include "rts2execcli.h"
#include "../writers/image.h"
#include "../utilsdb/target.h"
#include "../utils/rts2command.h"
#include "../utils/rts2config.h"

using namespace rts2image;

Rts2DevClientCameraExec::Rts2DevClientCameraExec (Rts2Conn * _connection, rts2core::ValueString *_expandPath):DevClientCameraImage (_connection), DevScript (_connection)
{
	expandPath = _expandPath;
	waitForExposure = false;
	waitMetaData = false;
	imgCount = 0;
}

Rts2DevClientCameraExec::~Rts2DevClientCameraExec ()
{
	deleteScript ();
}

Image * Rts2DevClientCameraExec::createImage (const struct timeval *expStart)
{
	exposureScript = getScript ();
	if (expandPath)
		return new Image (expandPath->getValue (), getExposureNumber (), expStart, connection);
	return DevClientCameraImage::createImage (expStart);
}

void Rts2DevClientCameraExec::postEvent (Rts2Event * event)
{
	Image *image;
	int type = event->getType ();
	switch (type)
	{
		case EVENT_KILL_ALL:
			waitForExposure = false;
			break;
		case EVENT_QUE_IMAGE:
		case EVENT_AFTER_COMMAND_FINISHED:
			image = (Image *) event->getArg ();
			if (!strcmp (image->getCameraName (), getName ()))
				queImage (image);
			break;
		case EVENT_COMMAND_OK:
			nextCommand ();
			break;
		case EVENT_COMMAND_FAILED:
			nextCommand ();
			//deleteScript ();
			break;
	}
	DevScript::postEvent (event);
	DevClientCameraImage::postEvent (event);
	// must be done after processing trigger in parent
	if (type == EVENT_TRIGGERED && !waitForExposure)
		nextCommand ();
}

void Rts2DevClientCameraExec::startTarget ()
{
	DevScript::startTarget ();
}

int Rts2DevClientCameraExec::getNextCommand ()
{
	// there is only one continue which can bring us on beginning
	while (true)
	{
		int ret = getScript ()->nextCommand (*this, &nextComd, cmd_device);
		if (nextComd)
		{
			// send command to other device
			if (strcmp (getName (), cmd_device))
			{
				cmdConn = getMaster ()->getOpenConnection (cmd_device);
				if (!cmdConn)
				{
					logStream (MESSAGE_ERROR)
						<< "Unknow device : " << cmd_device
						<< "for command " << nextComd->getText ()
						<< sendLog;
					// only there try to get next command
					continue;
				}
			}
			else
			{
				cmdConn = NULL;
			}
			nextComd->setOriginator (this);
			return ret;
		}
		cmdConn = NULL;
		return ret;
	}
}

bool Rts2DevClientCameraExec::canEndScript ()
{
	rts2core::Value *queExpNum = getConnection ()->getValue ("que_exp_num");
	if (queExpNum && queExpNum->getValueInteger () != 0)
		return false;
	if (isExposing ())
		return false;
	return DevScript::canEndScript ();
}

void Rts2DevClientCameraExec::nextCommand ()
{
	int ret;
	ret = haveNextCommand (this);
	#ifdef DEBUG_EXTRA
	logStream (MESSAGE_DEBUG) << "Rts2DevClientCameraExec::nextComd " << ret << sendLog;
	#endif						 /* DEBUG_EXTRA */
	if (!ret)
		return;

	waitMetaData = false;

	if (nextComd->getBopMask () & BOP_EXPOSURE)
	{
		// if command cannot be executed when telescope is moving, do not execute it
		// before target was moved
		if (currentTarget && !currentTarget->wasMoved())
			return;
		// do not execute next exposure before all meta data of the current exposure are written
		if (waitForMetaData ())
		{
			waitMetaData = true;
		  	return;
		}
	}

	rts2core::Value *val;

	//std::cout << "bopMask " << std::hex << nextComd->getBopMask () << std::endl;

	if (nextComd->getBopMask () & BOP_WHILE_STATE)
	{
		// if there are queued exposures, do not execute command
		val = getConnection ()->getValue ("que_exp_num");
		if (val && val->getValueInteger () != 0)
		{
			//std::cout << "val > 0" << std::endl;
			return;
		}
		// if there are commands in queue, do not execute command
		if (!connection->queEmptyForOriginator (this))
		{
			//std::cout << "not empty " << std::endl;
			return;
		}
	}
	else if (nextComd->getBopMask () & BOP_TEL_MOVE)
	{
		// if there are queued exposures, do not execute command
		val = getConnection ()->getValue ("que_exp_num");
		if (val && val->getValueInteger () != 0)
		{
			//std::cout << "TEL_MOVE val > 0" << std::endl;
			return;
		}
		if (waitForExposure)
		{
			//std::cout << "wait for exposure" << std::endl;
			return;
		}
	}
	else if (nextComd->getBopMask () & BOP_EXPOSURE)
	{
		if (waitForExposure)
			return;
	}

	// send command to other device
	if (cmdConn)
	{
		if (!(nextComd->getBopMask () & BOP_WHILE_STATE))
		{
			// if there are some commands in que, do not proceed, as they might change state of the device
			if (!connection->queEmptyForOriginator (this))
			{
				//std::cout << "queue not empty" << std::endl;
				return;
			}

			// do not execute if there are some exposures in queue
			val = getConnection ()->getValue ("que_exp_num");
			if (val && val->getValueInteger () > 0)
			{
				return;
			}

			if ((getConnection ()->getState () & CAM_EXPOSING) || (getConnection ()->getBopState () & BOP_TEL_MOVE) || (getConnection ()->getFullBopState () & BOP_TEL_MOVE))
			{
				//std::cout << "wrong state" << std::endl;
				return;
			}

			nextComd->setBopMask (nextComd->getBopMask () & ~BOP_TEL_MOVE);
		}

		// execute command
		// when it returns, we can execute next command
		cmdConn->queCommand (nextComd);
		nextComd = NULL;
		waitForExposure = false;
		return;
	}

	//#ifdef DEBUG_EXTRA
	logStream (MESSAGE_DEBUG) << "For " << getName () << " queueing " << nextComd->getText () << sendLog;
	//#endif						 /* DEBUG_EXTRA */
	waitForExposure = nextComd->getBopMask () & BOP_EXPOSURE;
	queCommand (nextComd);
	nextComd = NULL;			 // after command execute, it will be deleted
	if (waitForExposure)
		setTriggered ();
}

void Rts2DevClientCameraExec::queImage (Image * image)
{
	// find image processor with lowest que number..
	Rts2Conn *minConn = getMaster ()->getMinConn ("queue_size");
	if (!minConn)
		return;

	// try immediately processing..
	std::string after_command;
	if (Rts2Config::instance ()->getString (getName (), "after_exposure_cmd", after_command) == 0)
	{
		int timeout = 60;
		std::string arg;
		Rts2Config::instance ()->getInteger (getName (), "after_exposure_cmd_timeout", timeout);
		rts2plan::ConnImgProcess *afterCommand = new rts2plan::ConnImgProcess (getMaster (), after_command.c_str (), image->getAbsoluteFileName (), timeout, EVENT_AFTER_COMMAND_FINISHED);
		Rts2Config::instance ()->getString (getName (), "after_exposure_cmd_arg", arg);
		afterCommand->addArg (image->expand (arg));
		int ret = afterCommand->init ();
		if (ret)
		{
			delete afterCommand;
		}
		else
		{
			getMaster ()->addConnection (afterCommand);
		}
	}

	if (image->getImageType () != IMGTYPE_FLAT && image->getImageType () != IMGTYPE_DARK)
	{
		minConn->queCommand (new Rts2CommandQueImage (getMaster (), image));
	}
}

imageProceRes Rts2DevClientCameraExec::processImage (Image * image)
{
	int ret;
	// make sure script continues if it is waiting for metadata
	if (waitMetaData && !waitForExposure)
		nextCommand ();
	// try processing in script..
	if (exposureScript.get ())
	{
		ret = exposureScript->processImage (image);
		if (ret > 0)
		{
			return IMAGE_KEEP_COPY;
		}
		else if (ret == 0)
		{
			return IMAGE_DO_BASIC_PROCESSING;
		}
		// otherwise queue image processing
	}
	queImage (image);
	return IMAGE_DO_BASIC_PROCESSING;
}

void Rts2DevClientCameraExec::idle ()
{
	DevScript::idle ();
	DevClientCameraImage::idle ();
	// when it is the first command in the script..
	if (getScript ().get () && getScript ()->getExecutedCount () == 0)
		nextCommand ();
}

void Rts2DevClientCameraExec::exposureStarted ()
{
	if (nextComd && (nextComd->getBopMask () & BOP_WHILE_STATE))
		nextCommand ();
	
	if (waitForExposure)
	{
		waitForExposure = false;
		if (nextComd && (nextComd->getBopMask () & BOP_TEL_MOVE))
			nextCommand ();
	}

	DevClientCameraImage::exposureStarted ();
}

void Rts2DevClientCameraExec::exposureEnd ()
{
	rts2core::Value *val = getConnection ()->getValue ("que_exp_num");
	// if script is running, inform it about end of exposure..
	if (exposureScript.get ())
		exposureScript->exposureEnd ();
	// if script is running and it does not have anything to do, end it
	if (getScript ().get ()
		&& !nextComd
		&& getScript ()->isLastCommand ()
		&& (!val || val->getValueInteger () == 0)
		&& !getMaster ()->commandOriginatorPending (this, NULL)
		)
	{
		deleteScript ();
		getMaster ()->postEvent (new Rts2Event (EVENT_LAST_READOUT));
	}
	// execute value change, if we do not execute that during exposure
	if (strcmp (getName (), cmd_device) && nextComd && (!(nextComd->getBopMask () & BOP_WHILE_STATE)) &&
		!isExposing () && val && val->getValueInteger () == 0)
		nextCommand ();

	// execute next command if it's null
	if (nextComd == NULL)
		nextCommand ();

	// send readout after we deal with next command - which can be filter move
	DevClientCameraImage::exposureEnd ();
}

void Rts2DevClientCameraExec::stateChanged (Rts2ServerState * state)
{
	DevClientCameraImage::stateChanged (state);
	if (nextComd && cmdConn && !(state->getValue () & BOP_TEL_MOVE) && !(getConnection ()->getFullBopState () & BOP_TEL_MOVE))
		nextCommand ();
}

void Rts2DevClientCameraExec::exposureFailed (int status)
{
	// in case of an error..
	DevClientCameraImage::exposureFailed (status);
	logStream (MESSAGE_WARNING) << "detected exposure failure. Continuing with the script" << sendLog;
	nextCommand ();
}

void Rts2DevClientCameraExec::readoutEnd ()
{
	DevClientCameraImage::readoutEnd ();
	nextCommand ();
}

Rts2DevClientTelescopeExec::Rts2DevClientTelescopeExec (Rts2Conn * _connection):DevClientTelescopeImage (_connection)
{
	currentTarget = NULL;
	cmdChng = NULL;
	fixedOffset.ra = 0;
	fixedOffset.dec = 0;
}

void Rts2DevClientTelescopeExec::postEvent (Rts2Event * event)
{
	int ret;
	struct ln_equ_posn *offset;
	GuidingParams *gp;
	switch (event->getType ())
	{
		case EVENT_KILL_ALL:
			clearWait ();
			break;
		case EVENT_SET_TARGET:
			currentTarget = (rts2db::Target *) event->getArg ();
			break;
		case EVENT_NEW_TARGET:
		case EVENT_CHANGE_TARGET:
			{
				struct ln_equ_posn *pos = (struct ln_equ_posn *) event->getArg ();
				queCommand (new Rts2CommandMove (getMaster (), this, pos->ra, pos->dec), BOP_TEL_MOVE);
			}
			break;
		case EVENT_NEW_TARGET_ALTAZ:
		case EVENT_CHANGE_TARGET_ALTAZ:
			{
				struct ln_hrz_posn *hrz = (struct ln_hrz_posn *) event->getArg ();
				queCommand (new Rts2CommandMoveAltAz (getMaster (), this, hrz->alt, hrz->az), BOP_TEL_MOVE);
			}
			break;
		case EVENT_SLEW_TO_TARGET:
		case EVENT_SLEW_TO_TARGET_NOW:
			if (currentTarget)
			{
				currentTarget->beforeMove ();
				ret = syncTarget (event->getType () == EVENT_SLEW_TO_TARGET_NOW);
				switch (ret)
				{
					case OBS_DONT_MOVE:
						getMaster ()->postEvent (new Rts2Event (EVENT_OBSERVE, (void *) currentTarget));
						break;
					case OBS_MOVE:
						fixedOffset.ra = 0;
						fixedOffset.dec = 0;
					case OBS_MOVE_FIXED:
						queCommand (new Rts2CommandScriptEnds (getMaster ()));
					case OBS_ALREADY_STARTED:
						break;
				}
			}
			break;
		case EVENT_TEL_SCRIPT_RESYNC:
			cmdChng = NULL;
			checkInterChange ();
			break;
		case EVENT_TEL_SCRIPT_CHANGE:
			cmdChng = new Rts2CommandChange ((Rts2CommandChange *) event->getArg (), this);
			checkInterChange ();
			break;
		case EVENT_ENTER_WAIT:
			if (cmdChng)
			{
				queCommand (cmdChng);
				cmdChng = NULL;
			}
			else
			{
				ret = syncTarget ();
				if (ret == OBS_DONT_MOVE)
				{
					postEvent (new Rts2Event (EVENT_MOVE_OK));
				}
			}
			break;
		case EVENT_MOVE_OK:
		case EVENT_CORRECTING_OK:
		case EVENT_MOVE_FAILED:
			break;
		case EVENT_ADD_FIXED_OFFSET:
			offset = (ln_equ_posn *) event->getArg ();
			// ra hold offset in HA - that increase on west
			// but we get offset in RA, which increase on east
			fixedOffset.ra += offset->ra;
			fixedOffset.dec += offset->dec;
			break;
		case EVENT_ACQUSITION_END:
			break;
		case EVENT_TEL_START_GUIDING:
			gp = (GuidingParams *) event->getArg ();
			queCommand (new Rts2CommandStartGuide (getMaster (), gp->dir, gp->dist));
			break;
		case EVENT_TEL_STOP_GUIDING:
			queCommand (new Rts2CommandStopGuideAll (getMaster ()));
			break;
	}
	DevClientTelescopeImage::postEvent (event);
}

int Rts2DevClientTelescopeExec::syncTarget (bool now)
{
	struct ln_equ_posn coord;
	int ret;
	if (!currentTarget)
		return -1;
	getEqu (&coord);
	// startSlew fills coordinates, if needed..
	ret = currentTarget->startSlew (&coord);
	if (isnan (coord.ra) || isnan (coord.dec))
		return 0;
	int bopTel = now ? 0 : BOP_TEL_MOVE;
	switch (ret)
	{
		case OBS_MOVE:
			currentTarget->moveStarted ();
			queCommand (new Rts2CommandMove (getMaster (), this, coord.ra, coord.dec), bopTel);
			break;
		case OBS_MOVE_UNMODELLED:
			currentTarget->moveStarted ();
			queCommand (new Rts2CommandMoveUnmodelled (getMaster (), this, coord.ra, coord.dec), bopTel);
			break;
		case OBS_MOVE_FIXED:
			currentTarget->moveStarted ();
			logStream (MESSAGE_DEBUG) << "Rts2DevClientTelescopeExec::syncTarget ha " << coord.ra << " dec " << coord.dec << " oha " << fixedOffset.ra << " odec " << fixedOffset.dec << sendLog;
			// we are ofsetting in HA, but offset is in RA - hence -
			queCommand (new	Rts2CommandMoveFixed (getMaster (), this, coord.ra - fixedOffset.ra, coord.dec + fixedOffset.dec));
			break;
		case OBS_ALREADY_STARTED:
			currentTarget->moveStarted ();
			if (fixedOffset.ra != 0 || fixedOffset.dec != 0)
			{
			#ifdef DEBUG_EXTRA
				logStream (MESSAGE_DEBUG)<< "Rts2DevClientTelescopeExec::syncTarget resync offsets: ra " << fixedOffset.ra << " dec " << fixedOffset.dec << sendLog;
			#endif
				queCommand (new Rts2CommandChange (this, fixedOffset.ra, fixedOffset.dec), bopTel);
				fixedOffset.ra = 0;
				fixedOffset.dec = 0;
				break;
			}
			queCommand (new Rts2CommandResyncMove (getMaster (), this, coord.ra, coord.dec), bopTel);
			break;
		case OBS_DONT_MOVE:
			break;
	}
	return ret;
}

void Rts2DevClientTelescopeExec::checkInterChange ()
{
	int waitNum = 0;
	getMaster ()->postEvent (new Rts2Event (EVENT_QUERY_WAIT, (void *) &waitNum));
	if (waitNum == 0)
		getMaster ()->postEvent (new Rts2Event (EVENT_ENTER_WAIT));
}

void Rts2DevClientTelescopeExec::moveEnd ()
{
	if (moveWasCorrecting)
	{
		if (currentTarget)
			currentTarget->moveEnded ();
		getMaster ()->postEvent (new Rts2Event (EVENT_CORRECTING_OK));
	}
	else
	{
		if (currentTarget)
			currentTarget->moveEnded ();
		getMaster ()->postEvent (new Rts2Event (EVENT_MOVE_OK));
	}
	DevClientTelescopeImage::moveEnd ();
}

void Rts2DevClientTelescopeExec::moveFailed (int status)
{
	if (status == DEVDEM_E_IGNORE)
	{
		moveEnd ();
		return;
	}
	if (currentTarget && currentTarget->moveWasStarted ())
		currentTarget->moveFailed ();
	DevClientTelescopeImage::moveFailed (status);
	getMaster ()->postEvent (new Rts2Event (EVENT_MOVE_FAILED, (void *) &status));
}
