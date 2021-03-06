/*+
________________________________________________________________________

 (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 Author:	A.H.Bril
 Date:		3-5-1994
 Contents:	File utitlities
________________________________________________________________________

-*/

#include "file.h"
#include "webfile.h"
#include "filepath.h"
#include "bufstringset.h"
#include "dirlist.h"
#include "envvars.h"
#include "perthreadrepos.h"
#include "winutils.h"
#include "executor.h"
#include "ptrman.h"
#include "od_istream.h"
#include "oddirs.h"
#include "oscommand.h"
#include "uistrings.h"

#ifdef __win__
# include <direct.h>
#else
# include "sys/stat.h"
# include <unistd.h>
#endif

#ifndef OD_NO_QT
# include <QDateTime>
# include <QDir>
# include <QFile>
# include <QFileInfo>
# include <QProcess>
#else
# include <fstream>
#endif

#define mMBFactor (1024*1024)
const char* not_implemented_str = "Not implemented";


namespace File
{

static ExistsFn web_existsfn = 0;
static GetSizeFn web_getsizefn = 0;
static GetContentFn web_getcontentfn = 0;

void setWebHandlers( ExistsFn existfn, GetSizeFn sizefn, GetContentFn contfn )
{
    if ( existfn )
	web_existsfn = existfn;
    if ( sizefn )
	web_getsizefn = sizefn;
    if ( contfn )
	web_getcontentfn = contfn;
}

static inline bool isSane( const char*& fnm )
{
    if ( !fnm || !*fnm )
	return false;
    mSkipBlanks( fnm );
    return *fnm;
}

static inline bool fnmIsURI( const char*& fnm )
{
    if ( *fnm != 'f' && *fnm != 'h' && *fnm != 'F' && *fnm != 'H' )
	return false;

    const FixedString uri( fnm );
#   define mURIStartsWith(s) uri.startsWith( s, CaseInsensitive )

    if ( mURIStartsWith( "file://" ) )
	{ fnm += 7; return false; }
    if ( mURIStartsWith( "http://" )
      || mURIStartsWith( "https://" )
      || mURIStartsWith( "ftp://" ) )
	return true;

    return false;
}


static inline bool isLocal( const char* fnm )
{
    return isSane(fnm) && !fnmIsURI(fnm);
}


class RecursiveCopier : public Executor
{ mODTextTranslationClass(RecursiveCopier);
public:
			RecursiveCopier(const char* from,const char* to)
			    : Executor("Copying Directory")
			    , src_(from),dest_(to),fileidx_(0)
			    , totalnr_(0),nrdone_(0)
			    , msg_(tr("Copying files"))
			{
			    makeRecursiveFileList(src_,filelist_,false);
			    for ( int idx=0; idx<filelist_.size(); idx++ )
				totalnr_ += getFileSize( filelist_.get(idx) );
			}

    od_int64		nrDone() const		{ return nrdone_ / mMBFactor; }
    od_int64		totalNr() const		{ return totalnr_ / mMBFactor; }
    uiString		uiMessage() const	{ return msg_; }
    uiString		uiNrDoneText() const	{ return tr("MBytes copied"); }

protected:

    int			nextStep();

