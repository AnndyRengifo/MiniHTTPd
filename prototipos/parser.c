#include <stdio.h>
#include <string.h>

int main() {
    char peticion[] = "GET /index.html HTTP/1.1";
    char metodo[10];
    char ruta[100];
    char version[20];

    sscanf(peticion, "%9s %99s %19s",
           metodo, ruta, version);

    printf("Metodo: %s\n", metodo);
    printf("Ruta: %s\n", ruta);
    printf("Version: %s\n", version);

    return 0;
}
