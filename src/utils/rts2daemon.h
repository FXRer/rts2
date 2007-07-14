#ifndef __RTS2_DAEMON__
#define __RTS2_DAEMON__

#include "rts2block.h"
#include "rts2logstream.h"
#include "rts2value.h"
#include "rts2valuelist.h"
#include "rts2valuestat.h"
#include "rts2valueminmax.h"

#include <vector>

/**
 * Abstract class for centrald and all devices.
 *
 * This class contains functions which are common to components with one listening socket.
 */
class Rts2Daemon:public Rts2Block
{
private:
  // 0 - don't daemonize, 1 - do daemonize, 2 - is already daemonized, 3 - daemonized & centrald is running, don't print to stdout
  enum
  { DONT_DAEMONIZE, DO_DAEMONIZE, IS_DAEMONIZED, CENTRALD_OK } daemonize;
  int listen_sock;
  void addConnectionSock (int in_sock);
  int lockf;

  // daemon state
  int state;

  Rts2CondValueVector values;
  // values which do not change, they are send only once at connection
  // initialization
  Rts2ValueVector constValues;

  // This vector holds list of values which are currenlty beeing changed
  // It is used to manage BOP mask
  Rts2ValueVector bopValues;

  Rts2ValueTime *info_time;

  time_t idleInfoInterval;
  time_t nextIdleInfo;

  /**
   * Adds value to list of values supported by daemon.
   *
   * \param value Rts2Value which will be added.
   */
  void addValue (Rts2Value * value, int queCondition = 0, bool save_value =
		 false);
  Rts2CondValue *getValue (const char *v_name);

  Rts2Value *duplicateValue (Rts2Value * old_value, bool withVal = false);

  /**
   * Perform value changes. Check if value can be changed before performing change.
   * 
   * \return 0 when value change can be performed, -2 on error, -1 when value change is qued.
   */
  int setValue (Rts2CondValue * old_value_cond, char op,
		Rts2Value * new_value);

  /**
   * Holds vector of values which are indendet to be saved. There are two methods, saveValues and loadValues.
   * saveValues is called from \see setValue(Rts2Value,char,Rts2ValueVector) before first value value is changed. Once values are saved,
   * values_were_saved turns to true and we don't call saveValues before loadValues is called.
   * loadValues reset values_were_saved flag, load all values from savedValues
   */
  Rts2ValueVector savedValues;

  void saveValue (Rts2CondValue * val);
  void deleteSaveValue (Rts2CondValue * val);

  /**
   * Send new value over the wire to all connections.
   */

  void sendValueAll (Rts2Value * value);
protected:
  int checkLockFile (const char *lock_fname);
  void setNotDeamonize ()
  {
    daemonize = DONT_DAEMONIZE;
  }
  int doDeamonize ();
  int lockFile ();
  virtual void addSelectSocks (fd_set * read_set);
  virtual void selectSuccess (fd_set * read_set);

  Rts2ValueQueVector queValues;

  void loadValues ();

  /**
   * Create new value, and return pointer to it.
   *
   * \param value          Rts2Value which will be created
   * \param in_val_name    value name
   * \param in_description value description
   * \param writeToFits    when true, value will be writen to FITS
   * \param displayType    value display type, one of the RTS2_DT_xxx constant
   */
  template < typename T > void createValue (T * &val, char *in_val_name,
					    std::string in_description,
					    bool writeToFits =
					    true, int32_t displayType =
					    0, int queCondition =
					    0, bool save_value = false)
  {
    val = new T (in_val_name, in_description, writeToFits, displayType);
    addValue (val, queCondition, save_value);
  }
  /**
   * Create new constant value, and return pointer to it.
   *
   * \param value          Rts2Value which will be created
   * \param in_val_name    value name
   * \param in_description value description
   * \param writeToFits    when true, value will be writen to FITS
   */
  template < typename T > void createConstValue (T * &val, char *in_val_name,
						 std::string in_description,
						 bool writeToFits =
						 true, int32_t displayType =
						 0)
  {
    val = new T (in_val_name, in_description, writeToFits, displayType);
    addConstValue (val);
  }
  void addConstValue (Rts2Value * value);
  void addConstValue (char *in_name, const char *in_desc, char *in_value);
  void addConstValue (char *in_name, const char *in_desc, double in_value);
  void addConstValue (char *in_name, const char *in_desc, int in_value);
  void addConstValue (char *in_name, char *in_value);
  void addConstValue (char *in_name, double in_value);
  void addConstValue (char *in_name, int in_value);

  /**
   * BOP management routines.
   * Those routines are used to manage BOP (Block OPeration) mask. It is used
   * to decide, which actions can be performed without disturbing observation.
   */
  void addBopValue (Rts2Value * in_value);
  void removeBopValue (Rts2Value * in_value);

  void checkBopStatus ();

  /** 
   * Set value. That one should be owerwrited in descendants.
   * 
   * \param  old_value	old value (pointer), can be directly
   * 			compared by pointer stored in object
   * \param  new_value	new value
   * \return 0 when we can set value, -2 on error.
   */
  virtual int setValue (Rts2Value * old_value, Rts2Value * new_value);

  /**
   * Really perform value change.
   */
  int doSetValue (Rts2CondValue * old_cond_value, char op,
		  Rts2Value * new_value);

  /**
   * Returns whenever value change with old_value needs to be qued or
   * not.
   * \param old_value Rts2CondValue object describing the old_value
   */
  virtual bool queValueChange (Rts2CondValue * old_value) = 0;

  virtual int processOption (int in_opt);
  virtual int init ();
  virtual int initValues ();
  virtual int idle ();

public:
  Rts2Daemon (int in_argc, char **in_argv);
  virtual ~ Rts2Daemon (void);
  virtual int run ();

  void initDaemon ();

  void setIdleInfoInterval (time_t interval)
  {
    idleInfoInterval = interval;
    setTimeoutMin ((long int) interval * USEC_SEC);
  }

  virtual void forkedInstance ();
  virtual void sendMessage (messageType_t in_messageType,
			    const char *in_messageString);
  virtual void centraldConnRunning ();
  virtual void centraldConnBroken ();

  virtual int baseInfo ();
  int baseInfo (Rts2Conn * conn);
  int sendBaseInfo (Rts2Conn * conn);

  virtual int info ();
  int info (Rts2Conn * conn);
  int infoAll ();
  int sendInfo (Rts2Conn * conn);
  int sendMetaInfo (Rts2Conn * conn);

  virtual int setValue (Rts2Conn * conn, bool overwriteSaved);

  // state management functions
protected:
  /**
   * Called to set new state value
   */
  void setState (int new_state, const char *description);
  virtual void stateChanged (int new_state, int old_state,
			     const char *description);
public:
  /**
   * Called when state is changed.
   */
  void maskState (int state_mask, int new_state, const char *description =
		  NULL);
  int getState ()
  {
    return state;
  };
};

#endif /* ! __RTS2_DAEMON__ */
