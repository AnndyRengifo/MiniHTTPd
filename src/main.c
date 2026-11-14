#include <stdio.h>
#include <unistd.h>

#include <server.h>

int main(){
    int servidor;
    int cliente;

    //inicio del sv TCP
    servidor = iniciar_servidor(8080);

    if (servidor == -1) {
        return 1;
    }

    //mantenemos el server activo con el while y esperando clientes
    while (1){
        cliente = aceptar_cliente(servidor);

        if(cliente == -1){
            continue;
        }

        //probamos la conexion
        printf("Conexion recibida correctamente \n");

        close(cliente);
    }

    close(servidor);
    return 0;
}

