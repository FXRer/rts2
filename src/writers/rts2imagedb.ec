#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "rts2imagedb.h"

EXEC SQL include sqlca;

void
Rts2ImageDb::reportSqlError (char *msg)
{
  printf ("SQL error %i %s (in %s)\n", sqlca.sqlcode,
  sqlca.sqlerrm.sqlerrmc, msg);
}

int
Rts2ImageDb::updateObjectDB ()
{
  EXEC SQL BEGIN DECLARE SECTION;
  int d_img_id = imgId;
  int d_obs_id = obsId;
  char d_obs_subtype = 'S';
  int d_img_date = getExposureSec ();
  int d_img_usec = getExposureUsec ();
  double d_img_temperature;
  double d_img_exposure;
  double d_img_alt;
  double d_img_az;
  int d_epoch_id = epochId;
  int d_med_id = 0;
  int d_proccess_bitfield = processBitfiedl;
  VARCHAR d_mount_name[8];
  VARCHAR d_camera_name[8];
  VARCHAR d_img_filter[3];
  EXEC SQL END DECLARE SECTION;

  char filter[4] = "UNK";

  strncpy (d_mount_name.arr, getMountName (), 8);
  d_mount_name.len = strlen (getMountName ());

  strncpy (d_camera_name.arr, getCameraName (), 8);
  d_camera_name.len = strlen (getCameraName ());

  getValue ("FILTER", filter);

  d_img_filter.len = strlen (filter) > 3 ? 3 : strlen (filter);
  strncpy (d_img_filter.arr, filter, d_img_filter.len);

  getValue ("CCD_TEMP", d_img_temperature);
  getValue ("EXPOSURE", d_img_exposure);
  getValue ("ALT", d_img_alt);
  getValue ("AZ", d_img_az);

  EXEC SQL
  INSERT INTO
    images
  (
    img_id,
    obs_id,
    obs_subtype,
    mount_name,
    camera_name,
    img_temperature,
    img_exposure,
    img_filter,
    img_alt,
    img_az,
    img_date,
    img_usec,
    epoch_id,
    med_id,
    process_bitfield
  )
  VALUES
  (
    :d_img_id,
    :d_obs_id,
    :d_obs_subtype,
    :d_mount_name,
    :d_camera_name,
    :d_img_temperature,
    :d_img_exposure,
    :d_img_filter,
    :d_img_alt,
    :d_img_az,
    abstime (:d_img_date),
    :d_img_usec,
    :d_epoch_id,
    :d_med_id,
    :d_proccess_bitfield
  );
  // data found..do just update
  if (sqlca.sqlcode != 0)
  {
    printf ("Cannot insert new image, triing update (error: %i %s)\n",
    sqlca.sqlcode, sqlca.sqlerrm.sqlerrmc);
    EXEC SQL ROLLBACK;
    EXEC SQL
    UPDATE
      images
    SET
      img_date = abstime (:d_img_date),
      img_usec = :d_img_usec,
      epoch_id = :d_epoch_id,
      med_id   = :d_med_id,
      process_bitfield = :d_proccess_bitfield
    WHERE
        img_id = :d_img_id
      AND obs_id = :d_obs_id;
    // still error.. return -1
    if (sqlca.sqlcode != 0)
    {
      reportSqlError ("image OBJECT update");
      EXEC SQL ROLLBACK;
      return -1;
    }
  }
  EXEC SQL COMMIT;
  return updateAstrometry ();
}

