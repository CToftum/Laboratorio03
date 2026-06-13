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

    // El padre lee mientras algún pipe siga abierto
    while (c_abierto || d_abierto) {
        FD_ZERO(&read_fds);
        if (c_abierto) FD_SET(pipe_credito[0], &read_fds);
        if (d_abierto) FD_SET(pipe_debito[0], &read_fds);

        // Espera a que haya datos en alguno de los pipes
        select(max_fd + 1, &read_fds, NULL, NULL, NULL);

        // Verifica si el hijo de credito envio algo
        if (c_abierto && FD_ISSET(pipe_credito[0], &read_fds)) {
            if (read(pipe_credito[0], &monto, sizeof(int)) > 0) {
                printf("Credito reporto: +$%d\n", monto); // 
            } else {
                c_abierto = 0; // Lee 0 bytes, asume que el hijo cerro y finalizo correctamente
            }
        }

        // Verifica si el hijo de débito envió algo
        if (d_abierto && FD_ISSET(pipe_debito[0], &read_fds)) {
            if (read(pipe_debito[0], &monto, sizeof(int)) > 0) {
                printf("Debito reporto: -$%d\n", monto); // [cite: 27]
            } else {
                d_abierto = 0; // Lee 0 bytes, asume que el hijo cerro y finalizo correctamente
            }
        }
    }

    // Cuando ambos hijos finalizaron, el padre los espera
    wait(NULL);
    wait(NULL);

    // Muestra el saldo final y limpia 
    printf("\n--------------------------------\n");
    printf("Saldo final en la cuenta: $%d\n", mem_comp->saldo);

    close(pipe_credito[0]);
    close(pipe_debito[0]);
    sem_destroy(&mem_comp->semaforo);
    munmap(mem_comp, sizeof(MemCompartida));

    // El padre finaliza 
    return 0;
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