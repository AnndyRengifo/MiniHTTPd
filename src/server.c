/*
 * server.c - Funciones de sockets (TCP sobre IPv4).
 *
 * Secuencia de un servidor TCP:
 *   socket() -> setsockopt() -> bind() -> listen() -> accept() -> recv/send
 */
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#include "server.h"

int iniciar_servidor(int puerto) {
    int servidor;
    int opcion = 1;
    struct sockaddr_in direccion;

    /*
     * socket(dominio, tipo, protocolo)
     *   AF_INET     -> IPv4
     *   SOCK_STREAM -> TCP (flujo de bytes fiable y ordenado)
     *   0           -> protocolo por defecto para ese tipo (TCP)
     * Devuelve un descriptor de archivo (int).
     */
    servidor = socket(AF_INET, SOCK_STREAM, 0);
    if (servidor == -1) {
        perror("Error socket");
        return -1;
    }

    // SO_REUSEADDR: permite reusar el puerto al reiniciar el servidor
    // (si no, bind falla con "Address already in use" durante TIME_WAIT)
    if (setsockopt(servidor, SOL_SOCKET, SO_REUSEADDR,
                   &opcion, sizeof(opcion)) == -1) {
        perror("Error setsockopt");
        close(servidor);
        return -1;
    }

    /*
     * struct sockaddr_in {
     *     sa_family_t    sin_family;  // AF_INET
     *     in_port_t      sin_port;    // puerto en orden de red
     *     struct in_addr sin_addr;    // IP (INADDR_ANY = todas)
     * };
     * htons(): "host to network short", pasa el puerto a big-endian.
     */
    memset(&direccion, 0, sizeof(direccion));
    direccion.sin_family = AF_INET;
    direccion.sin_port = htons((uint16_t)puerto);
    direccion.sin_addr.s_addr = INADDR_ANY;

    // bind(): asocia el socket a la IP:puerto
    if (bind(servidor, (struct sockaddr *)&direccion,
             sizeof(direccion)) == -1) {
        perror("Error bind");
        close(servidor);
        return -1;
    }

    // listen(): el socket pasa a aceptar conexiones.
    // SOMAXCONN = tamaño máximo de la cola de conexiones pendientes.
    if (listen(servidor, SOMAXCONN) == -1) {
        perror("Error listen");
        close(servidor);
        return -1;
    }

    return servidor;
}

/*
 * Con epoll TODOS los sockets deben ser no bloqueantes.
 * fcntl F_GETFL lee las banderas actuales y F_SETFL las escribe
 * añadiendo O_NONBLOCK con un OR de bits (no se pierden las demás).
 */
int poner_no_bloqueante(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);

    if (flags == -1) {
        return -1;
    }
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

// accept(): saca un cliente de la cola y devuelve un fd NUEVO para él.
// NULL, NULL = no nos interesa la dirección del cliente.
// Devuelve -1 dejando errno intacto; EAGAIN es normal (no hay más clientes)
int aceptar_cliente(int servidor) {
    int uno = 1;
    int cliente = accept(servidor, NULL, NULL);

    // TCP_NODELAY desactiva el algoritmo de Nagle. Enviamos cabecera y
    // cuerpo en dos send(); con Nagle el 2º espera el ACK del 1º y el
    // cliente retrasa ese ACK ~40 ms => cada petición keep-alive tardaba 40 ms
    if (cliente != -1) {
        setsockopt(cliente, IPPROTO_TCP, TCP_NODELAY, &uno, sizeof(uno));
    }
    return cliente;
}

// Lectura cruda: no agrega '\0' ni imprime errores (main decide)
ssize_t recibir_datos(int cliente, char *buffer, size_t tamanio) {
    return recv(cliente, buffer, tamanio, 0);
}

// send() puede enviar MENOS bytes de los pedidos: repetimos hasta
// mandar todo (importante para archivos grandes como imágenes)
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
            // esperamos hasta 5 s a que se pueda escribir (POLLOUT)
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