int
Rts2ImageDb::updateDarkDB ()
{
  EXEC SQL BEGIN DECLARE SECTION;
  int d_img_id = imgId;
  int d_obs_id = obsId;
  int d_dark_date = getExposureSec ();
  int d_dark_usec = getExposureUsec ();
  double d_dark_exposure;
  double d_dark_temperature;
  float d_dark_mean = mean;
  int d_epoch_id = epochId;
  VARCHAR d_camera_name[8];
  EXEC SQL END DECLARE SECTION;

  getValue ("CCD_TEMP", d_dark_temperature);
  getValue ("EXPOSURE", d_dark_exposure);

  strncpy (d_camera_name.arr, cameraName, 8);
  d_camera_name.len = strlen (cameraName);

  EXEC SQL
  INSERT INTO
    darks
  (
    img_id,
    obs_id,
    dark_date,
    dark_usec,
    dark_exposure,
    dark_temperature,
    dark_mean,
    epoch_id,
    camera_name
  ) VALUES (
    :d_img_id,
    :d_obs_id,
    abstime (:d_dark_date),
    :d_dark_usec,
    :d_dark_exposure,
    :d_dark_temperature,
    :d_dark_mean,
    :d_epoch_id,
    :d_camera_name
  );
  if (sqlca.sqlcode)
  {
    reportSqlError ("image DARK update");
    EXEC SQL ROLLBACK;
    return -1;
  }
  EXEC SQL COMMIT;
  return 0;
}

int
Rts2ImageDb::updateFlatDB ()
{
  return -1;
}

int
Rts2ImageDb::updateDB ()
{
  switch (imageType)
  {
    case IMGTYPE_OBJECT:
      return updateObjectDB ();
    case IMGTYPE_DARK:
      return updateDarkDB ();
    case IMGTYPE_FLAT:
      return updateFlatDB ();
  }
  syslog (LOG_DEBUG, "unknow image type: %i", imageType);
  return -1;
}

int
Rts2ImageDb::updateAstrometry ()
{
  EXEC SQL BEGIN DECLARE SECTION;
  int d_obs_id = obsId;
  int d_img_id = imgId;
  VARCHAR s_astrometry[2000];
  EXEC SQL END DECLARE SECTION;

  long naxis[2];
  char *ctype[2];
  double crpix[2];
  double crval[2];
  double cdelt[2];
  double crota[2];
  double equinox;
  double epoch;
  int ret;

  ctype[0] = (char *) malloc (9);
  ctype[1] = (char *) malloc (9);

  ret = getValues ("NAXIS", naxis, 2);
  if (ret)
    return -1;
  ret = getValues ("CTYPE", (char **) &ctype, 2);
  if (ret)
    return -1;
  ret = getValues ("CRPIX", crpix, 2);
  if (ret)
    return -1;
  ret = getValues ("CRVAL", crval, 2);
  if (ret)
    return -1;
  ret = getValues ("CDELT", cdelt, 2);
  if (ret)
    return -1;
  ret = getValues ("CROTA", crota, 2);
  if (ret)
    return -1;
  ret = getValue ("EQUINOX", equinox);
  if (ret)
    return -1;
  ret = getValue ("EPOCH", epoch);
  if (ret)
    return -1;

  snprintf (s_astrometry.arr, 2000,
    "NAXIS1 %ld NAXIS2 %ld CTYPE1 %s CTYPE2 %s CRPIX1 %f CRPIX2 %f "
    "CRVAL1 %f CRVAL2 %f CDELT1 %f CDELT2 %f CROTA %f EQUINOX %f EPOCH %f",
    naxis[0], naxis[1], ctype[0], ctype[1], crpix[0], crpix[1],
    crval[0], crval[1], cdelt[0], cdelt[1], crota[0], equinox, epoch);
  s_astrometry.len = strlen (s_astrometry.arr);

  EXEC SQL UPDATE
    images
  SET
    astrometry = :s_astrometry,
    process_bitfield = process_bitfield | 1 | 2
  WHERE
      obs_id = :d_obs_id
    AND img_id = :d_img_id;
  EXEC SQL COMMIT;
  if (sqlca.sqlcode != 0)
  {
    reportSqlError ("astrometry update");
    return -1;
  }
  return 0;  
}

