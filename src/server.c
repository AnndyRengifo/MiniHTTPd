#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "server.h"

int iniciar_servidor(int puerto) {
    int servidor;
    int opcion = 1;
    struct sockaddr_in direccion;

    servidor = socket(AF_INET, SOCK_STREAM, 0);
    if (servidor == -1) {
        perror("Error socket");
        return -1;
    }

    if (setsockopt(servidor, SOL_SOCKET, SO_REUSEADDR,
                   &opcion, sizeof(opcion)) == -1) {
        perror("Error setsockopt");
        close(servidor);
        return -1;
    }

    memset(&direccion, 0, sizeof(direccion));
    direccion.sin_family = AF_INET;
    direccion.sin_port = htons((uint16_t)puerto);
    direccion.sin_addr.s_addr = INADDR_ANY;

    if (bind(servidor, (struct sockaddr *)&direccion,
             sizeof(direccion)) == -1) {
        perror("Error bind");
        close(servidor);
        return -1;
    }

    if (listen(servidor, SOMAXCONN) == -1) {
        perror("Error listen");
        close(servidor);
        return -1;
    }

    return servidor;
}

// Con epoll TODOS los sockets deben ser no bloqueantes
int poner_no_bloqueante(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);

    if (flags == -1) {
        return -1;
    }
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

// Devuelve -1 dejando errno intacto; EAGAIN es normal (no hay más clientes)
int aceptar_cliente(int servidor) {
    return accept(servidor, NULL, NULL);
}

// Lectura cruda: no agrega '\0' ni imprime errores (main decide)
ssize_t recibir_datos(int cliente, char *buffer, size_t tamanio) {
    return recv(cliente, buffer, tamanio, 0);
}

ssize_t enviar_datos(int cliente, const char *buffer, size_t tamanio) {
    size_t enviados = 0;
    ssize_t bytes;

    while (enviados < tamanio) {

        // MSG_NOSIGNAL: si el cliente cerró, send falla con EPIPE
        // en vez de matar el proceso con SIGPIPE
        bytes = send(cliente, buffer + enviados,
                     tamanio - enviados, MSG_NOSIGNAL);

        if (bytes == -1) {
            if (errno == EINTR) {
                continue;
            }

            // Buffer de envío lleno (socket no bloqueante):
            // esperamos hasta 5 s a que se pueda escribir
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                struct pollfd pfd;
                pfd.fd = cliente;
                pfd.events = POLLOUT;
                pfd.revents = 0;

                if (poll(&pfd, 1, 5000) <= 0) {
                    return -1;
                }
                continue;
            }
            return -1;
        }

        if (bytes == 0) {
            break;
        }
        enviados += (size_t)bytes;
    }

    return (ssize_t)enviados;
}