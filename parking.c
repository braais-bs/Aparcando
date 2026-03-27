#include <stdio.h>
#include <stdlib.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include "parking.h"

int mi_llegada_prueba(int id) {
    printf("Coche detectado: %d\n", id);
    return 0;
}//fin funcion mi_llegada_prueba

int main() {
        TIPO_FUNCION_LLEGADA puntero_func = mi_llegada_prueba;

        //creacion de memoria compartida
        int id_memoria = shmget(1234, 298220, IPC_CREAT | 0666);
        if (id_memoria == -1) { perror("Error SHM"); return 1; }

        //creacion de semaforos
        //semget(clave, num_semaforos, flags)
        int id_semaforos = semget(5678, 10, IPC_CREAT | 0666);
        if (id_semaforos == -1) { perror("Error SEM"); return 1; }

        printf("Recursos IPC listos -> SHM ID: %d, SEM ID: %d\n", id_memoria, id_semaforos);

        //llamadas a la funcion con ambos IDS
        int resultado = PARKING_inicio(
                1,              // ret
                &puntero_func,  // f_llegadasP
                id_semaforos,   // <--- PASAMOS EL ID DE SEMAFOROS AQUI
                0,              // buzon
                id_memoria,     // zona
                1               // debug
        );

        if (resultado == 0) {
                printf("¡LOGRADO! PARKING_inicio devolvio 0.\nEsto es solo una prueba para ver si funciona todo bien.\n");
        }//fin if

        else {
                printf("La funcion devolvio %d.\n", resultado);
        }//fin else

        //limpieza de recursos creados
        shmctl(id_memoria, IPC_RMID, NULL);
        semctl(id_semaforos, 0, IPC_RMID);

        return 0;
}//fin funcion main
