/* 
 * Abstract rotator class.
 * Copyright (C) 2011 Petr Kubanek <petr@kubanek.net>
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

#include "device.h" 

namespace rts2rotad
{

/**
 * Abstract class for rotators.
 *
 * PA in this class stands for Parallactic Angle.
 *
 * @author Petr Kubanek <petr@kubanek.net>
 */
class Rotator:public rts2core::Device
{
	public:
		/**
		 * Rotator constructor.
		 *
		 * @param ownTimer   if true, rotator own timer to keep trackign will be disabled. Usefully for inteligent devices,
		 *                   which has own tracking, or for multidev where timer is in base device.
		 * @param leftRotator rotator is on left side of Nasmyth focus
		 */
		Rotator (int argc, char **argv, const char *defname = "R0", bool ownTimer = false, bool _leftRotator = false);

		virtual int commandAuthorized (rts2core::Connection * conn);

		/**
		 * Update tracking frequency. Should be run after new tracking vector
		 * is send to the rotator motion controller.
		 */
		void updateTrackingFrequency ();

		virtual void postEvent (rts2core::Event * event);

		void checkRotators ();

		void unsetPaTracking () { paTracking->setValueBool (false); }

	protected:
		virtual int idle ();

		virtual int setValue (rts2core::Value *old_value, rts2core::Value *new_value);

		/**
		 * Called to set rotator target and start rotating.
		 */
		virtual void setTarget (double tv) = 0;

		void setCurrentPosition (double cp);

		/**
		 * Calculate current PA as linear estimate from the past value and speed.
		 */
		double getPA (double t);

		/**
		 * Returns >0 if rotator is rotating image.
		 */
		virtual long isRotating () = 0;

		/**
		 * Returns >0 if rotator is parking.
		 */
		virtual long isParking();

		/**
		 * Called at the end of rotation.
		 */
		virtual void endRotation () { infoAll (); }

		double getCurrentPosition () { return currentPosition->getValueDouble (); }
		double getTargetPosition () { return targetPosition->getValueDouble (); }
		double getDifference () { return toGo->getValueDouble (); }

		double getTargetMin () { return tarMin->getValueDouble (); }
		double getTargetMax () { return tarMax->getValueDouble (); }

		double getZenithAngleMin() { return zenMin->getValueDouble(); }
		double getZenithAngleMax() { return zenMax->getValueDouble(); }

		double getZeroOffset () { return zeroOffs->getValueDouble (); }
		double getOffset () { return offset->getValueDouble (); }

		/**
		 * Set parallactic angle offset.
		 */
		void setPAOffset (double paOff);

		/**
		 * Returns zenith angle from target angle.
		 */
		double getZenithAngleFromTarget(double angle);

	private:
		rts2core::ValueDouble *zeroOffs;
		rts2core::ValueDouble *offset;
		rts2core::ValueDouble *parkingPosition;

		rts2core::ValueDouble *currentPosition;
		rts2core::ValueDouble *targetPosition;
		rts2core::ValueDouble *tarMin;
		rts2core::ValueDouble *tarMax;
		rts2core::ValueDouble *paOffset;

		rts2core::ValueBool *paTracking;
		rts2core::ValueBool *negatePA;
		rts2core::ValueDoubleStat *trackingFrequency;
		rts2core::ValueInteger *trackingFSize;
		rts2core::ValueFloat *trackingWarning;
		double lastTrackingRun;

		bool hasTimer;

		rts2core::ValueTime *parallacticAngleRef;
		rts2core::ValueDouble *parallacticAngle;
		rts2core::ValueDouble *parallacticAngleRate;
		rts2core::ValueDouble *toGo;

		rts2core::ValueAltAz *telAltAz;
		rts2core::ValueDouble *zenAngle;
		rts2core::ValueDouble *zenMin;
		rts2core::ValueDouble *zenMax;

		void updateToGo ();

		// autorotating in plus direction..
		bool autoPlus;
		bool autoOldTrack;

		// rotator on left Nasmyth axe
		bool leftRotator;
};

}
