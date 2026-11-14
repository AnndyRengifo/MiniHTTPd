#include <stdio.h>
#include <unistd.h>

#include <server.h>

int main(){
    int servidor;
    int cliente;
    char buffer[4096];
    ssize_t bytes;

    //inicio del sv TCP
    servidor = iniciar_servidor(8080);

    if (servidor == -1) {
        return 1;
    }

    //mantenemos el server activo con el while y esperando clientes
    while (1){
        cliente = aceptar_cliente(servidor); //aceptamos la conexion del cliente

        if(cliente == -1){
            continue;
        }

        //recibimos los datos enviados por el cliente
        bytes = recibir_datos(cliente, buffer, sizeof(buffer));

        if (bytes > 0 ){
            printf("Datos recibidos: %zd bytes\n", bytes);
            printf("%s\n", buffer);
        }

        if (bytes == 0){
            printf("El cliente cerro la conexion\n");
        }

        close(cliente); // se cierra el cliente
    }

    close(servidor);
    
    return 0;
}

