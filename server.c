// required libraries
#define _GNU_SOURCE
#define _XOPEN_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <libgen.h>
#include <dirent.h>
#include <limits.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <fnmatch.h>

// defining constants as per the requirement
#define serPortCon 6312                   // port number globally defined
#define maxClient 30                      // maximum number of clients that can be connected
#define bufferLen 20000                   // buffer length defined as 20000 bytes
#define maxBuffSize 1024                  // maximum buffer size defined as 1024 bytes

int isErrFound = 0;                        // error flag to check if any error is found or not in the program

// function to send created tar file
void sendTmpTar(int clientSocket)
{
    struct stat statFl;
    // checking if the file exists, if yes then open the file
    if (stat("temp.tar.gz", &statFl) == 0)
    {
        // opening the file
        FILE *filePtr = fopen("temp.tar.gz", "rb");
        if (filePtr == NULL)
        {
            // printing error message if file not found
            printf("Sorry! Failed to open file\n");
            return;
        }
        // to get the size of the file
        fseek(filePtr, 0, SEEK_END);
        size_t fileLen = ftell(filePtr);
        fseek(filePtr, 0, SEEK_SET);

        // sending the file with success message
        send(clientSocket, "TEMP_TAR_READY", 15, 0);
        send(clientSocket, &fileLen, sizeof(size_t), 0);

        char buffStr[bufferLen]; // buffer to store the file content
        size_t remainingBytes = fileLen; // to store the remaining bytes

        // forking the process
        pid_t forkPid = fork();

        // checking if the forking is successful or not
        if (forkPid == -1)
        {
            // printing error message if forking failed
            printf("Forking failed\n");
            // closing the file
            fclose(filePtr);
            return;
        }

        // if the forking is successful - child process
        if (forkPid == 0)
        {
            // closing the standard output
            close(STDOUT_FILENO);
            // duplicating the client socket
            dup2(clientSocket, STDOUT_FILENO);

            // reading the file content and sending it to the client
            while (remainingBytes > 0)
            {
                // Here we are sending the file content in chunks of 1024 bytes
                size_t bytesToBeSent = (remainingBytes < sizeof(buffStr)) ? remainingBytes : sizeof(buffStr);
                // so after that wea re reading the file content
                size_t bytesToBeRead = fread(buffStr, 1, bytesToBeSent, filePtr);
                // if fread returns 0 then break the loop
                if (bytesToBeRead == 0)
                {
                    break;
                }
                // STDOUT_FILENO is the file descriptor for standard output
                // so we are writing the file content to the client socket using the file descriptor
                write(STDOUT_FILENO, buffStr, bytesToBeRead);
                // remaining bytes are updated
                remainingBytes -= bytesToBeRead;
            }
            // close the file
            fclose(filePtr);
            // exit the child process
            exit(EXIT_SUCCESS);
        }
        else
        {
            // waiting for the child process to complete
            int isStFlag; // declare a variable to store the status flag
            // waiting for the child process to complete and storing the status flag in isStFlag
            waitpid(forkPid, &isStFlag, 0);
            // closing the file
            fclose(filePtr);
        }
    }
    else
    {
        // sending error message to the client if the file is not found
        send(clientSocket, "TEMP_TAR_NOT_FOUND", 18, 0);
    }
}

// function to execute command using fork and exec system calls
void performExec(const char *command)
{
    // Fork a child process to execute the command
    pid_t forkPid = fork();

    // if forking failed
    if (forkPid <0)
    {
        // Print error message if forking failed
        printf("Forking Error\n");
        // and exit the program
        exit(EXIT_FAILURE);
    }
    // child process to execute the command
    if (forkPid > 0)
    {
        int status;

        // Wait for the child process to complete its execution
        waitpid(forkPid, &status, 0);
        // Check if the child process has exited successfully usiong macros
        if (!WIFEXITED(status) || WEXITSTATUS(status) != EXIT_SUCCESS)
        {
            // exit the program if the child process has not exited successfully
            exit(EXIT_FAILURE);
        }
    }
    else
    {
        // Execute the command using execlp system call
        execlp("/bin/bash", "/bin/bash", "-c", command, NULL);
        // Print error message if execlp failed
        perror("execlp");
        // exit the program
        exit(EXIT_FAILURE);
    }
}