    int			fileidx_;
    od_int64		totalnr_;
    od_int64		nrdone_;
    BufferStringSet	filelist_;
    BufferString	src_;
    BufferString	dest_;
    uiString		msg_;

};


#define mErrRet(s1) { msg_ = s1; return ErrorOccurred(); }
int RecursiveCopier::nextStep()
{
#ifdef OD_NO_QT
    return ErrorOccurred();
#else
    if ( fileidx_ >= filelist_.size() )
	return Finished();

    if ( !fileidx_ )
    {
	if ( File::exists(dest_) && !File::remove(dest_) )
	    mErrRet(tr("Cannot overwrite %1").arg(dest_) )
	if( !File::createDir(dest_) )
	    mErrRet( uiStrings::phrCannotCreateDirectory(toUiString(dest_)) )
    }

    const BufferString& srcfile = *filelist_[fileidx_];
    QDir srcdir( src_.buf() );
    BufferString relpath( srcdir.relativeFilePath(srcfile.buf()) );
    const BufferString destfile = FilePath(dest_,relpath).fullPath();
    if ( File::isLink(srcfile) )
    {
	BufferString linkval = linkValue( srcfile );
	if ( !createLink(linkval,destfile) )
	    mErrRet(
	       uiStrings::phrCannotCreate(tr("symbolic link %1").arg(destfile)))
    }
    else if ( isDirectory(srcfile) )
    {
	if ( !File::createDir(destfile) )
	    mErrRet( uiStrings::phrCannotCreateDirectory(toUiString(destfile)) )
    }
    else if ( !File::copy(srcfile,destfile) )
	mErrRet( uiStrings::phrCannotCreate( tr("file %1").arg(destfile)) )

    fileidx_++;
    nrdone_ += getFileSize( srcfile );
    return MoreToDo();
#endif
}


class RecursiveDeleter : public Executor
{
public:
			RecursiveDeleter(const char* dirnm,
					 const BufferStringSet* externallist=0,
					 bool filesonly=false)
			    : Executor("Removing Files")
			    , dirname_(dirnm)
			    , nrdone_(0)
			    , fileidx_(0)
			    , filesonly_(filesonly)
			{
			    if ( !externallist )
			    {
				filelist_.add( dirname_ );
				makeRecursiveFileList( dirname_, filelist_  );
			    }
			    else
				filelist_.copy( *externallist );

			    totalnr_ = filelist_.size();
			}

    od_int64		nrDone() const		{ return nrdone_; }
    od_int64		totalNr() const		{ return totalnr_; }
    uiString		uiMessage() const	{ return msg_; }
    uiString		uiNrDoneText() const
			{ return mToUiStringTodo( "Files removed" ); }

    int			nextStep();

protected:

