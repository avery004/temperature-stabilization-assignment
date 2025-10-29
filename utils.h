#ifndef UTILS_H
#define UTILS_H

#include <stddef.h>

#define NUM_EXTERNALS 4
#define MSG_TEMP_UPDATE 1
#define MSG_DONE 2
#define EPSILON 0.001f

struct msg {
    int type;
    int index;
    float temperature;
};

struct msg prepare_message(int type, int index, float temperature);
int send_message(int socket_fd, const struct msg *message);
int receive_message(int socket_fd, struct msg *message);

#endif
