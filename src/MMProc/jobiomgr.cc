/*+
________________________________________________________________________

 (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 Author:        A.H. Lammertink
 Date:          Oct 2004
________________________________________________________________________

-*/

#include "jobiomgr.h"

#include "commandlineparser.h"
#include "debug.h"
#include "envvars.h"
#include "file.h"
#include "filepath.h"
#include "genc.h"
#include "hostdata.h"
#include "ioman.h"
#include "iopar.h"
#include "jobinfo.h"
#include "keystrs.h"
#include "oddirs.h"
#include "queue.h"
#include "separstr.h"
#include "string2.h"
#include "oscommand.h"
#include "od_ostream.h"
#include "survinfo.h"
#include "systeminfo.h"
#include "netserver.h"
#include "timefun.h"

#include "mmcommunicdefs.h"
#ifndef __win__
# include <unistd.h>
#endif


#define mDebugOn	(DBG::isOn(DBG_MM))

#define mErrRet(msg) \
{\
    msg_ = msg;\
    DBG::message(DBG_MM,msg);\
    return false;\
}


CommandString::CommandString( const HostData& hostdata, const char* init )
    : hstdata_(hostdata)
    , cmd_(init)
{}


CommandString& CommandString::operator=( const char* str )
{
    cmd_ = str;
    return *this;
}


void CommandString::add( const char* txt )
{ cmd_.addSpace().add( txt ); }

void CommandString::addFlag( const char* f, const char* v )
{
    if ( !v || !*v ) return;

    BufferString flag;
    CommandLineParser::createKey( f, flag );
    add( flag.str() );
    add( v );
}


void CommandString::addFlag( const char* f, int v )
{
    BufferString flag;
    CommandLineParser::createKey( f, flag );
    flag.addSpace().add( v );
    add( flag.str() );
}


void CommandString::addFilePath( const FilePath& fp )
{
    const FilePath::Style stl( hstdata_.pathStyle() );
    cmd_.add( stl == FilePath::Unix ? " '" : " \"" )
	.add( fp.fullPath(stl) )
	.add( stl == FilePath::Unix ? "'" : "\"" );
}



/*!\brief Connects job to host.
 *
 */
class JobHostRespInfo
{
public:
			JobHostRespInfo( const HostData& hd, int descnr,
					 char resp = mRSP_WORK )
			: hostdata_(hd)
			, descnr_(descnr)
			, response_( resp ) {}

    HostData		hostdata_;
    int			descnr_;
    char		response_;
};


/*!\brief Sets up a thread that waits for clients to connect.

  The task of the JobIOHandler is to maintain contact with clients.
  Most of the fuctionality could be implemented in the JobIOMgr,
  but we want to be absolutely sure the listening thread
  does not interfear with the normal exection thread's data.

  Therefore, all requests to and from the JobIOHandler are
  made mutex protected.

*/
class JobIOHandler : public CallBacker
{
public:
JobIOHandler( int firstport )
    : exitreq_(0)
    , firstport_(firstport)
    , usedport_(firstport)
    , ready_(false)
{
    server_.readyRead.notify( mCB(this,JobIOHandler,socketCB) );
    listen( firstport_ );
}

virtual	~JobIOHandler()
{
    server_.close();
    server_.readyRead.remove( mCB(this,JobIOHandler,socketCB) );
}

    bool		ready() const	{ return ready_ && port() > 0; }
    int			port() const	{ return usedport_; }

    void		listen(int firstport,int maxtries=3000 );
    void		reqModeForJob(const JobInfo&, JobIOMgr::Mode);
    void		addJobDesc(const HostData&,int descnr);
    void		removeJobDesc(const char* hostnm, int descnr);
    ObjQueue<StatusInfo>& statusQueue() { return statusqueue_; }

protected:

    JobHostRespInfo*		getJHRFor(int desc,const char* hostnm);
    bool			readTag(char& tag,SeparString& sepstr,
					const OD::String& data);
    void			socketCB(CallBacker*);
    char			getRespFor(int desc,const char* hostnm);

    bool*			exitreq_;
    Network::Server		server_;
    int				firstport_;
    int				usedport_;
    ObjQueue<StatusInfo>	statusqueue_;
    ObjectSet<JobHostRespInfo>	jobhostresps_;
    bool			ready_;
};


void JobIOHandler::listen( int firstport, int maxtries )
{
    int currentport = firstport;
    for ( int idx=0; idx<maxtries; idx++, currentport++ )
    {
	server_.listen( System::localAddress(), currentport );
	if ( server_.isListening() )
	{
	    usedport_ = currentport;
	    ready_ = true;
	    break;
	}
	else
	    server_.close();
    }
}


