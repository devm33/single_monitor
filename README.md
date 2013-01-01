single_monitor
==============

Script to monitor files added to a single request directory and link them to an archive directory.

A list in the request directory is maintained with all of the files from the archive directory enumerated.

If a file is created in the request directory with the same filename as one in the archive directory the contents of the file from the archive will be copied into the file in the request directory.

If such a file does not exist or there is an error a message will be put in the request file.

The files in the archive directory are also monitored so that on any change the list in the request directory is updated.



Requires FAM
