#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include "utils.h"


struct msg prepare_message(int type, int index, float temperature)
{
    struct msg message;
    message.type = type;
    message.index = index;
    message.temperature = temperature;
    return message;
}

static int transfer_all(int socket_fd, void *buffer, size_t length, int is_send)
{
    size_t transferred = 0;
    char *data = buffer;

    while (transferred < length) {
        ssize_t result;
        if (is_send) {
            result = send(socket_fd, data + transferred, length - transferred, 0);
        } else {
            result = recv(socket_fd, data + transferred, length - transferred, 0);
        }

        if (result <= 0) {
            return -1;
        }

        transferred += (size_t)result;
    }

    return 0;
}

int send_message(int socket_fd, const struct msg *message)
{
    struct msg copy = *message;
    return transfer_all(socket_fd, &copy, sizeof(copy), 1);
}

int receive_message(int socket_fd, struct msg *message)
{
    return transfer_all(socket_fd, message, sizeof(*message), 0);
}