void JobIOHandler::addJobDesc( const HostData& hd, int descnr )
{
    jobhostresps_ += new JobHostRespInfo( hd, descnr );
}


void JobIOHandler::removeJobDesc( const char* hostnm, int descnr )
{
    JobHostRespInfo* jhri = getJHRFor( descnr, hostnm );
    if ( jhri )
    {
	jobhostresps_ -= jhri;
	delete jhri;
    }
}


JobHostRespInfo* JobIOHandler::getJHRFor( int descnr, const char* hostnm )
{
    JobHostRespInfo* jhri = 0;
    const int sz = jobhostresps_.size();
    for ( int idx=0; idx<sz; idx++ )
    {
	JobHostRespInfo* jhri_ = jobhostresps_[idx];
	if ( descnr < 0 || jhri_->descnr_==descnr )
	{
	    if ( !hostnm || !*hostnm )  { jhri = jhri_; break; }

	    BufferString shrthostnm = hostnm;
	    char* ptr = shrthostnm.find( '.' );
	    if ( ptr ) *ptr = '\0';
#ifndef __win__
	    if ( jhri_->hostdata_.isKnownAs(hostnm) ||
		 jhri_->hostdata_.isKnownAs(shrthostnm) )
#endif
		{ jhri = jhri_; break; }
	}
    }

    return jhri;
}


char JobIOHandler::getRespFor( int descnr, const char* hostnm )
{
    char resp = mRSP_STOP;
    JobHostRespInfo* jhri = getJHRFor( descnr, hostnm );
    if ( jhri ) resp = jhri->response_;
    return resp;
}


void JobIOHandler::reqModeForJob( const JobInfo& ji, JobIOMgr::Mode mode )
{
    char resp = mRSP_STOP;

    switch( mode )
    {
    case JobIOMgr::Work		: resp = mRSP_WORK; break;
    case JobIOMgr::Pause	: resp = mRSP_PAUSE; break;
    case JobIOMgr::Stop		: resp = mRSP_STOP; break;
    }

    const int descnr = ji.descnr_;
    BufferString hostnm;
    if ( ji.hostdata_ ) hostnm = ji.hostdata_->getHostName();

    JobHostRespInfo* jhri = getJHRFor( descnr, hostnm );
    if ( jhri ) jhri->response_ = resp;
}


void JobIOHandler::socketCB( CallBacker* cb )
{
    mCBCapsuleUnpack(int,socketid,cb);
    BufferString data;
    server_.read( socketid, data );
    char tag=mCTRL_STATUS;
    int jobid=-1;
    int status=mSTAT_UNDEF;
    BufferString hostnm;
    BufferString errmsg;
    int procid=-1;

    if ( !data.isEmpty() )
    {
	SeparString statstr;
	const bool ok = readTag( tag, statstr, data );
	if ( ok )
	{
	    jobid = statstr.getIValue( 0 );
	    status = statstr.getIValue( 1 );
	    hostnm = statstr[2];
	    procid = statstr.getIValue( 3 );
	    errmsg = statstr[4];

	    char response = getRespFor( jobid, hostnm );
	    if ( response != mRSP_STOP )
	    {
		statusqueue_.add( new StatusInfo( tag, jobid, status,
			procid, errmsg, hostnm, Time::getMilliSeconds()) );
	    }

	    // hardly ever needed and quite noisy.
	    static bool debug_resp = GetEnvVarYN("DTECT_MM_DEBUG_RESP");
	    if ( debug_resp && mDebugOn )
	    {
		BufferString msg( "JobIOMgr::JobIOMgr: Response to host " );
		msg.add( hostnm ).add( ", job: " ).add( jobid ).add( ": " );
		if ( response == mRSP_STOP )
		    msg += "STOP";
		else if ( response == mRSP_WORK )
		    msg += "WORK";
		else if ( response == mRSP_PAUSE )
		    msg += "PAUSE";
		else
		    msg += &response;
		DBG::message( msg );
	    }
	    char resp[2];
	    resp[0] = response;
	    resp[1] = '\0';
	    server_.write( socketid, resp );
	}
	else
	{
	    errmsg = server_.errorMsg();
	    ErrMsg( errmsg );
	}
    }
}


bool JobIOHandler::readTag( char& tag, SeparString& sepstr,
			    const OD::String& data )
{
    tag = data[0];
    sepstr.setSepChar( data[1] );
    sepstr = data.buf() + 2;
    return true;
}