// function to list all the directories in the home directory (dirlist -a)
void performDirlistA(int clientSocket)
{
    DIR *folder;                        // declare a pointer to the directory
    struct dirent **listOfFolders;      // declare a pointer to the directory entry
    int scanDirRet;                     // declare a variable to store the return value of scandir

    char *rootDir = getenv("HOME");     // get the home directory path

    // if the home directory is not set
    if (rootDir == NULL) 
    {
        printf("Sorry! HOME environment variable not set\n");
        exit(EXIT_FAILURE);
    }
    // scanning the directory using scandir and storing the result in listOfFolders
    scanDirRet = scandir(rootDir, &listOfFolders, NULL, alphasort);
    // if scanning the directory failed
    if (scanDirRet == -1)
    {
        // print error message
        printf("Sorry! Failed to scan directory\n");
        // and exit the program
        exit(EXIT_FAILURE);
    }

    // declaeing the buffer to store the list of directories
    char dirListToBeSent[maxBuffSize * scanDirRet];
    // to initialize the buffer with 0
    memset(dirListToBeSent, 0, sizeof(dirListToBeSent));
    // declaration to store the size of the buffer
    int sizeOfdirListToBeSent = 0;

    // Iterating through the list of directories
    for (int i = 0; i < scanDirRet; i++)
    {
        // excluding the hidden files and directories if exists
        if (listOfFolders[i]->d_type == DT_DIR && listOfFolders[i]->d_name[0] != '.' && strcmp(listOfFolders[i]->d_name, ".") != 0 && strcmp(listOfFolders[i]->d_name, "..") != 0)
        {
            // Writing name of the directory to the buffer and updating the size of the buffer
            int length = snprintf(dirListToBeSent + sizeOfdirListToBeSent, sizeof(dirListToBeSent) - sizeOfdirListToBeSent, "%s\n", listOfFolders[i]->d_name);
            // if the length is less than 0 or greater than the size of the buffer then print error message and exit the program
            if (length < 0 || length >= sizeof(dirListToBeSent) - sizeOfdirListToBeSent)
            {
                // Printing the suitable error message
                printf("Sorry! Failed to write into buffer, Insuffiecient Space\n");
                // and exit the program
                exit(EXIT_FAILURE);
            }
            sizeOfdirListToBeSent += length;
        }
        // free the memory allocated to the directory entry
        free(listOfFolders[i]);
    }
    // free the memory allocated to the list of directories
    free(listOfFolders);

    // sending the list of directories to the client using send() system call
    int bytesSent = send(clientSocket, dirListToBeSent, sizeOfdirListToBeSent, 0);
    // if the bytes sent is -1 then print error message
    if (bytesSent == -1)
    {
        // print error message
       printf("Failed to send data to client\n");
    }
    else
    {
        // else print success message
        printf("Data sent to client successfully\n");
    }
}

