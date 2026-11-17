#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>
#include <limits.h>

#include "files.h"

int leer_archivo(const char *ruta, char **contenido, size_t *tamanio) {
    int archivo;
    struct stat datos;
    char *buffer;
    size_t total = 0;
    ssize_t bytes;

    archivo = open(ruta, O_RDONLY);

    if (archivo == -1) {
        return -1;
    }

    if (fstat(archivo, &datos) == -1 || !S_ISREG(datos.st_mode)) {
        close(archivo);
        return -1;
    }

    buffer = malloc(datos.st_size + 1);

    if (buffer == NULL) {
        close(archivo);
        return -1;
    }

    while (total < (size_t)datos.st_size) {
        bytes = read(archivo, buffer + total, datos.st_size - total);

        if (bytes <= 0) {
            free(buffer);
            close(archivo);
            return -1;
        }

        total += bytes;
    }

    buffer[total] = '\0';

    close(archivo);

    *contenido = buffer;
    *tamanio = total;

    return 0;
}

int resolver_ruta(const char *directorio, const char *ruta_url,
                  char *ruta_final, size_t capacidad) {
    char raiz[PATH_MAX];
    char temporal[PATH_MAX];
    char ruta_real[PATH_MAX];
    const char *ruta;
    const char *consulta;
    int longitud;
    size_t tamanio_raiz;

    if (realpath(directorio, raiz) == NULL) {
        return -1;
    }

    if (ruta_url[0] != '/') {
        return -1;
    }

    ruta = ruta_url + 1;

    consulta = strchr(ruta, '?');

    if (consulta != NULL) {
        longitud = consulta - ruta;
    } else {
        longitud = strlen(ruta);
    }

    if (longitud == 0) {
        ruta = "index.html";
        longitud = strlen(ruta);
    }

    if (longitud >= PATH_MAX) {
        return -1;
    }

    {
        char nombre[PATH_MAX];

        memcpy(nombre, ruta, longitud);
        nombre[longitud] = '\0';

        if (strstr(nombre, "..") != NULL) {
            return -1;
        }

        int escritos;

        escritos = snprintf(temporal, sizeof(temporal), "%s/%s",
                            raiz, nombre);

        if (escritos < 0 || (size_t)escritos >= sizeof(temporal)) {
            return -1;
        }
    }

    if (realpath(temporal, ruta_real) == NULL) {
        return -1;
    }

    tamanio_raiz = strlen(raiz);

    if (strncmp(raiz, ruta_real, tamanio_raiz) != 0 ||
        (ruta_real[tamanio_raiz] != '\0' &&
         ruta_real[tamanio_raiz] != '/')) {
        return -1;
    }

    if (strlen(ruta_real) >= capacidad) {
        return -1;
    }

    strcpy(ruta_final, ruta_real);

    return 0;
}
