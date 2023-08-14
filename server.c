#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>

#define COMMUNICATION_PORT                55316
#define MAX_CLIENTS                       3
#define MSG_S2C_USER_QUEUE_FULL           "The number of users reaches the upper bound"
#define MSG_S2C_WELCOME_MESSAGE           "Welcome connect to the server"
#define CMD_C2S_QUIT                      "Quit connection"
#define CMD_C2S_OBATIN_USER_LIST          "Obtain the user list from the server"
#define MSG_S2C_SEND_USER_LIST            "The server is sending the user list"
#define MSG_S2C_USER_LIST_HAS_BEEN_SENT   "The user information has been sent"
#define CMD_C2S_SEND_MESSAGE_TO_ALL_USERS "Send a message to all users"
#define MSG_S2C_MESSAGE_TO_ALL_USERS      "The server is sending a message to all users"

//struct of user node
struct userNode{
    int             userID;
    char            userName[100];
    int             connfd;
    struct userNode * next;
};

//global variables
struct userNode * userList;
int userCount, historicalConnections;

//the child threads call this function
static void * childThread(void * arg);

int main(int argc, const char ** argv){
    struct sockaddr_in clientAddr;
    socklen_t len;
    char buff[1024];

    //initialize the golbal variables
    userList              = (struct userNode *)malloc(sizeof(struct userNode));
    userList->next        = NULL;
    userCount             = 0;
    historicalConnections = 0;

    //create the socket
    int socketServer = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(socketServer == -1){
        printf("1. Create socket failed.\n");
        return -1;
    }
    printf("1. The socket is created.\n");

    //bind the socket
    struct sockaddr_in servAddr;
    bzero(&servAddr, sizeof(servAddr));
    servAddr.sin_family      = AF_INET;
    servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servAddr.sin_port        = htons(COMMUNICATION_PORT);
    if(bind(socketServer, (struct sockaddr *)&servAddr, sizeof(servAddr)) == -1){
        printf("2. Bind failed.");
        return -1;    
    }
    printf("2. The address is bound.\n");

    //convert the state into listen state
    if(listen(socketServer, 5) == -1){
        printf("3. Listen failed.\n");
        return -1;
    }
    printf("3. Listen state.\n");

    printf("4. Waiting for connection.\n");

    while(1){
        //wait for connection
        int connfd = accept(socketServer, (struct sockaddr *)(&clientAddr), &len);
        if(connfd == -1){
            printf("4. Accept failed\n");
            return -1;
        }

        printf("    A new client is connecting to the server.\n");
        
        if(userCount < MAX_CLIENTS){
            //create the child thread
            pthread_t th;
            pthread_create(&th, NULL, childThread, (void *) &connfd);
        }

        //the number of clients reaches the upper bound
        else{
            printf("    The number of clients reaches the upper bound, connection is forbidden.\n");
            bzero(buff, sizeof(buff));
            strcpy(buff, MSG_S2C_USER_QUEUE_FULL);
            write(connfd, buff, strlen(buff));
        }        
    }
    
    free(userList);
    close(socketServer);
    return 0;
}

static void * childThread(void * arg){
    int connfd = * (int *)arg;
    char buff[1024];

    //send the welcome message
    bzero(buff, sizeof(buff));
    strcpy(buff, MSG_S2C_WELCOME_MESSAGE);
    write(connfd, buff, strlen(buff));

    //create a user node to store the information
    struct userNode * cn = (struct userNode *)malloc(sizeof(struct userNode));

    //insert the user node to the end of the user list
    struct userNode * p = userList;
    while(p->next != NULL)
        p = p->next;
    p->next = cn;
        
    //obtain the user name and initialize the user node
    cn->connfd = connfd;
    cn->userID = historicalConnections++;
    cn->next   = NULL;

    bzero(buff, sizeof(buff));
    read(connfd, buff, sizeof(buff));
    strcpy(cn->userName, buff);
    printf("    New client: connfd(%d), userID(%d), userName(%s), TID(%ld)\n", 
        cn->connfd, cn->userID, cn->userName, pthread_self());
    userCount++;

    int quitFlag = 1;
    while(quitFlag){
        //obtain the command from the client
        bzero(buff, sizeof(buff));
        read(connfd, buff, sizeof(buff));

        //user quits the connection
        if(strcmp(buff, CMD_C2S_QUIT) == 0){
            p = userList;
            while(p->next != cn)
                p = p->next;
            p->next = cn->next;

            userCount--;
            
            quitFlag = 0;

            printf("    Client: connfd(%d), userID(%d), userName(%s) quits the connection, clientCount(%d).\n",
                cn->connfd, cn->userID, cn->userName, userCount);
        }

        //user wants to obtain the user list
        if(strcmp(buff, CMD_C2S_OBATIN_USER_LIST) == 0){
            printf("    message from the client(%d): %s\n", connfd, buff);
            //tell the client that the server is about to send the user information
            bzero(buff, sizeof(buff));
            strcpy(buff, MSG_S2C_SEND_USER_LIST);
            write(connfd, buff, strlen(buff));
            sleep(0.1);

            //fetch the user information
            p = userList->next;
            int clientIdx = 1;
            while(p != NULL){
                //send the information of all clients
                bzero(buff, sizeof(buff));
                sprintf(buff, "%d. userID: %d userName: %s", clientIdx++, p->userID, p->userName);
                printf("    Message to the client(%d): %s\n", connfd, buff);
                write(connfd, buff, sizeof(buff));
                sleep(0.1);
                p = p->next;
            }

            //tell the client that the user information have been sent
            bzero(buff, sizeof(buff));
            strcpy(buff, MSG_S2C_USER_LIST_HAS_BEEN_SENT);
            write(connfd, buff, strlen(buff));
        }

        //user wants to send a message to all users
        if(strcmp(buff, CMD_C2S_SEND_MESSAGE_TO_ALL_USERS) == 0){
            printf("    message from the client(%d): %s\n", connfd, buff);

            //receive the message
            bzero(buff, sizeof(buff));
            read(connfd, buff, sizeof(buff));

            char message[2048];
            bzero(message, sizeof(message));
            sprintf(message, "Message from %s to all users: %s", cn->userName, buff);

            //send message to all users
            p = userList->next;
            while(p != NULL){
                if(p->connfd != connfd){
                    bzero(buff, sizeof(buff));
                    strcpy(buff, MSG_S2C_MESSAGE_TO_ALL_USERS);
                    write(p->connfd, buff, strlen(buff));
                    sleep(0.1);

                    write(p->connfd, message, strlen(message));
                    printf("    Send message from %s to %s\n", cn->userName, p->userName);
                    sleep(0.1);
                }
                p = p->next;
            }
        }
    }

    close(connfd);
    free(cn);
    return NULL;
}