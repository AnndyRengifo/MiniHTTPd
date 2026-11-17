#ifndef HTTP_H
#define HTTP_H

#include <stddef.h>

/*Limites de tamano si se superan la respuesta es 400 bad request*/
#define MAX_LINEA_SOLICITUD 2048  
#define MAX_URI 255
#define MAX_CABECERA 8192
#define MAX_NUM_CABECERAS 64

/*Resultado de analizar una solicitud*/
enum resultado_analisis{
    ANALISIS_OK = 0,
    ANALISIS_SOLICITUD_INVALIDA, /* 400 bad request*/
    ANALISIS_METODO_NO_PERMITIDO /*405 Method not allowed*/
};

struct solicitud {
    char metodo[8];
    char ruta[256];
    char version[16];
    char host[256];
    char user_agent[256];
    size_t content_length;
    int tiene_content_length;
    int mantener_conexion; /* 1 = keep-alive, 0 = cerrar*/
};

enum resultado_analisis analizar_solicitud(const char *buffer,
                                         struct solicitud *solicitud);  

int construir_respuesta(char *buffer,
                        size_t capacidad,
                        size_t *tamanio);

int analizar_formulario(const char *cuerpo,
                        size_t tamanio,
                        char *nombre,
                        size_t capacidad_nombre,
                        char *valor,
                        size_t capacidad_valor);

#endif