// required libraries
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <string.h>
#include <regex.h>
#include <time.h>
#include <stdbool.h>

// defining the constants
#define clientIP "127.0.0.1"    // client IP global variable
#define bufferLen 20000         // buffer length global variable
#define serPortCon 6312         // server port connection global variable
#define mirror1PortCon 6313     // mirror1 port connection global variable
#define mirror2PortCon 6314     // mirror2 port connection global variable

int sFlag = 0;          // sFlag flag - for size
int isNotFound = 0;     // isNotFound flag - for file not found
int isExtFlag = 0;      // isExtFlag flag - for extension
int isFileFound = 0;    // isFileFound flag - for file found
int isDateFlag = 0;     // isDateFlag flag - for future date

// this function will check if the w24project folder exists or not in the home directory.. if it is not present then it will create the folder
// create a folder in the home directory using the mkdir function with the permissions 0777 i.e read, write and execute permissions
void createW24projectFolder(const char *homeDir) 
{
    // Construct the absolute path to the W24project folder
    char destPath[1024];
    snprintf(destPath, sizeof(destPath), "%s/W24project", homeDir);

    // Check if the folder exists
    if (access(destPath, F_OK) != 0) 
    {
        // Folder doesn't exist, create it
        if (mkdir(destPath, 0777) == -1) 
        {
            perror("failed to create W24project folder");
            return;
        }
        printf("W24project created in home directory\n");
    }
}

// function to recieve the temp.tar.gz file from the server
// this will store it in the W24project folder
//using snprintf to construct the absolute path to the temp.tar.gz file within the W24project folder
// open the file in write binary mode using the constructed absolute path
// recieve and write the tar file content and close the file
void recTmpTar(int serverSocket) 
{
    // Get the home directory path from the environment variable HOME
    const char *homeDir = getenv("HOME");

    // Check if the HOME environment variable exists
    if (homeDir == NULL)
    {
        perror("Failed to get HOME environment variable");
        return;
    }

    // Create W24project folder if it doesn't exist
    createW24projectFolder(homeDir);

    // Construct the absolute path to the temp.tar.gz file within the W24project folder
    char filePath[1024];
    snprintf(filePath, sizeof(filePath), "%s/W24project/temp.tar.gz", homeDir);

    // Open the file in write binary mode using the constructed absolute path
    FILE *filePtr = fopen(filePath, "wb");
    if (filePtr == NULL) 
    {
        perror("Failed to open temp.tar.gz file");
        return;
    }

    // Receive and write the tar file content
    char buffStr[bufferLen];
    ssize_t receivedBytes;

    // Receive acknowledgment from the server
    memset(buffStr, 0, sizeof(buffStr));
    receivedBytes = recv(serverSocket, buffStr, sizeof(buffStr), 0);
    if (receivedBytes <= 0) 
    {
        perror("Failed to receive acknowledgment from server");
        fclose(filePtr);
        return;
    }

    // Check if the acknowledgment is correct
    if (strcmp(buffStr, "TEMP_TAR_READY") != 0) 
    {
        printf("Failed to receive acknowledgment TEMP_TAR_READY from server\n");
        fclose(filePtr);
        return;
    }

    // Receive the length of the file from the server
    size_t fileLen;
    receivedBytes = recv(serverSocket, &fileLen, sizeof(size_t), 0);
    if (receivedBytes <= 0) 
    {
        perror("Failed to receive file length from server");
        fclose(filePtr);
        return;
    }

    // Receive and write the file content
    size_t remainingBytes = fileLen;
    while (remainingBytes > 0) 
    {
        memset(buffStr, 0, sizeof(buffStr));
        receivedBytes = recv(serverSocket, buffStr, sizeof(buffStr), 0);
        if (receivedBytes <= 0) 
        {
            perror("Failed to receive file content from server");
            fclose(filePtr);
            return;
        }
        fwrite(buffStr, 1, receivedBytes, filePtr);
        remainingBytes -= receivedBytes;
    }

    // Close the file
    fclose(filePtr);
    printf("Successfully received and stored temp.tar file in W24project folder\n");
}

