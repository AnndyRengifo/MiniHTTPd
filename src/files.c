#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
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

    buffer = malloc((size_t)datos.st_size + 1);
    if (buffer == NULL) {
        close(archivo);
        return -1;
    }

    while (total < (size_t)datos.st_size) {
        bytes = read(archivo, buffer + total,
                     (size_t)datos.st_size - total);
        if (bytes <= 0) {
            free(buffer);
            close(archivo);
            return -1;
        }
        total += (size_t)bytes;
    }

    buffer[total] = '\0';
    close(archivo);

    *contenido = buffer;
    *tamanio = total;
    return 0;
}

static int valor_hex(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

// Decodifica %XX. -1 si el formato es inválido o aparece %00.
// Se decodifica ANTES de buscar ".." y de llamar a realpath().
static int decodificar_url(const char *origen, size_t longitud,
                           char *destino, size_t capacidad) {
    size_t j = 0;

    for (size_t i = 0; i < longitud; i++) {
        char c = origen[i];

        if (c == '%') {
            int alto, bajo;

            if (i + 2 >= longitud) {
                return -1;
            }
            alto = valor_hex(origen[i + 1]);
            bajo = valor_hex(origen[i + 2]);
            if (alto < 0 || bajo < 0) {
                return -1;
            }
            c = (char)(alto * 16 + bajo);
            if (c == '\0') {
                return -1;
            }
            i += 2;
        }

        if (j + 1 >= capacidad) {
            return -1;
        }
        destino[j++] = c;
    }

    destino[j] = '\0';
    return 0;
}

// 1 si ruta == raiz o está dentro de raiz
// (comprobar el '/' evita que /www2 pase como si fuera /www)
static int dentro_de_raiz(const char *raiz, const char *ruta) {
    size_t n = strlen(raiz);

    return strncmp(raiz, ruta, n) == 0 &&
           (ruta[n] == '\0' || ruta[n] == '/');
}

static int codigo_por_errno(void) {
    if (errno == ENOENT || errno == ENOTDIR) return 404;
    if (errno == EACCES) return 403;
    return 500;
}

int resolver_ruta(const char *directorio, const char *ruta_url,
                  char *ruta_final, size_t capacidad) {
    char raiz[PATH_MAX];
    char nombre[PATH_MAX];
    char temporal[PATH_MAX + 16];
    char ruta_real[PATH_MAX];
    struct stat datos;
    const char *consulta;
    size_t longitud;
    int escritos;

    if (realpath(directorio, raiz) == NULL) {
        return 500;
    }

    if (ruta_url[0] != '/') {
        return 400;
    }

    // Quitamos ?consulta si existe
    consulta = strchr(ruta_url, '?');
    longitud = (consulta != NULL) ? (size_t)(consulta - ruta_url)
                                  : strlen(ruta_url);

    // Saltamos la '/' inicial y decodificamos %XX
    if (decodificar_url(ruta_url + 1, longitud - 1,
                        nombre, sizeof(nombre)) == -1) {
        return 400;
    }

    // Primera barrera (realpath es la defensa definitiva)
    if (strstr(nombre, "..") != NULL) {
        return 403;
    }

    escritos = snprintf(temporal, sizeof(temporal), "%s/%s", raiz, nombre);
    if (escritos < 0 || (size_t)escritos >= sizeof(temporal)) {
        return 400;
    }

    if (realpath(temporal, ruta_real) == NULL) {
        return codigo_por_errno();
    }

    if (!dentro_de_raiz(raiz, ruta_real)) {
        return 403;
    }

    if (stat(ruta_real, &datos) == -1) {
        return codigo_por_errno();
    }

    // Si es un directorio, servimos su index.html
    if (S_ISDIR(datos.st_mode)) {
        escritos = snprintf(temporal, sizeof(temporal),
                            "%s/index.html", ruta_real);
        if (escritos < 0 || (size_t)escritos >= sizeof(temporal)) {
            return 400;
        }
        if (realpath(temporal, ruta_real) == NULL) {
            return codigo_por_errno();
        }
        if (!dentro_de_raiz(raiz, ruta_real)) {
            return 403;
        }
        if (stat(ruta_real, &datos) == -1) {
            return codigo_por_errno();
        }
    }

    if (!S_ISREG(datos.st_mode)) {
        return 403;
    }

    if (strlen(ruta_real) >= capacidad) {
        return 500;
    }

    memcpy(ruta_final, ruta_real, strlen(ruta_real) + 1);   // sin strcpy
    return 0;
}