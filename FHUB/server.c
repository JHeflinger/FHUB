/**
 * server.c - a program to take in client connections and manage them
 * author: Jason Heflinger
 * last modified: 6-8-2023
*/

// defines
#define WIN32_LEAN_AND_MEAN
#define DEFAULT_PORT          "69420"
#define ROOT_DIR              "ROOT"
#define MAX_PATH_SIZE         8192
#define MAX_LOGS              100000
#define PACKET_SIZE           4096
#define BUFFER_SIZE           2048
#define MAX_USERS             10
#define TRUE                  1
#define FALSE                 0

// third party includes
#include <ws2tcpip.h>
#include <sys/stat.h>
#include <direct.h>
#include <dirent.h>

// custom includes
#include "utils.h"

// global variables
char g_chatLog[MAX_LOGS][PACKET_SIZE]  = { 0 };
int  g_clients[MAX_USERS]              = { 0 };
char g_buffer[BUFFER_SIZE]             = { 0 };
char g_relativePath[MAX_PATH_SIZE]     = { 0 };
int  g_logIndex                        =   0  ; 
int  g_clientIndex                     =   0  ;
int  g_port                            =   0  ;
int  g_initialized                     =   0  ;
int  g_socket                          =   0  ;
int  g_monitor                         =   0  ;
int  g_shutdown                        =   0  ;
int  g_talkEnabled                     =   0  ;

// helper enum to describe packets
enum PACKET_TYPE {
    CHAT = 'c',
    SHUTDOWN = 's'
};

// function declarations
void  initialize(void);
void  hostConnection(void);
void  disconnect(void);
void  handleInput(void);
void  handleClient(int socket_fd);
void  addUser(int socket_fd);
void  addChat(char* chat);
void  handlePacket(char* buf, int socket_fd);
int   compareCommand(char* buffer, char* command, char* shortcut);
void  disconnectClient(int socket_fd);
void  listDirectory(void);
void  readFile(char* arg);
int   confirmArgs(int numArgs, int desiredArgs);
void  createItem(char* flag, char* name);
void  changeDirectory(char* directory);
void  getWorkingDir(char* path);

/**
 * prints out non blocking using intermediate input buffer
*/
#define ASYNC_PRINT(...) do { for (int i = 0; i < strlen(g_buffer); i++) printf("\b \b"); printf(__VA_ARGS__); printf("%s", g_buffer); } while (0)

/**
 * Main function that handles program flow. 
*/
int main(int argc, char *argv[]) {
    initialize();
    hostConnection();
    disconnect();
    return 0; 
}

/**
 * initializes critical data to run the program, including user
 * inputted data such as the port to host on
*/
void initialize() {
    // initialize windows socket library
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        setTextColor(RED);
        printf("ERROR   >> WSAStartup failed.\n");
        resetText();
        exit(1);
    }else if (LOBYTE(wsaData.wVersion) != 2 ||
        HIBYTE(wsaData.wVersion) != 2) {
        setTextColor(RED);
        printf("ERROR   >> Version 2.2 of Winsock is not available.\n");
        resetText();
        WSACleanup();
        exit(2);
    }

    // get port from user
    printf("What port will you be hosting on? (enter for default %s)\n", DEFAULT_PORT);
    char portStr[7];
    getInput(portStr, 7);

    // validate and set to default port if needed
    if (portStr[0] == '\0')
        memcpy(portStr, DEFAULT_PORT, 6);
    g_port = atoi(portStr);
    if (g_port <= 1024) {
        setTextColor(YELLOW);
        printf("WARNING: Indicated port is either invalid or reserved. Defaulting to %s\n", DEFAULT_PORT);
        resetText();
        g_port = atoi(DEFAULT_PORT);
    }

    g_initialized = TRUE;
}