// function to check the correct date format
// using regular expression we have performed the date validation
// a valid date should be in the format YYYY-MM-DD
// rPattern is the regular expression pattern for the date format. it will be used to check the date format using regcomp and regexec functions
int validateDate(const char *date)
{   
    // defining the variables
    regex_t r;
    int result;
    char rPattern[] = "^[0-9]{4}-(0[1-9]|1[0-2])-([0-2][0-9]|3[0-1])$"; // date format
    // checking if the regular expression is processed successfully
    result = regcomp(&r, rPattern, REG_EXTENDED);
    if (result)
    {
        fprintf(stderr, "**Failed to process the regular expression**\n");
        return 0;
    }
    // using the regexec function to check the date format
    if (regexec(&r, date, 0, NULL, 0))
    {
        // printing the message if the date format is incorrect
        regfree(&r);
        return 0;
    }
    // freeing the memory
    regfree(&r);
    return 1;
}

// function to check if the given date is in the future
// using the time.h library we have checked if the given date is in the future or not
// we have used the strptime function to convert the given date to the struct and then converted it to seconds using mktime function
// we have also used the localtime function to get the current date
// if the given date is greater than the current date then it is in the future
int isFutureDate(char *date)
{
    // getting the current date
    time_t now = time(NULL);

    // setting the date format
    const char *correctDate = "%Y-%m-%d";
    struct tm givenDate = {0};

    // converting the current date to the struct and the given date to the struct
    struct tm *curentDate = localtime(&now);
    strptime(date, correctDate, &givenDate);
    // converting the given date to seconds
    time_t dateSeconds = mktime(&givenDate);

    // checking if the given date is in the future
    if (dateSeconds > now)
    {   
        isDateFlag = 1; // setting the flag if the given date is in the future
        return 1;
    }
    // returning 0 if the given date is not in the future
    else
    {
        return 0;
    }
}

// function to validate the dirlist command
// checking if the command is dirlist -a or dirlist -t using strncmp built-in function to compare the strings
int validateDirlist(const char *parsedCmd)
{   
    // checking if the command is dirlist -a
    if (strncmp(parsedCmd, "dirlist -a", 10) == 0)
    {
        return 1; // returning 1 if the command is dirlist -a
    }
    // checking if the command is dirlist -t
    else if (strncmp(parsedCmd, "dirlist -t", 10) == 0)
    {
        return 1; // returning 1 if the command is dirlist -t
    }
}

// function to validate the w24fn command
// checking if the command is w24fn <filename> using strncmp built-in function to compare the strings
// checking if the filename is valid using the correct extensions array
//if filename is valid then return 1 else return 0
int validateW24fn(const char *parsedCmd)
{
    // checking if the command is w24fn
    if (strncmp(parsedCmd, "w24fn", 5) != 0)
    {
        return 0; // returning 0 if the command is not w24fn
    }
    // checking if the command is w24fn <filename>
    const char *fileNamePtr = parsedCmd + 5;
    while (*fileNamePtr == ' ')
    {
        ++fileNamePtr;
    }
    // checking if the filename is valid
    const char *extnArr[] = {"txt", "pdf", "png", "jpg", "jpeg", "c", "sh", "xml"};
    // looping through the correct extensions array
    for (int k = 0; k < sizeof(extnArr) / sizeof(extnArr[0]); ++k)
    {
        if (strstr(fileNamePtr, extnArr[k]))
        {
            // while the filename is not null
            while (*fileNamePtr != '\0')
            {
                if (*fileNamePtr == ' ')
                {
                    return 0; // returning 0 if the filename is invalid
                }
                // incrementing the pointer
                ++fileNamePtr;
            }
            return 1; // returning 1 if the filename is valid
        }
    }
    return 0; // returning 0 if the filename is invalid
}


