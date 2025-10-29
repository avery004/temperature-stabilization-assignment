#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include "utils.h"


int main (int argc, char *argv[])
{
    if (argc != 3){
        printf("Usage: %s <external index 1-4> <initial temperature>\n", argv[0]);
        return EXIT_FAILURE;
    }

    int socket_desc;
    struct sockaddr_in server_addr;
    
    int externalIndex = atoi(argv[1]); 
    float initialTemperature = atof(argv[2]); 

    if (externalIndex < 1 || externalIndex > NUM_EXTERNALS){
        printf("Error: external index must be between 1 and %d\n", NUM_EXTERNALS);
        return EXIT_FAILURE;
    }

    float externalTemperature = initialTemperature;
    int iteration = 0;
    
    // Create socket:
    socket_desc = socket(AF_INET, SOCK_STREAM, 0);
    
    if(socket_desc < 0){
        printf("Unable to create socket\n");
        return -1;
    }
    
    printf("Socket created successfully\n");
    
    // Set port and IP the same as server-side:
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(2000);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
        

    // Send connection request to server:
    if(connect(socket_desc, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0){
        printf("Unable to connect\n");
        return -1;
    }
    printf("Connected with server successfully\n");
    printf("--------------------------------------------------------\n\n");
       
    struct msg outgoing = prepare_message(MSG_TEMP_UPDATE, externalIndex, externalTemperature);

    if (send_message(socket_desc, &outgoing) < 0){
        printf("Unable to send initial message\n");
        close(socket_desc);
        return EXIT_FAILURE;
    }

    bool done = false;
    while (!done){
        struct msg incoming;
        if (receive_message(socket_desc, &incoming) < 0){
            printf("Error while receiving server's message\n");
            close(socket_desc);
            return EXIT_FAILURE;
        }

        if (incoming.type == MSG_DONE){
            printf("External %d stabilized at %.4f with central temperature %.4f\n",
                   externalIndex, externalTemperature, incoming.temperature);
            done = true;
        } else if (incoming.type == MSG_TEMP_UPDATE){
            iteration++;
            float centralTemperature = incoming.temperature;
            printf("Iteration %d: central temperature %.4f received by external %d\n",
                   iteration, centralTemperature, externalIndex);
            externalTemperature = (3.0f * externalTemperature + 2.0f * centralTemperature) / 5.0f;
            printf("Iteration %d: external %d updated temperature to %.4f\n",
                   iteration, externalIndex, externalTemperature);

            outgoing = prepare_message(MSG_TEMP_UPDATE, externalIndex, externalTemperature);
            if (send_message(socket_desc, &outgoing) < 0){
                printf("Error while sending updated temperature\n");
                close(socket_desc);
                return EXIT_FAILURE;
            }
        } else {
            printf("Received unknown message type %d\n", incoming.type);
            close(socket_desc);
            return EXIT_FAILURE;
        }
    }
    
    // Close the socket:
    close(socket_desc);
    
    return 0;
}
