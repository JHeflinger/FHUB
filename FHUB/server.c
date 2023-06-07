/**
 * server.c - a program to take in client connections and manage them
 * author: Jason Heflinger
 * last modified: 6-6-2023
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

// helper enum to describe packets
enum PACKET_TYPE {
    CHAT = 'c',
    SHUTDOWN = 's'
};

// function declarations
void initialize(void);
void hostConnection(void);
void disconnect(void);
void handleInput(void);
void handleClient(int socket_fd);
void addUser(int socket_fd);
void addChat(char* chat);
void handlePacket(char* buf, int socket_fd);
int  compareCommand(char* buffer, char* command, char shortcut);
void disconnectClient(int socket_fd);
void listDirectory(void);

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
        printf("ERROR >> WSAStartup failed.\n");
        resetText();
        exit(1);
    }else if (LOBYTE(wsaData.wVersion) != 2 ||
        HIBYTE(wsaData.wVersion) != 2) {
        setTextColor(RED);
        printf("ERROR >> Version 2.2 of Winsock is not available.\n");
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
        printf("WARNING: Indicated port is either invalid or reserved. Defaulting to %s\n", DEFAULT_PORT);
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
        printf("ERROR >> hosting was attempted without initialization.\n");
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
        printf("ERROR >> Socket creation error \n");
        resetText();
        exit(4);
    }
    ioctlsocket(server_fd, FIONBIO, 1);
    g_socket = server_fd;

    // Forcefully attaching socket to the desired port
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        setTextColor(RED);
        printf("ERROR >> failed to attach socket to port");
        resetText();
        exit(5);
    }
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(g_port);
    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        setTextColor(RED);
        printf("ERROR >> bind failed");
        resetText();
        exit(6);
    }

    // start listening
    if (listen(server_fd, MAX_USERS) < 0) {
        setTextColor(RED);
        printf("ERROR >> failed to listen");
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
        printf("ERROR >> Failed to create new input thread.\n");
        resetText();
    }

    // accept and handle clients
    while (!g_shutdown) {
        int client_socket;
        if ((client_socket = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen)) < 0) {
            setTextColor(RED);
            printf("ERROR >> failed to accept\n");
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
                printf("ERROR >> Failed to create new client thread.\n");
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
    printf("SERVER >> Shutting down...\n");
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
        int index = 0;
        char curr = 0;

        // gather user input
        do {
            if (kbhit()) { // if input is detected
                // get the next character
                curr = getch();

                // update the input buffer and print out the typed input
                g_buffer[index++] = curr;
                printf("%c", curr);
                if (curr == '\b') printf(" \b"); // compensate for backspaces
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
            printf("ADMIN >> %s\n", g_buffer);
            char command[strlen(g_buffer) - 1];
            strcpy(command, g_buffer + 1);
            if (compareCommand(command, "monitor", 'm')) {
                g_monitor = !g_monitor;
                if (g_monitor) printf("SERVER >> monitoring toggled ON\n");
                else printf("SERVER >> monitoring toggled OFF\n");
            } else if (compareCommand(command, "exit", 'e')) { // TODO: add confirmation check if users are online 
                g_shutdown = TRUE;
                disconnect();
            } else if (compareCommand(command, "help", 'h')) {
                printf("\nFHUB (SERVER) VERSION 0.0.1\n\n"
                "COMMANDS: "
                "\n\t- [/help]    [/h]    prompts help output"
                "\n\t- [/monitor] [/m]    toggles monitoring log on or off"
                "\n\t- [/list]    [/l]    lists all files in the current directory"
                "\n\t- [/exit]    [/e]    shuts down the application and disconnects all clients"
                "\n\n"
                "\nTHANK YOU FOR USING FHUB\n\n\n");
            } else if (compareCommand(command, "list", 'l')) {
                listDirectory();
            } else {
                setTextColor(RED);
                printf("SERVER >> Invalid command\n");
                resetText();
            }
        }
        
        // clear buffer
        memset(g_buffer, '\0', BUFFER_SIZE);
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
int compareCommand(char* buffer, char* command, char shortcut) {
    int singleton = (strlen(buffer) == 1 || buffer[1] == ' ');
    return (strcmp(buffer, command) == 0 || (singleton && buffer[0] == shortcut));
}

void listDirectory() {
    //construct path
    char path[strlen(ROOT_DIR) + 1 + strlen(g_relativePath)];
    strcpy(path, ROOT_DIR);
    if (strlen(g_relativePath) > 0) {
        path[strlen(ROOT_DIR) + 1] = '/';
        memcpy(path + strlen(ROOT_DIR) + 1, g_relativePath, strlen(g_relativePath));
        path[strlen(ROOT_DIR) + 2 + strlen(g_relativePath)] = '\0';
    }

    struct stat sb;
    if (stat(path, &sb) == 0 && S_ISDIR(sb.st_mode)) {
        DIR* directory = opendir(path);
        if (directory == NULL) {
            setTextColor(RED);
            printf("ERROR >> failed to open directory\n");
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
            printf("SERVER >> No root directory detected. Creating a new directory...\n");
            if (_mkdir(ROOT_DIR) == 0) {
                setTextColor(GREEN);
                printf("SERVER >> Root directory created!\n");
            } else {
                setTextColor(RED);
                printf("ERROR >> unable to create root directory\n");
            }
        } else {
            setTextColor(RED);
            printf("ERROR >> current directory does not exist\n");
        }
        resetText();
    }
}