// function to list all the directories in the home directory (dirlist -t)
void performDirlistT(int clientSocket)
{
    DIR *folder;                        // declare a pointer to the directory
    struct dirent *dirEntry;            // declare a pointer to the directory entry
    struct stat stBuff;                 // declare a structure to store the stst of the directory

    char *rootDir = getenv("HOME");     // get the home directory path

    // if the home directory is not set
    if (rootDir == NULL)
    {
        // Printing the suitable error message
        printf("Sorry! HOME environment variable not set\n");
        // and exit the program
        exit(EXIT_FAILURE);
    }

    // Open the root directory
    folder = opendir(rootDir);
    // if we are unable to open the directory
    if (folder == NULL)
    {
        // print appropreate error message 
        printf("Sorry! Failed to open directory\n");
        // and exit the program
        exit(EXIT_FAILURE);
    }

    // Now creating a structure to store the directory name and creation time
    struct
    {
        char *name;             // to store the name of the directory
        time_t creation_time;   // to store the creation time of the directory
    } *dirArr = NULL;           // declare a pointer to the structure

    // declare a variable to store the count of directories
    int dirCnt = 0;

    // Read all entries in the directory
    while ((dirEntry = readdir(folder)) != NULL)
    {
        // here we are excluding the hidden files and directories
        if (dirEntry->d_name[0] == '.')
            continue;
        // here we are creating the complete path of the directory using the root directory and the directory name and mallocating the memory for the complete path
        char *completeDirPath = malloc(strlen(rootDir) + strlen(dirEntry->d_name) + 2);
        // if the memory allocation failed then print error message and exit the program
        if (completeDirPath == NULL)
        {
            // print appropriate error message
            perror("malloc");
            // and exit the program
            exit(EXIT_FAILURE);
        }
        // here we are creating the complete path of the directory using the root directory and the directory name
        sprintf(completeDirPath, "%s/%s", rootDir, dirEntry->d_name);
        // here we are getting the status i.e. the stat of the directory
        if (stat(completeDirPath, &stBuff) == -1)
        {
            // if the stat failed then print error message 
            perror("stat");
            // and exit the program
            exit(EXIT_FAILURE);
        }

        // if the it is a directory
        if (S_ISDIR(stBuff.st_mode))
        {
            // reallocating the memory for the directory array
            dirArr = realloc(dirArr, (dirCnt + 1) * sizeof(*dirArr));
            // if the memory allocation failed then print error message and exit the program
            if (dirArr == NULL)
            {
                // print appropriate error message
                perror("realloc");
                // and exit the program
                exit(EXIT_FAILURE);
            }
            // Now we are using strdup to copy the string
            dirArr[dirCnt].name = strdup(dirEntry->d_name);
            // if the strdup failed then print error message and exit the program
            if (dirArr[dirCnt].name == NULL)
            {
                // print appropriate error message
                perror("strdup");
                // and exit the program
                exit(EXIT_FAILURE);
            }

            char command[1024]; // declare a buffer to store the command

            // here we are getting the creation time of the directory from stat
            sprintf(command, "stat -c %%W \"%s\"", completeDirPath);
            // open the command in read mode and if error then print error message
            FILE *fp = popen(command, "r");
            if (fp == NULL)
            {
                printf("Failed to execute command\n");
                // and exit the program
                exit(EXIT_FAILURE);
            }

            // creation time of the directory
            if (fscanf(fp, "%ld", &(dirArr[dirCnt].creation_time)) != 1)
            {
                // if the fscanf failed then print error message and exit the program
                printf("Failed to read output of command\n");
                // and exit the program
                exit(EXIT_FAILURE);
            }
            // close the file
            pclose(fp);
            // increment the count of directories
            dirCnt++;
        }
        // free the memory allocated to the complete path
        free(completeDirPath);
    }
    // and then close the directory
    closedir(folder);

    // For sorting the directories based on the creation time
    for (int i = 0; i < dirCnt - 1; i++)
    {
        // nested loop to sort the directories based on the creation time
        for (int j = 0; j < dirCnt - i - 1; j++)
        {
            // if the creation time of the directory is greater than the creation time of the next directory
            if (dirArr[j].creation_time > dirArr[j + 1].creation_time)
            {
                // swapping and sorting the directories based on the creation time
                char *temp = dirArr[j].name;
                time_t temp_time = dirArr[j].creation_time;
                
                dirArr[j].name = dirArr[j + 1].name;
                dirArr[j].creation_time = dirArr[j + 1].creation_time;

                dirArr[j + 1].name = temp;
                dirArr[j + 1].creation_time = temp_time;
            }
        }
    }

    // declare a variable to store the size of the buffer
    size_t sizeOfdirListToBeSent = 0;
    // Iterating through the sorted list of directories
    for (int i = 0; i < dirCnt; i++)
    {
        // updating the size of the buffer
        sizeOfdirListToBeSent += strlen(dirArr[i].name) + 1;
    }

    // declare a buffer to store the list of directories using malloc
    char *dirListToBeSent = malloc(sizeOfdirListToBeSent);
    // if not able to allocate memory then 
    if (dirListToBeSent == NULL)
    {
        // print error message 
        perror("malloc");
        // and exit the program
        exit(EXIT_FAILURE);
    }
    dirListToBeSent[0] = '\0'; // set the first character of the buffer to NULL

    for (int i = 0; i < dirCnt; i++)
    {
        // Concatenating the directory name to the buffer
        strcat(dirListToBeSent, dirArr[i].name);
        // Concatenating the new line character to the buffer
        strcat(dirListToBeSent, "\n");
        // free the memory allocated to the directory name
        free(dirArr[i].name);
    }

    // sending the list of directories to the client using send() system call
    int bytesSent = send(clientSocket, dirListToBeSent, sizeOfdirListToBeSent, 0); 
    // if the bytes sent is -1 then print error message
    if (bytesSent == -1)
    {
        // print error message
        perror("*Failing to send data to client*");
    }
    else
    {
        // else print success message
        printf("Sent data to client successfully.\n");
    }

    // free the memory allocated to the list of directories
    free(dirListToBeSent);
    // free the memory allocated to the directory array
    free(dirArr);
}

