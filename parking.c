#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include "parking.h"


#define NUM_SEM_PROPIOS 6
#define NUM_ALGORITMOS 4
#define TAM_PARKING 80

#define IDX_SEM_CHOFER (nSem + 0)
#define IDX_SEM_MUTEX (nSem + 1)
#define IDX_SEM_ORDEN(a) (nSem + 2 + (a))

typedef struct {
    int acera[NUM_ALGORITMOS][TAM_PARKING];
    int proxAparcar[NUM_ALGORITMOS];
    int ultimoAparcado;
    int terminar;
    int n_aparcar;
    int n_desaparcar;
} MEM_PROPIA;

// variables globales
int retardo, num_choferes, debug, prio_PA, prio_PD; // (estas lueog lo suyo seria hacer un struct)
int id_mem = -1, id_sem = -1, id_buzon = -1;  // -1 indica que aun no se crearon
int nSem, tam;

char *mem_base = NULL;
MEM_PROPIA *mp = NULL;


void validar_argumentos(int argc, char *argv[]) {

    if (argc < 3 || argc > 5) {
        fprintf(stderr, "Error: numero de argumentos incorrecto\n");
        exit(1);
    }

    retardo = atoi(argv[1]);
    if (retardo < 0) {
        fprintf(stderr, "Error: el retardo debe ser >= 0\n");
        exit(1);
    }

    num_choferes = atoi(argv[2]);
    if (num_choferes <= 0) {
        fprintf(stderr, "Error: el numero de choferes debe ser > 0\n");
        exit(1);
    }

    // argumentos opcionales
    debug = 0; prio_PA = 0; prio_PD = 0;

    for (int i = 3; i < argc; i++) {
        if (strcmp(argv[i], "D") == 0) debug = 1;
        else if (strcmp(argv[i], "PA") == 0) prio_PA = 1;
        else if (strcmp(argv[i], "PD") == 0) prio_PD = 1;
        else {
            fprintf(stderr, "Error: argumento desconocido '%s'\n", argv[i]);
            exit(1);
        }
    }

    if (prio_PA && prio_PD) {
        fprintf(stderr, "Error: PA y PD no pueden usarse a la vez\n");
        exit(1);
    }

} //fin funcion validar_argumentos


int mi_llegada_prueba(HCoche hc) {
    printf("Coche detectado\n");
    return 0;
} //fin funcion mi_llegada_prueba


void limpiar(void) {
    if (mem_base != NULL && mem_base != (char *)-1) { 
        shmdt(mem_base); 
    }
    if (id_mem   != -1) { 
        shmctl(id_mem,   IPC_RMID, NULL); 
    }
    if (id_sem   != -1) { 
        semctl(id_sem, 0, IPC_RMID); 
    }
    if (id_buzon != -1) { 
        msgctl(id_buzon, IPC_RMID, NULL); 
    }
} //fin funcion limpiar


int main(int argc, char *argv[]) {

    // validar y cargar argumentos en las variables globales
    validar_argumentos(argc, argv);


    //--------------------
    // Inicializacion
    //--------------------

    // array de 4 funciones, una por algoritmo (PRIMER, SIGUIENTE, MEJOR, PEOR)
    TIPO_FUNCION_LLEGADA funciones[4] = {
        mi_llegada_prueba,
        mi_llegada_prueba,
        mi_llegada_prueba,
        mi_llegada_prueba
    };

    // obtener tamaño de la memoria compartida y el numero de semaforos
    tam  = PARKING_getTamaNoMemoriaCompartida();
    nSem = PARKING_getNSemAforos();

    //creacion de la memoria compartida
    id_mem = shmget(IPC_PRIVATE, tam + sizeof(MEM_PROPIA), IPC_CREAT | 0600);
    if (id_mem == -1) { 
        perror("shmget"); 
        limpiar(); 
        return 1; 
    }

    // adjuntar la memoria al espacio de direcciones del proceso
    mem_base = (char *)shmat(id_mem, NULL, 0);
    if (mem_base == (char *)-1) { 
        perror("shmat"); 
        limpiar(); 
        return 1; 
    }

    // mp apunta a la zona propia, justo detras de la de la biblioteca
    mp = (MEM_PROPIA *)(mem_base + tam);
    memset(mp, 0, sizeof(MEM_PROPIA));

    // el primer coche en aparcar en cada algoritmo es el numero 1
    for (int i = 0; i < NUM_ALGORITMOS; i++) {
        mp->proxAparcar[i] = 1;
    }


    //creacion de los semaforos
    id_sem = semget(IPC_PRIVATE, nSem + NUM_SEM_PROPIOS, IPC_CREAT | 0600);
    if (id_sem == -1) { 
        perror("semget"); 
        limpiar(); 
        return 1; 
    }

    // inicializar semaforos propios
    semctl(id_sem, IDX_SEM_CHOFER, SETVAL, 0);
    semctl(id_sem, IDX_SEM_MUTEX, SETVAL, 1);
    for (int i = 0; i < NUM_ALGORITMOS; i++) {
        semctl(id_sem, IDX_SEM_ORDEN(i), SETVAL, 1);
    }


    //creacion del buzon
    id_buzon = msgget(IPC_PRIVATE, IPC_CREAT | 0600);
    if (id_buzon == -1) { 
        perror("msgget"); 
        limpiar(); 
        return 1; 
    }


    // iniciar la simulacion con los valores leidos de los argumentos
    int resultado = PARKING_inicio(retardo, funciones, id_sem, id_buzon, id_mem, debug);

    //===========================================================
    // Ejecutar con D para que se vea, o con d 2>debug.txt para guardar el debug en un txt en lugar de verlo por pantalla (solo el stderr)
    if (debug) {
        fprintf(stderr, "[MAIN] IPC creados -> SHM:%d SEM:%d MSG:%d\n", id_mem, id_sem, id_buzon);
        // PRUEBA PARA VER SI INICIA CORRECTAMENTE
        if (resultado == 0) {
            printf("PARKING_inicio funciona correctamente\n");
        } else {
            printf("PARKING_inicio devuelve %d, por tanto falló\n", resultado);
        }
    }
    //===========================================================

    //limpieza de los recursos utilizados
    limpiar();

    return 0;
} //fin funcion main
