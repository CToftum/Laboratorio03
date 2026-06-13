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

int main() {
}