// function to find the file in the home directory based on the file name (w24fn)
void performW24fn(int clientSocket, const char *fileName)
{
    int pipeFd[2];                  // declare a pipe file descriptor
    if (pipe(pipeFd) == -1)
    {
        // print error message if pipe creation failed
        printf("Failed to create pipe\n");
        // and exit the program
        return;
    }

    // Fork a child process to execute the command
    pid_t forkPid = fork();
    // if forking failed
    if (forkPid < 0)
    {
        // print error message if forking failed
        printf("Forking Error\n");
        // close the pipe file descriptors 
        close(pipeFd[0]); // close the read end of the pipe
        close(pipeFd[1]); // close the write end of the pipe
        return;
    }

    // if the child process
    if (forkPid == 0)
    {
        // close the read end of the pipe
        close(pipeFd[0]);

        // Redirect stdout to the write end of the pipe
        if (dup2(pipeFd[1], STDOUT_FILENO) == -1)
        {
            // print error message if redirection failed
            printf("Failed to redirect stdout\n");
            close(pipeFd[1]); // close the write end of the pipe    
            exit(EXIT_FAILURE); // exit the program
        }

        // Execute the command to find the file in the home directory based on the file name
        execlp("find", "find", getenv("HOME"), "-name", fileName, "-printf", "%p %s %m %TY-%Tm-%Td %TH:%TM:%TS\\n", NULL);

        // print error message if execlp failed
        perror("*Failing of 'execlp' Operation*");
        // close the write end of the pipe
        close(pipeFd[1]);
        // exit the program
        exit(EXIT_FAILURE);
    }
    else
    {
        // close the write end of the pipe
        close(pipeFd[1]);
        char opBuff[bufferLen]; // declare a buffer to store the output of the command
        // clear the buffer
        memset(opBuff, 0, sizeof(opBuff));
        // read the output of the command from the read end of the pipe
        ssize_t bytesToBeRead = read(pipeFd[0], opBuff, sizeof(opBuff) - 1);

        // if the bytes to be read is -1 then print error message
        if (bytesToBeRead == -1)
        {
            close(pipeFd[0]); // close the read end of the pipe
            return;
        }
        // close the read end of the pipe
        close(pipeFd[0]);

        // if the bytes to be read is greater than 0
        if (bytesToBeRead > 0)
        {
            char *pathOfFile = strtok(opBuff, " ");    // get the path of the file
            // if the path of the file is not NULL
            if (pathOfFile)
            {
                char *sizeOFFile = strtok(NULL, " ");           // to get the size of the file
                char *permissionOFFile = strtok(NULL, " ");     // to get the permission of the file
                char *dateOFFile = strtok(NULL, " ");           // to get the date of the file
                char *timeOFFile = strtok(NULL, "\n");          // to get the time of the file

                // Convert permission from octal to symbolic
                // char var4Permission[11]; // Permissions are represented as -rwxrwxrwx (9 characters) plus NULL terminator
                // mode_t mode = strtol(permissionOFFile, NULL, 8);
                // snprintf(var4Permission, sizeof(var4Permission), "%s%s%s%s%s%s%s%s%s",
                //          (mode & S_IRUSR) ? "r" : "-", (mode & S_IWUSR) ? "w" : "-",
                //          (mode & S_IXUSR) ? "x" : "-", (mode & S_IRGRP) ? "r" : "-",
                //          (mode & S_IWGRP) ? "w" : "-", (mode & S_IXGRP) ? "x" : "-",
                //          (mode & S_IROTH) ? "r" : "-", (mode & S_IWOTH) ? "w" : "-",
                //          (mode & S_IXOTH) ? "x" : "-");

                // Construct response
                char rspnBuff[bufferLen]; // declare a buffer to store the response
                // construct the response message
                snprintf(rspnBuff, sizeof(rspnBuff), "File found in the root directory:\nPath: %s\nSize: %s bytes\nPermissions: %s\nDate: %s Time: %s\n", pathOfFile, sizeOFFile, permissionOFFile, dateOFFile, timeOFFile);

                // send the response message to the client
                send(clientSocket, rspnBuff, strlen(rspnBuff), 0);
            }
            else
            {
                // declare a buffer to store the response message
                const char *notFoundBuff = "File not found\n"; 
                // send the response message to the client
                send(clientSocket, notFoundBuff, strlen(notFoundBuff), 0);
            }
        }
        else
        {
            // print error message
            perror("*Failing of 'read' Operation*");
            // declare a buffer to store the response message 
            const char *notFoundBuff = "File not found\n"; 
            // send the response message to the client
            send(clientSocket, notFoundBuff, strlen(notFoundBuff), 0);
        }
    }
}

