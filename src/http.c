/*
 * http.c - Parsing de la petición GET y generación de la respuesta.
 *
 * Una petición HTTP/1.1 tiene esta forma (cada línea termina en \r\n):
 *
 *   GET /index.html HTTP/1.1      <- línea de solicitud
 *   Host: localhost:8080          <- cabeceras "Nombre: valor"
 *   User-Agent: curl/8.0
 *   Connection: keep-alive
 *                                 <- línea vacía = fin de cabeceras
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <strings.h>
#include <ctype.h>

#include "http.h"
#include "files.h"
#include "mime.h"
#include "server.h"

// Copia un campo verificando que tenga espacio suficiente.
// Reemplaza a strcpy: si no cabe, error (nunca desborda).
static int copiar_campo(char *destino, size_t capacidad,
                        const char *origen, size_t longitud) {
    if (longitud >= capacidad) {
        return -1;
    }
    memcpy(destino, origen, longitud);
    destino[longitud] = '\0';
    return 0;
}

// Copia [inicio, fin) quitando espacios/tabulaciones en los extremos
static int extraer_valor(const char *inicio, const char *fin,
                         char *destino, size_t capacidad) {
    while (inicio < fin && (*inicio == ' ' || *inicio == '\t')) {
        inicio++;
    }
    while (fin > inicio && (fin[-1] == ' ' || fin[-1] == '\t')) {
        fin--;
    }
    return copiar_campo(destino, capacidad, inicio, (size_t)(fin - inicio));
}

enum resultado_analisis analizar_solicitud(const char *buffer,
                                           struct solicitud *solicitud) {
    const char *fin_linea;
    const char *fin;
    const char *linea;
    const char *separador;
    const char *dos_puntos;
    const char *sp1;
    const char *sp2;
    char nombre[64];
    char valor[256];
    int num_cabeceras = 0;

    memset(solicitud, 0, sizeof(*solicitud));

    // ---- 1. Línea de solicitud: METODO RUTA VERSION ----
    // Se separa por los dos espacios: sp1 y sp2
    fin_linea = strstr(buffer, "\r\n");
    if (fin_linea == NULL) {
        return ANALISIS_SOLICITUD_INVALIDA;
    }

    if ((size_t)(fin_linea - buffer) > MAX_LINEA_SOLICITUD) {
        return ANALISIS_SOLICITUD_INVALIDA;
    }

    sp1 = memchr(buffer, ' ', (size_t)(fin_linea - buffer));
    if (sp1 == NULL) {
        return ANALISIS_SOLICITUD_INVALIDA;
    }
    sp2 = memchr(sp1 + 1, ' ', (size_t)(fin_linea - (sp1 + 1)));
    if (sp2 == NULL) {
        return ANALISIS_SOLICITUD_INVALIDA;
    }

    // URI más larga que MAX_URI: se RECHAZA (no se trunca en silencio)
    if (sp2 - sp1 - 1 <= 0 || (size_t)(sp2 - sp1 - 1) > MAX_URI) {
        return ANALISIS_SOLICITUD_INVALIDA;
    }

    if (copiar_campo(solicitud->metodo, sizeof(solicitud->metodo),
                     buffer, (size_t)(sp1 - buffer)) == -1 ||
        copiar_campo(solicitud->ruta, sizeof(solicitud->ruta),
                     sp1 + 1, (size_t)(sp2 - sp1 - 1)) == -1 ||
        copiar_campo(solicitud->version, sizeof(solicitud->version),
                     sp2 + 1, (size_t)(fin_linea - sp2 - 1)) == -1) {
        return ANALISIS_SOLICITUD_INVALIDA;
    }

    // Primero la versión (basura -> 400), después el método (-> 405)
    if (strcmp(solicitud->version, "HTTP/1.1") != 0 &&
        strcmp(solicitud->version, "HTTP/1.0") != 0) {
        return ANALISIS_SOLICITUD_INVALIDA;
    }

    if (strcmp(solicitud->metodo, "GET") != 0) {
        return ANALISIS_METODO_NO_PERMITIDO;
    }

    // HTTP/1.1 es persistente por defecto; HTTP/1.0 se cierra por defecto
    solicitud->mantener_conexion =
        (strcmp(solicitud->version, "HTTP/1.1") == 0);

    // ---- 2. Cabeceras: una por línea, "Nombre: valor" ----
    fin = strstr(buffer, "\r\n\r\n");
    if (fin == NULL) {
        return ANALISIS_SOLICITUD_INVALIDA;
    }

    linea = fin_linea + 2;

    while (linea < fin) {

        separador = strstr(linea, "\r\n");
        if (separador == NULL || separador > fin) {
            return ANALISIS_SOLICITUD_INVALIDA;
        }

        if (++num_cabeceras > MAX_NUM_CABECERAS) {
            return ANALISIS_SOLICITUD_INVALIDA;
        }

        dos_puntos = memchr(linea, ':', (size_t)(separador - linea));
        if (dos_puntos == NULL || dos_puntos == linea) {
            return ANALISIS_SOLICITUD_INVALIDA;
        }

        // Nombre demasiado largo: no es una cabecera que nos interese
        if (copiar_campo(nombre, sizeof(nombre), linea,
                         (size_t)(dos_puntos - linea)) == 0) {

            // Los nombres de cabecera no distinguen mayúsculas
            for (size_t i = 0; nombre[i] != '\0'; i++) {
                nombre[i] = (char)tolower((unsigned char)nombre[i]);
            }

            // Solo copiamos el valor de las cabeceras que usamos
            // (así un Cookie largo de un navegador no da 400)
            if (strcmp(nombre, "host") == 0) {
                if (extraer_valor(dos_puntos + 1, separador,
                                  solicitud->host,
                                  sizeof(solicitud->host)) == -1) {
                    return ANALISIS_SOLICITUD_INVALIDA;
                }
            } else if (strcmp(nombre, "user-agent") == 0) {
                if (extraer_valor(dos_puntos + 1, separador,
                                  solicitud->user_agent,
                                  sizeof(solicitud->user_agent)) == -1) {
                    return ANALISIS_SOLICITUD_INVALIDA;
                }
            } else if (strcmp(nombre, "connection") == 0) {
                if (extraer_valor(dos_puntos + 1, separador,
                                  valor, sizeof(valor)) == -1) {
                    return ANALISIS_SOLICITUD_INVALIDA;
                }
                if (strcasecmp(valor, "close") == 0) {
                    solicitud->mantener_conexion = 0;
                } else if (strcasecmp(valor, "keep-alive") == 0) {
                    solicitud->mantener_conexion = 1;
                }
            }
        }

        linea = separador + 2;
    }

    // RFC 7230: HTTP/1.1 sin Host es una petición inválida
    if (strcmp(solicitud->version, "HTTP/1.1") == 0 &&
        solicitud->host[0] == '\0') {
        return ANALISIS_SOLICITUD_INVALIDA;
    }

    return ANALISIS_OK;
}

static const char *texto_estado(int codigo) {
    switch (codigo) {
    case 200: return "OK";
    case 400: return "Bad Request";
    case 403: return "Forbidden";
    case 404: return "Not Found";
    case 405: return "Method Not Allowed";
    default:  return "Internal Server Error";
    }
}

// Escribe SOLO la cabecera. Devuelve su longitud o -1 si no cabe.
// snprintf (no sprintf) nunca escribe más de 'capacidad' bytes.
// Respuesta generada:
//   HTTP/1.1 200 OK
//   Content-Type: text/html          <- tipo MIME (mime.c)
//   Content-Length: 347              <- tamaño exacto del cuerpo
//   Connection: keep-alive | close
//   (línea vacía y luego el cuerpo)
int construir_cabecera(char *buffer, size_t capacidad,
                       int codigo, const char *tipo_mime,
                       size_t longitud_cuerpo, int mantener_conexion) {
    const char *extra = (codigo == 405) ? "Allow: GET\r\n" : "";
    int r;

    r = snprintf(buffer, capacidad,
                 "HTTP/1.1 %d %s\r\n"
                 "Content-Type: %s\r\n"
                 "Content-Length: %zu\r\n"
                 "%s"
                 "Connection: %s\r\n"
                 "\r\n",
                 codigo, texto_estado(codigo), tipo_mime,
                 longitud_cuerpo, extra,
                 mantener_conexion ? "keep-alive" : "close");

    if (r < 0 || (size_t)r >= capacidad) {
        return -1;
    }
    return r;
}

// Respuesta de error con un pequeño cuerpo HTML (400, 403, 404...)
int enviar_error(int cliente, int codigo, int mantener_conexion) {
    char cuerpo[160];
    char cabecera[512];
    int lc;
    int lh;

    lc = snprintf(cuerpo, sizeof(cuerpo),
                  "<html><body><h1>%d %s</h1></body></html>\n",
                  codigo, texto_estado(codigo));
    if (lc < 0 || (size_t)lc >= sizeof(cuerpo)) {
        return -1;
    }

    lh = construir_cabecera(cabecera, sizeof(cabecera), codigo,
                            "text/html", (size_t)lc, mantener_conexion);
    if (lh < 0) {
        return -1;
    }

    if (enviar_datos(cliente, cabecera, (size_t)lh) == -1 ||
        enviar_datos(cliente, cuerpo, (size_t)lc) == -1) {
        return -1;
    }
    return 0;
}

// Una línea de log por petición (aquí se aprovecha User-Agent)
static void registrar(const struct solicitud *sol, int codigo) {
    printf("%s %s %d \"%s\"\n",
           sol->metodo[0] ? sol->metodo : "-",
           sol->ruta[0] ? sol->ruta : "-",
           codigo,
           sol->user_agent[0] ? sol->user_agent : "-");
    fflush(stdout);
}

// Flujo completo: analizar -> resolver ruta (files.c) -> leer archivo
// -> cabecera con MIME -> enviar. Devuelve 1 = keep-alive, 0 = cerrar.
int procesar_peticion(int cliente, char *buffer, size_t tam,
                      const char *raiz) {
    struct solicitud sol;
    enum resultado_analisis r;
    char ruta[4096];
    char cabecera[512];
    char *contenido = NULL;
    size_t tamanio = 0;
    int codigo;
    int lh;
    int mantener;
    char guardado;

    // analizar_solicitud espera una cadena terminada en '\0': la cerramos
    // al final de ESTA petición y luego restauramos el byte (puede ser el
    // inicio de una petición encadenada / pipelining).
    guardado = buffer[tam];
    buffer[tam] = '\0';
    r = analizar_solicitud(buffer, &sol);
    buffer[tam] = guardado;

    // Tras 400 o 405 cerramos siempre: pueden quedar bytes de un cuerpo
    // sin leer y no podemos fiarnos de dónde empieza la siguiente petición
    if (r == ANALISIS_SOLICITUD_INVALIDA) {
        registrar(&sol, 400);
        enviar_error(cliente, 400, 0);
        return 0;
    }

    if (r == ANALISIS_METODO_NO_PERMITIDO) {
        registrar(&sol, 405);
        enviar_error(cliente, 405, 0);
        return 0;
    }

    mantener = sol.mantener_conexion;

    codigo = resolver_ruta(raiz, sol.ruta, ruta, sizeof(ruta));
    if (codigo != 0) {
        registrar(&sol, codigo);
        return enviar_error(cliente, codigo, mantener) == 0 && mantener;
    }

    if (leer_archivo(ruta, &contenido, &tamanio) == -1) {
        registrar(&sol, 500);
        return enviar_error(cliente, 500, mantener) == 0 && mantener;
    }

    lh = construir_cabecera(cabecera, sizeof(cabecera), 200,
                            obtener_tipo_mime(ruta), tamanio, mantener);
    if (lh < 0) {
        free(contenido);
        registrar(&sol, 500);
        return enviar_error(cliente, 500, mantener) == 0 && mantener;
    }

    // Cabecera y cuerpo se envían por separado (una imagen pesa MB)
    if (enviar_datos(cliente, cabecera, (size_t)lh) == -1 ||
        enviar_datos(cliente, contenido, tamanio) == -1) {
        free(contenido);
        return 0;
    }

    free(contenido);
    registrar(&sol, 200);
    return mantener;
}