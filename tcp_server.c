#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "utils.h"

static int establish_connections(int client_sockets[NUM_EXTERNALS])
{
    int listener = socket(AF_INET, SOCK_STREAM, 0);
    if (listener < 0) {
        printf("Error while creating socket\n");
        exit(EXIT_FAILURE);
    }
    printf("Socket created successfully\n");

    int opt = 1;
    if (setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        printf("Warning: unable to set SO_REUSEADDR\n");
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(2000);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (bind(listener, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        printf("Couldn't bind to the port\n");
        close(listener);
        exit(EXIT_FAILURE);
    }
    printf("Done with binding\n");

    if (listen(listener, NUM_EXTERNALS) < 0) {
        printf("Error while listening\n");
        close(listener);
        exit(EXIT_FAILURE);
    }
    printf("\n\nListening for incoming connections.....\n\n");
    printf("-------------------- Initial connections ---------------------------------\n");

    struct sockaddr_in client_addr;
    socklen_t client_size = sizeof(client_addr);

    for (int i = 0; i < NUM_EXTERNALS; ++i) {
        client_sockets[i] = accept(listener, (struct sockaddr *)&client_addr, &client_size);
        if (client_sockets[i] < 0) {
            printf("Can't accept\n");
            close(listener);
            exit(EXIT_FAILURE);
        }
        printf("External process connected from %s:%i\n",
               inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
        client_size = sizeof(client_addr);
    }
    printf("--------------------------------------------------------------------------\n");
    printf("All four external processes are now connected\n");
    printf("--------------------------------------------------------------------------\n\n");

    return listener;
}

int main(int argc, char *argv[])
{
    if (argc != 2) {
        printf("Usage: %s <initial central temperature>\n", argv[0]);
        return EXIT_FAILURE;
    }

    float central_temperature = atof(argv[1]);

    int client_sockets[NUM_EXTERNALS];
    for (int i = 0; i < NUM_EXTERNALS; ++i) {
        client_sockets[i] = -1;
    }

    int listener = establish_connections(client_sockets);
    int exit_status = EXIT_FAILURE;

    printf("Initial central temperature: %.4f\n", central_temperature);

    float previous_external[NUM_EXTERNALS] = {0};
    bool have_previous = false;
    bool stabilized = false;
    int iteration = 0;

    while (!stabilized) {
        iteration++;

        float external[NUM_EXTERNALS] = {0};
        bool received[NUM_EXTERNALS] = {false};

        for (int i = 0; i < NUM_EXTERNALS; ++i) {
            struct msg incoming;
            if (receive_message(client_sockets[i], &incoming) < 0) {
                printf("Error: Couldn't receive message from external process\n");
                goto cleanup;
            }
            if (incoming.type != MSG_TEMP_UPDATE) {
                printf("Warning: Unexpected message type %d from external %d\n",
                       incoming.type, incoming.index);
                goto cleanup;
            }
            int idx = incoming.index - 1;
            if (idx < 0 || idx >= NUM_EXTERNALS) {
                printf("Warning: Received invalid external index %d\n", incoming.index);
                goto cleanup;
            }
            external[idx] = incoming.temperature;
            received[idx] = true;
            printf("Iteration %d: external %d reported %.4f\n",
                   iteration, incoming.index, incoming.temperature);
        }

        for (int i = 0; i < NUM_EXTERNALS; ++i) {
            if (!received[i]) {
                printf("Error: Missing temperature from external process %d\n", i + 1);
                goto cleanup;
            }
        }

        float sum = 0.0f;
        for (int i = 0; i < NUM_EXTERNALS; ++i) {
            sum += external[i];
        }

        float updated_central = (2.0f * central_temperature + sum) / 6.0f;
        printf("Iteration %d: central temperature updated from %.4f to %.4f\n",
               iteration, central_temperature, updated_central);

        bool iteration_stable = false;
        if (have_previous) {
            iteration_stable = fabsf(updated_central - central_temperature) < EPSILON;
            for (int i = 0; i < NUM_EXTERNALS && iteration_stable; ++i) {
                if (fabsf(external[i] - previous_external[i]) >= EPSILON) {
                    iteration_stable = false;
                }
            }
        }

        struct msg response;
        if (iteration_stable) {
            printf("System stabilized after %d iterations.\n", iteration);
            response = prepare_message(MSG_DONE, 0, updated_central);
            stabilized = true;
            exit_status = EXIT_SUCCESS;
        } else {
            response = prepare_message(MSG_TEMP_UPDATE, 0, updated_central);
        }

        for (int i = 0; i < NUM_EXTERNALS; ++i) {
            if (send_message(client_sockets[i], &response) < 0) {
                printf("Error: Can't send message to external process %d\n", i + 1);
                goto cleanup;
            }
        }

        for (int i = 0; i < NUM_EXTERNALS; ++i) {
            previous_external[i] = external[i];
        }
        central_temperature = updated_central;
        have_previous = true;

        printf("\n");
    }

    printf("Final central temperature: %.4f\n", central_temperature);

cleanup:
    for (int i = 0; i < NUM_EXTERNALS; ++i) {
        if (client_sockets[i] >= 0) {
            close(client_sockets[i]);
            client_sockets[i] = -1;
        }
    }
    if (listener >= 0) {
        close(listener);
    }

    return exit_status;
}