// function to find the file in the home directory based on the file size (w24fz)
void performW24fz(int clientSocket, const char *parsedCmd)
{
    // declaring the variables to store the size of the file
    long long sizeArg1, sizeArg2;

    // parsing the command to get the size of the file
    int var4ParsedCmndRes = sscanf(parsedCmd, "w24fz %lld %lld %*[-u]", &sizeArg1, &sizeArg2);

    // making a temporary directory to store the files
    char tmpTarDir[] = "temp_files";
    mkdir(tmpTarDir, 0700);

    // forking the process
    pid_t forkPid = fork();
    // if forking failed
    if (forkPid < 0)
    {
        // print error message if forking failed
        printf("Forking Error\n");
        return;
    }
    // child process
    if (forkPid == 0)
    {
        // redirecting the standard output to the client socket
        dup2(clientSocket, STDOUT_FILENO);
        // array of extensions
        const char *extnArr[] = {"txt", "xml" , "sh" , "jpeg", "cpp", "db" "pdf", "c", "jpg", "png", "jpeg"};
        int extnPermitted = sizeof(extnArr) / sizeof(extnArr[0]);

        // command to find the files based on the size and extension
        char cmd[bufferLen];
        snprintf(cmd, sizeof(cmd), "find $HOME -type f \\( \\( -size +%lldc -a -size -%lldc \\) -o \\( -size %lldc -o -size %lldc \\) \\) \\( -name '*.%s' -o -name '*.%s' -o -name '*.%s' -o -name '*.%s' -o -name '*.%s' -o -name '*.%s' \\) -exec cp {} %s/ \\;",
                 sizeArg1, sizeArg2, sizeArg1, sizeArg2,
                 extnArr[0], extnArr[1], extnArr[2],
                 extnArr[3], extnArr[4], extnArr[5],
                 tmpTarDir);

        // execute the command
        performExec(cmd);

        // check if the files are copied
        int isCopied = 0;
        DIR *tmpDirPtr = opendir(tmpTarDir); // open the temporary directory
        if (tmpDirPtr)
        {
            // read the files in the temporary directory
            struct dirent *ptr;
            while ((ptr = readdir(tmpDirPtr)) != NULL)
            {
                // if this is a file regular file
                if (ptr->d_type == DT_REG)
                {
                    // setting the flag to 1
                    isCopied = 1;
                    break;
                }
            }
            closedir(tmpDirPtr);
        }

        if (isCopied)
        {
            // if successfully copied then create a tar file
            char createTar[bufferLen];
            // generate the command to create the tar file
            snprintf(createTar, sizeof(createTar), "tar czf temp.tar.gz %s", tmpTarDir);
            // execute the command
            performExec(createTar);
        }

        // generate the command to remove the temporary directory
        char removeTar[bufferLen];
        snprintf(removeTar, sizeof(removeTar), "rm -r %s", tmpTarDir);
        // execute the command
        performExec(removeTar);
        // exit the child process
        exit(isCopied ? EXIT_SUCCESS : EXIT_FAILURE);
    }
    else
    {
        // parent process
        //waiting for a child process to complete its execution
        int isStFlag;
        waitpid(forkPid, &isStFlag, 0);

        if (WIFEXITED(isStFlag) && WEXITSTATUS(isStFlag) == EXIT_SUCCESS)
        {
            // if the child process has exited successfully then send the success message to the client
            const char *rspMsg = "Tar file created successfully within the given size range\n";
            send(clientSocket, rspMsg, strlen(rspMsg), 0);
        }
        else
        {
            // if child process has not exited successfully then send the error message to the client
            isErrFound = 1;
            const char *rspMsg = "No Files Found Under the range of the sizes given by client";
            send(clientSocket, rspMsg, strlen(rspMsg), 0);
        }
    }
}

