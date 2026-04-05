#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include "parking.h"


#define NUM_ALGORITMOS 4
#define NUM_SEM_PROPIOS (2 + NUM_ALGORITMOS)
#define TAM_PARKING 80

#define IDX_SEM_CHOFER (nSem + 0)
#define IDX_SEM_MUTEX (nSem + 1)
#define IDX_SEM_ORDEN(a) (nSem + 2 + (a))

volatile sig_atomic_t terminar = 0;
volatile sig_atomic_t turno_recibido = 0;

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


// declaracion de los prototipos de las funciones
void ayudaPrograma(char *argv[]);
void validar_argumentos(int argc, char *argv[]);
int primer_ajuste(HCoche hc);
int siguiente_ajuste(HCoche hc);
int mejor_ajuste(HCoche hc);
int peor_ajuste(HCoche hc);
void limpiar(void);
void manejar_ctrlc(int signal);
void parking(int argcc, char *argvc[]);
int main(int argc, char *argv[]);

void ayudaPrograma(char *argv[]){
    printf("=====AYUDA PROGRAMA [%s]=====\n",argv[0]);
    printf("Ejemplo uso:\n");
    printf("\t%s [numero de retardo] [numero de choferes]\n", argv[0]);
}//fin funcion ayudaPrograma

void validar_argumentos(int argc, char *argv[]){
    if (argc < 3 || argc > 5) {
        fprintf(stderr, "Error: numero de argumentos incorrecto\n");
        ayudaPrograma(argv);
        exit(1);
    }

    retardo = atoi(argv[1]);
    if (retardo < 0) {
        fprintf(stderr, "Error: el retardo debe ser >= 0\n");
        exit(1);
    }

    num_choferes = atoi(argv[2]);
    if (num_choferes <= 0){
        fprintf(stderr, "Error: el numero de choferes debe ser > 0\n");
        exit(1);
    }

    // argumentos opcionales
    debug = 0; prio_PA = 0; prio_PD = 0;

    for (int i = 3; i < argc; i++) {
        if (strcmp(argv[i], "D") == 0) {
            debug = 1;
        } else if (strcmp(argv[i], "PA") == 0) {
            prio_PA = 1;
        } else if (strcmp(argv[i], "PD") == 0) {
            prio_PD = 1;
        } else {
            fprintf(stderr, "Error: argumento desconocido '%s'\n", argv[i]);
            exit(1);
        }
    }

    if (prio_PA && prio_PD) {
        fprintf(stderr, "Error: PA y PD no pueden usarse a la vez\n");
        exit(1);
    }
}//fin funcion validar_argumentos

int primer_ajuste(HCoche hc) {
    pause();
    return 0;
}//fin funcion primer_ajuste

int siguiente_ajuste(HCoche hc) {
    pause();
    return 0;
}//fin funcion siguiente_ajuste

int mejor_ajuste(HCoche hc) {
    pause();
    return 0;
}//fin funcion mejor_ajuste

int peor_ajuste(HCoche hc) {
    pause();
    return 0;
}//fin funcion peor_ajuste

void limpiar(void) {
    //=======================
    if (debug) fprintf(stderr, "[D-IPC] Limpiando recursos\n");
    //=======================
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
}//fin funcion limpiar

void manejar_ctrlc(int signal) {
    //=======================
    if (debug) fprintf(stderr, "[D-SIG] Señal %d recibida\n", signal);
    //=======================
    terminar = 1; //la funcion de esto es que cuando se ejecute el programa parar los bucles de creacion de los hijos cuando se reciba ctrl-c para limpiar bien los procesos
    system("tput cnorm"); //esto lo que hace es volver a poner el cursor "normal", ya que cuando se ejecuta el programa a veces el cursor se queda en modo "escondido" esto lo que hace es cambiarle a modo mostar
}//fin funcion manejar_ctrlc

