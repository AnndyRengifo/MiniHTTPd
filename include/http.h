#ifndef HTTP_H
#define HTTP_H

#include <stddef.h>

struct solicitud {
    char metodo[8];
    char ruta[256];
    char version[16];
    char host[256];
    char user_agent[256];
    size_t content_length;
    int tiene_content_length;
};

int analizar_solicitud(const char *buffer, struct solicitud *solicitud);

int construir_respuesta(char *buffer, size_t capacidad, size_t *tamanio);

#endif