int
Rts2ImageDb::setDarkFromDb ()
{
  EXEC SQL BEGIN DECLARE SECTION;
  VARCHAR d_camera_name[8];
  int d_date = getExposureSec ();
  VARCHAR d_dark_name[250];
  double d_img_temperature;
  double d_img_exposure;
  EXEC SQL END DECLARE SECTION;

  if (!cameraName)
    return -1;

  if (!(getType () == IMGTYPE_OBJECT
  || getType () == IMGTYPE_FLAT))
    return 0;
    
  strncpy (d_camera_name.arr, cameraName, 8);
  d_camera_name.len = strlen (cameraName);

  getValue ("CCD_TEMP", d_img_temperature);
  getValue ("EXPOSURE", d_img_exposure);

  EXEC SQL DECLARE dark_cursor CURSOR FOR
    SELECT
      dark_name (obs_id, img_id, dark_date, dark_usec, epoch_id, camera_name)
    FROM
      darks
    WHERE
        camera_name = :d_camera_name
      AND dark_exposure = :d_img_exposure
      AND abs (dark_temperature - :d_img_temperature) < 2
      AND abs (EXTRACT (EPOCH FROM dark_date) - :d_date) < 43200
    ORDER BY
      dark_date DESC;
  EXEC SQL OPEN dark_cursor;
  EXEC SQL FETCH next FROM dark_cursor INTO
    :d_dark_name;
  if (sqlca.sqlcode)
  {
    syslog (LOG_DEBUG, "Rts2ImageDb::setDarkFromDb SQL error: %s (%i)",
      sqlca.sqlerrm.sqlerrmc, sqlca.sqlcode);
    EXEC SQL CLOSE dark_cursor;
    EXEC SQL ROLLBACK;
    return -1;
  }
  EXEC SQL CLOSE dark_cursor;
  setValue ("DARK", d_dark_name.arr, "dark image full path");
  processBitfiedl |= DARK_OK;

  return 0;
}

Rts2ImageDb::Rts2ImageDb (int in_epoch_id, int in_targetId, Rts2DevClientCamera * camera,
      	       int in_obsId, const struct timeval *exposureStart, int in_imgId) : 
  Rts2Image (in_epoch_id, in_targetId, camera, in_obsId, exposureStart, in_imgId)
{
  processBitfiedl = 0;
  getValue ("PROC", processBitfiedl);
}

Rts2ImageDb::Rts2ImageDb (const char *in_filename) : Rts2Image (in_filename)
{
  processBitfiedl = 0;
  getValue ("PROC", processBitfiedl);
}

Rts2ImageDb::~Rts2ImageDb ()
{
  updateDB ();
}

int
Rts2ImageDb::toArchive ()
{
  int ret; 
  ret = Rts2Image::toArchive ();
  if (ret)
    return ret;

  processBitfiedl |= ASTROMETRY_OK | ASTROMETRY_PROC;

  return 0;
}

int
Rts2ImageDb::toTrash ()
{
  int ret; 

  processBitfiedl &= (~ASTROMETRY_OK);
  processBitfiedl |= ASTROMETRY_PROC;

  ret = Rts2Image::toTrash ();
  if (ret)
    return ret;

  return 0;
}

// write changes of image to DB..

int
Rts2ImageDb::saveImage ()
{
  updateDB ();
  setValue ("PROC", processBitfiedl, "procesing status; info in DB");
  setDarkFromDb ();
  return Rts2Image::saveImage ();
}

int
Rts2ImageDb::deleteImage ()
{
  EXEC SQL BEGIN DECLARE SECTION;
  int d_img_id = imgId;
  int d_obs_id = obsId;
  EXEC SQL END DECLARE SECTION;

  if (getType () == IMGTYPE_OBJECT)
  {
    EXEC SQL
    DELETE FROM
      images
    WHERE
	img_id = :d_img_id
      AND obs_id = :d_obs_id;
  }
  else if (getType () == IMGTYPE_DARK)
  {
    EXEC SQL
    DELETE FROM
      darks
    WHERE
	img_id = :d_img_id
      AND obs_id = :d_obs_id;
  }
  if (sqlca.sqlcode)
  {
     reportSqlError ("Rts2ImageDb::deleteImage");
     EXEC SQL ROLLBACK;
  }
  else
  {
     EXEC SQL COMMIT;
  }
  return Rts2Image::deleteImage ();
}
