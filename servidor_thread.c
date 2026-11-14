#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

void *atender(void *arg){
	int cliente = *(int *)arg;
	char mensaje[1024];
	int bytes;

	free(arg);

	bytes = recv(cliente, mensaje, sizeof(mensaje) - 1, 0);

	if (bytes > 0){
		mensaje[bytes] = '\0';

		printf("Mensaje recibido: %s", mensaje);

		send(cliente, "Atendido por un hilo\n",
			sizeof("Atendido por un hilo\n") - 1, 0);
	}
	
	close(cliente);

	return NULL;
}

int main(){
	int servidor, cliente;
	struct sockaddr_in direccion;
	pthread_t hilo;
	int *dato;

	servidor = socket(AF_INET, SOCK_STREAM, 0);

	if(servidor == -1){
		perror("Error socket");
		return 1;
	}

	direccion.sin_family = AF_INET;
	direccion.sin_port = htons(8080);
	inet_pton(AF_INET, "127.0.0.1", &direccion.sin_addr);
	
	if(bind(servidor, (struct sockaddr *)&direccion,
		sizeof(direccion)) == -1){
		perror("Error bind");
		close(servidor);
		return 1;
	}
		
	if(listen(servidor, 10) == -1){
		perror("Error listen");
		close(servidor);
		return 1;
	}

	printf("Servidor multihilo esperando clientes....\n");

	while(1){
		cliente = accept(servidor, NULL, NULL);
	
		if (cliente == -1){
			perror("Error accept");
			continue;
		}
	
		dato = malloc(sizeof(int));
		
		if(dato == NULL){
			perror("Error malloc");
			close(cliente);	
			continue;
		}
		
		*dato = cliente;
		
		if(pthread_create(&hilo, NULL, atender, dato) != 0){
			perror("Error pthread_create");
			free(dato);
			close(cliente);
			continue;
		}

		pthread_detach(hilo);
	}

	close(servidor);
	return 0;
}

