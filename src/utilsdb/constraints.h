/* 
 * Constraints.
 * Copyright (C) 2003-2007 Petr Kubanek <petr@kubanek.net>
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

#ifndef __RTS2_CONSTRAINTS__
#define __RTS2_CONSTRAINTS__

#include "target.h"

#include <libxml/parser.h>
#include <libxml/tree.h>

#include <xmlerror.h>

#define CONSTRAINT_TIME         "time"
#define CONSTRAINT_AIRMASS      "airmass"
#define CONSTRAINT_HA           "HA"
#define CONSTRAINT_LDISTANCE    "lunarDistance"
#define CONSTRAINT_LALTITUDE    "lunarAltitude"
#define CONSTRAINT_LPHASE       "lunarPhase"
#define CONSTRAINT_SDISTANCE    "solarDistance"
#define CONSTRAINT_SALTITUDE    "sunAltitude"

namespace rts2db
{

class Target;

/**
 * Simple interval for constraints. Has lower and upper bounds.
 *
 * @author Petr Kubanek <petr@kubanek.net>
 */ 
class ConstraintDoubleInterval
{
	public:
		ConstraintDoubleInterval (double _lower, double _upper) { lower = _lower; upper = _upper; }
		bool satisfy (double val);
		friend std::ostream & operator << (std::ostream & os, ConstraintDoubleInterval &cons)
		{
			if (!isnan (cons.lower))
				os << cons.lower << " < ";
			if (!isnan (cons.upper))
			  	os << " < " << cons.upper;
			return os;
		}

		/**
		 * Print interval.
		 */
		void print (std::ostream &os);
	private:
		double lower;
		double upper;
};

/**
 * Abstract class for constraints.
 *
 * @author Petr Kubanek <petr@kubanek.net>
 */
class Constraint
{
	public:
		Constraint () {};

		/**
		 * Copy constraint intervals.
		 */
		Constraint (Constraint &cons);

		virtual void load (xmlNodePtr cons);
		virtual bool satisfy (Target *tar, double JD) = 0;

		/**
		 * Add interval from string. String containts colon (:) separating various intervals.
		 *
		 * @param interval   colon separated interval boundaries
                 */
		virtual void parseInterval (const char *interval);

		void print (std::ostream &os);

		virtual const char* getName () = 0;
	protected:
		void clearIntervals () { intervals.clear (); }
		void add (const ConstraintDoubleInterval &inte) { intervals.push_back (inte); }
		void addInterval (double lower, double upper) { intervals.push_back (ConstraintDoubleInterval (lower, upper)); }
		virtual bool isBetween (double JD);

	private:
		std::list <ConstraintDoubleInterval> intervals;
};

/**
 * Class for time intervals.
 *
 * @author Petr Kubanek <petr@kubanek.net>
 */
class ConstraintTime:public Constraint
{
	public:
		virtual void load (xmlNodePtr cons);
		virtual bool satisfy (Target *tar, double JD);

		virtual const char* getName () { return CONSTRAINT_TIME; }
};

class ConstraintAirmass:public Constraint
{
	public:
		virtual bool satisfy (Target *tar, double JD);

		virtual const char* getName () { return CONSTRAINT_AIRMASS; }
};

class ConstraintHA:public Constraint
{
	public:
		virtual bool satisfy (Target *tar, double JD);

		virtual const char* getName () { return CONSTRAINT_HA; }
};

class ConstraintLunarDistance:public Constraint
{
	public:
		virtual bool satisfy (Target *tar, double JD);

		virtual const char* getName () { return CONSTRAINT_LDISTANCE; }
};

class ConstraintLunarAltitude:public Constraint
{
	public:
		virtual bool satisfy (Target *tar, double JD);

		virtual const char* getName () { return CONSTRAINT_LALTITUDE; }
};

class ConstraintLunarPhase:public Constraint
{
	public:
		virtual bool satisfy (Target *tar, double JD);

		virtual const char* getName () { return CONSTRAINT_LPHASE; }
};

class ConstraintSolarDistance:public Constraint
{
	public:
		virtual bool satisfy (Target *tar, double JD);

		virtual const char* getName () { return CONSTRAINT_SDISTANCE; }
};

class ConstraintSunAltitude:public Constraint
{
	public:
		virtual bool satisfy (Target *tar, double JD);

		virtual const char* getName () { return CONSTRAINT_SALTITUDE; }
};

class Constraints:public std::map <std::string, ConstraintPtr >
{
	public:
		Constraints () {}

		/**
		 * Copy constructor. Creates new constraint members, so if they are changed in copy, they do not affect master.
		 */
		Constraints (Constraints &cs);

		~Constraints ();
		/**
		 * Check if constrains are satisfied.
		 *
		 * @param tar   target for which constraints will be checked
		 * @param JD    Julian date of constraints check
		 */
		bool satisfy (Target *tar, double JD);

		/**
		 * Return number of violated constainst.
		 *
		 * @param tar       target for which violated constrains will be calculated
		 * @param JD        Julian date of constraints check
		 * @param violated  list of the violated constraints
		 */
		size_t getViolated (Target *tar, double JD, std::list <ConstraintPtr> &violated);

		/**
		 * Return number of satisfied constainst.
		 *
		 * @param tar       target for which violated constrains will be calculated
		 * @param JD        Julian date of constraints check
		 * @param satisfied list of the satisifed constraints
		 */
		size_t getSatisfied (Target *tar, double JD, std::list <ConstraintPtr> &satisfied);

		/**
		 * Load constraints from XML constraint node. Please see constraints.xsd for details.
		 *
		 * @throw XmlError
		 */
		void load (xmlNodePtr _node);

		/**
		 * Load constraints from file.
		 *
		 * @param filename   name of file holding constraints in XML
		 */
		void load (const char *filename);

		/**
		 * Parse constraint intervals.
		 *
		 * @param name        constraint name
		 * @param interval    constraint interval
		 */
		void parseInterval (const char *name, const char *interval);

		/**
		 * Print constraints.
		 */
		void print (std::ostream &os);

	private:
		Constraint *createConstraint (const char *name);
};

/**
 * Master constraint singleton.
 *
 * @author Petr Kubanek <petr@kubanek.net>
 */
class MasterConstraints
{
  	public:
		static Constraints & getConstraint ();
};

}

#endif /* ! __RTS2_CONSTRAINTS__ */
