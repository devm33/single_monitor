/**********************************************************************
 * monitor.cc
 * 
 * Script to monitor files added to a single request directory and link 
 * them to an archive directory.
 * 
 * Depends:
 * 	libfam for fam notifications
 * 
 * @author Devraj Mehta
 **********************************************************************/

#include<string>
#include<stdexcept>
#include<functional>
#include<iostream>
#include<list>
#include<errno.h>
#include<stdlib.h>
#include<string.h>
extern "C" {
	#include<errno.h>
	extern int errno ;
	#include<fam.h>
	#include<unistd.h> // access() , sleep()
	#include<signal.h> // signal()
}
using namespace std;

//Local Environment variables
string request("/home/devraj/Dropbox/cat-pictures/"); //req dir
string listfname("list.txt"); //filename for list in req dir
string archive("/home/devraj/Videos/"); //archive dir

bool runFam = false ; //global for continuing to listen to fam connection
FAMConnection* fc = NULL ;
typedef list< string* > DirList ;
DirList dirNames ; //global for storing all watched directories for clean up
typedef list< FAMRequest* > RequestList ;
RequestList requests ; //global for storing all fam request objs for clean up
DirList archiveDirs; //global for the archive directories


void addArchiveDirs() {
//TODO add support for multiple archive dirs
// really i just want this to be recursive
}

void checkStrerror( int error ){
	if( 0 != error ){
		cerr << '\t' << "System error is: " << strerror( errno ) << endl ;
	}
	return ;
}

// Handle SIGINT (aka control-C): cancel FAM monitors
void sighandler_SIGINT( int sig ){
	cout << "[exit]" << endl ;
	runFam = false ;
	return ;
}

// Register a directory with fam
void register_directory( string* dir ){
	FAMRequest* fr = new FAMRequest() ;
	cout << "Registering directory \"" << *dir << "\" with FAM" << endl ;
	try{
		if( 0 != access( dir->c_str() , F_OK ) ){
			throw( runtime_error( "directory access problem" ) ) ;
		}
		if( 0 != FAMMonitorDirectory( fc , dir->c_str() , fr , dir ) ){
			throw( runtime_error( "problem registering directory with FAM" ) ) ;
		} else { // store for clean up
			dirNames.push_back( dir ) ;
			requests.push_back( fr ) ;
		}
	}catch( const runtime_error& e ){
		cerr << "Unable to access directory." << endl ;
		checkStrerror( errno ) ;
		delete( fr ) ;
		delete( dir ) ;
	}
	return ;
}


int main( const int argc , const char** argv ){
	// logging
	system( "date" );	
	
	// register a function to handle SIGINT signals
	if( SIG_ERR == signal( SIGINT , sighandler_SIGINT ) ){
		cerr << "Error: Unable to set signal handler for SIGINT (Control-C)" << endl ;
		return( 1 ) ;
	}
	
	//FAM vars
	fc = new FAMConnection() ; //FIXME? what's with the re-def here (defined global?) //XXX did rming that break this?
	FAMEvent* fe = new FAMEvent() ; // event data is put here.  This pointer will be reused for each event.

	// init FAM connection
	if( 0 != FAMOpen( fc ) ){
		cerr << "Unable to open FAM connection." << endl ;
		cerr << "(Hint: make sure FAM (via xinetd) and portmapper are running.)" << endl ;
		checkStrerror( errno ) ;
		return( 1 ) ;
	}
	
	//build watch list of archive directories
	//TODO ^
	
	//register the archive directories with fam
	for(DirList::const_iterator ix( archiveDirs.begin() ) , stop( archiveDirs.end() ) ; ix != stop ; ++ix){
		register_directory( reinterpret_cast< string* >( *ix ) );
	}	
	
	//register the request dir
	if( 1 == argc ){
		register_directory( &request );
	}
	
	//check to make sure there are connected dirs to monitor
	if( dirNames.empty() ){
		cerr << "No directories to monitor; exiting with error." << endl ;
		return( 1 ) ;
	}
	
	int createsTriggered = 0 ; // a counter for the number of expected create events
	runFam = true ; // enable the event loop
	
	while( runFam ){
		if( 1 != FAMPending( fc ) ){
			sleep( 2 ) ;
			continue ;
		}
		int rc = FAMNextEvent( fc , fe ) ;
		if( 1 != rc ){
			cerr << "FAMNextEvent returned error" << endl ;
			continue ;
		}
		string* dir = reinterpret_cast< string* >( fe->userdata ) ;
		string archfile = archive + fe->filename ;
		string fname = fe->filename ;
		
		if( archive == *dir ){ //TODO change this to check list of archiveDirs (add loop and flag)
			if( fe->code == FAMChanged || fe->code == FAMCreated || fe->code == FAMDeleted ){
				//update list.txt in req dir //TODO move this to method updateList( listfname, archive )
				string update( "ls " );
				update = update + archive + " > " + dropbox + listfname; //TODO concatenate all the different archive dirs together
				system( update.c_str() );
				createsTriggered += 1;
				cout << createsTriggered << " creates triggered" << endl;
			}
		}
		else { //if not archive dir assume request dir
			if( fe->code == FAMCreated ){ // we only care about creation events in the req dir
				if(createsTriggered > 0) { //skip self triggered events
					createsTriggered -= 1;
					cout << " skipped create. creates left: " << createsTriggered << endl ;
					continue;
				}
				if(fname == listfname) { //hey! no changing this file
					//TODO call updateList( listfname, archive )
					continue; //TODO 
				}
				cout << "DIR: request ";
				cout << "CREATED " ;
				cout << "Event on watched dir \"" << *dir << "\", file \"" << fname << "\" : " ;
				cout << "file was created" ;
				cout << endl ;
				
				if( 0 != access(  archfile.c_str() , F_OK ) ){
					cerr << archfile << " file access problem" ;
				}
				system( ("cp -f " + archfile + " " + request + fname).c_str() );
				createsTriggered += 1;
				cout << createsTriggered << " creates triggered" << endl;
			}
		}
	} //while( runFam )

	// - - - - - - - - - - - - - - - - - - - -
	// cleanup

	for(
		RequestList::const_iterator ix( requests.begin() ) , stop( requests.end() ) ;
		ix != stop ;
		++ix
	){
		cout << "[Cancelling monitor for FAMRequest " << (*ix)->reqnum << "]" << endl ;
		FAMCancelMonitor( fc , *ix ) ;
		delete( *ix ) ;
	}

	for(
		DirList::const_iterator iy( dirNames.begin() ) , stop( dirNames.end() ) ;
		iy != stop ;
		++iy
	){
		cout << "deleting dir " << (*iy)->c_str() << endl ;
		delete( *iy ) ;
	}

	// disconnect from the FAM service
	FAMClose( fc ) ;
	delete( fe ) ;
	delete( fc ) ;

	return( 0 ) ;

} // main()

