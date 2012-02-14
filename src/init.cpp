// Copyright Arnt Gulbrandsen <arnt@gulbrandsen.priv.no>; BSD-licensed.

#include "init.h"

#include <boost/thread.hpp>

#include <sys/types.h>
#include <sys/wait.h>


/*! \class Init init.h

    The Init class manages all the subprocesses.

    This is the core of Nodee: When a service needs to be started,
    eventually a Process will be created and handed over to Init for
    management. Init will be happy as long as nothing happens, and
    when the process (lowercase) exits, Init notices and informs the
    Process, so that the Process can take appropriate action.
*/



/*! Constructs an empty Init. */

Init::Init()
{
    boost::thread t( *this );
}


/*! Destroys the object. Does not kill any subprocesses. */

Init::~Init()
{
    // nothing needed
}


/*! Does all there is to do. */

void Init::start()
{
    while ( true )
	check();
}


/*! Waits for and processes a single child event. */

void Init::check()
{
    int status;
    int pid = ::wait( &status );

    if ( pid <= 0 ) {
	// we have no children to wait for.
	::sleep( 2 );
	return;
    }

    // we now have a pid. find out what happened to it.
    int exitStatus = -1;
    if ( WIFEXITED( status ) )
	exitStatus = WEXITSTATUS( status );
    bool signalled = WIFSIGNALED( status );
    int signal = 0;
    if ( signalled )
	signal = WTERMSIG( status );

    // find the relevant Process object, ping it and forget about it.
    std::list<Process>::iterator i = l.begin();
    while ( i != l.end() && i->pid() != pid )
	++i;
    if ( i != l.end() && i->pid() == pid ) {
	i->handleExit( exitStatus, signal );
	if ( !i->pid() )
	    l.remove( *i );
    }
}


/*! Returns a reference to Init's list of managed processes. Callers
    should not change the list, but may change the included objects.
*/

std::list<Process> & Init::processes()
{
    return l;
}


/*! Starts managing \a p. This is a copy operation; the managed object
    is not yours.

    Returns a pointer to the copied object.
*/

Process * Init::manage( const Process & p )
{
    l.push_back( p );
    return &*l.rbegin();
}


/*! Returns a copy of the Process object for \a pid, or an invalid
    Process object if \a pid is not the pid of a managed service.
*/

Process Init::find( int pid ) const
{
    std::list<Process>::const_iterator i = l.begin();
    while ( i != l.end() && i->pid() != pid )
	++i;
    if ( i == l.end() )
	return Process();
    return *i;
}


/*! boost::thread wants to call start() by this name, so here's a
    wrapper around start().
*/   

void Init::operator()()
{
    start();
}
