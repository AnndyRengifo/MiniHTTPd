#ifndef FILES_H
#define FILES_H

#include <stddef.h>

int leer_archivo(const char *ruta, char **contenido, size_t *tamanio);

int resolver_ruta(const char *directorio, const char *ruta_url,
                  char *ruta_final, size_t capacidad);

#endif