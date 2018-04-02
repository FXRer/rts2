/* 
 * Sitech rotator.
 * Copyright (C) 2016 Petr Kubanek <petr@kubanek.net>
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

#ifndef __RTS2_SITECH_ROTATOR_H__
#define __RTS2_SITECH_ROTATOR_H__

#include "rotad.h"
#include "connection/sitech.h"
#include "sitech-multi.h"

namespace rts2rotad
{

class SitechMulti;

class SitechRotator: public Rotator
{
	public:
		SitechRotator (const char axis, const char *name, rts2core::ConnSitech *conn, SitechMulti *multiBase, const char *defaults, bool _leftRotator);

		void calculateTarget (double pa_current, double pa_future, double t_diff, uint32_t tarpos, uint32_t tarspeed);

		void getConfiguration ();

		void getPIDs ();

		void processAxisStatus (rts2core::SitechAxisStatus *der_status);

		bool updated;

		// derotator statistics
		rts2core::ValueLong *r_pos;
		rts2core::ValueLong *t_pos;

		rts2core::ValueDouble *acceleration;
		rts2core::ValueDouble *max_velocity;
		rts2core::ValueDouble *current;
		rts2core::ValueInteger *pwm;
		rts2core::ValueInteger *temp;
		rts2core::ValueInteger *pid_out;

		rts2core::ValueBool *autoMode;

		rts2core::ValueLong *mclock;
		rts2core::ValueInteger *temperature;

		rts2core::IntegerArray *PIDs;

		rts2core::ValueInteger *countUp;
		rts2core::ValueDouble *PIDSampleRate;

		rts2core::ValueString *errors;
		rts2core::ValueInteger *errors_val;
		rts2core::ValueInteger *auto_reset;
		rts2core::ValueDoubleStat *pos_error;

		rts2core::ValueInteger *supply;

		rts2core::ValueLong *ticks;
		rts2core::ValueFloat *speed;

	protected:
		virtual int info ();

		virtual int commandAuthorized (rts2core::Connection *conn);

		virtual int setValue (rts2core::Value *oldValue, rts2core::Value *newValue);

		virtual void setTarget (double tv);
		virtual long isRotating ();

	private:
		rts2core::ConnSitech *sitech;
		SitechMulti *base;
		char axis;

		int last_errors;

		void goAuto ();
};

}

#endif //! __RTS2_SITECH_ROTATOR_H__
