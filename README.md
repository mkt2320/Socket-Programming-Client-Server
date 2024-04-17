In this client-server project, a client can request a file or a set of files from the server. The server searches for the file/s in its file directory rooted at its ~ and returns the file/files 
requested to the client (or an appropriate message otherwise). 

The server, mirror1 and mirror2 and the client processes must run on different machines/terminals and must communicate using sockets only.

**Multiple clients can connect** 
to the serverw from different machines and can request file/s as per the commands listed below:

=====================================================================================================================================================================================================================

**List of Client Commands:**
 
1) dirlist -a 
=> the serverw24 must return the list of subdirectories/folders(only) under its home directory in the alphabetical order and the client must print the same
eg: **clientw24$ dirlist -a** 

2) dirlist -t 
=> the serverw24 must return the list of subdirectories/folders(only) under its home directories in the order in which they were created (with the oldest created directory listed first) and the client must print the same
eg: **clientw24$ dirlist -t**

3) w24fn filename
=> If the file filename is found in its file directory tree rooted at ~, the serverw24 must return the filename, size(in bytes), date created and file permissions to the client and the client prints the received information on its terminal. 
Note: if the file with the same name exists in multiple folders in the directory tree rooted at ~, the serverw24 sends information pertaining to the first successful search/match of filename Else the client prints “File not found” 
eg: **client24$ w24fs sample.txt**

4) w24fz size1 size2 
=> The serverw24 must return to the client temp.tar.gz that contains all the files in the directory tree rooted at its ~ whose file-size in bytes is >=size1 and <=size2 
=> size1 < = size2 (size1>= 0 and size2>=0) 
=> If none of the files of the specified size are present, the serverw24 sends “No file found” to the client (which is then printed on the client terminal by the client) 
eg: **client24$ w24fz 1240 12450**

5)  w24ft <extension list> //up to 3 different file types 
=> the serverw24 must return temp.tar.gz that contains all the files in its directory tree rooted at ~ belonging to the file type/s listed in the extension list, else the serverw24 sends the message “No file found” to the client (which is printed on the client terminal by the client) 
=> The extension list must have at least one file type and can have up to 3 different file types 
eg: **client24$ w24ft c txt **
eg: **client24$ w24ft jpg bmp pdf**

6) w24fdb date 
=> The serverw24 must return to the client temp.tar.gz that contains all the files in the directory tree rooted at ~ whose date of creation is <=date 
eg: **client24$ w24fdb 2023-01-01**

7) w24fda date 
=> The serverw24 must return to the client temp.tar.gz that contains all the files in the directory tree rooted at ~ whose date of creation is >=date 
eg: **client24$ w24fda 2023-03-31**

8)  quitc
=> The command is transferred to the serverw24 and the client process is terminated

=====================================================================================================================================================================================================================

All files returned from the serverw24 must be stored in a folder named **w24project** in the home directory of the client.

**Alternating Between the serverw24, mirror1 and mirror2 **
The **serverw24**, **mirror1** and **mirror2** (mirror1 and mirror2 are serverw24’s copies possibly with a few additions/changes) are to run on three different machines/terminals. 
=> The first 3 client connections are to be handled by the serverw24. 
=> The next 3 connections (4-6) are to be handled by the mirror1. 
=> The next 3 connections (7-9) are to be handled by mirror2 
=> The remaining client connections are to be handled by the serverw24, mirror1 and mirror2 in an alternating manner- (ex: connection 10 is to be handled by the serverw24, connection 11 by the mirror, connection 12 by mirror2 and so on…) 