// function to validate the w24fz command
// checking if the command is w24fz <size1> <size2> using strncmp built-in function to compare the strings
// checking if the size1 is less than or equal to size2
//if size1 is less than or equal to size2 then return 1 else return 0
int validateW24fz(const char *parsedCmd)
{
    // checking if the command is w24fz
    if (strncmp(parsedCmd, "w24fz", 5) != 0)
    {
        return 0;
    }

    // checking if the command is w24fz <size1> <size2>
    // getting the parameters of the command
    const char *args = parsedCmd + 5;
    char arg1[16], arg2[16], opCmd[3];
    // getting the final result of the command
    int ans = sscanf(args, "%s %s %2s", arg1, arg2, opCmd);
    if (ans < 2)
    {
        return 0;
    }
    // parameter validation - size1 and size2 should be integers
    for (int k = 0; k < strlen(arg2); ++k)
    {
        if (!isdigit(arg2[k]))
        {
            return 0;
        }
    }
    // parameter validation - size1 and size2 should be integers
    for (int k = 0; k < strlen(arg1); ++k)
    {
        if (!isdigit(arg1[k]))
        {
            return 0;
        }
    }
    
    // checking if size1 is greater than size2
    if (atoi(arg2) < atoi(arg1))
    {
        sFlag = 1;
    }
    // returning 1 if the size1 is less than or equal to size2
    return atoi(arg2) >= atoi(arg1);
}

// function to validate the w24ft command
// checking if the command is w24ft <ext1> <ext2> <ext3> using strncmp built-in function to compare the strings
// checking if the extension is valid
//if extension is valid then return 1 else return 0
int validateW24ft(const char *parsedCmd)
{
    // checking if the command is w24ft
    if (strncmp(parsedCmd, "w24ft", 5) != 0)
    {
        return 0;
    }
    // checking if the command is w24ft <ext1> <ext2> <ext3>
    parsedCmd += 5;

    while (*parsedCmd == ' ')
    {
        parsedCmd++;
    }

    // checking if the extension is valid
    int isExtnPresent = 0;
    int extnCnt = 0;

    // looping through the extensions given in the command
    while (*parsedCmd != '\0')
    {
        // if the extension is not null
        if (*parsedCmd != '\0')
        {
            extnCnt++; // incrementing the total extensions

            // while the extension is not null and not a space
            while (*parsedCmd != '\0' && *parsedCmd != ' ')
            {
                // if the extension is not a valid extension
                if (*parsedCmd == '-')
                {
                    // if the extension is not included
                    while (*parsedCmd != '\0' && *parsedCmd != ' ')
                        parsedCmd++; // incrementing the pointer
                }
                // if the extension is valid
                else
                {
                    isExtnPresent = 1; // setting the flag

                    // while the extension is not null and not a space
                    while (*parsedCmd != '\0' && *parsedCmd != ' ')
                    {
                        parsedCmd++; // incrementing the pointer
                    }
                }
            }
            // if the extension is not null
            while (*parsedCmd == ' ')
                parsedCmd++;
        }
    }
    // if the extension is not included
    if (extnCnt < 1 || extnCnt > 3)
    {
        // setting the flag
        isExtFlag = 1;
        return 0;
    }
    return 1;   // returning 1 if the extension is included
}


// function to validate the w24f command
// checking if the command is w24f <date> using strncmp built-in function to compare the strings
// we have used the validateDate function to check if the date is in the correct format
// we have used the isFutureDate function to check if the date is in the future
//if date is valid then return 1 else return 0
int validateW24fdb(const char *parsedCmd)
{
    // checking if the command is w24f
    if (strncmp(parsedCmd, "w24fdb", 6) != 0)
    {
        return 0;
    }
    return 1;

    // checking if the command is w24f <date>
    const char *args = parsedCmd + 6;
    char date[16], opArr[2];
    // setting the optArray to null
    opArr[0] = '\0';

    // getting the final result of the command
    int ans = sscanf(args, "%s %2s", date, opArr);
    if (ans < 1)
    {
        // returning 0 if the final result is less than 1
        return 0;
    }

    // checking if the date is in the future
    if (!validateDate(date))
    {
        return 0;
    }
    // returning 1 if the date is in the past
    return 1;
}

