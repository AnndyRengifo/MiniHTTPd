#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>

int main(){
	int servidor, cliente;
	struct sockaddr_in direccion;
	fd_set lectura;
	char mensaje[1024];
	int listo, bytes;

	servidor = socket(AF_INET, SOCK_STREAM, 0);
	
	direccion.sin_family = AF_INET;
	direccion.sin_port = htons(8080);
	direccion.sin_addr.s_addr = INADDR_ANY;

	bind(servidor, (struct sockaddr *)&direccion, sizeof(direccion));
	listen(servidor, 5);

	printf("Servidor esperando conexiones...\n");

	cliente = accept(servidor, NULL, NULL);

	while(1){
		FD_ZERO(&lectura);
		FD_SET(cliente, &lectura);

		listo = select(cliente + 1, &lectura, NULL, NULL, NULL);

		if (listo > 0 && FD_ISSET(cliente, &lectura)) {
			bytes = recv(cliente, mensaje, sizeof(mensaje) - 1, 0);
	
		if (bytes <= 0) break;
	
		mensaje[bytes] = '\0';
		printf("Recibido: %s", mensaje);
		}
	}

	close(cliente);
	close(servidor);
	return 0;
}


