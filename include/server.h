#ifndef SERVER_H
#define SERVER_H

#include <sys/types.h>
#include <stddef.h>

int iniciar_servidor(int puerto);
int aceptar_cliente(int servidor);
ssize_t recibir_datos(int cliente, char *buffer, int tamanio);
ssize_t enviar_datos(int cliente, const char *buffer, size_t tamanio);

#endif