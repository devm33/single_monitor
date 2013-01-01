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
	extern int errno;
	#include<fam.h>
	#include<unistd.h> // access() , sleep()
	#include<signal.h> // signal()
}
using namespace std;

// Local Environment variables
string request("/home/devraj/Dropbox/poppins-handbag/"); //req dir
string listfname("list.txt"); //filename for list in req dir
string archive("/home/devraj/Archives/"); //root archive dir

// Global program variables
bool runFam = false; //bool for continuing to listen to fam connection
FAMConnection* fc = NULL;
typedef list< string* > DirList;
DirList dirNames; //list for watched directories for clean up
typedef list< FAMRequest* > RequestList;
RequestList requests; //list for fam request objs for clean up
int createsTriggered; //counter for the number of expected create events
bool updatedListEvent; //flag for self-triggered list update

// System error method
void checkStrerror( int error ){
	if( 0 != error ){
		cerr << '\t' << "System error is: " << strerror( errno ) << endl;
	}
	return;
}

// Handle SIGINT (aka control-C): cancel FAM monitors
void sighandler_SIGINT( int sig ){
	cout << endl << "[exit]" << endl;
	runFam = false;
	return;
}

// Register a directory with fam
void registerDirectory( string* dir, bool cleanup ){
	FAMRequest* fr = new FAMRequest();
	cout << "Registering directory \"" << *dir << "\" with FAM" << endl;
	try{
		if( 0 != access( dir->c_str() , F_OK ) ){
			throw( runtime_error( "directory access problem" )  );
		}
		if( 0 != FAMMonitorDirectory( fc , dir->c_str() , fr , dir ) ){
			throw( runtime_error( "problem registering directory with FAM" ) );
		} else { // store for clean up
			if( cleanup ){
				dirNames.push_back( dir );
			}
			requests.push_back( fr );
		}
	}catch( const runtime_error& e ){
		cerr << "Unable to access directory." << endl;
		checkStrerror( errno );
		delete( fr );
		delete( dir );
	}
	return;
}

// Update the file list in the req dir
//TODO change this method to use native cpp file handling rather than 
//     system calls (to be cross os)
void updateList(){
	cout << "Updating the file list" << endl;
	//TODO add note to this file about not changing it manually
	
	string update( "ls " );
	update = update + archive + " > " + request + listfname;
	system( update.c_str() );
	
	updatedListEvent = true;
}

int main( const int argc , const char** argv ){
	// Logging
	system( "date" );	
	
	// Register a function to handle SIGINT signals
	if( SIG_ERR == signal( SIGINT , sighandler_SIGINT ) ){
		cerr << "Error: Unable to set signal handler for SIGINT (Control-C)" << endl;
		return( 1 );
	}
	
	// FAM vars
	fc = new FAMConnection();
	FAMEvent* fe = new FAMEvent(); // Event data is put here.  This pointer will be reused for each event.

	// Init FAM connection
	if( 0 != FAMOpen( fc ) ){
		cerr << "Unable to open FAM connection." << endl;
		cerr << "(Hint: make sure FAM (via xinetd) and portmapper are running.)" << endl;
		checkStrerror( errno );
		return( 1 );
	}
	
	//TODO check for access to these directories:
	// Register the archive dir
	cout << "Archive directory:" << endl;
	registerDirectory( &archive, false );
	
	// Register the request dir
	cout << "Request directory:" << endl;
	registerDirectory( &request, false );
	
	// Check to make sure there are connected dirs to monitor
	if( requests.empty() ){
		cerr << "No directories to monitor; exiting with error." << endl;
		return( 1 );
	}
	
	// Init session vars
	createsTriggered = 0;
	runFam = true; // enable the event loop
	
	// Ensure list in req dir is accurate since we were offline
	updateList();	
	
	// Begin monitor loop
	while( runFam ){
		if( 1 != FAMPending( fc ) ){ // No event pending, wait & continue
			sleep( 2 );
			continue;
		}
		int rc = FAMNextEvent( fc , fe );
		if( 1 != rc ){
			cerr << "FAMNextEvent returned error" << endl;
			continue;
		}
		string* dir = reinterpret_cast< string* >( fe->userdata );
		string fname = fe->filename;
		cout << "Event in " << *dir << "on file " << fname << endl;
		
		if( request == *dir ){ // Event in req dir
			if(fname == listfname) {
				if( updatedListEvent ) { // Self-triggered
					updatedListEvent = false;
				}
				else { // Hey! no changing this file
					updateList();
				}
			}
			else if( fe->code == FAMCreated ){ // In the req dir we only really care about create events
				if(createsTriggered > 0) { // Skip self-triggered events
					createsTriggered -= 1;
					cout << " Skipping self-triggered create. " << createsTriggered << " selfies left: " << endl;
				}
				else { // Respond to request
					cout << " Registering file as request" << endl;
					string archfile = archive + fe->filename;
					if( 0 != access(  archfile.c_str() , F_OK ) ){
						cerr << archfile << " file access problem";
					}
					
					//TODO change this to native cpp
					system( ("cp -f " + archfile + " " + request + fname).c_str() );
					
					createsTriggered += 1;
					cout << createsTriggered << " creates triggered" << endl;
				}
			}
		}
		else { // Otherwise this is an archive dir; simply update the list
			if( fe->code == FAMChanged || fe->code == FAMCreated || fe->code == FAMDeleted ){ //TODO Is this check necessary?
				updateList();
			}
		}
	} //while( runFam )

	// - - - - - - - - - - - - - - - - - - - -
	// cleanup

	for(
		RequestList::const_iterator ix( requests.begin() ) , stop( requests.end() );
		ix != stop;
		++ix
	){
		cout << "[Cancelling monitor for FAMRequest " << (*ix)->reqnum << "]" << endl;
		FAMCancelMonitor( fc , *ix );
		delete( *ix );
	}

	for(
		DirList::const_iterator iy( dirNames.begin() ) , stop( dirNames.end() );
		iy != stop;
		++iy
	){
		cout << "deleting dir " << (*iy)->c_str() << endl;
		delete( *iy );
	}
	
	// disconnect from the FAM service
	FAMClose( fc );
	delete( fe );
	delete( fc );

	return( 0 );

} // main()

