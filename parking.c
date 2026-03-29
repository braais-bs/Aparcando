#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include "parking.h"

// variables globales
int retardo, num_choferes, debug, prio_PA, prio_PD;
int id_mem = -1, id_sem = -1, id_buzon = -1;  // -1 indica que aun no se crearon


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
    int tam  = PARKING_getTamaNoMemoriaCompartida();
    int nSem = PARKING_getNSemAforos();

    //creacion de la memoria compartida
    id_mem = shmget(IPC_PRIVATE, tam, IPC_CREAT | 0600);
    if (id_mem == -1) { perror("shmget"); return 1; }

    //creacion de los semaforos
    id_sem = semget(IPC_PRIVATE, nSem, IPC_CREAT | 0600);
    if (id_sem == -1) { perror("semget"); return 1; }

    //creacion del buzon
    id_buzon = msgget(IPC_PRIVATE, IPC_CREAT | 0600);
    if (id_buzon == -1) { perror("msgget"); return 1; }

    // iniciar la simulacion con los valores leidos de los argumentos
    int resultado = PARKING_inicio(retardo, funciones, id_sem, id_buzon, id_mem, debug);

    //===========================================================
    // PRUEBA PARA VER SI INICIA CORRECTAMENTE
    if (resultado == 0)
        printf("PARKING_inicio funciona correctamente\n");
    else
        printf("PARKING_inicio devuelve %d, por tanto falló\n", resultado);
    //===========================================================

    //limpieza de los recursos utilizados
    shmctl(id_mem, IPC_RMID, NULL);
    semctl(id_sem, 0, IPC_RMID);
    msgctl(id_buzon, IPC_RMID, NULL);

    return 0;
} //fin funcion main
