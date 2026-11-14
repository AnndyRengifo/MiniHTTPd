
#include <stdio.h>
#include <unistd.h>
#include <string.h>
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

    setsockopt(servidor, SOL_SOCKET, SO_REUSEADDR,  // facilita las pruebas al evitar el puerto
               &opcion, sizeof(opcion));               

    memset(&direccion, 0, sizeof(direccion));

    direccion.sin_family = AF_INET;
    direccion.sin_port = htons(puerto);
    direccion.sin_addr.s_addr = INADDR_ANY; //el server escucha todas las interfaces IPV4

    if (bind(servidor, (struct sockaddr *)&direccion,
             sizeof(direccion)) == -1) {
        perror("Error bind");
        close(servidor);
        return -1;
    }

    if (listen(servidor, 10) == -1) {
        perror("Error listen");
        close(servidor);
        return -1;
    }

    printf("Servidor escuchando en puerto %d\n", puerto);

    return servidor;
}

int aceptar_cliente(int servidor) {
    int cliente;

    cliente = accept(servidor, NULL, NULL);

    if (cliente == -1) {
        perror("Error accept");
        return -1;
    }

    printf("Cliente conectado\n");

    return cliente;
}