// JobIOMgr
JobIOMgr::JobIOMgr( int firstport, float prioritylevel )
    : iohdlr_(*new JobIOHandler(firstport))
{
    for ( int count=0; count<10 && !iohdlr_.ready(); count++ )
	{ sleepSeconds( 0.1 ); }

    setPriority( prioritylevel );

    if ( mDebugOn )
    {
	BufferString msg( "JobIOMgr::JobIOMgr " );
	if( iohdlr_.ready() )
	{
	    msg += "ready and listening to port ";
	    msg += iohdlr_.port();
	}
	else
	    msg += "NOT ready (yet). Clients might not be able to connect.";

	DBG::message( msg );
    }
}


JobIOMgr::~JobIOMgr()
{ delete &iohdlr_; }


void JobIOMgr::setPriority( float p )
{ execpars_.prioritylevel_ = p; }

void JobIOMgr::reqModeForJob( const JobInfo& ji, Mode m )
{ iohdlr_.reqModeForJob(ji,m); }

void JobIOMgr::removeJob( const char* hostnm, int descnr )
{ iohdlr_.removeJobDesc(hostnm,descnr); }

ObjQueue<StatusInfo>& JobIOMgr::statusQueue()
{ return iohdlr_.statusQueue(); }

bool JobIOMgr::isReady() const
{ return iohdlr_.ready(); }


bool JobIOMgr::startProg( const char* progname,
	IOPar& iop, const FilePath& basefp, const JobInfo& ji,
	const char* rshcomm )
{
    DBG::message(DBG_MM,"JobIOMgr::startProg");
    if ( !ji.hostdata_ )
	mErrRet("Internal: No hostdata provided")

    const HostData& machine = *ji.hostdata_;
    FilePath ioparfp;
    if ( !mkIOParFile(basefp,machine,iop,ioparfp,msg_) )
	return false;

    OS::MachineCommand mc;
    mkCommand( mc, machine, progname, basefp, ioparfp, ji, rshcomm );

    iohdlr_.addJobDesc( machine, ji.descnr_ );
    const BufferString cmd( mc.getLocalCommand() );
    if ( mDebugOn )
    {
	const BufferString msg( "Executing: ", cmd );
	DBG::message(msg);
    }

    OS::CommandLauncher cl( mc );
    if ( !cl.execute(execpars_) )
    {
	iohdlr_.removeJobDesc( machine.getHostName(), ji.descnr_ );
	mErrRet( BufferString("Failed to submit command '", cmd, "'") )
    }

    return true;
}

#undef mErrRet
#define mErrRet() \
{\
    DBG::message(DBG_MM,msg);\
    return false;\
}


#ifdef __win__
extern const OD::String& getTempBaseNm();

extern int& MMJob_getTempFileNr();

static FilePath getConvertedFilePath( const HostData& hd, const FilePath& fp )
{
    FilePath newfp = hd.prefixFilePath( HostData::Data );
    if ( !newfp.nrLevels() ) return fp;

    BufferString proc( getTempBaseNm() );
    proc += "_";
    proc += MMJob_getTempFileNr()-1;
    newfp.add( GetSurveyName() ).add( "Proc" )
	 .add( proc ).add( fp.fileName() );
    return newfp;
}


bool JobIOMgr::mkIOParFile( const FilePath& basefp,
			    const HostData& remotemachine, const IOPar& iop,
			    FilePath& iopfp, BufferString& msg )
{
    iopfp = basefp; iopfp.setExtension( ".par", false );
    IOPar newiop( iop );
    const FilePath::Style machpathstyle( remotemachine.pathStyle() );

    FilePath remoteparfp = getConvertedFilePath( remotemachine, basefp );
    BufferString bs( remoteparfp.fullPath() );
    bs.replace( '.',  '_' );
    FilePath logfp( bs );
    remoteparfp.setExtension( ".par", false );
    logfp.setExtension( ".log", false );
    const FilePath remotelogfp( logfp );
    newiop.set( sKey::LogFile(), remotelogfp.fullPath(machpathstyle) );

    const FilePath remdata = remotemachine.prefixFilePath( HostData::Data );
    const char* tmpstor = iop.find( sKey::TmpStor() );
    if ( tmpstor )
    {
	const FilePath path( tmpstor );
	FilePath remotetmpdir( remdata.nrLevels() ? remdata.fullPath()
						  : path.fullPath() );
	if ( remdata.nrLevels() )
	{
	    remotetmpdir.add( GetSurveyName() ).add( "Seismics" )
			.add( path.fileName() );
	}

	newiop.set( sKey::TmpStor(),remotetmpdir.fullPath(machpathstyle) );
    }

    newiop.set( sKey::DataRoot(), remdata.fullPath(machpathstyle) );
    newiop.set( sKey::Survey(), IOM().surveyName() );

    const BufferString remotelogfnm( logfp.fullPath(machpathstyle) );
    const BufferString remoteiopfnm( iopfp.fullPath() );
    if ( File::exists(remotelogfnm) ) File::remove( remotelogfnm );
    if ( File::exists(remoteiopfnm) ) File::remove( remoteiopfnm );

    od_ostream iopstrm( remoteiopfnm );
    if ( !iopstrm.isOK() )
    {
	msg.set( "Cannot open '" ).add( remoteiopfnm ).add( "' for write ..." );
	mErrRet()
    }
    else if ( !newiop.write(iopstrm,sKey::Pars()) )
    {
	msg.set( "Cannot write parameters into '" ).add( remoteiopfnm );
	msg.add( "'" );
	mErrRet()
    }

    iopfp.set( remoteparfp.fullPath(machpathstyle) );
    return true;
}