/**
 * Creates and hosts a new connection through a specifed port that listens
 * for new clients
*/
void hostConnection() {
    // check if program is initialized
    if (!g_initialized) {
        setTextColor(RED);
        printf("ERROR   >> hosting was attempted without initialization.\n");
        resetText();
        exit(3);
    }
    
    // Create socket
    printf("Starting chat server on port %d...\n", g_port);
    int server_fd, new_socket, valread;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        setTextColor(RED);
        printf("ERROR   >> Socket creation error \n");
        resetText();
        exit(4);
    }
    ioctlsocket(server_fd, FIONBIO, 1);
    g_socket = server_fd;

    // Forcefully attaching socket to the desired port
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        setTextColor(RED);
        printf("ERROR   >> failed to attach socket to port");
        resetText();
        exit(5);
    }
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(g_port);
    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        setTextColor(RED);
        printf("ERROR   >> bind failed");
        resetText();
        exit(6);
    }

    // start listening
    if (listen(server_fd, MAX_USERS) < 0) {
        setTextColor(RED);
        printf("ERROR   >> failed to listen");
        resetText();
        exit(7);
    }
    setTextColor(GREEN);
    printf("Server initialized! Now listening on port %d\n", g_port);
    resetText();

    // start input thread
    HANDLE inputThread;
    inputThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)handleInput, NULL, 0, NULL);
    if (inputThread == NULL) {
        setTextColor(RED);
        printf("ERROR   >> Failed to create new input thread.\n");
        resetText();
    }

    // accept and handle clients
    while (!g_shutdown) {
        int client_socket;
        if ((client_socket = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen)) < 0) {
            setTextColor(RED);
            printf("ERROR   >> failed to accept\n");
            resetText();
            continue;
        } else {
            setTextColor(GREEN);
            if (g_monitor) ASYNC_PRINT("MONITOR >> New client connected\n");
            resetText();
            HANDLE clientThread;
            clientThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)handleClient, client_socket, 0, NULL);
            if (clientThread == NULL) {
                setTextColor(RED);
                printf("ERROR   >> Failed to create new client thread.\n");
                resetText();
                continue;
            }
            addUser(client_socket);
        }
    }
}

/**
 * Disconnects server from socket and takes
 * care of any needed cleanup
*/
void disconnect() {
    setTextColor(YELLOW);
    printf("SERVER  >> Shutting down...\n");
    resetText();
    close(g_socket);
    for (int i = 0; i < g_clientIndex; i++)
        close(g_clients[i]);
    exit(0);
}

