#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <signal.h>

int main(){
	int servidor, cliente, bytes;
	struct sockaddr_in direccion;
	char mensaje[1024];
	pid_t pid;

	servidor = socket(AF_INET, SOCK_STREAM, 0);

	if (servidor == -1){
		perror("Error socket");
		return 1;
	}

	direccion.sin_family = AF_INET;
	direccion.sin_port = htons(8080);
	inet_pton(AF_INET, "127.0.0.1", &direccion.sin_addr);

	if (bind(servidor, (struct sockaddr *)&direccion,
		sizeof(direccion)) == -1){
		perror("Error bind");
		close(servidor);
		return 1;
	}
	
	if (listen(servidor, 5) == -1){
		perror("Error listen");
		close(servidor);
		return 1;
	}

	signal(SIGCHLD, SIG_IGN);

	printf("Servidor concurrente esperando clientes..\n");

	while(1){
		cliente = accept(servidor, NULL, NULL);

		if (cliente == -1){
			perror("Error accept");
			continue;
		}
		
		pid = fork();

		if (pid == 0) {
			close(servidor);
		
			bytes = recv(cliente, mensaje, sizeof(mensaje) - 1, 0);
			
			if (bytes > 0) {
				mensaje[bytes] = '\0';
				printf("Mensaje recibido: %s", mensaje);

				send(cliente, "Atendido por el servidor\n", 26, 0);
			}

			close(cliente);
			_exit(0);
		}

		if (pid > 0){
			close(cliente);
		}

		if (pid == -1){
			perror("Error fork");
			close(cliente);
		}
	}

	close(servidor);
	return 0;
}



