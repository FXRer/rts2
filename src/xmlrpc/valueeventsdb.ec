/* 
 * Value changes triggering infrastructure. 
 * Copyright (C) 2009 Petr Kubanek <petr@kubanek.net>
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

#include "xmlrpcd.h"

#include "../utilsdb/sqlerror.h"

EXEC SQL include sqlca;

using namespace rts2xmlrpc;

int ValueChangeRecord::getRecvalId (const char *suffix)
{
	EXEC SQL BEGIN DECLARE SECTION;
	int db_recval_id;
	VARCHAR db_device_name[25];
	VARCHAR db_value_name[26];
	EXEC SQL END DECLARE SECTION;

	std::map <const char *, int>::iterator iter = dbValueIds.find (suffix);

	if (iter != dbValueIds.end ())
		return iter->second;

	db_device_name.len = deviceName.length ();
	if (db_device_name.len > 25)
		db_device_name.len = 25;
	strncpy (db_device_name.arr, deviceName.c_str (), db_device_name.len);

	db_value_name.len = valueName.length ();
	if (db_value_name.len > 25)
		db_value_name.len = 25;
	strncpy (db_value_name.arr, valueName.c_str (), db_value_name.len);
	db_value_name.arr[db_value_name.len] = '\0';
	if (suffix != NULL)
	{
		strncat (db_value_name.arr, suffix, 25 - db_value_name.len);
		db_value_name.len += strlen (suffix);
		if (db_value_name.len > 25)
			db_value_name.len = 25;
	}
	EXEC SQL SELECT recval_id INTO :db_recval_id
		FROM recvals WHERE device_name = :db_device_name AND value_name = :db_value_name;
	if (sqlca.sqlcode)
	{
		if (sqlca.sqlcode == ECPG_NOT_FOUND)
		{
			// insert new record
			EXEC SQL SELECT nextval ('recval_ids') INTO :db_recval_id;
			EXEC SQL INSERT INTO recvals VALUES (:db_recval_id, :db_device_name, :db_value_name, 1);
			if (sqlca.sqlcode)
				throw rts2db::SqlError ();
		}
		else
		{
			throw rts2db::SqlError ();
		}
	}

	dbValueIds[suffix] = db_recval_id;

	return db_recval_id;
}

void ValueChangeRecord::recordValueDouble (int recval_id, double val, double validTime)
{
	EXEC SQL BEGIN DECLARE SECTION;
	int db_recval_id = recval_id;
	double  db_value = val;
	double db_rectime = validTime;
	EXEC SQL END DECLARE SECTION;

	EXEC SQL INSERT INTO records_double
	VALUES
	(
		:db_recval_id,
		to_timestamp (:db_rectime),
		:db_value
	);

	if (sqlca.sqlcode)
		throw rts2db::SqlError ();
}

void ValueChangeRecord::recordValueBoolean (int recval_id, bool val, double validTime)
{
	EXEC SQL BEGIN DECLARE SECTION;
	int db_recval_id = recval_id;
	double db_value = val;
	double db_rectime = validTime;
	EXEC SQL END DECLARE SECTION;

	EXEC SQL INSERT INTO records_boolean
	VALUES
	(
		:db_recval_id,
		to_timestamp (:db_rectime),
		:db_value
	);

	if (sqlca.sqlcode)
		throw rts2db::SqlError ();
}

void ValueChangeRecord::run (XmlRpcd *_master, Rts2Value *val, double validTime)
{

	std::ostringstream _os;

	switch (val->getValueType ())
	{
		case RTS2_VALUE_DOUBLE:
		case RTS2_VALUE_FLOAT:
			recordValueDouble (getRecvalId (), val->getValueDouble (), validTime);
			break;
		case RTS2_VALUE_RADEC:
			recordValueDouble (getRecvalId ("RA"), ((Rts2ValueRaDec *) val)->getRa (), validTime);
			recordValueDouble (getRecvalId ("DEC"), ((Rts2ValueRaDec *) val)->getDec (), validTime);
			break;
		case RTS2_VALUE_ALTAZ:
			recordValueDouble (getRecvalId ("ALT"), ((Rts2ValueAltAz *) val)->getAlt (), validTime);
			recordValueDouble (getRecvalId ("AZ"), ((Rts2ValueAltAz *) val)->getAz (), validTime);
			break;
		case RTS2_VALUE_BOOL:
			recordValueBoolean (getRecvalId (), ((Rts2ValueBool *) val)->getValueBool (), validTime);
			break;
		default:
			_os << "Cannot record value " << valueName;
			throw rts2core::Error (_os.str ());
	}
	EXEC SQL COMMIT;
	if (sqlca.sqlcode)
		throw rts2db::SqlError ();
}
