#include <iostream>
#include <string>
#include <vector>
#include <bits/stdc++.h>
#include <sys/socket.h> 
#include <netdb.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/poll.h>
#include <sys/wait.h>
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>

using namespace std;

/**
 *  ftp.cpp
 * 
 *  File-Transfer-Protocol that supports commands for
 *  open, cd subdir, ls, get file, put file, close, quit.
 *  
 *  To run: ./ftp ftp.tripod.com
 *  Username: css432
 *  Password: *******
 * 
 *  Commands:
 *      - open 21     : opens the connection to the server if executed without hostname
 *      - cd subdir   : changes the current directory to specified directory
 *      - ls          : lists all the files in the current directory  
 *      - get file    : gets the specified file from the server
 *      - put         : prompts to enter filename and puts the file onto the server
 *      - close       : closes the connection to the server, but does not exit program
 *      - quit        : closes connection if still active and exits the programs
 * 
 *  Kylun Robbins
 *  05/18/20
 */

// Function Defintions
vector<string> splitInput(string input);
void serverResponse();
void open(int ipPort);
void passwordVerification();
int poll();
void close();
void ls();
bool pasv();
void cd(vector<string> inputArray);
void put();
bool verifyFile(string& file);
void setTypeI();
void get(vector<string> inputArray);

// Data
const int BUFF_SIZE = 8192;
char buffer[BUFF_SIZE];         // Buffer for communication from server to client
char* hostname;                 // To record the servername from command-line argument
bool isLoggedIn = false;        // To keep track if user is logged in or not
int clientSD;                   // To keep track of client socket
struct hostent *host;
int passiveSD;                  // To keep track of server socket in pasv()
int pid;                        // For fork in pasv()

/**
 *  main(int argc, char* argv[])
 *
 *  Driver function for program.
 *
 *  @param argc Number of command-line arguments
 *  @param argv Values of command-line arguments
 */
int main(int argc, char* argv[])
{
    // Check arguments
    // If two arguments are given then hostname is second
    if(argc == 2)
    {
        hostname = argv[1];             // Store hostname
        open(21);                       // Call open with default port 21
    } 
    // Run client but do not connect to server
    else
    {
        // Store value of server in hostname
        char tripod[] = "ftp.tripod.com";
        hostname = tripod;
    }

    // User input loop
    while(true)
    {
        cout << "---> ";                    // "Terminal Line"

        string input;                       
        getline(cin, input);                // Read input from user

        if(input.empty())                   // If no input, just hit enter
        {
            continue;                       // do nothing, reset loop
        }

        // Split input into array
        vector<string> inputArray = splitInput(input);

        string command = inputArray[0];     // commmand from input

        if(command == "open")
        {
            if(isLoggedIn)
            {
                cout << "Already logged in." << endl;
            }
            else if(inputArray.size() < 2)
            {
                cout << "Not enough arguments. Must specify port" << endl;
                cout << "Format: open [IP_PORT]" << endl;
            }
            else
            {   
                // Convert string command line input to int for open()
                open(stoi(inputArray[1]));
            }
        }
        else if(command == "cd")
        {
            if(isLoggedIn)
            {
                cd(inputArray);
            }
            else
            {
                cout << "Not connected to server." << endl;
            }
        }
        else if(command == "ls")
        {
            if(isLoggedIn)
            {
                pasv();
                // cout << "Completed Passive Connection" << endl;
                ls();
            }
            else
            {
                cout << "Not connected to server." << endl;
            }
        }
        else if(command == "get")
        {
            if(isLoggedIn)
            {
                get(inputArray);
            }
            else
            {
                cout << "Not connected to server." << endl;
            }
        }
        else if(command == "put")
        {
            if(isLoggedIn)
            {
                put();
            }
            else
            {
                cout << "Not connected to server." << endl;
            }
        }
        else if(command == "close")
        {
            // If logged in 
            if(isLoggedIn)
            {
                close();                    // Close connection
                isLoggedIn = false;         // Change log in flag
            }
            else
            {
                cout << "Not connected to server." << endl;
            }
        }
        else if(command == "quit")
        {
            // If logged in 
            if(isLoggedIn)
            {
                close();                    // Close connection
                isLoggedIn = false;         // Change log in flag
            }
        
            break;                          // Quit the ftp program
        }
        else
        {
            cout << "Invalid command." << endl;
            cout << "For a list of valid commands type: " << endl;
            cout << "help" << endl;            
        }
    }
}


