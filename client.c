#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>

#define COMMUNICATION_PORT                55316
#define MSG_S2C_USER_QUEUE_FULL           "The number of users reaches the upper bound"
#define CMD_C2S_QUIT                      "Quit connection"
#define CMD_C2S_OBATIN_USER_LIST          "Obtain the user list from the server"
#define MSG_S2C_SEND_USER_LIST            "The server is sending the user list"
#define MSG_S2C_USER_LIST_HAS_BEEN_SENT   "The user information has been sent"
#define CMD_C2S_SEND_MESSAGE_TO_ALL_USERS "Send a message to all users"
#define MSG_S2C_MESSAGE_TO_ALL_USERS      "The server is sending a message to all users"

//the thread who receives the message sent from the server
static void * messageReceiverThread(void * arg);

int main(int argc, const char ** argv){
    char buff[1024];
    char message[1024];
    
    //create the socket
    int connfd = socket(AF_INET, SOCK_STREAM, 0);
    if(connfd == -1){
        printf("1. Create socket failed\n");
        return -1;
    }

    struct sockaddr_in servAddr;
    bzero(&servAddr, sizeof(servAddr));
    servAddr.sin_family      = AF_INET;
    servAddr.sin_addr.s_addr = inet_addr("192.168.1.103");
    servAddr.sin_port        = htons(COMMUNICATION_PORT);

    //connect to the sever
    if (connect(connfd, (struct sockaddr *)&servAddr, sizeof(servAddr)) == -1){
        printf("1. Connect failed\n");
        return -1;
    }
    printf("1. Connected to the server!\n");

    bzero(buff, sizeof(buff));
    read(connfd, buff, sizeof(buff));
    if(strcmp(buff, MSG_S2C_USER_QUEUE_FULL) == 0){
        printf("    Message from the server: %s\n", buff);
        return -1;
    }

    //print the welcome message
    printf("    Message from the server: %s\n", buff);

    //send the user name to the server
    char userName[100];
    bzero(userName, sizeof(userName));
    printf("    Input user name: ");
    fgets(userName, 100, stdin);
    userName[strlen(userName) - 1] = '\0';
    bzero(buff, sizeof(buff));
    strcpy(buff, userName);
    write(connfd, buff, strlen(buff));

    //create a thread to receive messages sent from the server
    pthread_t th;
    pthread_create(&th, NULL, messageReceiverThread, (void *)&connfd);

    int quitFlag = 1;
    while(quitFlag){
        
        printf("    Command:\n");
        printf("    1. Obtain the user list.\n");
        printf("    2. Send message to all clients.\n");
        printf("    0. Quit.\n");
        printf("    Input Command:\n");
        

        int command;
        scanf("%d", &command);
        getchar();

        switch(command){
            case 0: bzero(buff, sizeof(buff));
                    strcpy(buff, CMD_C2S_QUIT);                   
                    write(connfd, buff, strlen(buff));
                    quitFlag = 0;
                    break;

            case 1: bzero(buff, sizeof(buff));
                    strcpy(buff, CMD_C2S_OBATIN_USER_LIST);
                    write(connfd, buff, strlen(buff));
                    break;

            case 2: bzero(buff, sizeof(buff));
                    strcpy(buff, CMD_C2S_SEND_MESSAGE_TO_ALL_USERS);
                    write(connfd, buff, strlen(buff));

                    printf("    Message: ");
                    bzero(message, sizeof(message));
                    fgets(message, 1024, stdin);
                    message[strlen(message) - 1] = '\0';
                    bzero(buff, sizeof(buff));
                    strcpy(buff, message);
                    write(connfd, buff, strlen(buff));
                    break;

            default:
                    printf("    Unknown command!\n");

        }
    }

    close(connfd);
    printf("2. Sokcet is closed\n");
    return 0;
}

static void * messageReceiverThread(void * arg){
    int connfd = * (int *)arg;
    char buff[1024];

    while(1){
        //receive message from the server
        bzero(buff, sizeof(buff));
        read(connfd, buff, sizeof(buff));

        
        //the server is sending the user list
        if(strcmp(buff, MSG_S2C_SEND_USER_LIST) == 0){
            bzero(buff, sizeof(buff));
            read(connfd, buff, sizeof(buff));
            while(strcmp(buff, MSG_S2C_USER_LIST_HAS_BEEN_SENT) != 0){ 
                printf("    Message from the server: %s\n", buff);       
                bzero(buff, sizeof(buff));
                read(connfd, buff, sizeof(buff));     
            } 
        }

        //the server is sending a message to all users
        if(strcmp(buff, MSG_S2C_MESSAGE_TO_ALL_USERS) == 0){
            bzero(buff, sizeof(buff));
            read(connfd, buff, sizeof(buff));
            printf("    %s\n", buff);
        }
    }


    return NULL;
}