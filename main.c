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
    // 4. Creación de la memoria compartida por el padre 
    // Usamos mmap para crear una región anónima y compartida
    mem_comp = mmap(NULL, sizeof(MemCompartida), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (mem_comp == MAP_FAILED) {
        perror("Error al crear memoria compartida");
        exit(1);
    }

    mem_comp->saldo = 0; // Inicializamos el saldo en 0
    
    // Inicializamos el semaforo (1 = compartido entre procesos, 1 = valor inicial para exclusion mutua)
    if (sem_init(&mem_comp->semaforo, 1, 1) == -1) {
        perror("Error al inicializar semaforo");
        exit(1);
    }

    // 5. Creación de los pipes
    int pipe_credito[2], pipe_debito[2];
    if (pipe(pipe_credito) == -1 || pipe(pipe_debito) == -1) {
        perror("Error al crear pipes");
        exit(1);
    }

    // 6. Creación de procesos hijos
    pid_t pid_credito = fork();
    if (pid_credito == 0) {
        // Hijo: Credito
        credito("credito.txt", pipe_credito); 
    }

    pid_t pid_debito = fork();
    if (pid_debito == 0) {
        // Hijo: Debito
        debito("debito.txt", pipe_debito);
    }

    // 7. Proceso Padre 
    // Cierra los extremos de escritura de los pipes, ya que el padre solo lee
    close(pipe_credito[1]);
    close(pipe_debito[1]);

    int monto;
    int c_abierto = 1;
    int d_abierto = 1;
    fd_set read_fds;
    
    // Calculamos el descriptor más alto para la funcion select()
    int max_fd = (pipe_credito[0] > pipe_debito[0]) ? pipe_credito[0] : pipe_debito[0];

    printf("--- Iniciando el procesamiento de montos ---\n");

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