/**
 * Handles user input on the server side for server admins
 * and deployers who want to manage the server in real time
*/
void handleInput() {
    while(!g_shutdown) {
        // print precursor
        if (strlen(g_relativePath) == 0) {
            printf("R:> ");
        } else printf("R:/%s> ", g_relativePath);

        int index = 0;
        char curr = 0;

        // gather user input
        do {
            if (kbhit()) { // if input is detected
                // get the next character
                curr = getch();

                // update the input buffer and print out the typed input
                if (curr == '\b') {
                    g_buffer[--index] = '\0';
                    printf("\b \b");
                } else {
                    g_buffer[index++] = curr;
                    printf("%c", curr);
                }
            }
        } while (curr != '\r' && curr != '\n'); //loop until carriage return or newline

        // replace newline with string end
        for(int i = 0; i < BUFFER_SIZE; i++) {
            if (g_buffer[i] == '\n' || g_buffer[i] == '\r') {
                g_buffer[i] = '\0';
                break;
            }
        }

        // clear typed
        printf("\r");
        for (int i = 0; i < strlen(g_buffer); i++) printf(" ");
        printf("\r");

        // proccess command
        if(g_buffer[0] == '/') {
            printf("ADMIN   >> %s\n", g_buffer);
            char command[strlen(g_buffer) - 1];

            //parse args
            strcpy(command, g_buffer + 1);
            while(command[strlen(command) - 1] == ' ') command[strlen(command) - 1] = '\0';
            int numargs = 1;
            int inquotes = FALSE;
            for(int i = 0; i < strlen(command); i++) {
                if (command[i] == ' ' && !inquotes)
                    numargs++;
                else if (command[i] == '"')
                    inquotes = !inquotes;
            }
            char args[numargs][strlen(command)];
            char arg[strlen(command)];
            int argindex = 0;
            int argsindex = 0;
            inquotes = FALSE;
            for(int i = 0; i < strlen(command); i++) {
                if (command[i] == ' ' && !inquotes) {
                    arg[argindex] = '\0';
                    argindex = 0;
                    strcpy(args[argsindex], arg);
                    memset(arg, '\0', strlen(command));
                    argsindex++;
                } else if (command[i] == '"') {
                    inquotes = !inquotes;
                } else {
                    arg[argindex] = command[i];
                    argindex++;
                }
            }
            arg[argindex] = '\0';
            strcpy(args[argsindex], arg);

            if (compareCommand(args[0], "monitor", "m")) {
                if (confirmArgs(numargs, 1)) {
                    g_monitor = !g_monitor;
                    if (g_monitor) printf("SERVER  >> monitoring toggled ON\n");
                    else printf("SERVER  >> monitoring toggled OFF\n");
                }
            } else if (compareCommand(args[0], "exit", "e")) { // TODO: add confirmation check if users are online 
                if (confirmArgs(numargs, 1)) {
                    g_shutdown = TRUE;
                    disconnect();
                }
            } else if (compareCommand(args[0], "help", "h")) {
                if (confirmArgs(numargs, 1)) {
                    printf("\nFHUB (SERVER) VERSION 0.0.1\n\n"
                    "COMMANDS: "
                    "\n\t- [/help]       prompts help output"
                    "\n\t- [/monitor]    toggles monitoring log on or off"
                    "\n\t- [/list]       lists all files in the current directory"
                    "\n\t- [/talk]       toggles chatting with connected clients"
                    "\n\t- [/exit]       shuts down the application and disconnects all clients"
                    "\n"
                    "\n\t- [/read] <filename>          reads a file and outputs its contents to the terminal"
                    "\n\t- [/changedir] <dir>          changes working directory to the specified directory"
                    "\n"
                    "\n\t- [/create] <flag> <name>     creates a file or directory (-f for file or -d for directory)"
                    "\n\n"
                    "\nTHANK YOU FOR USING FHUB\n\n\n");
                }
            } else if (compareCommand(args[0], "list", "l")) {
                if (confirmArgs(numargs, 1)) {
                    listDirectory();
                }
            } else if (compareCommand(args[0], "talk", "t")) {
                if (confirmArgs(numargs, 1)) {
                    g_talkEnabled = !g_talkEnabled;
                    if (g_talkEnabled) printf("SERVER  >> talking toggled ON\n");
                    else printf("SERVER  >> talking toggled OFF\n");
                }
            } else if (compareCommand(args[0], "read", "r")) {
                if (confirmArgs(numargs, 2)) {
                    readFile(args[1]);
                }
            } else if (compareCommand(args[0], "create", "c")) {
                if (confirmArgs(numargs, 3)) {
                    createItem(args[1], args[2]);
                }
            } else if (compareCommand(args[0], "changedir", "cd")) {
                if (confirmArgs(numargs, 2)) {
                    changeDirectory(args[1]);
                }
            } else {
                setTextColor(RED);
                printf("SERVER  >> Invalid command\n");
                resetText();
            }
        } else if (g_talkEnabled) {
            printf("ADMIN   >> %s\n", g_buffer);
            char packet[PACKET_SIZE];
            packet[0] = CHAT;
            strcpy(packet + 1, "ADMIN>");
            strcpy(packet + strlen(packet), g_buffer);
            addChat(packet);
            handlePacket(packet, g_socket);
        } else {
            setTextColor(YELLOW);
            printf("SERVER  >> talking is not enabled!\n");
            resetText();
        }
        
        // clear buffer
        memset(g_buffer, '\0', BUFFER_SIZE);
    }
}

/**
 * Creates a folder or directory given a flag
 * and a name
*/
void createItem(char* flag, char* name) {
    char path[MAX_PATH_SIZE];
    getWorkingDir(path);
    path[strlen(path)] = '/';
    strcpy(path + strlen(path), name);

    if (strcmp(flag, "-f") == 0) {
        FILE *file;
        file = fopen(path, "w");
        if (file == NULL) {
            setTextColor(RED);
            printf("ERROR   >> unable to create file\n");
            resetText();
            return;
        }
        fclose(file);
    } else if (strcmp(flag, "-d") == 0) {
        int result = mkdir(path);
        if (result == -1) {
            setTextColor(RED);
            printf("ERROR   >> unable to create directory\n");
            resetText();
            return;
        }
    } else {
        setTextColor(RED);
        printf("ERROR   >> invalid use of create. Usage is [/create] <flag> <name>. see [/help] for more information.\n");
        resetText();
    }
}

