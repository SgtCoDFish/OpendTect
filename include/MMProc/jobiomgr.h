#ifndef jobiomgr_h
#define jobiomgr_h

/*+
________________________________________________________________________

 (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 Author:	A.H.Lammertink
 Date:		Oct 2004
________________________________________________________________________

-*/

#include "mmprocmod.h"

#include "bufstring.h"
#include "callback.h"
#include "oscommand.h"

class CommandString;
class FilePath;
class HostData;
class JobInfo;
class JobIOHandler;
template <class T> class ObjQueue;

/*!
\brief Encapsulates status message from a running client.

 * Running clients report back to the master on a regular basis.
 * Whenever a client contacts the master, whatever it has
 * to say is put into a StatusInfo structure.
 * These are put in a mutexed queue in order to keep a strict separation
 * between the communication thread and the GUI/manager thread.
 *
*/

mExpClass(MMProc) StatusInfo
{
public:
			StatusInfo( char tg, int desc, int stat, int pid,
				    const char* mg, const char* hostname,
				    int time )
			    : tag(tg), descnr(desc), status(stat), msg(mg)
			    , hostnm(hostname), timestamp(time), procid(pid) {}

    char		tag;
    int			descnr;
    int			status;
    int			timestamp;
    int			procid;
    BufferString	hostnm;
    BufferString	msg;
};


/*!
\brief Handles starting & stopping of jobs on client machines. Sets up a
separate thread to maintain contact with client.
*/

mExpClass(MMProc) JobIOMgr : public CallBacker
{
public:
    enum		Mode { Work, Pause, Stop };

			JobIOMgr(int firstport=19345,float priority=-1.f);
    virtual		~JobIOMgr();

    const char*		peekMsg()  { if ( msg_.size() ) return msg_; return 0; }
    void		fetchMsg( BufferString& bs )	{ bs = msg_; msg_ = "";}

    bool		startProg(const char*,IOPar&,const FilePath&,
				  const JobInfo&,const char*);

    void		setPriority( float p );
    void		reqModeForJob(const JobInfo&,Mode);
    void		removeJob(const char*,int);
    bool		isReady() const;

    ObjQueue<StatusInfo>& statusQueue();

    static bool		mkIOParFile(const FilePath& basefnm,
				    const HostData&,const IOPar&,
				    FilePath&,BufferString& msg);
protected:

    JobIOHandler&	iohdlr_;
    BufferString	msg_;
    OS::CommandExecPars execpars_;

    void		mkCommand(OS::MachineCommand&,const HostData&,
				  const char* progname,const FilePath& basefp,
				  const FilePath& iopfp,const JobInfo&,
				  const char* rshcomm);

};


mGlobal(MMProc) const OD::String& getTempBaseNm();
mGlobal(MMProc) int mkTmpFileNr();


mClass(MMProc) CommandString
{
public:
			CommandString(const HostData& targetmachine,
				      const char* init=0);

    CommandString&	operator=(const char*);

    void		addFlag(const char* flag,const char* value);
    void		addFlag(const char* flag,int value);
    void		addFilePath(const FilePath&);

    const OD::String&	string()		{ return cmd_; }

private:

    void		add(const char*);

    BufferString	cmd_;
    const HostData&	hstdata_;

};

#endif
