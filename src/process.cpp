// Copyright Arnt Gulbrandsen <arnt@gulbrandsen.priv.no>; BSD-licensed.

#include "process.h"

#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <sysexits.h>

#include <boost/lexical_cast.hpp>

#include "conf.h"
#include "init.h"
#include "uid.h"


/*! Constructs a Process to look out for some child process.

    The caller manually has to hand ownership over to Init.

    This class never kills or otherwise affects the child process, it
    merely records information about it, such as the
    expectedTypicalMemory() and value() of the process. Nodee uses
    this to adjust the kernel's OOM killer policies, so that if the
    host runs out of memory, the most likely process killed is a
    low-value server that occupied lots of RAM.

    If objects of this class were to own or otherwise act on the
    process they mimic, then Init couldn't copy and delete Process
    objects quite so freely. The ability to treat Process objects
    as values is central to Nodee's pointerlessness.
*/

Process::Process()
    : p( 0 ), mp( ::getpid() ),
      faults( 0 ), prevFaults( 0 ),
      rss( 0 ), next( 0 ),
      starts( 0 ), waitUntil( 0 )
{
}


/*! Fork a child process, redirect its stdin/stdout/stderr
    appriopriately, and call start() in the child.
*/

void Process::fork()
{
    if ( p )
	return;

    time_t now = time( 0 );
    starts++;

    int tmp = ::fork();
    if ( tmp < 0 ) {
	// an error. record the problem somehow, then just return.
	p = 0;
	return;
    } else if ( tmp == 0 ) {
	// we're in the child.

	// the setregid and setreuid calls will return failure if
	// nodee is being debugged as non-root. I think that's
	// fine, so I just cast to void to underscore the point.
	if ( g )
	    (void)::setregid( g, g );
	if ( u )
	    (void)::setreuid( u, u );
	if ( now < waitUntil )
	    ::sleep( waitUntil - now );
	start();
    } else {
	// we're in the parent.
	p = tmp;
	waitUntil = now + s.restartPeriod();
    }
}


/*! Notifies this object that it's process is gone, and how.

    Init will delete the Process after calling this.
*/

void Process::handleExit( int status, int signal )
{
    status = status;
    signal = signal;

    p = 0;

    if ( next )
	next->fork();
    else if ( starts < s.maxRestarts() )
        fork();
}


/*! Called in the child process to start the child's work. */

void Process::start()
{
    if ( ::getpid() == mp )
	return;
    
    string script = s.startupScript();
    if ( script[0] == '/' ) {
	// nothing needed, it's an absolute path
    } else if ( script.empty() ) {
	script = root() + "/scripts/startup";
    } else {
	script = root() + "/" + script;
    }
    char * args[1025];
    args[0] = const_cast<char*>(script.c_str());
    int n = 1;
    map<string,string> o( s.startupOptions() );
    map<string,string>::iterator i( o.begin() );
    while ( i != o.end() && n < 1023 ) {
	args[n++] = const_cast<char*>( i->first.c_str() );
	args[n++] = const_cast<char*>( i->second.c_str() );
	++i;
    }
    args[n++] = 0;
    ::execv( script.c_str(), args );
    ::exit( EX_NOINPUT );
}



/*! Launches a new Process based on \a what, managed by \a
 *  init. Returns quickly; the new Process will go on its way.

*/

