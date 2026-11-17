#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <stdint.h>

#include "http.h"

// Copia un campo verificando que tenga espacio suficiente
static int copiar_campo(char *destino, size_t capacidad,
                        const char *origen, size_t longitud) {

    if (longitud >= capacidad) {
        return -1;
    }

    memcpy(destino, origen, longitud);
    destino[longitud] = '\0';

    return 0;
}

// Elimina espacios y tabulaciones al inicio y al final
static void limpiar_espacios(char *texto) {
    char *inicio = texto;
    size_t longitud;

    while (*inicio == ' ' || *inicio == '\t') {
        inicio++;
    }

    memmove(texto, inicio, strlen(inicio) + 1);

    longitud = strlen(texto);

    while (longitud > 0 &&
           (texto[longitud - 1] == ' ' ||
            texto[longitud - 1] == '\t')) {
        texto[--longitud] = '\0';
    }
}

int analizar_solicitud(const char *buffer, struct solicitud *solicitud) {
    const char *linea;
    const char *fin;
    const char *fin_linea;
    const char *separador;
    const char *dos_puntos;

    char nombre[64];
    char valor[512];
    char *fin_numero;

    unsigned long long numero;
    size_t longitud;

    memset(solicitud, 0, sizeof(*solicitud));

    // Localizamos el final de la primera línea
    fin_linea = strstr(buffer, "\r\n");

    if (fin_linea == NULL) {
        return -1;
    }

    // Extraemos método, ruta y versión
    if (sscanf(buffer, "%7s %255s %15s",
               solicitud->metodo,
               solicitud->ruta,
               solicitud->version) != 3) {
        return -1;
    }

    // Comprobamos que existan todas las cabeceras
    fin = strstr(buffer, "\r\n\r\n");

    if (fin == NULL) {
        return -1;
    }

    linea = fin_linea + 2;

    while (linea < fin) {

        separador = strstr(linea, "\r\n");

        if (separador == NULL || separador > fin) {
            return -1;
        }

        dos_puntos = memchr(linea, ':', separador - linea);

        if (dos_puntos == NULL || dos_puntos == linea) {
            return -1;
        }

        // Copiamos el nombre de la cabecera
        longitud = dos_puntos - linea;

        if (copiar_campo(nombre, sizeof(nombre),
                         linea, longitud) == -1) {
            return -1;
        }

        // Copiamos el valor de la cabecera
        longitud = separador - dos_puntos - 1;

        if (copiar_campo(valor, sizeof(valor),
                         dos_puntos + 1, longitud) == -1) {
            return -1;
        }

        limpiar_espacios(valor);

        // Los nombres de cabecera no distinguen mayúsculas
        for (size_t i = 0; nombre[i] != '\0'; i++) {
            nombre[i] = tolower((unsigned char)nombre[i]);
        }

        if (strcmp(nombre, "host") == 0) {
            if (copiar_campo(solicitud->host,
                             sizeof(solicitud->host),
                             valor, strlen(valor)) == -1) {
                return -1;
            }
        }

        if (strcmp(nombre, "user-agent") == 0) {
            if (copiar_campo(solicitud->user_agent,
                             sizeof(solicitud->user_agent),
                             valor, strlen(valor)) == -1) {
                return -1;
            }
        }

        if (strcmp(nombre, "content-length") == 0) {

            // Content-Length debe contener solamente dígitos
            if (valor[0] == '\0') {
                return -1;
            }

            for (size_t i = 0; valor[i] != '\0'; i++) {
                if (!isdigit((unsigned char)valor[i])) {
                    return -1;
                }
            }

            errno = 0;
            numero = strtoull(valor, &fin_numero, 10);

            if (errno != 0 || *fin_numero != '\0' ||
                numero > (unsigned long long)SIZE_MAX) {
                return -1;
            }

            solicitud->content_length = (size_t)numero;
            solicitud->tiene_content_length = 1;
        }

        linea = separador + 2;
    }

    // Validamos la versión HTTP
    if (strcmp(solicitud->version, "HTTP/1.1") != 0 &&
        strcmp(solicitud->version, "HTTP/1.0") != 0) {
        return -1;
    }

    return 0;
}

// Construye una respuesta HTTP fija
int construir_respuesta(char *buffer, size_t capacidad, size_t *tamanio) {
    const char *cuerpo = "Hola desde MiniHTTPd\n";
    int cabecera;
    size_t longitud_cuerpo = strlen(cuerpo);

    cabecera = snprintf(buffer, capacidad,
                        "HTTP/1.1 200 OK\r\n"
                        "Content-Type: text/plain\r\n"
                        "Content-Length: %zu\r\n"
                        "Connection: close\r\n"
                        "\r\n",
                        longitud_cuerpo);

    if (cabecera < 0 || (size_t)cabecera >= capacidad) {
        return -1;
    }

    if ((size_t)cabecera + longitud_cuerpo > capacidad) {
        return -1;
    }

    memcpy(buffer + cabecera, cuerpo, longitud_cuerpo);

    *tamanio = (size_t)cabecera + longitud_cuerpo;

    return 0;
}

//Extrae el primer campo de un formulario nombre=valor
int analizar_formulario(const char *cuerpo, size_t tamanio,
                        char *nombre, size_t capacidad_nombre,
                        char *valor, size_t capacidad_valor) {

    const char *igual;
    const char *separador;
    size_t longitud_nombre;
    size_t longitud_valor;

    // Buscamos el signo =
    igual = memchr(cuerpo, '=', tamanio);

    if (igual == NULL || igual == cuerpo) {
        return -1;
    }

    // Calculamos el tamaño del nombre
    longitud_nombre = (size_t)(igual - cuerpo);

    // Buscamos el siguiente campo separado por &
    separador = memchr(igual + 1, '&',
                       tamanio - longitud_nombre - 1);

    if (separador != NULL) {
        longitud_valor = (size_t)(separador - igual - 1);
    } else {
        longitud_valor = tamanio - longitud_nombre - 1;
    }

    // Comprobamos que los campos tengan espacio suficiente
    if (longitud_nombre >= capacidad_nombre ||
        longitud_valor >= capacidad_valor) {
        return -1;
    }

    // Copiamos el nombre y el valor
    memcpy(nombre, cuerpo, longitud_nombre);
    nombre[longitud_nombre] = '\0';

    memcpy(valor, igual + 1, longitud_valor);
    valor[longitud_valor] = '\0';

    return 0;
}