/**
 *  splitInput(sting input)
 *
 *  Splits the argument string by spaces and adds them into 
 *  a vector that is returned.
 * 
 *  https://www.geeksforgeeks.org/split-a-sentence-into-words-in-cpp/
 *
 *  @param input String that is split by space delimiter 
 *  @return a vector of strings with words as elements
 */
vector<string> splitInput(string input)
{
    vector<string> array;
    istringstream ss(input);

    // Traverse through all words in string
    do
    {
        // Store the word
        string word;
        ss >> word;

        array.push_back(word);

    } while(ss);
    
    return array;
}


/**
 *  response()
 * 
 *  Retrieves the response from the server into
 *  the variable buffer
 * 
 */
void serverResponse()
{
    bzero(buffer, sizeof(buffer));          // Zero out buffer before communication

    read(clientSD, buffer, sizeof(buffer)); // Read the response into buffer
}

/**
 *  open(int ipPort)
 * 
 *  Establishes a TCP connection to the IP
 *  on the port.
 * 
 *  @param ipPort Port at which connection will be established with server
 */
void open(int ipPort)
{

    // Connecting to socket
    clientSD = socket(AF_INET, SOCK_STREAM, 0);

    // Error check
    if(clientSD < 0)
    {
        cout << "Could not connect to server." << endl;
        exit(0);
    }

    host = gethostbyname(hostname);

    struct sockaddr_in socketAddress;
    bzero((char *)&socketAddress, sizeof(socketAddress));
    socketAddress.sin_family = AF_INET;
    socketAddress.sin_addr.s_addr = inet_addr(inet_ntoa( *(struct in_addr*) (*host->h_addr_list) ));
    socketAddress.sin_port = htons(ipPort);

    int connected = connect(clientSD, (sockaddr*)&socketAddress, sizeof(socketAddress));

    // Error checking
    if(connected < 0)
    {
        cout << "Cannot connect to server." << endl;
        exit(0);
    }

    serverResponse();                           // Get response from server
    cout << buffer;                             // and print them out

    // Get user's username
    cout << "---> Username (" << hostname << ":" << getenv("USER") << "): ";
    char username[BUFF_SIZE];
    cin >> username;

    // Preparing char array to send to server
    char command[BUFF_SIZE];
    strcpy(command, "USER ");
    strcat(command, username);
    strcat(command, "\r\n");

    // Send username to server
    write(clientSD, (char*)&command, strlen(command));

    // Get response from server after username is sent
    serverResponse();
    cout << buffer;

    // Get password
    passwordVerification();

    // Poll the socket
    while(poll() > 0)
    {
        // While poll return > 0
        // Get response and print it
        serverResponse();
        cout << buffer;
    }

    // Error Check
    if(poll() == -1)
    {
        cout << "Socket cannot be polled." << endl;
    }

    isLoggedIn = true;                      // Update log in flag
}


/**
 *  poll() 
 *
 *  Polls the socket to check if there is more
 *  data to be read from server 
 * 
 *  Modified code from:
 *  http://courses.washington.edu/css432/dimpsey/lab/project_faq.html
 * 
 *  @return an integer > 0 if there is more data, else if no more data
 */
int poll()
{
    struct pollfd ufds;
    ufds.fd = clientSD;                     // a socket descriptor to exmaine for read
    ufds.events = POLLIN;                   // check if this sd is ready to read
    ufds.revents = 0;                       // simply zero-initialized
    return poll( &ufds, 1, 1000 );          // poll this socket for 1000msec (=1sec)
}

/**
 *  passwordVerification()
 * 
 *  Gets password input from user, keeps prompting 
 *  until correct password is entered
 * 
 */ 
void passwordVerification()
{
    // Loop until correct password is entered
    while(true)
    {
        cout << "---> Enter Password: ";
        char userPassword[BUFF_SIZE];
        cin >> userPassword;

        // Preparing char array to send to server
        char command[BUFF_SIZE];
        strcpy(command, "PASS ");
        strcat(command, userPassword);
        strcat(command, "\r\n");

        // Send to server
        write(clientSD, (char*)&command, strlen(command));

        // Get response from server
        serverResponse();
        cout << buffer;

        string errorCode = "501";                   // Error code for bad password

        // If response is not incorrect password error
        if(!strstr(buffer, errorCode.c_str()))      
        {
            // Break, because correct password was entered
            break;                                  
        }
    }
    cin.ignore();
}


/**
 *  close()
 * 
 *  Closes the connection to the server.
 */
void close()
{
    // Preparing char array to send to server
    char command[BUFF_SIZE];
    strcpy(command, "QUIT");
    strcat(command, "\r\n");      

    // Send QUIT command to server
    write(clientSD, (char*)&command, strlen(command));

    serverResponse();
    cout << buffer;

    // Shutdown socket
    shutdown(clientSD, SHUT_WR);            
}

