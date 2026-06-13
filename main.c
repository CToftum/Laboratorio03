#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <sys/select.h>
#include <fcntl.h>

// 1. Estructura de la memoria compartida
typedef struct {
    int saldo;
    sem_t semaforo;
} MemCompartida;

MemCompartida *mem_comp;

// 2. Prototipos de funciones 
void debito(char * archivo_montos, int p[]);
void credito(char * archivo_montos, int p[]);

int main() {
}


// 3. Funciones 
void credito(char * archivo_montos, int p[]) {
    close(p[0]); // Cierra el extremo de lectura del pipe
    
    FILE *file = fopen(archivo_montos, "r");
    if (file == NULL) {
        perror("Error al abrir archivo de crédito");
        exit(1);
    }

    int monto;
    while (fscanf(file, "%d", &monto) != EOF) {
        sem_wait(&mem_comp->semaforo); // Solicitamos entrar a la sección critica 
        mem_comp->saldo += monto;      // Suma montos a la variable compartida 
        sem_post(&mem_comp->semaforo); // Liberamos la sección critica

        write(p[1], &monto, sizeof(int)); // Envia el monto al padre por el pipe 
    }

    fclose(file);
    close(p[1]); // Cierra el pipe antes de finalizar 
    exit(0);     // Finaliza luego de procesar todos los números 
}

void debito(char * archivo_montos, int p[]) {
    close(p[0]); // Cierra el extremo de lectura del pipe
    
    FILE *file = fopen(archivo_montos, "r");
    if (file == NULL) {
        perror("Error al abrir archivo de debito");
        exit(1);
    }

    int monto;
    while (fscanf(file, "%d", &monto) != EOF) {
        sem_wait(&mem_comp->semaforo); // Solicitamos entrar a la sección critica 
        mem_comp->saldo -= monto;      // Resta montos a la variable compartida 
        sem_post(&mem_comp->semaforo); // Liberamos la sección critica

        write(p[1], &monto, sizeof(int)); // Envía el monto al padre por el pipe 
    }

    fclose(file);
    close(p[1]); // Cierra el pipe antes de finalizar
    exit(0);     // Finaliza luego de procesar todos los numeros
}