    od_int64		fileidx_;
    od_int64		totalnr_;
    od_int64		nrdone_;
    BufferStringSet	filelist_;
    BufferString	dirname_;
    uiString		msg_;
    bool		filesonly_;
};


int RecursiveDeleter::nextStep()
{
    if ( nrdone_ >= totalnr_ )
	return Finished();

    fileidx_ = totalnr_ - ( nrdone_ + 1 );
    const BufferString& filename = filelist_.get( fileidx_ );
    bool res = true;
    if ( File::isDirectory(filename) )
    {
	if ( filesonly_ )
	{
	    if ( isDirEmpty(filename) )
		res = File::removeDir( filename );
	}
	else
	    res = File::removeDir( filename );
    }
    else if( File::exists(filename) )
	res = File::remove( filename );

    if ( !res )
    {
	uiString msg( mToUiStringTodo("Failed to remove ") );
	msg.append( filename );
	msg_ = msg;
	return ErrorOccurred();
    }

    nrdone_++;
    return MoreToDo();
}


Executor* getRecursiveCopier( const char* from, const char* to )
{ return isSane(from) && isSane(to) ? new RecursiveCopier( from, to ) : 0; }

Executor* getRecursiveDeleter( const char* dirname,
			       const BufferStringSet* externallist,
			       bool filesonly )
{ return !isSane(dirname) ? 0
       : new RecursiveDeleter( dirname, externallist, filesonly ); }


void makeRecursiveFileList( const char* dir, BufferStringSet& filelist,
			    bool followlinks )
{
    if ( !isSane(dir) )
	return;

    DirList files( dir, DirList::FilesOnly );
    for ( int idx=0; idx<files.size(); idx++ )
    {
	if ( !followlinks )
	    filelist.add( files.fullPath(idx) );
	else
	    filelist.addIfNew( files.fullPath(idx) );
    }

    DirList dirs( dir, DirList::DirsOnly );
    for ( int idx=0; idx<dirs.size(); idx++ )
    {
	BufferString curdir( dirs.fullPath(idx) );
	if ( !followlinks )
	    filelist.add( curdir );
	else if ( !filelist.addIfNew(curdir) )
	    continue;

	if ( !File::isLink(curdir) )
	    makeRecursiveFileList( curdir, filelist, followlinks );
	else if ( followlinks )
	{
	    curdir = File::linkTarget( curdir );
	    if ( !filelist.isPresent(curdir) )
		makeRecursiveFileList( curdir, filelist, followlinks );
	}
    }
}


od_int64 getFileSize( const char* fnm, bool followlink )
{
    if ( isURI(fnm) )
	return web_getsizefn ? (*web_getsizefn)( fnm ) : 0;

    if ( !followlink && isLink(fnm) )
    {
        od_int64 filesize = 0;
#ifdef __win__
        HANDLE file = CreateFile ( fnm, GENERIC_READ, 0, NULL, OPEN_EXISTING,
                                   FILE_ATTRIBUTE_NORMAL, NULL );
        filesize = GetFileSize( file, NULL );
        CloseHandle( file );
#else
        struct stat filestat;
        filesize = lstat( fnm, &filestat )>=0 ? filestat.st_size : 0;
#endif

	return filesize;
    }

#ifndef OD_NO_QT
    QFileInfo qfi( fnm );
    return qfi.size();
#else
    struct stat st_buf;
    int status = stat(fnm, &st_buf);
    if (status != 0)
	return 0;

    return st_buf.st_size;
#endif
}

bool exists( const char* fnm )
{
    if ( !isSane(fnm) )
	return false;
    else if ( fnmIsURI(fnm) )
	return web_existsfn ? (*web_existsfn)( fnm ) : od_istream(fnm).isOK();

#ifndef OD_NO_QT
    return (*fnm == '@' && *(fnm+1)) || QFile::exists( fnm );
    // support, like od_istream, commands. These start with '@'.
#else
    return od_istream(fnm).isOK();
#endif
}


bool isEmpty( const char* fnm )
{
    return getFileSize( fnm ) < 1;
}


bool isDirEmpty( const char* dirnm )
{
    if ( !isLocal(dirnm) )
	return true;

#ifndef OD_NO_QT
    const QDir qdir( dirnm );
    return qdir.entryInfoList(QDir::NoDotAndDotDot|
			      QDir::AllEntries).count() == 0;
#else
    return false;
#endif
}


bool isFile( const char* fnm )
{
    if ( !isSane(fnm) )
	return false;
    else if ( fnmIsURI(fnm) )
	return true;

#ifndef OD_NO_QT
    QFileInfo qfi( fnm );
    return qfi.isFile();
#else
    struct stat st_buf;
    int status = stat(fnm, &st_buf);
    if (status != 0)
	return false;

    return S_ISREG (st_buf.st_mode);
#endif
}


bool isDirectory( const char* fnm )
{
    if ( !isSane(fnm) )
	return false;
    else if ( fnmIsURI(fnm) )
	return false;

#ifndef OD_NO_QT
    QFileInfo qfi( fnm );
    if ( qfi.isDir() )
	return true;

    BufferString lnkfnm( fnm, ".lnk" );
    qfi.setFile( lnkfnm.buf() );
    return qfi.isDir();
#else
    struct stat st_buf;
    int status = stat(fnm, &st_buf);
    if (status != 0)
	return false;

    if ( S_ISDIR (st_buf.st_mode) )
	return true;

    BufferString lnkfnm( fnm, ".lnk" );
    status = stat ( lnkfnm.buf(), &st_buf);
    if (status != 0)
	return false;

    return S_ISDIR (st_buf.st_mode);
#endif
}


bool isURI( const char*& fnm )
{
    return isSane(fnm) && fnmIsURI(fnm);
}


const char* getCanonicalPath( const char* dir )
{
    mDeclStaticString( ret );
#ifndef OD_NO_QT
    const QDir qdir( dir );
    ret = qdir.canonicalPath();
#else
    pFreeFnErrMsg(not_implemented_str);
    ret = dir;
#endif
    return ret.buf();
}


const char* getRelativePath( const char* reltodir, const char* fnm )
{
#ifndef OD_NO_QT
    BufferString reltopath = getCanonicalPath( reltodir );
    BufferString path = getCanonicalPath( fnm );
    mDeclStaticString( ret );
    const QDir qdir( reltopath.buf() );
    ret = qdir.relativeFilePath( path.buf() );
    return ret.isEmpty() ? fnm : ret.buf();
#else
    pFreeFnErrMsg(not_implemented_str);
    return fnm;
#endif
}


bool isLink( const char* fnm )
{
    if ( !isLocal(fnm) )
	return false;

#ifndef OD_NO_QT
    QFileInfo qfi( fnm );
    return qfi.isSymLink();
#else
    pFreeFnErrMsg(not_implemented_str);
    return false;
#endif
}


void hide( const char* fnm, bool yn )
{
    if ( !isLocal(fnm) || !exists(fnm) )
	return;

#ifdef __win__
    const int attr = GetFileAttributes( fnm );
    if ( yn )
    {
	if ( (attr & FILE_ATTRIBUTE_HIDDEN) == 0 )
	    SetFileAttributes( fnm, attr | FILE_ATTRIBUTE_HIDDEN );
    }
    else
    {
	if ( (attr & FILE_ATTRIBUTE_HIDDEN) == FILE_ATTRIBUTE_HIDDEN )
	    SetFileAttributes( fnm, attr & ~FILE_ATTRIBUTE_HIDDEN );
    }
#endif
}


bool isHidden( const char* fnm )
{
    if ( !isLocal(fnm) )
	return false;

#ifndef OD_NO_QT
    QFileInfo qfi( fnm );
    return qfi.isHidden();
#else
    pFreeFnErrMsg(not_implemented_str);
    return false;
#endif
}


bool isReadable( const char* fnm )
{
    if ( !isSane(fnm) )
	return false;
    else if ( fnmIsURI(fnm) )
	return exists(fnm);

#ifdef OD_NO_QT
    struct stat st_buf;
    int status = stat(fnm, &st_buf);
    if (status != 0)
	return false;

    return st_buf.st_mode & S_IRUSR;
#else
    QFileInfo qfi( fnm );
    return qfi.isReadable();
#endif
}


bool isWritable( const char* fnm )
{
    if ( !isSane(fnm) )
	return false;
    else if ( fnmIsURI(fnm) )
	return true;

#ifdef OD_NO_QT
    struct stat st_buf;
    int status = stat(fnm, &st_buf);
    if (status != 0)
	return false;

    return st_buf.st_mode & S_IWUSR;
#else
    QFileInfo qfi( fnm );
    return qfi.isWritable();
#endif
}


bool isExecutable( const char* fnm )
{
    if ( !isLocal(fnm) )
	return false;

#ifndef OD_NO_QT
    QFileInfo qfi( fnm );
    return qfi.isReadable() && qfi.isExecutable();
#else
    struct stat st_buf;
    int status = stat(fnm, &st_buf);
    if (status != 0)
	return false;

    return st_buf.st_mode & S_IXUSR;
#endif
}



bool isFileInUse( const char* fnm )
{
#ifdef __win__
    if ( isURI(fnm) )
	return false;

    HANDLE handle = CreateFileA( fnm,
				 GENERIC_READ | GENERIC_WRITE,
				 0,
				 0,
				 OPEN_EXISTING,
				 0,
				 0 );
    const bool ret = handle == INVALID_HANDLE_VALUE;
    CloseHandle( handle );
    return ret;
#else
    return false;
#endif
}


bool createDir( const char* fnm )
{
    if ( !isLocal(fnm) )
	return false;

#ifndef OD_NO_QT
    QDir qdir; return qdir.mkpath( fnm );
#else
    pFreeFnErrMsg(not_implemented_str);
    return false;
#endif
}


bool rename( const char* oldname, const char* newname )
{
    if ( !isSane(oldname) || !isSane(newname)
      || fnmIsURI(oldname) || fnmIsURI(newname) )
	return false;

#ifndef OD_NO_QT
    return QFile::rename( oldname, newname );
#else
    pFreeFnErrMsg(not_implemented_str);
    return false;
#endif

}


bool createLink( const char* fnm, const char* linknm )
{
    if ( !isLocal(fnm) )
	return false;

#ifndef OD_NO_QT
#ifdef __win__
    BufferString winlinknm( linknm );
    if ( !firstOcc(linknm,".lnk")  )
	winlinknm += ".lnk";
    return QFile::link( fnm, winlinknm.buf() );
#else
    return QFile::link( fnm, linknm );
#endif // __win__

#else
    pFreeFnErrMsg(not_implemented_str);
    return false;
#endif
}


bool saveCopy( const char* from, const char* to )
{
    if ( !isLocal(from) || !isLocal(to) )
	return false;

    if ( isDirectory(from) )
	return false;
    if ( !exists(to) )
	return File::copy( from, to );

    const BufferString tmpfnm( to, ".tmp" );
    if ( !File::rename(to,tmpfnm) )
	return false;

    const bool res = File::copy( from, to );
    res ? File::remove( tmpfnm ) : File::rename( tmpfnm, to );
    return res;
}


bool copy( const char* from, const char* to, uiString* errmsg )
{
    if ( !isLocal(from) || !isLocal(to) )
	return false;

    if ( isDirectory(from) || isDirectory(to)  )
	return copyDir( from, to, errmsg );

    uiString errmsgloc;
    if ( !checkDirectory(from,true,errmsg ? *errmsg:errmsgloc) ||
	 !checkDirectory(to,false,errmsg ? *errmsg:errmsgloc) )
	return false;

    if ( exists(to) && !isDirectory(to) )
	File::remove( to );

#ifdef OD_NO_QT
    pFreeFnErrMsg(not_implemented_str);
    return false;
#else
    QFile qfile( from );
    bool ret = qfile.copy( to );
    if ( !ret && errmsg )
	errmsg->setFrom( qfile.errorString() );

    return ret;
#endif
}


bool copyDir( const char* from, const char* to, uiString* errmsg )
{
    if ( !isLocal(from) || !isLocal(to) )
	return false;

    if ( !exists(from) || exists(to) )
	return false;

    uiString errmsgloc;
    if ( !checkDirectory(from,true,errmsg ? *errmsg:errmsgloc) ||
	 !checkDirectory(to,false,errmsg ? *errmsg:errmsgloc) )
	return false;

#ifndef OD_NO_QT
    PtrMan<Executor> copier = getRecursiveCopier( from, to );
    const bool res = copier->execute();
    if ( !res && errmsg )
	*errmsg = copier->uiMessage();
#else

    BufferString cmd;
#ifdef __win__
    cmd = "xcopy /E /I /Q /H ";
    cmd.add(" \"").add(from).add("\" \"").add(to).add("\"");
#else
    cmd = "/bin/cp -r ";
    cmd.add(" '").add(from).add("' '").add(to).add("'");
#endif

    bool res = !system( cmd );
    if ( res ) res = exists( to );
#endif
    return res;
}


bool resize( const char* fnm, od_int64 newsz )
{
    if ( !isLocal(fnm) )
	return false;
    else if ( newsz < 0 )
	return remove( fnm );
#ifndef OD_NO_QT
    return QFile::resize( fnm, newsz );
#else
    pFreeFnErrMsg(not_implemented_str);
    return false;
#endif
}


bool remove( const char* fnm )
{
    if ( !isSane(fnm) )
	return true;
    else if ( fnmIsURI(fnm) )
	return false;
#ifndef OD_NO_QT
    return isFile(fnm) ? QFile::remove( fnm ) : removeDir( fnm );
#else
    pFreeFnErrMsg(not_implemented_str);
    return false;
#endif
}


bool removeDir( const char* dirnm )
{
    if ( !isSane(dirnm) )
	return true;
    else if ( fnmIsURI(dirnm) )
	return false;

#ifdef OD_NO_QT
    return false;
#else
    if ( !exists(dirnm) )
	return true;

    if ( isLink(dirnm) )
	return QFile::remove( dirnm );

# ifdef __win__
    return winRemoveDir( dirnm );
# else
    BufferString cmd;
    cmd = "/bin/rm -rf";
    cmd.add(" \"").add(dirnm).add("\"");
    bool res = QProcess::execute( QString(cmd.buf()) ) >= 0;
    if ( res ) res = !exists(dirnm);
    return res;
# endif
#endif
}


bool changeDir( const char* dir )
{
    if ( !isLocal(dir) )
	return false;
#ifdef __win__
    return _chdir( dir )==0;
#else
    return chdir( dir )==0;
#endif
}


bool checkDirectory( const char* fnm, bool forread, uiString& errmsg )
{
    if ( !isSane(fnm) )
    {
	errmsg = od_static_tr( "FilecheckDirectory",
			       "Please specify a directory name" );
	return false;
    }
    else if ( fnmIsURI(fnm) )
    {
	errmsg = od_static_tr( "FilecheckDirectory",
			       "Web addresses not supported (yet)" );
	return false;
    }

    FilePath fp( fnm );
    BufferString dirnm( fp.pathOnly() );

    const bool success = forread ? isReadable( dirnm ) : isWritable( dirnm );
    uiString postfix = od_static_tr( "FilecheckDirectory", "in folder: %1" )
				   .arg( dirnm );
    errmsg = forread ? uiStrings::phrCannotRead( postfix )
		     : uiStrings::phrCannotWrite( postfix );
    errmsg.append( uiStrings::sCheckPermissions(), true );

    return success;
}


bool makeWritable( const char* fnm, bool yn, bool recursive )
{
    if ( !isSane(fnm) )
	return false;
    else if ( fnmIsURI(fnm) )
	return true;

#ifdef OD_NO_QT
    return false;
#else
    BufferString cmd;
# ifdef __win__
    cmd = "attrib"; cmd += yn ? " -R " : " +R ";
    cmd.add("\"").add(fnm).add("\"");
    if ( recursive && isDirectory(fnm) )
	cmd += "\\*.* /S ";
# else
    cmd = "chmod";
    if ( recursive && isDirectory(fnm) )
	cmd += " -R ";
    cmd.add(yn ? " ug+w \"" : " a-w \"").add(fnm).add("\"");
# endif

    return QProcess::execute( QString(cmd.buf()) ) >= 0;
#endif
}


bool makeExecutable( const char* fnm, bool yn )
{
    if ( !isSane(fnm) )
	return false;
    else if ( fnmIsURI(fnm) )
	return !yn;

#if ((defined __win__) || (defined OD_NO_QT) )
    return true;
#else
    BufferString cmd( "chmod" );
    cmd.add(yn ? " +r+x \"" : " -x \"").add(fnm).add("\"");
    return QProcess::execute( QString(cmd.buf()) ) >= 0;
#endif
}


bool setPermissions( const char* fnm, const char* perms, bool recursive )
{
    if ( !isLocal(fnm) )
	return false;

#if ((defined __win__) || (defined OD_NO_QT) )
    return false;
#else
    BufferString cmd( "chmod " );
    if ( recursive && isDirectory(fnm) )
	cmd += " -R ";
    cmd.add( perms ).add( " \"" ).add( fnm ).add( "\"" );
    return QProcess::execute( QString(cmd.buf()) ) >= 0;
#endif
}


bool getContent( const char* fnm, BufferString& bs )
{
    if ( !isSane(fnm) )
	return false;
    else if ( fnmIsURI(fnm) )
	return web_getcontentfn ? (*web_getcontentfn)( fnm, bs ) : false;

    bs.setEmpty();
    if ( !fnm || !*fnm ) return false;

    od_istream stream( fnm );
    if ( stream.isBad() )
	return false;

    return !stream.isOK() ? true : stream.getAll( bs );
}


od_int64 getKbSize( const char* fnm )
{
    od_int64 kbsz = getFileSize( fnm ) / 1024;
    return kbsz;
}


BufferString getFileSizeString( od_int64 filesz ) // filesz in kB
{
    BufferString szstr;
    if ( filesz > 1024 )
    {
	const bool doGb = filesz > 1048576;
	const int nr = doGb ? mNINT32(filesz/10485.76) : mNINT32(filesz/10.24);
	szstr = nr/100;
	const int rest = nr%100;
	szstr += rest < 10 ? ".0" : "."; szstr += rest;
	szstr += doGb ? " GB" : " MB";
    }
    else if ( filesz == 0 )
	szstr = "< 1 kB";
    else
    {
	szstr = filesz;
	szstr += " kB";
    }

    return szstr;
}


BufferString getFileSizeString( const char* fnm )
{ return getFileSizeString( getKbSize(fnm) ); }


#define mRetUnknown { ret.set( "<unknown>" ); return ret.buf(); }


const char* timeCreated( const char* fnm, const char* fmt )
{
    mDeclStaticString( ret );
    if ( !isLocal(fnm) )
	mRetUnknown

#ifndef OD_NO_QT
    const QFileInfo qfi( fnm );
    ret = qfi.created().toString( fmt );
    return ret.buf();
#else
    pFreeFnErrMsg(not_implemented_str);
    mRetUnknown
#endif
}


const char* timeLastModified( const char* fnm, const char* fmt )
{
    mDeclStaticString( ret );
    if ( !isLocal(fnm) )
	mRetUnknown

#ifndef OD_NO_QT
    const QFileInfo qfi( fnm );
    ret = qfi.lastModified().toString( fmt );
    return ret.buf();
#else
    pFreeFnErrMsg(not_implemented_str);
    mRetUnknown
#endif
}


od_int64 getTimeInSeconds( const char* fnm, bool lastmodif )
{
    if ( !isLocal(fnm) )
	return 0;

#ifndef OD_NO_QT
    const QFileInfo qfi( fnm );
    return lastmodif ? qfi.lastModified().toTime_t() : qfi.created().toTime_t();
#else
    struct stat st_buf;
    int status = stat(fnm, &st_buf);
    if (status != 0)
	return 0;

    return lastmodif ? st_buf.st_mtime : st_buf.st_ctime;
#endif
}


const char* linkValue( const char* linknm )
{
    if ( !isSane(linknm) )
	return "";
    else if ( fnmIsURI(linknm) )
	return linknm;

#ifdef __win__
    return linkTarget( linknm );
#else
    mDeclStaticString( ret );
    ret.setMinBufSize( 1024 );
    const int len = readlink( linknm, ret.getCStr(), 1024 );
    if ( len < 0 )
	return linknm;

    ret[len] = '\0';
    return ret.buf();
#endif
}


const char* linkTarget( const char* linknm )
{
    if ( !isSane(linknm) )
	return "";
    else if ( fnmIsURI(linknm) )
	return linknm;

    mDeclStaticString( ret );
#ifndef OD_NO_QT
    const QFileInfo qfi( linknm );
    ret = qfi.isSymLink() ? qfi.symLinkTarget()
			  : linknm;
#else
    pFreeFnErrMsg(not_implemented_str);
    ret = linknm;
#endif
    return ret.buf();
}


const char* getCurrentPath()
{
    mDeclStaticString( ret );

#ifndef OD_NO_QT
    ret = QDir::currentPath();
#else
    ret.setMinBufSize( 1024 );
# ifdef __win__
    _getcwd( ret.buf(), ret.minBufSize() );
# else
    getcwd( ret.getCStr(), ret.minBufSize() );
# endif
#endif
    return ret.buf();
}


const char* getHomePath()
{
    mDeclStaticString( ret );
#ifndef OD_NO_QT
    ret = QDir::homePath();
#else
    pFreeFnErrMsg(not_implemented_str);
    ret = GetEnvVar( "HOME" );
#endif
    return ret.buf();
}


const char* getTempPath()
{
    mDeclStaticString( ret );
#ifndef OD_NO_QT
    ret = QDir::tempPath();
# ifdef __win__
    ret.replace( '/', '\\' );
# endif
#else
    pFreeFnErrMsg(not_implemented_str);
# ifdef __win__
    ret = "/tmp";
# else
    ret = "C:\\TEMP";
# endif
#endif
    return ret.buf();
}


const char* getRootPath( const char* path )
{
    mDeclStaticString( ret );
#ifndef OD_NO_QT
    const QDir qdir( path );
    ret = qdir.rootPath();
#else
    pFreeFnErrMsg(not_implemented_str);
# ifdef __win__
    ret = "/";
# else
    ret = "C:\\";
# endif
#endif
    return ret.buf();
}


bool launchViewer( const char* fnm, const ViewPars& vp )
{
    if ( !exists(fnm) )
	return false;

    BufferString cmd( "\"",
		      FilePath(GetExecPlfDir(),"od_FileBrowser").fullPath(),
		      "\"" );
    if ( vp.style_ != Text )
	cmd.add( " --style ")
	   .add( vp.style_ == File::Table	? "table"
	      : (vp.style_ == File::Log		? "log" : "bin") );
    if ( vp.editable_ )
	cmd.add( " --edit" );
    cmd.add( " --maxlines " ).add( vp.maxnrlines_ );
#ifdef __mac__
    cmd.add( " --nofork" );
#endif
    cmd.add( " " ).add(" \" ").add( fnm ).add(" \" ");

    OS::CommandLauncher cl = OS::MachineCommand( cmd );
    return cl.execute();
}

} // namespace File
