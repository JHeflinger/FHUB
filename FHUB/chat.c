/**
 * chat.c - a program to connect with a server application
 * author: Jason Heflinger
 * last modified: 6-7-2023
*/

// defines
#define WIN32_LEAN_AND_MEAN
#define DEFAULT_IP            "127.0.0.1"
#define DEFAULT_PORT          "69420"
#define DEFAULT_USERNAME      "ANONYMOUS"
#define PACKET_SIZE           4096
#define BUFFER_SIZE           2048
#define MAX_LOGS              100000
#define TRUE                  1
#define FALSE                 0

// standard library includes
#include <ws2tcpip.h>
#include <pthread.h>

// custom includes
#include "utils.h"

// global variables
char g_chatLog[MAX_LOGS][PACKET_SIZE]  = { 0 };
char g_ipAddr[17]                      = { 0 };
char g_username[512]                   = { 0 };
char g_buffer[BUFFER_SIZE]             = { 0 };
int  g_logIndex                        =   0  ;
int  g_port                            =   0  ;
int  g_socket                          =   0  ;
int  g_initialized                     =   0  ;

// helper enum to describe packets
enum PACKET_TYPE {
    CHAT = 'c',
    SHUTDOWN = 's'
};

// function declarations
void   initialize(void);
void   createConnection(void);
void   update(void);
void   disconnect(void);
void*  updateOutput(void* arg);
void*  updateInput(void* arg);
int    compareCommand(char* buffer, char* command, char shortcut);
void   sendPacket(char* buf, char type);

/**
 * main function. General high level functionality
 * to run the chat program
*/
int main() {
    // initialize data
    initialize();

    // create a connection with the server
    createConnection();

    // update the program
    update();

    // disconnect from server and clean up
    disconnect();

    return 0;
}

/**
 * initializes critical data to run the program, including user
 * inputted data such as IP, port, and username
*/
void initialize() {
    // initialize windows socket library
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        setTextColor(RED);
        printf("ERROR: WSAStartup failed.\n");
        resetText();
        exit(1);
    }else if (LOBYTE(wsaData.wVersion) != 2 ||
        HIBYTE(wsaData.wVersion) != 2) {
        setTextColor(RED);
        printf("ERROR: Version 2.2 of Winsock is not available.\n");
        resetText();
        WSACleanup();
        exit(2);
    }

    // get ip data from user
    printf("What IP are you connecting to? (enter for default %s)\n", DEFAULT_IP);
    getInput(g_ipAddr, 17);
    if (g_ipAddr[0] == '\0')
        memcpy(g_ipAddr, DEFAULT_IP, 10);

    // get port data from user
    printf("What port will you be on? (enter for default %s)\n", DEFAULT_PORT);
    char portStr[7];
    getInput(portStr, 7);

    // verify port is correct, defaults if invalid
    if (portStr[0] == '\0')
        memcpy(portStr, DEFAULT_PORT, 6);
    g_port = atoi(portStr);
    if (g_port <= 1024) {
        setTextColor(YELLOW);
        printf("WARNING: Indicated port is either invalid or reserved. Defaulting to %s\n", DEFAULT_PORT);
        resetText();
        g_port = atoi(DEFAULT_PORT);
    }

    // get username from user
    printf("What do you want to be called? (enter for default %s)\n", DEFAULT_USERNAME);
    getInput(g_username, 512);
    if (g_username[0] == '\0')
        memcpy(g_username, DEFAULT_USERNAME, 10);
    
    g_initialized = TRUE;
}

/**
 * Creates a connection with the server given that the correct data has been
 * initialized. 
*/
void createConnection() {
    // check if program is initialized
    if (!g_initialized) {
        setTextColor(RED);
        printf("ERROR: Connection was attempted without initialization.\n");
        resetText();
        exit(3);
    }

    // create socket
    printf("Connecting to %s on port %d as user %s\n", g_ipAddr, g_port, g_username);
    int status, valread, client_fd;
    struct sockaddr_in serv_addr;
    if ((client_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        setTextColor(RED);
        printf("ERROR: Socket creation error \n");
        resetText();
        exit(1);
    }
    ioctlsocket(client_fd, FIONBIO, 1);

    // process IP and port information
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(g_port);
    if (inet_pton(AF_INET, g_ipAddr, &serv_addr.sin_addr) <= 0) {
        setTextColor(RED);
        printf("ERROR: Invalid address/Address not supported \n");
        resetText();
        exit(1);
    }

    // attempt connection
    if ((status = connect(client_fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr))) < 0) {
        setTextColor(RED);
        printf("ERROR: connection failed\n");
        resetText();
        exit(1);
    }
    
    // connection success!
    g_socket = client_fd;
}