// function to find the file in the home directory based on the file extension (w24ft)
void performW24ft(int clientSocket, const char *parsedCmd)
{
    // declare a buffer to store the extension name
    char extnName[bufferLen];
    sscanf(parsedCmd, "w24ft %[^\n] %*[-u]", extnName);
    // declare a pointer to the substring
    char *subStr = strtok(extnName, " ");
    int extnCnt = 0;

    // make a temporary directory to store the files
    char tmpTarDir[] = "temp_files";
    mkdir(tmpTarDir, 0700);

    // forking the process
    pid_t forkPid = fork();

    if (forkPid <0)
    {
        printf("Forking Error\n");
        return;
    }
    // child process
    if (forkPid == 0)
    {
        // redirecting the standard output to the client socket
        dup2(clientSocket, STDOUT_FILENO);

        // traverse through the extensions
        while (subStr != NULL && extnCnt < 4)
        {
            char cmd[bufferLen];
            // generate the command to find the files based on the extension using find command
            // find command is used to search for files in a directory hierarchy
            snprintf(cmd, sizeof(cmd), "find $HOME -name '*.%s' -type f -exec cp {} %s/ \\;", subStr, tmpTarDir);
            // printf("Command: %s\n", cmd);
            performExec(cmd);
            // increment the extension count
            extnCnt++;
            subStr = strtok(NULL, " ");
        }

        // check if the files are copied
        int isCopied = 0;
        DIR *tmpDirPtr = opendir(tmpTarDir);
        if (tmpDirPtr)
        {
            // read the files in the temporary directory
            struct dirent *ptr;
            while ((ptr = readdir(tmpDirPtr)) != NULL)
            {
                // if this is a file regular file
                if (ptr->d_type == DT_REG)
                {
                    isCopied = 1;
                    break;
                }
            }
            closedir(tmpDirPtr);
        }
        // if the files are copied then create a tar file
        if (isCopied)
        {
            // generate the command to create the tar file
            char createTar[bufferLen];
            snprintf(createTar, sizeof(createTar), "tar czf temp.tar.gz %s", tmpTarDir);
            performExec(createTar);
        }

        // Clears temp. direc.
        char removeTar[bufferLen];
        snprintf(removeTar, sizeof(removeTar), "rm -r %s", tmpTarDir);
        performExec(removeTar);
        // exit the child process
        exit(isCopied ? EXIT_SUCCESS : EXIT_FAILURE);
    }
    else
    {
        // parent process
        //waiting for a child process to complete its execution
        int isStFlag;
        waitpid(forkPid, &isStFlag, 0);

        if (WIFEXITED(isStFlag) && WEXITSTATUS(isStFlag) == EXIT_SUCCESS)
        {
            // if the child process has exited successfully then send the success message to the client
            const char *rspMsg = "Tar file created successfully for the given extensions\n";
            send(clientSocket, rspMsg, strlen(rspMsg), 0);
        }
        else
        {   
            // if child process has not exited successfully then send the error message to the client
            isErrFound = 1;
            const char *rspMsg = "No Files Found for the given extensions";
            send(clientSocket, rspMsg, strlen(rspMsg), 0);
        }
    }
}

// function to find the file in the home directory based on the file date (w24fd)
void performW24fdb(int clientSocket, const char *parsedCmd)
{
    // declare a buffer to store the date
    char date[11];
    // declare a buffer to store the command
    char searchFiles[bufferLen];
    // parse the command to get the date
    sscanf(parsedCmd, "w24fdb %s", date);

    // using snprintf to generate the command to find the files based on the date
    snprintf(searchFiles, bufferLen,
             "find $HOME -type f ! -newerct '%s 23:59:59' -print0",
             date);

    // forking the process
    pid_t forkPid = fork();

    if (forkPid < 0)
    {
        perror("Error forking process");
        send(clientSocket, "Error forking process", 21, 0);
        return;
    }
    // child process
    if (forkPid == 0)
    {
        // redirecting the standard output to the client socket
        if (dup2(clientSocket, STDOUT_FILENO) == -1)
        {
            perror("Failed to redirect stdout");
            exit(EXIT_FAILURE);
        }
        // generate the command to find the files based on the date
        char resultBuffer[256];
        snprintf(searchFiles, bufferLen,
                 "find $HOME -type f ! -newerct '%s 23:59:59' -print0 | wc -c",
                 date);
        
        // open the pipe to execute the command
        FILE *pipe = popen(searchFiles, "r");
        if (!pipe)
        {
            perror("Failed to check file count");
            exit(EXIT_FAILURE);
        }
        // read the output of the command
        fgets(resultBuffer, sizeof(resultBuffer), pipe);
        pclose(pipe);
        // convert the result buffer to integer
        int fileCnt = atoi(resultBuffer);
        if (fileCnt == 0)
        {
            // if no files found then send the message to the client
            send(clientSocket, "No Files found before specified date", 36, 0);
            exit(EXIT_SUCCESS);
        }
        else
        {
            // generate the command to create the tar file
            snprintf(searchFiles, bufferLen,
                     "find $HOME -type f ! -newerct '%s 23:59:59' -print0 | tar -czf temp.tar.gz --null --files-from=-",
                     date);
            int ret = execlp("/bin/bash", "/bin/bash", "-c", searchFiles, NULL);
            // if the command execution failed then print error message
            if (ret == -1)
            {
                perror("Failed to create tar file");
                send(clientSocket, "Failed to create tar file", 25, 0);
                exit(EXIT_FAILURE);
            }
            else
            {
                send(clientSocket, "TEMP_TAR_READY", 15, 0);
                exit(EXIT_SUCCESS);
            }
        }
    }
    else
    {
        // parent process
        //waiting for a child process to complete its execution
        int status;

        waitpid(forkPid, &status, 0);
        if (!WIFEXITED(status) || WEXITSTATUS(status) != EXIT_SUCCESS)
        {
            // if the child process has not exited successfully then send the error message to the client
            send(clientSocket, "Command execution failed", 24, 0);
        }
        else
        {
            // if the child process has exited successfully then send the success message to the client
            char *resMsg = "Tar file created successfully for the date<= specified date\n";
            send(clientSocket, resMsg, strlen(resMsg), 0);
            // send(clientSocket, "TEMP_TAR_READY", 15, 0);
        }
    }
}