/**
 *  ls()
 * 
 *  Lists all the files in the current directory.
 *  Utilizes fork to have parent send 'LIST' command
 *  to server, while child process collects names of
 *  all files in folder.
 */
void ls()
{
    string list;

    pid = fork();

    // Error 
    if(pid < 0)
    {
        cout << "Fork failed." << endl;
    }
    // Parent process
    else if(pid > 0)
    {
        char command[BUFF_SIZE];
        strcpy(command, "LIST");
        strcat(command, "\r\n");
        write(clientSD, (char*)&command, strlen(command));

        // cout << "Waiting for child" << endl;
        wait(NULL);   
    }
    // Child Process
    else
    {
        // Zero out the buffer
        bzero(buffer, sizeof(buffer));

        while(read(passiveSD, (char*)&buffer, sizeof(buffer)) > 0)
        {
            list.append(buffer);
        }

        // Poll the socket
        while(poll() > 0)
        {
            // While poll return > 0
            // Get response and print it
            serverResponse();
            cout << buffer;
        }

        cout << list << endl;

        // serverResponse();
        // cout << buffer;
        // cout << "Exiting child process" << endl;
        exit(0);
    }

    close(passiveSD);
}

/**
 *  pasv() 
 * 
 *  Establishes a passive connection to the server.
 *  
 *  @return bool representing if the connection was succesful
 */ 
bool pasv()
{
    // Prepare to send PASV to server
    char command[BUFF_SIZE];
    strcpy(command, "PASV");
    strcat(command, "\r\n");

    // Send PASV
    write(clientSD, (char*)&command, strlen(command));

    serverResponse();
    cout << buffer;

    int x, y, z, a, x1, y2;         // For response in pasv()

    // Record response values 
    sscanf(buffer, "227 Entering Passive Mode (%d,%d,%d,%d,%d,%d)", &x, &y, &z, &a, &x1, &y2);

    // Find port the server is listening on
    int port = (x1 * 256) + y2;

    // Connect to server using port
    struct sockaddr_in socketAddress;
    bzero((char *)&socketAddress, sizeof(socketAddress));
    socketAddress.sin_family = AF_INET;
    socketAddress.sin_addr.s_addr = inet_addr(inet_ntoa( *(struct in_addr*) (*host->h_addr_list) ));
    socketAddress.sin_port = htons(port);

    passiveSD = socket(AF_INET, SOCK_STREAM, 0);

    // Error check
    if(passiveSD < 0)
    {
        cout << "Could not connect to server." << endl;
        return false;
    }

    int connected = connect(passiveSD, (sockaddr*)&socketAddress, sizeof(socketAddress));

    // Error checking
    if(connected < 0)
    {
        cout << "Cannot connect to server." << endl;
        return false;
    }

    return true;
}

/**
 *  cd(vector<string> inputArray)
 * 
 *  Changes the current directory based on
 *  command-line input
 * 
 *  @param inputArray Array of command-line arguments
 */ 
void cd(vector<string> inputArray) 
{
    // If there are less than 2 arguments, the subdir wasn't given
    // Was going to do == 2, but have bug with 3rd index being a new line
    if(inputArray.size() < 2)
    {
        cout << "Incorrect arguments. Format: 'cd subdir'" << endl;
        return;
    }

    // Prepare to send command and subdir to server
    char command[BUFF_SIZE];
    strcpy(command, "CWD ");
    strcat(command, inputArray[1].c_str());
    strcat(command, "\r\n");

    // Send command and subdir to server
    write(clientSD, (char*)&command, strlen(command));
    
    // Get and print server response
    // serverResponse();
    // cout << buffer;
    
    while(poll() > 0)
    {
        // While poll return > 0
        // Get response and print it
        serverResponse();
        cout << buffer;
    }
}

/**
 *  put()
 * 
 *  Transfers the specified file to the
 *  server.
 */
