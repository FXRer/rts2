/*!
 * Standart image header. Used for transforming image header data between
 * camera deamon and camera client.
 *
 * $Id$
 *
 * It's quite necessary to have such a head, since condition on camera could
 * change unpredicitably after readout command was issuedddand client could
 * therefore receiver misleading information.
 *
 * That header is in no way mean as universal header for astronomical images.
 * It's only internal used among componnents of the rts software. It should
 * NOT be stored in permanent form, since it could change with versions of the
 * software.
 *
 * @author petr
 */

#ifndef __RTS_IMGHDR__
#define __RTS_IMGHDR__

#include <time.h>

#define SHUTTER_OPEN  0x01
#define SHUTTER_CLOSED  0x02
#define SHUTTER_SYNCHRO 0x03

#define FILTER_SIZE       10

#define MAX_AXES  5				 //! Maximum number of axes we should considered.

// various datatypes
#define RTS2_DATA_BYTE    8
#define RTS2_DATA_SHORT   16
#define RTS2_DATA_LONG    32
#define RTS2_DATA_LONGLONG  64
#define RTS2_DATA_FLOAT   -32
#define RTS2_DATA_DOUBLE  -64

// unsigned data types
#define RTS2_DATA_SBYTE   10
#define RTS2_DATA_USHORT  20
#define RTS2_DATA_ULONG   40

struct imghdr
{
	int data_type;
	int naxes;					 //! Number of axes.
	long sizes[MAX_AXES];		 //! Sizes in given axes.
	int binnings[MAX_AXES];		 //! Binning in each axe - eg. 2 -> 1 image pixel on given axis is equal 2 ccd pixels.
	int filter;					 //! Camera filter
	int shutter;
	int x, y;					 //! image beginning (detector coordinates)
	double subexp;				 //! image subexposure
	int nacc;					 //! number of accumulations used to take image
};
#endif							 // __RTS_IMGHDR__
