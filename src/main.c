
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "server.h"
#include "http.h"

// Recibe la solicitud hasta completar cabeceras y cuerpo
static int recibir_solicitud_completa(int cliente,
                                      char *buffer,
                                      size_t capacidad,
                                      struct solicitud *solicitud,
                                      size_t *tamanio_total) {
    ssize_t bytes;
    size_t total = 0;
    size_t fin_cabeceras;
    size_t tamanio_esperado;
    char *fin;

    // Recibimos datos hasta encontrar el final de las cabeceras
    while (1) {

        if (total >= capacidad - 1) {
            return -1;
        }

        bytes = recibir_datos(cliente, buffer + total,
                              (int)(capacidad - total));

        if (bytes <= 0) {
            return -1;
        }

        total += (size_t)bytes;

        fin = strstr(buffer, "\r\n\r\n");

        if (fin != NULL) {
            fin_cabeceras = (size_t)(fin - buffer) + 4;
            break;
        }
    }

    // Analizamos las cabeceras para conocer Content-Length
    if (analizar_solicitud(buffer, solicitud) == -1) {
        return -1;
    }

    tamanio_esperado = fin_cabeceras;

    // Calculamos cuánto debe medir la solicitud completa
    if (solicitud->tiene_content_length) {

        if (solicitud->content_length >
            capacidad - fin_cabeceras - 1) {
            return -1;
        }

        tamanio_esperado += solicitud->content_length;
    }

    // Seguimos recibiendo hasta completar el cuerpo
    while (total < tamanio_esperado) {

        if (total >= capacidad - 1) {
            return -1;
        }

        bytes = recibir_datos(cliente, buffer + total,
                              (int)(capacidad - total));

        if (bytes <= 0) {
            return -1;
        }

        total += (size_t)bytes;
    }

    *tamanio_total = total;

    return 0;
}

int main(void) {
    int servidor;
    int cliente;

    char buffer[4096];
    char respuesta[1024];
    char nombre[128];
    char valor[256];
    char *inicio_cuerpo;
    char *fin_cabeceras;

    size_t tamanio_solicitud;
    size_t tamanio_respuesta;
    size_t tamanio_cabeceras;
    size_t tamanio_cuerpo;

    struct solicitud solicitud;

    servidor = iniciar_servidor(8080);

    if (servidor == -1) {
        perror("Error al iniciar el servidor");
        return 1;
    }

    printf("MiniHTTPd escuchando en el puerto 8080...\n");

    while (1) {

        cliente = aceptar_cliente(servidor);

        if (cliente == -1) {
            perror("Error al aceptar cliente");
            continue;
        }

        // Recibimos la solicitud completa
        if (recibir_solicitud_completa(cliente, buffer,
                                       sizeof(buffer), &solicitud,
                                       &tamanio_solicitud) == -1) {

            printf("Solicitud incompleta, invalida o demasiado grande\n");
            close(cliente);
            continue;
        }

        //locallizamos el final de las cabeceras
        fin_cabeceras = strstr(buffer, "\r\n\r\n");

        if (fin_cabeceras == NULL){
            printf("No se encontraron las cabeceras HTTP\n");
            close(cliente);
            continue;
        }

        //calculamos donde comienza el cuerpo
        inicio_cuerpo = fin_cabeceras + 4;

        tamanio_cabeceras = (size_t)(inicio_cuerpo - buffer);
        tamanio_cuerpo = tamanio_solicitud - tamanio_cabeceras;

        // Procesamos los datos enviados mediante POST
        if (strcmp(solicitud.metodo, "POST") == 0) {

            if (!solicitud.tiene_content_length ||
                tamanio_cuerpo != solicitud.content_length) {

                printf("El tamanio del cuerpo no coincide\n");
                close(cliente);
                continue;
            }

            if (analizar_formulario(inicio_cuerpo, tamanio_cuerpo,
                                    nombre, sizeof(nombre),
                                    valor, sizeof(valor)) == -1) {

                printf("No se pudo analizar el formulario\n");
                close(cliente);
                continue;
            }

            printf("\nDatos del formulario:\n");
            printf("Nombre del campo: %s\n", nombre);
            printf("Valor del campo: %s\n", valor);
        }

        printf("\nSolicitud recibida (%zu bytes):\n",
               tamanio_solicitud);
               
        printf("Tamanio de cabeceras: %zu bytes\n",
               tamanio_cabeceras);

        printf("Tamanio del cuerpo: %zu bytes\n",
               tamanio_cuerpo);

        printf("Cuerpo recibido: ");

        if (tamanio_cuerpo > 0){
            fwrite(inicio_cuerpo, 1, tamanio_cuerpo, stdout);
        }

        printf("\n");

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

        // Enviamos la respuesta
        if (enviar_datos(cliente, respuesta,
                         tamanio_respuesta) == -1) {
            perror("Error al enviar respuesta");
        }

        close(cliente);
    }

    close(servidor);

    return 0;
}
