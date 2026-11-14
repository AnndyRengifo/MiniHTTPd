#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

int main(){
	int servidor, cliente;
	struct sockaddr_in direccion;
	char mensaje[1024];
	int bytes;

	servidor = socket(AF_INET, SOCK_STREAM, 0);

	if (servidor == -1){
		perror("Error socket");
		return 1;
	}

	direccion.sin_family = AF_INET;
	direccion.sin_port = htons(8080);
	inet_pton(AF_INET, "127.0.0.1", &direccion.sin_addr);

	if (bind(servidor, (struct sockaddr *)&direccion,
		sizeof(direccion)) == -1) {
		perror("Error bind");
		close(servidor);
		return 1;
	}

	if (listen(servidor, 5) == -1) {
		perror("Error listen");
		close(servidor);
		return 1;
	}

	printf("Servidor esperando en el puerto 8080...\n");

	cliente = accept(servidor, NULL, NULL);

	if (cliente == -1) {
		perror("Error accept");
		close(servidor);
		return 1;
	}

	bytes = recv(cliente, mensaje, sizeof(mensaje) - 1, 0);
	
	if (bytes > 0) {
		mensaje[bytes] = '\0';

		printf("Cliente dice: %s", mensaje);

		send(cliente, "Tu Mensaje fue recibido\n", 50, 0);
	}
	
	close(cliente);
	close(servidor);

	return 0;
}

