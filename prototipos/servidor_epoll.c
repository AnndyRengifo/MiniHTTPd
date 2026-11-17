
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/epoll.h>

int main() {
    int servidor, cliente, epoll_fd;
    int flags, cantidad, bytes;
    struct sockaddr_in direccion;
    struct epoll_event evento, eventos[10];
    char mensaje[1024];

    servidor = socket(AF_INET, SOCK_STREAM, 0);

    if (servidor == -1) {
        perror("Error socket");
        return 1;
    }

    int opcion = 1;
    setsockopt(servidor, SOL_SOCKET, SO_REUSEADDR,
               &opcion, sizeof(opcion));

    direccion.sin_family = AF_INET;
    direccion.sin_port = htons(8080);
    direccion.sin_addr.s_addr = INADDR_ANY;

    if (bind(servidor, (struct sockaddr *)&direccion,
             sizeof(direccion)) == -1) {
        perror("Error bind");
        close(servidor);
        return 1;
    }

    if (listen(servidor, 10) == -1) {
        perror("Error listen");
        close(servidor);
        return 1;
    }

    flags = fcntl(servidor, F_GETFL, 0);
    fcntl(servidor, F_SETFL, flags | O_NONBLOCK);

    epoll_fd = epoll_create1(0);

    if (epoll_fd == -1) {
        perror("Error epoll_create1");
        close(servidor);
        return 1;
    }

    evento.events = EPOLLIN;
    evento.data.fd = servidor;

    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, servidor, &evento);

    printf("Servidor escuchando en el puerto 8080...\n");

    while (1) {
        cantidad = epoll_wait(epoll_fd, eventos, 10, -1);

        if (cantidad == -1) {
            if (errno == EINTR) continue;
            perror("Error epoll_wait");
            break;
        }

        for (int i = 0; i < cantidad; i++) {

            if (eventos[i].data.fd == servidor) {

                while (1) {
                    cliente = accept(servidor, NULL, NULL);

                    if (cliente == -1) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK)
                            break;
                        if (errno == EINTR) continue;
                        perror("Error accept");
                        break;
                    }

                    flags = fcntl(cliente, F_GETFL, 0);
                    fcntl(cliente, F_SETFL, flags | O_NONBLOCK);

                    evento.events = EPOLLIN | EPOLLRDHUP;
                    evento.data.fd = cliente;

                    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD,
                                  cliente, &evento) == -1) {
                        perror("Error epoll_ctl");
                        close(cliente);
                        continue;
                    }

                    printf("Nuevo cliente: FD %d\n", cliente);
                }

            } else {

                cliente = eventos[i].data.fd;
                int cerrar = 0;

                if (eventos[i].events & EPOLLIN) {

                    while (1) {
                        bytes = recv(cliente, mensaje,
                                     sizeof(mensaje) - 1, 0);

                        if (bytes > 0) {
                            mensaje[bytes] = '\0';
                            printf("Cliente %d: %s", cliente, mensaje);
                            fflush(stdout);
                        } else if (bytes == 0) {
                            cerrar = 1;
                            break;
                        } else if (errno == EAGAIN ||
                                   errno == EWOULDBLOCK) {
                            break;
                        } else if (errno == EINTR) {
                            continue;
                        } else {
                            perror("Error recv");
                            cerrar = 1;
                            break;
                        }
                    }
                }

                if (eventos[i].events & (EPOLLERR | EPOLLHUP |
                                         EPOLLRDHUP))
                    cerrar = 1;

                if (cerrar) {
                    printf("Cliente desconectado: FD %d\n", cliente);
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, cliente, NULL);
                    close(cliente);
                }
            }
        }
    }

    close(epoll_fd);
    close(servidor);
    return 0;
}