// function to find the file in the home directory based on the file date (w24fd)
void performW24fda(int clientSocket, const char *parsedCmd)
{
    // declare a buffer to store the date
    char date[11];
    char searchFiles[bufferLen];
    // parse the command to get the date
    sscanf(parsedCmd, "w24fda %s", date);
    // forking the process
    pid_t forkPid = fork();

    if (forkPid <0)
    {
        perror("Error forking process");
        return;
    }
    // child process
    if (forkPid == 0)
    {
        // redirecting the standard output to the client socket
        dup2(clientSocket, STDOUT_FILENO);

        // generate the command to find the files based on the date
        snprintf(searchFiles, bufferLen,
                 "find $HOME -type f -newerct '%s 00:00:00' -exec tar czf temp.tar.gz '{}' +",
                 date);
        // execute the command
        execlp("/bin/bash", "/bin/bash", "-c", searchFiles, NULL);

        perror("execlp");
        exit(EXIT_FAILURE);
    }
    else
    {
        // parent process
        //waiting for a child process to complete its execution
        int status;
        waitpid(forkPid, &status, 0);

        if (WIFEXITED(status) && WEXITSTATUS(status) == EXIT_SUCCESS)
        {
            // if the child process has exited successfully then send the success message to the client
            const char *resMsg = "Tar file created successfully for the date>= specified date\n";
            send(clientSocket, resMsg, strlen(resMsg), 0);
        }
        else
        {
            // if child process has not exited successfully then send the error message to the client
            const char *resMsg = "Failed to process files.";
            send(clientSocket, resMsg, strlen(resMsg), 0);
        }
    }
}

// validate client request and perform the requested operation
void crequest(int clientSocket)
{
    // declare a buffer to store the command sent by the client
    char buffStr[bufferLen];
    ssize_t recievedBytes;
    bool isQuitFlag = false;

    // loop to receive the command from the client
    while (!isQuitFlag)
    {
        isErrFound = 0;
        // clear the buffer
        memset(buffStr, 0, sizeof(buffStr));
        // receive the command from the client
        recievedBytes = recv(clientSocket, buffStr, sizeof(buffStr), 0);
        if (recievedBytes <= 0)
        {
            printf("Connection Closed by Client\n");
            break;
        }

        //printing the command sent by client
        printf("Received Command from Client: %s\n", buffStr);
        // check the command sent by the client
        if ((strncmp(buffStr, "temp_tar", 8)) == 0 && isErrFound == 0)
        {
            // if the command is temp_tar then send the temporary tar file to the client
            sendTmpTar(clientSocket);
        }
        else if (strncmp(buffStr, "dirlist -a", 10) == 0)
        {
            // if the command is dirlist -a then list all the directories in the home directory
            performDirlistA(clientSocket);
        }

        else if (strncmp(buffStr, "dirlist -t", 10) == 0)
        {
            // if the command is dirlist -t then list all the directories in the home directory based on the creation time
            performDirlistT(clientSocket);
        }
        else if (strncmp(buffStr, "w24fn", 5) == 0)
        {
            // if the command is w24fn then find the file in the home directory based on the file name
            char fileName[bufferLen];
            sscanf(buffStr, "w24fn %s", fileName);
            performW24fn(clientSocket, fileName);
        }
        else if (strncmp(buffStr, "w24fz", 5) == 0)
        {
            // if the command is w24fz then find the file in the home directory based on the file size
            performW24fz(clientSocket, buffStr);
        }
        else if (strncmp(buffStr, "w24ft", 5) == 0)
        {
            // if the command is w24ft then find the file in the home directory based on the file extension
            performW24ft(clientSocket, buffStr);
        }
        else if (strncmp(buffStr, "w24fdb", 6) == 0)
        {
            // if the command is w24fdb then find the file in the home directory based on the file date
            performW24fdb(clientSocket, buffStr);
        }
        else if (strncmp(buffStr, "w24fda", 6) == 0)
        {
            // if the command is w24fda then find the file in the home directory based on the file date
            performW24fda(clientSocket, buffStr);
        }
        else if (strncmp(buffStr, "quitc", 4) == 0)
        {
            // if the command is quitc then quit the client
            isQuitFlag = true;
        }
        else
        {
            // if the command is invalid then send the invalid command message to the client
            const char *rspMsg = "Recieved Invalid Command\n";
            send(clientSocket, rspMsg, strlen(rspMsg), 0);
            // Need to add memset here
        }
    }
    close(clientSocket);
}