/**
 * handles client received packets and proccesses them
 * accordingly
*/
void handleClient(int socket_fd) {
    while (!g_shutdown) { // TODO: add afk timer later
        char packet[PACKET_SIZE] = { 0 };
        int recCode = recv(socket_fd, packet, PACKET_SIZE, 0);
        if (recCode >= 0) {
            if (g_monitor) ASYNC_PRINT("MONITOR >> received new packet: %s\n", packet);
            addChat(packet);
            handlePacket(packet, socket_fd);
        }
    }
}

/**
 * handles a packet given the packet and the socket
 * that sent the packet
*/
void handlePacket(char* buf, int socket_fd) {
    switch (buf[0]) {
        case CHAT:
            if (g_talkEnabled && socket_fd != g_socket) {
                char chat[PACKET_SIZE];
                int index = 1;
                for(index = 1; buf[index] != '>'; index++);
                strcpy(chat, buf + 1);
                strcpy(chat + index - 1, " >> ");
                strcpy(chat + index + 3, buf + index + 1);
                setTextColor(BLUE);
                ASYNC_PRINT("CLIENT  >> %s\n", chat);
                resetText();
            }
            if (g_monitor) ASYNC_PRINT("MONITOR >> updating client chatrooms\n");
            for(int i = 0; i < g_clientIndex; i++) {
                send(g_clients[i], buf, strlen(buf), 0);
            }
            break;
        case SHUTDOWN:
            setTextColor(YELLOW);
            if (g_monitor) ASYNC_PRINT("MONITOR >> client disconnected");
            resetText();
            disconnectClient(socket_fd);
            break;
    }
}

/**
 * disconnects a certain client given their socket
*/
void disconnectClient(int socket_fd) {
    close(socket_fd);
    int found = FALSE;
    for(int i = 0; i < g_clientIndex; i++) {
        if (found)
            g_clients[i - 1] = g_clients[i];
        if (g_clients[i] == socket_fd) 
            found = TRUE;
    }
    g_clientIndex--;
}

/**
 * adds a chat to the chat log
*/
void addChat(char* chat) {
    strcpy(g_chatLog[g_logIndex], chat);
    g_logIndex++;
}

/**
 * adds a user into the recorded current
 * users
*/
void addUser(int socket_fd) {
    g_clients[g_clientIndex] = socket_fd;
    g_clientIndex++;
}

/**
 * compares a given buffer to a command and it's respective shortcut
*/
int compareCommand(char* buffer, char* command, char* shortcut) {
    return (strcmp(buffer, command) == 0 || strcmp(buffer, shortcut) == 0);
}

/**
 * compares arguments and prints out error if false, and returns whether
 * number of arguments were valid or not.
*/
int confirmArgs(int numArgs, int desiredArgs) {
    if (numArgs != desiredArgs) {
        setTextColor(RED);
        printf("ERROR   >> Invalid use of command. Received %d arguments, should've received %d.\n", numArgs - 1, desiredArgs - 1);
        resetText();
        return 0;
    }
    return 1;
}