void put()
{
    // Get input for local file
    cout << "---> [Local File]: ";
    string localFileName;
    getline(cin, localFileName);

    // Get input for remote file
    cout << "---> [Remote File]: ";
    string remoteFileName;
    getline(cin, remoteFileName);

    // Verify that the local file actually exists
    if(!verifyFile(localFileName))
    {
        cout << localFileName << " cannot be found." << endl;
        return;
    }

    setTypeI();                     // Set data type to 'I' (binary)

    // Establish a passive connection to server
    if(!pasv())
    {
        return;                     // If connection fails, return
    }

    // Fork a process so the child can write to server
    pid = fork();

    // Error
    if(pid < 0)
    {
        cout << "Fork failed." << endl;
        return;
    }
    // Parent process
    else if(pid > 0)
    {
        // Prepare command 
        char command[BUFF_SIZE];
        strcpy(command, "STOR ");
        strcat(command, remoteFileName.c_str());
        strcat(command, "\r\n");
        
        // Write command to server
        write(clientSD, (char*)&command, strlen(command));
        
        // Wait for child process to finish
        wait(NULL);
    }
    // Child process
    else
    {
        /*
        // Open local file
        ifstream file;
        file.open(localFileName.c_str());

        // While there is more data to read
        while(true)
        {
            if(file.read(buffer, sizeof(buffer)))
            {
                // Write data to server
                write(passiveSD, buffer, sizeof(buffer));  
            }
            else
            {
                // Break when reading is done
                break;
            }
        }
        file.close();
        */

        // Refactored to include O_RDONLY

        // Open file with O_RDONLY option
        int file = open(localFileName.c_str(), O_RDONLY);

        while(true)
        {
            // Record number of bytes read
            int numRead = read(file, buffer, sizeof(buffer));

            // If no bytes are read, exit loop
            if(numRead == 0)
            {
                break;
            }
            
            // Write bytes read to server
            write(passiveSD, buffer, numRead);
        }

        close(file);
        exit(0);
    }  

    close(passiveSD);               // Close passive connection

    // Poll the socket
    while(poll() > 0)
    {
        // While poll return > 0
        // Get response and print it
        serverResponse();
        cout << buffer;
    }
} 

/**
 *  verifyFile(string& file)
 * 
 *  Verifies if the argument file exists or not
 * 
 *  Reference: 
 *  https://stackoverflow.com/questions/1647557/ifstream-how-to-tell-if-specified-file-doesnt-exist
 * 
 *  @param file A string representation of the file to verify
 *  @return A boolean representing if the file exists or not
 */
bool verifyFile(string &file)
{
    ifstream myFile(file.c_str());
    if(myFile.fail())
    {
        return false;
    }
    else
    {
        return true;
    }
}

/**
 *  setTypeI()
 * 
 *  Sets the data type to binary for sending
 *  data to the server
 */ 
void setTypeI()
{
    // Prepare and send command to server
    char command[BUFF_SIZE];
    strcpy(command, "TYPE I");
    strcat(command, "\r\n");
    write(clientSD, (char*)&command, strlen(command));

    // Get server response and print
    serverResponse();
    cout << buffer;
}


/**
 *  get()
 * 
 *  Gets a file from the server and 
 *  stores a copy on the client system
 * 
 *  @param inputArray Vector of command line arguments
 */ 
void get(vector<string> inputArray)
{
    if(inputArray.size() < 2)
    {
        cout << "Incorrect arguments. Format: 'get filename'" << endl;
        return;
    }

    setTypeI();                     // Set data type to 'I' (binary)

    // Esatblish a passive connection to server
    if(!pasv())
    {
        return;
    }

    // Fork so child process can read from server
    pid = fork();   

    // Error
    if(pid < 0)
    {
        cout << "Fork failed." << endl;
        return;
    }
    // Parent process
    else if(pid > 0)
    {
        // Prepare command 
        char command[BUFF_SIZE];
        strcpy(command, "RETR ");
        // inputArray[1] holds the filename from command line argument
        strcat(command, inputArray[1].c_str());
        strcat(command, "\r\n");

        // Write command to server
        write(clientSD, (char*)&command, strlen(command));

        // Wait for child process
        wait(NULL);
    }
    // Child process
    else
    {
        // Poll server for responses
        while(poll() == 1)
        {
            // Get and print response
            serverResponse();
            cout << buffer;

            string error = "550";

            if(strstr(buffer, error.c_str()))
            {
                exit(0);
                return;
            }
        }

        // Open a file
        int file = open(inputArray[1].c_str(), O_WRONLY | O_CREAT, 
                                S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

        // Zero out buffer
        bzero(buffer, sizeof(buffer));

        // While there is more data to be read
        while(true)
        {
            // Read into buffer and record number of bytes read
            int numRead = read(passiveSD, buffer, sizeof(buffer));

            // If there was no bytes read, break the loop, eof
            if(numRead == 0)
            {
                break;
            }

            // Write buffer to local file
            write(file, buffer, numRead);
        }                   

        close(file);
        exit(0); 
    }
    close(passiveSD);

    // Poll the socket
    while(poll() > 0)
    {
        // While poll return > 0
        // Get response and print it
        serverResponse();
        cout << buffer;
    }
}