/**
 * starts and handles all updating and real
 * time events in the program
*/
void update() {
    // clear and prep the terminal 
    system("@cls||clear");
    printf("<============== Connected! Welcome to the chat room! ==============>\n");

    // create separate threads for input and output
    pthread_t thread1, thread2;
    int id1, id2;
    pthread_create(&thread1, NULL, updateOutput, &id1);
    pthread_create(&thread2, NULL, updateInput, &id2);

    // wait for threads to finish
    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);
}

/**
 * Disconnects the program from the server and
 * performs any needed clean up
*/
void disconnect() {
    setTextColor(YELLOW);
    printf("SERVER >> Disconnecting...\n");
    resetText();
    char buf[10];
    sendPacket(buf, SHUTDOWN);
    close(g_socket);
    exit(0);
}

/**
 * updates the output received from the server into the user's
 * local terminal. this is asynchronous and therefore non-blocking to 
 * the user's input
*/
void* updateOutput(void* arg) {
    int open = TRUE;
    while(open) {
        char buffer[PACKET_SIZE] = { 0 };
        if (recv(g_socket, buffer, PACKET_SIZE, 0) >= 0) {
            // grab packet
            char chat[PACKET_SIZE + 20] = { '\0' }; // 20 is added as arbitrary padding
            int index = 1;
            for(index = 1; buffer[index] != '>'; index++);
            mempcpy(chat, buffer + 1, strlen(buffer) - 1);

            // proccess packet for output
            char* syntax = " >> ";
            memcpy(chat + index - 1, syntax, 4);
            memcpy(chat + index + 3, buffer + index + 1, strlen(buffer) - index - 1);

            // add chat to log
            strcpy(g_chatLog[g_logIndex], chat);
            g_logIndex++;

            // prints out latest chat from log
            for(int i = 0; i < strlen(g_buffer); i++) // deletes written text
                printf("\b \b");
            printf("%s\n", g_chatLog[g_logIndex - 1]); // prints our chat from our updated log
            printf("%s", g_buffer); // prints out our interrupted input
        }
    }

    pthread_exit(NULL);
}

/**
 * updates the input received from the user and sends it into the
 * server. this is asynchronous and therefore non-blocking to 
 * the server's output
*/
void* updateInput(void* arg) { 
    //hard reset for safety
    for (int i = 0; i < strlen(g_buffer); i++) printf(" ");
    printf("\r");
    memset(g_buffer, '\0', BUFFER_SIZE);

    int open = TRUE;
    while(open) {
        char packet[PACKET_SIZE] = { 0 };
        int index = 0;
        char curr = 0;
        int reset = FALSE;

        // gather user input
        do {
            if (kbhit()) { // if input is detected
                // get the next character
                curr = getch();

                // reset the buffer if this is a new chat
                if (!reset) {
                    reset = TRUE;
                    memset(g_buffer, '\0', BUFFER_SIZE);
                    memset(packet, '\0', 4096);
                }

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

        if (g_buffer[0] == '/') {
            // clear typed
            for (int i = 0; i < strlen(g_buffer); i++) printf(" ");
            printf("\r");

            char command[strlen(g_buffer) - 1];
            strcpy(command, g_buffer + 1);
            if (compareCommand(command, "exit", 'e')) { // TODO: add confirmation check
                disconnect();
                break;
            } else if (compareCommand(command, "help", 'h')) {
                printf("\nFHUB (CLIENT) VERSION 0.0.1\n\n"
                "COMMANDS: "
                "\n\t- [/help]    [/h]    prompts help output"
                "\n\t- [/exit]    [/e]    shuts down the application and disconnects the client"
                "\n\n"
                "\nTHANK YOU FOR USING FHUB\n\n\n");
            } else {
                setTextColor(RED);
                printf("SERVER >> Invalid command\n");
                resetText();
            }

            // clear buffer
            memset(g_buffer, '\0', BUFFER_SIZE);
            continue;
        }

        // send packet and clear input buffer
        sendPacket(packet, CHAT);
        memset(g_buffer, '\0', BUFFER_SIZE);
    }

    pthread_exit(NULL);
}

/**
 * compares a given buffer to a command and it's respective shortcut
*/
int compareCommand(char* buffer, char* command, char shortcut) {
    int singleton = (strlen(buffer) == 1 || buffer[1] == ' ');
    return (strcmp(buffer, command) == 0 || (singleton && buffer[0] == shortcut));
}

void sendPacket(char* buf, char type) {
    buf[0] = type;
    switch (type) {
        case CHAT:
            memcpy(buf + 1, g_username, strlen(g_username));
            buf[strlen(g_username) + 1] = '>';
            memcpy(buf + strlen(g_username) + 2, g_buffer, strlen(g_buffer));
            send(g_socket, buf, strlen(buf), 0);
            break;
        case SHUTDOWN:
            ioctlsocket(g_socket, FIONBIO, 0);
            send(g_socket, buf, strlen(buf), 0);
            break;
    }
}