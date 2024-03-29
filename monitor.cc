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
#include<fstream>
#include<dirent.h>
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
string archive("/home/devraj/Archives/"); //default root archive dir
string request("/home/devraj/Dropbox/poppins-handbag/"); //default req dir
string dirfname("directories"); //file to read directories from if present
string listfname("list.txt"); //filename for list in req dir

// Global program variables
bool runFam = false; //bool for continuing to listen to fam connection
FAMConnection* fc = NULL;
typedef list< string* > DirList;
DirList dirNames; //list for watched directories for clean up
typedef list< FAMRequest* > RequestList;
RequestList requests; //list for fam request objs for clean up

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
void updateList(){
	cout << "Updating the file list" << endl;

	ofstream listf;
	listf.open((request + listfname).c_str());
	if(listf.is_open()){
		listf << "List of archive directory contents:" << endl;
		listf << "note: monitor will update this file automatically, ";
		listf << "do not edit it manually" << endl << endl;
		
		DIR *dir;
		struct dirent *cur;
		dir = opendir(archive.c_str());
		if(dir != NULL){
			while((cur = readdir(dir)) != NULL){
				if(cur->d_name[0] != '.'){
					listf << cur->d_name << endl;
				}
			}
			closedir(dir);
		}
		else {
			cerr << "Error opening archive dir " << archive << endl;
		}
		
		listf.close();
	}
	else {
		cerr << "Error opening list file " << request << listfname << endl;
	}
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
	

	// Register archive and request directories
	if(0 == access(dirfname.c_str(), R_OK)){ // Load from file if available and overwrite defaults
		cout << "Loading directories from file" << endl;
		ifstream dirfile(dirfname.c_str());
		getline(dirfile, archive); //dirfile >> archive;
		getline(dirfile, request); //dirfile >> request;
		dirfile.close();
	}
	cout << "Archive directory:" << endl;
	if(0 != access(archive.c_str(), X_OK)){
		cerr << " Cannot access (x) " << archive << endl;
		return 1;
	}
	else {
		registerDirectory( &archive, false );
	}
	cout << "Request directory:" << endl;
	if(0 != access(request.c_str(), X_OK)){
		cerr << " Cannot access (x) " << request << endl;
		return 1;
	}
	else {
		registerDirectory( &request, false );
	}
		
	// Check to make sure there are connected dirs to monitor
	if( requests.empty() ){
		cerr << "No directories to monitor; exiting with error." << endl;
		return( 1 );
	}
	
	// Ensure list is up-to-date
	updateList();
	
	// Init session vars
	runFam = true; // enable the event loop
	
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
		if( fe->code != 1){ // Ignore clutter of change events (they come in sets)
			cout << "Event in " << *dir << "on file " << fname;
			cout << "\t" << fe->code <<  endl;
		}
		
		if( request == *dir ){ // Event in req dir
			if( fe->code == FAMCreated ){ // In the req dir we only really care about create events
				cout << " Registering file as request: " << fname << endl;
				string archfile = archive + fe->filename;
				if( 0 != access(  archfile.c_str() , F_OK ) ){ // Check existence
					cout << "  Request invalid, file not found: " << archfile << endl;
					ofstream dest((request+fname).c_str());
					dest << "File not found" << endl;
					dest.close();
				}
				else if( 0 != access(  archfile.c_str() , R_OK ) ){ // Check access
					cerr << archfile << " File access problem: " << archfile << endl;
					ofstream dest((request+fname).c_str());
					dest << "File access problem" << endl;
					dest.close();
				}
				else { // All signs go
					cout <<  "  Valid, responding..." << endl;
					ifstream src(archfile.c_str(), ios::binary);
					ofstream dest((request+fname).c_str(), ios::binary);
					dest << src.rdbuf();
					dest.close();
					src.close();
					cout << "  Done." << endl;
				}
			}
		}
		else { // Otherwise this is an archive dir; simply update the list
			if( fe->code == FAMCreated || fe->code == FAMDeleted ){ 
				// Note: only bother updating the list on deleted and created
				//       because nothing else will change the file listing
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