//Luego si se me va te lo dejo aqui por si lo lees yo creo que main deberiamos de vaciarlo porque de main solo tendrian que haber llamadas a fuciones y tal
int main(int argc, char *argv[]) {
    //configurar sennales
//        struct sigaction sa_ctrlc, sa_usr1; //FIXME: Esto lo he cogido de un codigo mio lo tengo que agregar si es necesario por ahora dejemoslo asi para que no de error, todos los que tengo la sa_usr1 es lo mismo por eso estan comentados
    struct sigaction sa_ctrlc;
    memset(&sa_ctrlc, 0, sizeof(sa_ctrlc));
//        memset(&sa_usr1, 0, sizeof(sa_usr1));
    sa_ctrlc.sa_handler = manejar_ctrlc;
//        sa_usr1.sa_handler = manejar_usr1;
    sigaction(SIGINT, &sa_ctrlc, NULL);
    sigaction(SIGTERM, &sa_ctrlc, NULL);
//        sigaction(SIGUSR1, &sa_usr1, NULL);
 
    // validar y cargar argumentos en las variables globales
    validar_argumentos(argcc, argvc);

    
    //--------------------
    // Inicializacion
    //--------------------

    // array de 4 funciones, una por algoritmo (PRIMER, SIGUIENTE, MEJOR, PEOR)
    TIPO_FUNCION_LLEGADA funciones[4] = {
        primer_ajuste,
        siguiente_ajuste,
        mejor_ajuste,
        peor_ajuste
    };

    // obtener tamaño de la memoria compartida y el numero de semaforos
    tam  = PARKING_getTamaNoMemoriaCompartida();
    nSem = PARKING_getNSemAforos();

    //creacion de la memoria compartida
    id_mem = shmget(IPC_PRIVATE, tam + sizeof(MEM_PROPIA), IPC_CREAT | 0600);
    if (id_mem == -1) {
        perror("shmget");
        limpiar();
        exit(1);
    }

    // adjuntar la memoria al espacio de direcciones del proceso
    mem_base = (char *)shmat(id_mem, NULL, 0);
    if (mem_base == (char *)-1) {
        perror("shmat");
        limpiar();
        exit(1);
    }

    // mp apunta a la zona propia, justo detras de la de la biblioteca
    mp = (MEM_PROPIA *)(mem_base + tam);
    memset(mp, 0, sizeof(MEM_PROPIA));

    // el primer coche en aparcar en cada algoritmo es el numero 1
    for (int i = 0; i < NUM_ALGORITMOS; i++) {
        mp->proxAparcar[i] = 1;
    }//fin for


    //creacion de los semaforos
    id_sem = semget(IPC_PRIVATE, nSem + NUM_SEM_PROPIOS, IPC_CREAT | 0600);
    if (id_sem == -1) {
        perror("semget");
        limpiar();
        exit(1);
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
        exit(1);
    }


    // iniciar la simulacion con los valores leidos de los argumentos
    int resultado = PARKING_inicio(retardo, funciones, id_sem, id_buzon, id_mem, debug);

    //===========================================================
    if (debug) {
        fprintf(stderr, "[D-IPC] IPC creados -> SHM:%d SEM:%d MSG:%d\n", id_mem, id_sem, id_buzon);
        fprintf(stderr, "[D-PKG] PARKING_inicio retornó: %d\n", resultado);
        if (resultado == 0) {
            fprintf(stderr, "[D-PKG] PARKING_inicio funciona correctamente\n");
        }
        else {
            fprintf(stderr, "[D-PKG] PARKING_inicio falló\n");
        }
    }
    //===========================================================

    //limpieza de los recursos utilizados
    limpiar();
    system("tput cnorm"); //esto lo que hace es volver a poner el cursor "normal", ya que cuando se ejecuta el programa a veces el cursor se queda en modo "escondido" esto lo que hace es cambiarle a modo mostar

    return 0;
} //fin funcion main
