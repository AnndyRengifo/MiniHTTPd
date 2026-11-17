/*
 * main.c - Bucle de eventos con epoll.
 *
 * Idea general (modelo event-driven, UN solo hilo):
 *   1. Crear el socket servidor (server.c) y ponerlo no bloqueante.
 *   2. Crear una instancia epoll y registrar el socket servidor.
 *   3. Bucle: epoll_wait() nos dice QUÉ sockets tienen datos listos.
 *        - Si es el servidor  -> hay clientes nuevos: accept().
 *        - Si es un cliente   -> hay bytes para leer: recv() + HTTP.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/epoll.h>

#include "server.h"
#include "http.h"

#define MAX_EVENTOS 64   // máximo de eventos que devuelve un epoll_wait

/*
 * Estado de UNA conexión. Se guarda en el heap (calloc) y su puntero
 * se mete en epoll_event.data.ptr, así cuando epoll nos avisa
 * recuperamos directamente la conexión con sus bytes acumulados.
 * Es necesario porque una petición puede llegar en varios trozos.
 */
struct conexion {
    int    fd;                       // descriptor del socket del cliente
    size_t usado;                    // bytes acumulados en buffer
    char   buffer[MAX_CABECERA + 1]; // +1 para poder poner '\0'
};

// sig_atomic_t: tipo seguro para modificar dentro de un manejador de señal
static volatile sig_atomic_t terminar = 0;

static void manejar_sigint(int senal) {
    (void)senal;
    terminar = 1;
}

static void cerrar_conexion(int epfd, struct conexion *c) {
    epoll_ctl(epfd, EPOLL_CTL_DEL, c->fd, NULL);  // dejar de vigilarlo
    close(c->fd);
    free(c);
}

// El servidor está "listo" => hay uno o VARIOS clientes en la cola
// de listen(). Aceptamos en bucle hasta que accept() dé EAGAIN.
static void aceptar_nuevos(int epfd, int servidor) {
    while (1) {
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
            return;                   // EAGAIN: ya no hay más en cola
        }

        if (poner_no_bloqueante(cliente) == -1) {
            close(cliente);
            continue;
        }

        c = calloc(1, sizeof(*c));    // calloc deja usado = 0
        if (c == NULL) {
            close(cliente);
            continue;
        }
        c->fd = cliente;

        /*
         * struct epoll_event {
         *     uint32_t     events;  // banderas: qué vigilar
         *     epoll_data_t data;    // dato nuestro (unión: fd o ptr)
         * };
         * EPOLLIN = avisar cuando haya datos para leer.
         * Modo por defecto: level-triggered (avisa mientras queden datos).
         */
        ev.events = EPOLLIN;
        ev.data.ptr = c;

        // EPOLL_CTL_ADD: añadir este socket a la lista vigilada
        if (epoll_ctl(epfd, EPOLL_CTL_ADD, cliente, &ev) == -1) {
            perror("Error epoll_ctl");
            close(cliente);
            free(c);
        }
    }
}

// Un cliente tiene datos: leemos y atendemos las peticiones completas
static void atender_cliente(int epfd, struct conexion *c,
                            const char *raiz) {
    ssize_t n;
    char *fin;
    size_t tam;

    // Leemos a continuación de lo que ya teníamos (nunca más allá del
    // tamaño del buffer => no hay buffer overflow)
    n = recibir_datos(c->fd, c->buffer + c->usado,
                      MAX_CABECERA - c->usado);

    if (n == 0) {                     // recv() == 0: el cliente cerró
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

    // Una petición HTTP termina en una línea vacía: "\r\n\r\n".
    // Puede haber 0, 1 o varias (pipelining) en el buffer.
    while ((fin = memmem(c->buffer, c->usado, "\r\n\r\n", 4)) != NULL) {

        tam = (size_t)(fin - c->buffer) + 4;

        // procesar_peticion devuelve 1 = keep-alive, 0 = cerrar
        if (!procesar_peticion(c->fd, c->buffer, tam, raiz)) {
            cerrar_conexion(epfd, c);
            return;
        }

        // Descartamos la petición atendida; el resto pasa al inicio
        memmove(c->buffer, c->buffer + tam, c->usado - tam);
        c->usado -= tam;
    }

    // Buffer lleno y sin "\r\n\r\n": cabeceras demasiado grandes -> 400
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

    // Uso: ./minihttpd [puerto] [carpeta]
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

    // SIGPIPE: escribir a un cliente que ya cerró mataría el proceso
    signal(SIGPIPE, SIG_IGN);

    // Ctrl+C (SIGINT) y SIGTERM salen del bucle limpiamente.
    // Sin SA_RESTART: epoll_wait devuelve EINTR y revisamos la bandera.
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = manejar_sigint;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    // socket() + bind() + listen()  (ver server.c)
    servidor = iniciar_servidor(puerto);
    if (servidor == -1) {
        return 1;
    }
    // No bloqueante: accept() devuelve EAGAIN en vez de quedarse esperando
    if (poner_no_bloqueante(servidor) == -1) {
        perror("Error fcntl");
        close(servidor);
        return 1;
    }

    // epoll_create1(0): crea la instancia epoll, devuelve un fd
    epfd = epoll_create1(0);
    if (epfd == -1) {
        perror("Error epoll_create1");
        close(servidor);
        return 1;
    }

    // Registramos el servidor. data.ptr == NULL lo identifica
    // (los clientes tienen un puntero a su struct conexion).
    ev.events = EPOLLIN;              // "listo" = hay clientes por aceptar
    ev.data.ptr = NULL;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, servidor, &ev) == -1) {
        perror("Error epoll_ctl");
        close(epfd);
        close(servidor);
        return 1;
    }

    printf("MiniHTTPd en http://localhost:%d  (carpeta: %s)\n",
           puerto, raiz);

    // ---------------- Bucle de eventos ----------------
    while (!terminar) {
        // Espera (timeout -1 = sin límite) y llena 'eventos' con
        // los sockets listos. Devuelve cuántos hay.
        cantidad = epoll_wait(epfd, eventos, MAX_EVENTOS, -1);

        if (cantidad == -1) {
            if (errno == EINTR) {     // interrumpido por una señal
                continue;
            }
            perror("Error epoll_wait");
            break;
        }

        for (int i = 0; i < cantidad; i++) {
            struct conexion *c = eventos[i].data.ptr;

            if (c == NULL) {
                aceptar_nuevos(epfd, servidor);   // socket servidor
            } else {
                atender_cliente(epfd, c, raiz);   // socket cliente
            }
        }
    }

    printf("\nCerrando servidor...\n");
    close(epfd);
    close(servidor);
    return 0;
}
