#include <string.h>
#include <strings.h>

#include "mime.h"

struct tipo_mime {
    const char *extension;
    const char *tipo;
};

static const struct tipo_mime tipos[] = {
    {".html", "text/html"},
    {".css",  "text/css"},
    {".js",   "application/javascript"},
    {".png",  "image/png"},
    {".jpg",  "image/jpeg"},
    {".jpeg", "image/jpeg"}
};

const char *obtener_tipo_mime(const char *ruta) {
    const char *extension;
    size_t cantidad;

    extension = strrchr(ruta, '.');

    if (extension == NULL) {
        return "application/octet-stream";
    }

    cantidad = sizeof(tipos) / sizeof(tipos[0]);

    for (size_t i = 0; i < cantidad; i++) {
        if (strcasecmp(extension, tipos[i].extension) == 0) {
            return tipos[i].tipo;
        }
    }

    return "application/octet-stream";
}