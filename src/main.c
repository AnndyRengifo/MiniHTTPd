#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <sys/socket.h>
#include <sys/epoll.h>

#include "server.h"
#include "http.h"

#define MAX_EVENTOS 64

// Estado de UNA conexión: sobrevive entre llamadas a epoll_wait
struct conexion {
    int    fd;
    size_t usado;                    // bytes acumulados en buffer
    char   buffer[MAX_CABECERA + 1]; // +1 para el '\0'
};

static volatile sig_atomic_t terminar = 0;

static void manejar_sigint(int senal) {
    (void)senal;
    terminar = 1;
}

static void cerrar_conexion(int epfd, struct conexion *c) {
    epoll_ctl(epfd, EPOLL_CTL_DEL, c->fd, NULL);
    close(c->fd);
    free(c);
}

static void aceptar_nuevos(int epfd, int servidor) {
    while (1) {                       // pueden haber llegado varios
        struct conexion *c;
        struct epoll_event ev;
        int cliente = aceptar_cliente(servidor);

        if (cliente == -1) {
            if (errno == EINTR) {
                continue;
            }
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                perror("Error accept");
            }
            return;
        }

        if (poner_no_bloqueante(cliente) == -1) {
            close(cliente);
            continue;
        }

        c = calloc(1, sizeof(*c));
        if (c == NULL) {
            close(cliente);
            continue;
        }
        c->fd = cliente;

        ev.events = EPOLLIN;
        ev.data.ptr = c;              // así recuperamos la conexión

        if (epoll_ctl(epfd, EPOLL_CTL_ADD, cliente, &ev) == -1) {
            perror("Error epoll_ctl");
            close(cliente);
            free(c);
        }
    }
}

static void atender_cliente(int epfd, struct conexion *c,
                            const char *raiz) {
    ssize_t n;
    char *fin;
    size_t tam;

    n = recibir_datos(c->fd, c->buffer + c->usado,
                      MAX_CABECERA - c->usado);

    if (n == 0) {                     // el cliente cerró
        cerrar_conexion(epfd, c);
        return;
    }
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
            return;                   // nada que leer por ahora
        }
        cerrar_conexion(epfd, c);
        return;
    }

    c->usado += (size_t)n;

    // Puede haber 0, 1 o varias peticiones completas en el buffer
    while ((fin = memmem(c->buffer, c->usado, "\r\n\r\n", 4)) != NULL) {

        tam = (size_t)(fin - c->buffer) + 4;

        if (!procesar_peticion(c->fd, c->buffer, tam, raiz)) {
            cerrar_conexion(epfd, c);
            return;
        }

        // Descartamos la petición atendida; el resto pasa al inicio
        memmove(c->buffer, c->buffer + tam, c->usado - tam);
        c->usado -= tam;
    }

    // Buffer lleno y sin fin de cabeceras: petición demasiado grande
    if (c->usado >= MAX_CABECERA) {
        printf("- - 400 \"cabeceras demasiado grandes\"\n");
        enviar_error(c->fd, 400, 0);
        cerrar_conexion(epfd, c);
    }
}

int main(int argc, char *argv[]) {
    int puerto = 8080;
    const char *raiz = "www";
    int servidor, epfd, cantidad;
    struct epoll_event ev, eventos[MAX_EVENTOS];
    struct sigaction sa;

    if (argc > 1) {
        char *fin;
        long p = strtol(argv[1], &fin, 10);
        if (*fin != '\0' || p < 1 || p > 65535) {
            fprintf(stderr, "Uso: %s [puerto] [carpeta]\n", argv[0]);
            return 1;
        }
        puerto = (int)p;
    }
    if (argc > 2) {
        raiz = argv[2];
    }

    // Un cliente que se desconecta no debe matar al servidor
    signal(SIGPIPE, SIG_IGN);

    // Ctrl+C sale del bucle limpiamente (sin SA_RESTART, para que
    // epoll_wait devuelva EINTR y revisemos la bandera)
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = manejar_sigint;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    servidor = iniciar_servidor(puerto);
    if (servidor == -1) {
        return 1;
    }
    if (poner_no_bloqueante(servidor) == -1) {
        perror("Error fcntl");
        close(servidor);
        return 1;
    }

    epfd = epoll_create1(0);
    if (epfd == -1) {
        perror("Error epoll_create1");
        close(servidor);
        return 1;
    }

    // data.ptr == NULL identifica al socket servidor
    ev.events = EPOLLIN;
    ev.data.ptr = NULL;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, servidor, &ev) == -1) {
        perror("Error epoll_ctl");
        close(epfd);
        close(servidor);
        return 1;
    }

    printf("MiniHTTPd en http://localhost:%d  (carpeta: %s)\n",
           puerto, raiz);

    while (!terminar) {
        cantidad = epoll_wait(epfd, eventos, MAX_EVENTOS, -1);

        if (cantidad == -1) {
            if (errno == EINTR) {
                continue;
            }
            perror("Error epoll_wait");
            break;
        }

        for (int i = 0; i < cantidad; i++) {
            struct conexion *c = eventos[i].data.ptr;

            if (c == NULL) {
                aceptar_nuevos(epfd, servidor);
            } else {
                atender_cliente(epfd, c, raiz);
            }
        }
    }

    printf("\nCerrando servidor...\n");
    close(epfd);
    close(servidor);
    return 0;
}