#else

bool JobIOMgr::mkIOParFile( const FilePath& basefp,
			    const HostData& machine, const IOPar& iop,
			    FilePath& iopfp, BufferString& msg )
{
    iopfp = basefp; iopfp.setExtension( ".par", false );
    const BufferString iopfnm( iopfp.fullPath() );
    FilePath logfp(basefp); logfp.setExtension( ".log", false );
    const BufferString logfnm( logfp.fullPath() );

    FilePath remotelogfnm( machine.convPath(HostData::Data,logfp) );

    IOPar newiop( iop );
    newiop.set( sKey::LogFile(), remotelogfnm.fullPath(machine.pathStyle()) );

    const char* tmpstor = iop.find( sKey::TmpStor() );
    if ( tmpstor )
    {
	const FilePath remotetmpdir =
		machine.convPath( HostData::Data, tmpstor );
	newiop.set( sKey::TmpStor(),
		    remotetmpdir.fullPath(machine.pathStyle()) );
    }

    const FilePath remotedr =
		machine.convPath( HostData::Data, GetBaseDataDir() );
    newiop.set( sKey::DataRoot(),
		remotedr.fullPath(machine.pathStyle()) );
    newiop.set( sKey::Survey(), IOM().surveyName() );

    if ( File::exists(iopfnm) ) File::remove( iopfnm );
    if ( File::exists(logfnm) ) File::remove( logfnm );

    od_ostream iopstrm( iopfnm );
    if ( !iopstrm.isOK() )
    {
	msg.set( "Cannot open '" ).add( iopfnm ).add( "' for write ..." );
	mErrRet()
    }
    if ( !newiop.write(iopstrm,sKey::Pars()) )
    {
	msg.set( "Cannot write parameters into '" ).add( iopfnm ).add( "'" );
	mErrRet()
    }

    iopfp = machine.convPath( HostData::Data, iopfp );
    return true;
}

#endif


void JobIOMgr::mkCommand( OS::MachineCommand& mc, const HostData& machine,
			  const char* progname, const FilePath& basefp,
			  const FilePath& iopfp, const JobInfo& ji,
			  const char* rshcomm )
{
    const BufferString remhostaddress =
		       System::hostAddress( machine.getHostName() );
    const HostData& localhost = machine.localHost();
    const bool remote = !machine.isKnownAs( HostData::localHostName() ) ||
			remhostaddress != System::localAddress();
    const bool unixtounix = remote && !localhost.isWindows() &&
			    !machine.isWindows();

    if ( remote )
    {
	mc.setRemExec( unixtounix ? rshcomm
				  : OS::MachineCommand::odRemExecCmd() );
	mc.setHostName( machine.getHostName() );
    }

    CommandString argstr( machine );
    FilePath progfp( progname );
    if ( unixtounix )
    { //ssh-rsh requires full path to exec
	progfp.insert( GetExecPlfDir() );
	progfp = machine.convPath( HostData::Appl, progfp, &localhost );
    }

    argstr.addFlag( OS::MachineCommand::sKeyMasterHost(),
		 System::localAddress() );
    argstr.addFlag( OS::MachineCommand::sKeyMasterPort(), iohdlr_.port() );
    argstr.addFlag( OS::MachineCommand::sKeyJobID(), ji.descnr_ );

    BufferString cmd( unixtounix ? progfp.fullPath( machine.pathStyle() ).str()
				 : progfp.fileName().str() );

    argstr.addFilePath( iopfp );
    cmd.addSpace().add( argstr.string() );

    mc.setCommand( cmd.str() );
}