// main function
int main()
{
    // declare the variables to store the server port number and the maximum number of clients
    int serverSocket; 
    int clientSocket;
    // isUsed is used to set the socket option
    int isUsed = 1;
    int clientCnt = 0;
    // declare the variables to store the server port number and the maximum number of clients
    struct sockaddr_in serverAddrPort;
    struct sockaddr_in clientAddrPort;
    socklen_t clientAddrPortSize = sizeof(clientAddrPort);
    
    // create a socket
    if ((serverSocket = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        // print error message if socket creation failed
        printf("Sorry! Socket Creation Failed\n");
        exit(EXIT_FAILURE);
    }
    // set the socket option
    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &isUsed, sizeof(int)) == -1)
    {
        // print error message if setsockopt failed
        printf("Sorry! setsockopt Failed\n");
        exit(EXIT_FAILURE);
    }

    // set the server address and port
    serverAddrPort.sin_family = AF_INET;
    serverAddrPort.sin_port = htons(serPortCon);
    serverAddrPort.sin_addr.s_addr = INADDR_ANY;

    // bind the server address and port
    if (bind(serverSocket, (struct sockaddr *)&serverAddrPort, sizeof(serverAddrPort)) == -1)
    {
        // print error message if binding failed
        printf("Sorry! Binding Failed\n");
        exit(EXIT_FAILURE);
    }

    // listen for the client request
    if (listen(serverSocket, maxClient) == -1)
    {
        // print error message if listening failed
        printf("Sorry! Listening Failed\n");
        exit(EXIT_FAILURE);
    }
    // print the success message
    printf("Server Established Successfully and waiting for clinet request on port: %d...\n", serPortCon);

    // loop to accept the client request
    while (true)
    {
        // print the prompt
        printf("\n");
        printf("\033[1;32mserverw24$ \033[0m ");
        // accept the client request
        if ((clientSocket = accept(serverSocket, (struct sockaddr *)&clientAddrPort, &clientAddrPortSize)) == -1)
        {
            // print error message if accepting the client request failed
            printf("Sorry! Failed to Accept the Client's Connection\n");
            continue;
        }
        // increment the client count
        clientCnt++;
        // redirect the client based on the client count
        int redirectClientFlag = 0;
        // if the client count is greater than 9 then redirect the client
        if (clientCnt > 9)
        {
            // redirect the client based on the client count
            redirectClientFlag = clientCnt % 3;
        }
        // send the client count to the client
        send(clientSocket, &clientCnt, sizeof(clientCnt), 0);

        // if the client count is less than or equal to 3 then accept the client request
        if (clientCnt <= 3 || redirectClientFlag == 1)
        {
            // send the server name to the client
            send(clientSocket, "SERVER", 6, 0);
            printf("New Client Connected %s:%d\n", inet_ntoa(clientAddrPort.sin_addr), ntohs(clientAddrPort.sin_port));
            printf("Client: %d connected...\n", clientCnt);

            // fork the process
            pid_t forkPid = fork();
            if (forkPid < 0)
            {
                printf("forking failed\n");
                continue;
            }
            // if the child process
            else if (forkPid == 0)
            {
                // close the server socket
                close(serverSocket);
                crequest(clientSocket);
                exit(0);
            }
            else
            {
                //parent process
                // close the client socket
                close(clientSocket);
            }
        }
        // if the client count is greater than 3 and less than or equal to 6 then redirect the client to MIRROR1
        else if ((clientCnt > 3 && clientCnt <= 6) || redirectClientFlag == 2)
        {
            // send the client to MIRROR1
            printf("Client %d is redirected to MIRROR1...\n",clientCnt);
            send(clientSocket, "MIRROR1", 7, 0);
        }
        // if the client count is greater than 6 and less than or equal to 9 then redirect the client to MIRROR2
        else if ((clientCnt > 6 && clientCnt <= 9) || redirectClientFlag == 0)
        {
            // send the client to MIRROR2
            printf("Client %d is redirected to MIRROR2...\n",clientCnt);
            send(clientSocket, "MIRROR2", 7, 0);
        }
    }
    // close the server socket
    close(serverSocket);
    return 0;
}