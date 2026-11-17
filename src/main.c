#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "server.h"
#include "http.h"

int main(void) {
    int servidor;
    int cliente;

    char buffer[4096];
    char respuesta[1024];

    ssize_t bytes;
    size_t tamanio_respuesta;

    struct solicitud solicitud;

    // Iniciamos el servidor en el puerto 8080
    servidor = iniciar_servidor(8080);

    if (servidor == -1) {
        perror("Error al iniciar el servidor");
        return 1;
    }

    printf("MiniHTTPd escuchando en el puerto 8080...\n");

    while (1) {

        // Esperamos una conexión de un cliente
        cliente = aceptar_cliente(servidor);

        if (cliente == -1) {
            perror("Error al aceptar cliente");
            continue;
        }

        // Recibimos los datos enviados por el cliente
        bytes = recibir_datos(cliente, buffer, sizeof(buffer));

        if (bytes <= 0) {
            if (bytes == -1) {
                perror("Error al recibir datos");
            }

            close(cliente);
            continue;
        }

        printf("\nSolicitud recibida:\n");
        printf("%s\n", buffer);

        // Analizamos la solicitud HTTP
        if (analizar_solicitud(buffer, &solicitud) == -1) {
            printf("Solicitud HTTP no valida\n");
            close(cliente);
            continue;
        }

        // Mostramos los datos extraídos
        printf("Metodo: %s\n", solicitud.metodo);
        printf("Ruta: %s\n", solicitud.ruta);
        printf("Version: %s\n", solicitud.version);
        printf("Host: %s\n", solicitud.host);
        printf("User-Agent: %s\n", solicitud.user_agent);

        if (solicitud.tiene_content_length) {
            printf("Content-Length: %zu\n",
                   solicitud.content_length);
        } else {
            printf("Content-Length: no especificado\n");
        }

        // Construimos la respuesta HTTP
        if (construir_respuesta(respuesta, sizeof(respuesta),
                                &tamanio_respuesta) == -1) {
            printf("Error al construir respuesta\n");
            close(cliente);
            continue;
        }

        // Enviamos la respuesta al cliente
        if (enviar_datos(cliente, respuesta,
                         tamanio_respuesta) == -1) {
            perror("Error al enviar respuesta");
        }

        // Cerramos la conexión con el cliente
        close(cliente);
    }

    close(servidor);

    return 0;
}