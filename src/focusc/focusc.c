/*!
 * @file Plan test client
 * $Id$
 *
 * Client to test camera deamon.
 *
 * @author petr
 */

#define _GNU_SOURCE

#include <errno.h>
#include <libnova/libnova.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <pthread.h>
#include <mcheck.h>
#include <getopt.h>

#include "../utils/devcli.h"
#include "../utils/devconn.h"
#include "../utils/mkpath.h"
#include "../utils/mv.h"
#include "../writers/fits.h"
#include "status.h"

#define EXPOSURE_TIME	5.0

float exposure_time = EXPOSURE_TIME;

int increase_exposure = 0;

int date_exposure = 0;

int sec_mod = 0;

struct tm exp_start;

struct device *camera;

char *dark_name = NULL;

pid_t parent_pid;

#define fits_call(call) if (call) fits_report_error(stderr, status);

/*!
 * Handle camera data connection.
 *
 * @params sock 	socket fd.
 *
 * @return	0 on success, -1 and set errno on error.
 */
int
data_handler (int sock, size_t size, struct image_info *image)
{
  struct fits_receiver_data receiver;
  struct tm gmt;
  char data[DATA_BLOCK_SIZE];
  int ret = 0;
  ssize_t s;
  char filen[250];
  char *filename;

  gmtime_r (&image->exposure_tv.tv_sec, &gmt);

  if (date_exposure)
    asprintf (&filename, "%04i%02i%02i%02i%02i%02i-%03i.fits",
	      exp_start.tm_year + 1900, exp_start.tm_mon + 1,
	      exp_start.tm_mday, exp_start.tm_hour, exp_start.tm_min,
	      exp_start.tm_sec, (int) (image->exposure_tv.tv_usec / 1000));
  else if (increase_exposure)
    asprintf (&filename, "tmp%i_%04i.fits", parent_pid, increase_exposure++);
  else
    asprintf (&filename, "tmp%i.fits", parent_pid);
  strcpy (filen, filename);

  printf ("filename: %s\n", filename);

  if (fits_create (&receiver, filename) || fits_init (&receiver, size))
    {
      perror ("camc data_handler fits_init");
      ret = -1;
      goto free_filename;
    }

/*  if (image->target_type == TARGET_DARK)
    {
      if (dark_name)
	free (dark_name);
      dark_name = (char *) malloc (strlen (filename) + 1);
      strcpy (dark_name, filename);
    } */
  receiver.info = image;
  printf ("reading data socket: %i size: %i\n", sock, size);

  while ((s = devcli_read_data (sock, data, DATA_BLOCK_SIZE)) > 0)
    {
      if ((ret = fits_handler (data, s, &receiver)) < 0)
	goto free_filename;
      if (ret == 1)
	break;
    }

  if (s < 0)
    {
      perror ("camc data_handler");
      ret = -1;
      goto free_filename;
    }

  printf ("reading finished\n");

  if (fits_write_image_info (&receiver, image, dark_name)
      || fits_close (&receiver))
    {
      perror ("camc data_handler fits_write");
      ret = -1;
      goto free_filename;
    }

  free (filename);

  return 0;
free_filename:
  free (filename);
  return ret;
#undef receiver
}

int
readout ()
{
  struct image_info *info;
  struct timezone tz;

  info = (struct image_info *) malloc (sizeof (struct image_info));
  gettimeofday (&info->exposure_tv, &tz);
  info->exposure_length = exposure_time;
  info->target_id = -1;
  info->observation_id = -1;
  info->target_type = 1;
  info->camera_name = camera->name;
  if (devcli_command (camera, NULL, "base_info")
      || devcli_command (camera, NULL, "info")
      || !memcpy (&info->camera, &camera->info, sizeof (struct camera_info))
      || !memset (&info->telescope, 0, sizeof (struct telescope_info))
      || devcli_image_info (camera, info)
      || devcli_wait_for_status (camera, "img_chip", CAM_MASK_EXPOSE,
				 CAM_NOEXPOSURE, 0))
    {
      free (info);
      return -1;
    }

  if (devcli_command (camera, NULL, "readout 0"))
    {
      free (info);
      return -1;
    }
  free (info);
  return 0;
}

