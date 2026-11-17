#ifndef HTTP_H
#define HTTP_H

#include <stddef.h>

// Límites de seguridad (si se superan -> 400 Bad Request)
#define MAX_LINEA_SOLICITUD 2048   // "GET /ruta HTTP/1.1"
#define MAX_URI             255    // cabe en solicitud.ruta[256]
#define MAX_NUM_CABECERAS   50
#define MAX_CABECERA        8192   // bloque completo de cabeceras

enum resultado_analisis {
    ANALISIS_OK = 0,
    ANALISIS_SOLICITUD_INVALIDA,
    ANALISIS_METODO_NO_PERMITIDO
};

struct solicitud {
    char metodo[16];
    char ruta[256];
    char version[16];
    char host[256];
    char user_agent[256];
    int  mantener_conexion;
};

enum resultado_analisis analizar_solicitud(const char *buffer,
                                           struct solicitud *solicitud);

int construir_cabecera(char *buffer, size_t capacidad,
                       int codigo, const char *tipo_mime,
                       size_t longitud_cuerpo, int mantener_conexion);

int enviar_error(int cliente, int codigo, int mantener_conexion);

// Atiende UNA petición (cabeceras en buffer[0..tam)).
// Devuelve 1 si hay que mantener la conexión, 0 si hay que cerrarla.
int procesar_peticion(int cliente, char *buffer, size_t tam,
                      const char *raiz);

#endif