// function to validate the w24fda command
// checking if the command is w24fda <date> using strncmp built-in function to compare the strings
// we have used the validateDate function to check if the date is in the correct format
// we have used the isFutureDate function to check if the date is in the future
//if date is valid then return 1 else return 0
int validateW24fda(const char *parsedCmd)
{
    // checking if the command is w24fda
    if (strncmp(parsedCmd, "w24fda", 6) != 0)
    {
        return 0;
    }
    // checking if the command is w24fda <date>
    const char *args = parsedCmd + 6;
    char date[16], opArr[2];

    // setting the optArray to null
    opArr[0] = '\0';

    // getting the final result of the command
    int ans = sscanf(args, "%s %2s", date, opArr);
    // returning 0 if the final result is less than 1
    if (ans < 1)
    {
        return 0;
    }
    // checking if the date is in the future
    if (isFutureDate(date))
    {
        return 0;
    }
    // checking if the date is in the correct format
    if (!validateDate(date))
    {
        return 0;
    }
    // returning 1 if the date is in the past
    return 1;
}

// function to validate the quitc command
// checking if the command is quitc using strncmp built-in function to compare the strings
int validateQuitc(const char *parsedCmd)
{
    // checking if the command is quitc
    return (strcmp(parsedCmd, "quitc") == 0);
}