int
main (int argc, char **argv)
{
  uint16_t port = SERVERD_PORT;

  char *server;
  time_t start_time;

  char *camera_name;

  int c = 0;

#ifdef DEBUG
  mtrace ();
#endif
  /* get attrs */

  parent_pid = getpid ();

  camera_name = "C0";

  while (1)
    {
      static struct option long_option[] = {
	{"device", 1, 0, 'd'},
	{"exposure", 1, 0, 'e'},
	{"inc", 0, 0, 'i'},
	{"date", 0, 0, 'b'},
	{"secmod", 1, 0, 's'},
	{"port", 1, 0, 'p'},
	{"help", 0, 0, 'h'},
	{0, 0, 0, 0}
      };
      c = getopt_long (argc, argv, "bd:e:is:p:h", long_option, NULL);

      if (c == -1)
	break;

      switch (c)
	{
	case 'd':
	  camera_name = optarg;
	  break;
	case 'e':
	  exposure_time = atof (optarg);
	  break;
	case 'i':
	  increase_exposure = 1;
	  break;
	case 'b':
	  date_exposure = 1;
	  break;
	case 'p':
	  port = atoi (optarg);
	  break;
	case 's':
	  sec_mod = atoi (optarg);
	  break;
	case 'h':
	  printf ("Options:\n\tport|p <port_num>\t\tport of the server\n"
		  "\tdate|b\t\tfilenames are UT date\n"
		  "\texposure|e <exposure in sec>\t\texposure time in seconds\n"
		  "\tdevice|d <device_name>\t\tdevice for which to take exposures\n"
		  "\tinc|i\t\tincrease exposure count after every row\n"
		  "\tsecmod|s\t\texposure every UT second\n");
	  exit (EXIT_SUCCESS);
	case '?':
	  break;
	default:
	  printf ("?? getopt returned unknow character %o ??\n", c);
	}
    }
  if (optind != argc - 1)
    {
      printf ("You must pass server address\n");
      exit (EXIT_FAILURE);
    }

  server = argv[optind++];

  printf ("connecting to %s:%i\n", server, port);

  /* connect to the server */
  if (devcli_server_login (server, port, "petr", "petr") < 0)
    {
      perror ("devcli_server_login");
      exit (EXIT_FAILURE);
    }

  camera = devcli_find (camera_name);
  camera->data_handler = data_handler;

  if (!camera)
    {
      printf ("**** Cannot find camera\n");
      exit (EXIT_FAILURE);
    }

#define CAMD_WRITE_READ(command) if (devcli_command (camera, NULL, command) < 0) \
  				{ \
      		                  perror ("devcli_write_read_camd"); \
				  return EXIT_FAILURE; \
				}

  CAMD_WRITE_READ ("ready");
  CAMD_WRITE_READ ("base_info");
  CAMD_WRITE_READ ("info");
  CAMD_WRITE_READ ("chipinfo 0");

  umask (0x002);

  devcli_server_command (NULL, "priority 137");

  printf ("waiting for priority\n");

  devcli_wait_for_status (camera, "priority", DEVICE_MASK_PRIORITY,
			  DEVICE_PRIORITY, 0);

  printf ("waiting end\n");

  time (&start_time);
  start_time += 20;

  while (1)
    {
      time_t t = time (NULL);
      int sleep_sec;
      int day_sec;

      devcli_wait_for_status (camera, "priority", DEVICE_MASK_PRIORITY,
			      DEVICE_PRIORITY, 0);

      printf ("OK\n");


      time (&t);
      printf ("exposure countdown %s..\n", ctime (&t));
      t += exposure_time;
      printf ("readout at: %s\n", ctime (&t));
      if (devcli_wait_for_status (camera, "img_chip", CAM_MASK_READING,
				  CAM_NOTREADING, 0))
	{
	  perror ("waiting for readout end:");
	}
      time (&t);
      gmtime_r (&t, &exp_start);

      if (sec_mod > 0)
	{
	  day_sec =
	    exp_start.tm_hour * 3600 + exp_start.tm_min * 60 +
	    exp_start.tm_sec;
	  sleep_sec = sec_mod - day_sec % sec_mod;
	  printf ("sleeping for %i sec..\n", sleep_sec);
	  sleep (sleep_sec);
	  printf ("sleeping ends..\n");
	}

      if (devcli_command (camera, NULL, "expose 0 %i %f", 1, exposure_time))
	{
	  perror ("expose:");
	}
      time (&t);

      gmtime_r (&t, &exp_start);

      readout ();
    }

  devcli_server_disconnect ();

  return 0;
}
