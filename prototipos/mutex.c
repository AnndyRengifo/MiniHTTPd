#include <stdio.h>
#include <pthread.h>

int contador = 0;
pthread_mutex_t mutex;

void *incrementar(void *arg){
	int i;
	
	for (i = 0; i < 100000; i++){
		pthread_mutex_lock(&mutex);
		
		contador++;
	
		pthread_mutex_unlock(&mutex);
	}

	return NULL;
}

int main(){
	pthread_t hilo1, hilo2;
	
	pthread_mutex_init(&mutex, NULL);

	pthread_create(&hilo1, NULL, incrementar, NULL);
	pthread_create(&hilo2, NULL, incrementar, NULL);
	
	pthread_join(hilo1, NULL);
	pthread_join(hilo2, NULL);

	printf("Contador: %d\n", contador);
	
	pthread_mutex_destroy(&mutex);

	return 0;
}