void Process::launch( const ServerSpec & what, Init & init )
{
    // we have three processes: A useful process and two preliminary
    // chores (I only just managed to avoid the word foreplay, oops,
    // it snuck its way in, I'll be more disciplined from now on)
    Process useful;
    useful.assignUidGid();

    Process install( useful.u, useful.g );
    Process download( useful.u, useful.g );

    // each of them receive basically the same spec
    useful.s = what;
    download.s = what;
    install.s = what;

    // but we change the prelimiaries so they'll do their chores
    // instead of trying to start the real thing
    map<string,string> options;
    options["--url"] = what.artifactUrl();
    options["--filename"] = what.artifactFilename();
    download.s.setStartupScript( Conf::scriptdir + "/download", options );
    options.erase( "--url" );
    options["--uid"] = boost::lexical_cast<string>( useful.u );
    options["--gid"] = boost::lexical_cast<string>( useful.u );
    options["--rootdir"] = useful.root();
    install.s.setStartupScript( Conf::scriptdir + "/install", options );

    // all three are managed by init. close your eyes and don't notice
    // the gruesome hack.
    download.next = init.manage( useful );
    install.next = init.manage( download );
    init.manage( install )->fork();
}


/*! Constructs a copy of \a other. Deep copy, no sharing. */

Process::Process( const Process & other )
    : p( other.p ), mp( other.mp ), s( other.s ),
      faults( other.faults ),
      prevFaults( other.prevFaults ),
      rss( other.rss ),
      u( other.u ), g( other.g ),
      next( other.next ),
      starts( other.starts ), waitUntil( other.waitUntil )
{
}


/*! Constructs a Process without any ServerSpec and with uid() \a uid
    and gid() \a gid.

    This is a helper for Script, which needs to start() using those
    IDs.
*/

Process::Process( int uid, int gid )
    : p( 0 ), mp( ::getpid() ),
      faults( 0 ), prevFaults( 0 ),
      rss( 0 ), u( uid ), g( gid ),
      next( 0 ),
      starts( 0 ), waitUntil( 0 )
{
}


void Process::operator=( const Process & other )
{
    p = other.p;
    s = other.s;
    faults = other.faults;
    prevFaults = other.prevFaults;
    rss = other.rss;
    next = other.next;
    starts = other.starts;
    waitUntil = other.waitUntil;
}


/*! Records \a r as the current RSS, in kbytes */

void Process::setCurrentRss( int r )
{
    rss = r;
}

/*! Returns the recorder RSS size, in kbytes */
int Process::currentRss() const
{
    return rss;
}


/*! Records that \a f faults have occured since time immemorial. */

void Process::setPageFaults( int x )
{
    prevFaults = faults;
    faults = x;
}


/*! Returns how many faults have occured between the last and second-to-last
    calls to setPageFaults().
*/

int Process::recentPageFaults() const
{
    return faults - prevFaults;
}


/*! Sets the object's state to look as though it has forked and the
    child's pid is \a fakepid. Used only for testing.
*/

void Process::fakefork( int fakepid )
{
    p = fakepid;
}


/*! Stops the process, either by calling the script specified in the
    ServerSpec or by killing it. If the latter, then the kill is
    rude. Anyone who wants a pleasant kill can supply a suitable
    script.
*/

void Process::stop()
{
    if ( !valid() )
	return;

    string script = s.shutdownScript();
    if ( script.empty() ) {
	::kill( 9, p );
    } else {
	// trouble here. need new functionality.  the uid used needs
	// to be visible to the c++, not assigned by sh at startup
	// time. foo.
    }
}


/*! Returns the UID used by this child, or 0 if the Process is not
    valid(). In theory, even valid() processes may run as root, but in
    practice that should not happen.
*/

int Process::uid() const
{
    return valid() ? u : 0;
}


/*! Returns the GID used by this child, or 0 if the Process is not
    valid(). In theory, even valid() processes may run as root, but in
    practice that should not happen.
*/

int Process::gid() const
{
    return valid() ? g : 0;
}


/*! Picks otherwise unused UID and GID for this process. */

void Process::assignUidGid()
{
    u = chooseFreeUid();
    g = chooseFreeGid();
}


/*! Returns the root directory used by this Process. Automatically
    computed so as to be unique for each Process.
*/

string Process::root() const
{
    return Conf::basedir + "/" + Conf::workdir + "/" + s.coordinate() +
	boost::lexical_cast<string>( s.port() );
}
