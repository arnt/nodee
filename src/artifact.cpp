// Copyright Arnt Gulbrandsen <arnt@gulbrandsen.priv.no>; BSD-licensed.

#include "artifact.h"

#include "conf.h"

#include <stdio.h>

#include <algorithm>

#include <list>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/filesystem.hpp>

using namespace boost::filesystem;
using boost::property_tree::ptree;

/* ubuntu 10.04 includes boost 1.40, which has boost::filesystem API
   2. 11.10 includes API 3. rather than demand and upgraded or
   downgraded boost, I tried to write code that works with both. this
   wrapper is needed to get a filename from an iterator.
*/

static string filename( const boost::filesystem::directory_iterator & i )
{
#if defined(BOOST_FILESYSTEM_VERSION) && BOOST_FILESYSTEM_VERSION == 3
    return i->path().filename().native();
#else
    return i->filename();
#endif
}

static string fullname( const boost::filesystem::directory_iterator & i )
{
#if defined(BOOST_FILESYSTEM_VERSION) && BOOST_FILESYSTEM_VERSION == 3
    return i->path().string();
#else
    return i->string();
#endif
}


/*! Returns a json object containing a list of installed artifacts.
    Format to be decided later; I don't think this is useful, so I'll
    just do something and if we turn out to need it, but different,
    we'll know how by then.
*/

string Artifact::list()
{
    ostringstream os;
    ptree pt;

    directory_iterator i = directory_iterator( Conf::artefactdir );
    int n = 1;
    while ( i != directory_iterator() ) {
	if ( is_regular_file( *i ) ) {
	    pt.put( boost::lexical_cast<string>( n ), filename( i ) );
	    ++i;
	    n++;
	}
    }

    write_json( os, pt );
    return os.str();
}