/**
 * Changes the working directory to the specified directory
*/
void changeDirectory(char* directory) {
    //if (directory[0] == '.' && directory[1] == '.') {
    //    if (strlen(g_relativePath) <= 0) {
    //       setTextColor(RED);
    //        printf("ERROR   >> Cannot go back further than the root directory\n");
    //        resetText();
    //    } 
    //}
    char path[MAX_PATH_SIZE];
    getWorkingDir(path);

    //parse steps
    while(directory[strlen(directory) - 1] == '/' || directory[strlen(directory) - 1] == '\\') directory[strlen(directory) - 1] = '\0'; // get rid of trailing slashes
    while(directory[0] == '/' || directory[0] == '\\') strcpy(directory, directory + 1); // get rid of forward slashes
    int numsteps = 1;
    for(int i = 0; i < strlen(directory); i++)
        if (directory[i] == '/' || directory[i] == '\\')
            numsteps++;
    char steps[numsteps][strlen(directory)];
    char step[strlen(directory)];
    int stepindex = 0;
    int stepsindex = 0;
    for(int i = 0; i < strlen(directory); i++) {
        if (directory[i] == '/' || directory[i] == '\\') {
            step[stepindex] = '\0';
            stepindex = 0;
            strcpy(steps[stepsindex], step);
            memset(step, '\0', strlen(directory));
            stepsindex++;
        } else step[stepindex++] = directory[i];
    }
    step[stepindex] = '\0';
    strcpy(steps[stepsindex], step);

    // construct complete path
    for(int i = 0; i < numsteps; i++) {
        if (strcmp(steps[i], "..") == 0) {
            if (strlen(path) != strlen(ROOT_DIR)) {
                while (path[strlen(path) - 1] != '/' && path[strlen(path) - 1] != '\\') path[strlen(path) - 1] = '\0'; // delete step
                path[strlen(path) - 1] = '\0'; // get rid of trailing slash
            } else {
                setTextColor(RED);
                printf("ERROR   >> Cannot go back further than the root directory\n");
                resetText();
                return;
            }
        } else {
            int pathlen = strlen(path);
            path[pathlen] = '/';
            strcpy(path + pathlen + 1, steps[i]);
        }
    }

    // validate directory exists
    DIR *dir = opendir(path);
    if (dir) closedir(dir);
    else {
        setTextColor(RED);
        printf("ERROR   >> Directory does not exist or is not accessible\n");
        resetText();
        return;
    }

    // make path the g_relative path
    strcpy(g_relativePath, path + strlen(ROOT_DIR) + 1);
}

/**
 * Reads from a specified file and outputs it to the terminal
*/
void readFile(char* arg) {
    //construct path
    char path[MAX_PATH_SIZE];
    getWorkingDir(path);
    path[strlen(path)] = '/';
    strcpy(path + strlen(path), arg);

    FILE *file;
    char ch;
    file = fopen(path, "r");
    if (file == NULL) {
        setTextColor(RED);
        printf("ERROR   >> File could not be opened or could not be found\n");
        resetText();
        return;
    }

    do {
        ch = fgetc(file);
        putchar(ch);

    } while(ch != EOF);
    printf("\n");
    fclose(file);
}

void listDirectory() {
    //construct path
    char path[MAX_PATH_SIZE];
    getWorkingDir(path);
    printf("path: %s\n", path);

    struct stat sb;
    if (stat(path, &sb) == 0 && S_ISDIR(sb.st_mode)) {
        DIR* directory = opendir(path);
        if (directory == NULL) {
            setTextColor(RED);
            printf("ERROR   >> failed to open directory\n");
            resetText();
            return;
        }

        struct dirent* entry;
        printf("\n");
        setHighlight(YELLOW);
        printf("DIRECTORY: %s/", path);
        resetText();
        printf("\n\n");
        while((entry = readdir(directory)) != NULL) {
            char filepath[MAX_PATH_SIZE];
            strcpy(filepath, path);
            filepath[strlen(path)] = '/';
            strcpy(filepath + strlen(path) + 1, entry->d_name);

            struct stat fileInfo;
            if (stat(filepath, &fileInfo) == 0) {
                if (S_ISDIR(fileInfo.st_mode)) {
                    setBoldText();
                }
                printf("%s\n", entry->d_name);
                resetText();
            }
        }
        printf("\n");

        closedir(directory);
    } else {
        // directory doesn't exist
        if (strlen(g_relativePath) == 0) {
            setTextColor(YELLOW);
            printf("SERVER  >> No root directory detected. Creating a new directory...\n");
            if (_mkdir(ROOT_DIR) == 0) {
                setTextColor(GREEN);
                printf("SERVER  >> Root directory created!\n");
            } else {
                setTextColor(RED);
                printf("ERROR   >> unable to create root directory\n");
            }
        } else {
            setTextColor(RED);
            printf("ERROR   >> current directory does not exist\n");
        }
        resetText();
    }
}

/**
 * gets the working directory and copies it into
 * the given path pointer
*/
void getWorkingDir(char* path) {
    strcpy(path, ROOT_DIR);
    if (strlen(g_relativePath) > 0) {
        path[strlen(ROOT_DIR) + 1] = '/';
        memcpy(path + strlen(ROOT_DIR) + 1, g_relativePath, strlen(g_relativePath));
        path[strlen(ROOT_DIR) + 2 + strlen(g_relativePath)] = '\0';
    }
}