// connection to the server from professor's code
// main function 
// in this function we have created the client socket and connected it to the server
// sending a command to the server, mirror1 or mirror2 and waiting for the response
int main()
{
    int clientSocket;                           // client socket
    struct sockaddr_in serverAddrPort;          // server address port
    struct sockaddr_in mirror1AddrPort;         // mirror1 address port
    struct sockaddr_in mirror2AddrPort;         // mirror2 address port
    char buffStr[bufferLen];                   
    ssize_t recievedBytes;                      
    bool ischeckTmp = false;                    
    int clientCnt = 0;                          

    // creating the client socket
    clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    serverAddrPort.sin_family = AF_INET;
    serverAddrPort.sin_port = htons(serPortCon);
    serverAddrPort.sin_addr.s_addr = inet_addr(clientIP);

    // setting the mirror1 address ports
    mirror1AddrPort.sin_family = AF_INET;
    mirror1AddrPort.sin_port = htons(mirror1PortCon);
    mirror1AddrPort.sin_addr.s_addr = inet_addr(clientIP);

    // setting the mirror2 address ports
    mirror2AddrPort.sin_family = AF_INET;
    mirror2AddrPort.sin_port = htons(mirror2PortCon);
    mirror2AddrPort.sin_addr.s_addr = inet_addr(clientIP);

    // connecting the client socket to the server
    connect(clientSocket, (struct sockaddr *)&serverAddrPort, sizeof(serverAddrPort));
    // recieving the client count
    recv(clientSocket, &clientCnt, sizeof(clientCnt), 0);

    memset(buffStr, 0, sizeof(buffStr));        // clearing the buffer

    // recieving the message from the server
    recv(clientSocket, buffStr, sizeof(buffStr), 0);

    // if the message is "MIRROR1"
    if (strcmp(buffStr, "MIRROR1") == 0)
    {
        close(clientSocket);
        // creating the client socket
        clientSocket = socket(AF_INET, SOCK_STREAM, 0);
        // connecting the client socket to the mirror1
        connect(clientSocket, (struct sockaddr *)&mirror1AddrPort, sizeof(mirror1AddrPort));
        // sending the client count
        send(clientSocket, &clientCnt, sizeof(clientCnt), 0);
        printf("Mirror1 connected successfully\n");
    }
    // if the message is "MIRROR2"
    else if (strcmp(buffStr, "MIRROR2") == 0)
    {
        // creating the client socket
        clientSocket = socket(AF_INET, SOCK_STREAM, 0);
        // connecting the client socket to the mirror2
        connect(clientSocket, (struct sockaddr *)&mirror2AddrPort, sizeof(mirror2AddrPort));
        // sending the client count
        send(clientSocket, &clientCnt, sizeof(clientCnt), 0);
        printf("Mirror2 connected successfully\n");
    }
    // if the message is "SERVER"
    else if (strcmp(buffStr, "SERVER") == 0)
    {
        // sending the client count
        printf("Server connected successfully\n");
    }

    // while the client is connected
    while (1)
    {
        // clearing the buffer
        ischeckTmp = false;
        isNotFound = 0;
        printf("\n");
        printf("\033[1;32mclientw24$ \033[0m ");
        // printf("\nclient24$ ");

        // getting the command from the user
        fgets(buffStr, sizeof(buffStr), stdin);

        size_t strLen = strlen(buffStr);
        // if the last character is a new line
        if (strLen > 0 && buffStr[strLen - 1] == '\n')
        {
            // setting the last character to null
            buffStr[strLen - 1] = '\0';
        }
        // if the command is quitc
        if (validateQuitc(buffStr))
        {
            // sending the command to the server
            send(clientSocket, buffStr, strlen(buffStr), 0);
            // clearing the buffer
            memset(buffStr, 0, sizeof(buffStr));
            break;
        }
        // checking if the given command ia valid or not and retruning the error message
        else if ((validateDirlist(buffStr) != 1) && (!validateW24fz(buffStr)) && !validateW24fn(buffStr) && !validateW24ft(buffStr) && !validateW24fdb(buffStr) && !validateW24fda(buffStr))
        {
            // if the size is greater than size2
            if (sFlag == 1)
            {
                // printing the error message
                printf("size1 should be <= size2\n");
                sFlag = 0;
                continue;
            }
            // if the date is in the future
            else if (isDateFlag == 1)
            {
                // printing the error message
                printf("Date should be in the past\n");
                isDateFlag = 0;
                continue;
            }
            // if the extensions are more than 3
            else if (isExtFlag == 1)
            {
                // printing the error message
                printf("Upto 3 extensions are allowed\n");
                isExtFlag = 0;
                continue;
            }

            // If givan command is not valid
            printf("Please Enter a Valild command in below format:\n");
            printf("dirlist -a\n");
            printf("dirlist -t\n");
            printf("w24fn <filename>\n");
            printf("w24fz <size1> <size2>\n");
            printf("w24ft <ext1> <ext2> <ext3>\n");
            printf("w24fda <date>\n");
            printf("w24fdb <date>\n");
            printf("quitc\n");
            continue;
        }
        // if validateW24fz or validateW24ft is true
        if (validateW24fz(buffStr) || validateW24ft(buffStr) || validateW24fda(buffStr) || validateW24fdb(buffStr))
        {
            // sending the command to the server
            ischeckTmp = true;
        }

        // sending the command to the server
        send(clientSocket, buffStr, strlen(buffStr), 0);
        // clearing the buffer
        memset(buffStr, 0, sizeof(buffStr));
        // recieving the response from the server
        recievedBytes = recv(clientSocket, buffStr, sizeof(buffStr), 0);
        // checking if the recieved bytes are less than or equal to 0 and handling the error
        if (recievedBytes <= 0)
        {
            perror("**Failing of recvCommand**");
            break;
        }
        // printing the response from the server
        printf("Response from Server: \n%s\n", buffStr);
        // checking if the response is "No Files Found Under the range of the sizes given by client"
        if (strncmp(buffStr, "No Files Found Under the range of the sizes given by client", strlen(buffStr)) == 0)
        {
            isNotFound = 1; // setting the flag if the file is not found
        }
        // checking if the response is "No Files Found for the given extensions"
        else if (strncmp(buffStr, "Incorrect type of Cmnd Given", sizeof(buffStr)) == 0)
        {
            isNotFound = 1; // setting the flag if the file is not found
        }
        // checking if the response is "No Files Found for the given extensions"
        else if (strncmp(buffStr, "No Files Found for the given extensions", sizeof(buffStr)) == 0)
        {
            isNotFound = 1; // setting the flag if the file is not found
        }
        // checking if the response is "No files found before the specified date"
        else if (strncmp(buffStr, "No files found before specified date", sizeof(buffStr)) == 0)
        {
            isNotFound = 1; // setting the flag if the file is not found
        }
        else if(isDateFlag == 1)
        {
            isNotFound = 1; // setting the flag
        }
        // if check temp flag is true and file is not found
        else if (ischeckTmp && !isNotFound)
        {
            // sending the command to the server
            send(clientSocket, "temp_tar", 8, 0);
            // recieving the temp.tar file from the server
            recTmpTar(clientSocket);
        }
    }
    // closing the client socket
    close(clientSocket);
    return 0;
}