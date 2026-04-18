/*
* Sistemas Operativos II - Práctica Linux - Aparcando
* Curso: 2025-2026
* Práctica Linux de Grupo
* Autores: Brais Bértolo Senra, Juan Riego Vila
*/

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
void parking();
void inicializar_semaforos();
void inicializacion_buzones();
void inicializar_simulacion(TIPO_FUNCION_LLEGADA funciones[]);
void crear_chofer();
void finalizar_simulacion();
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
        }//fin if

        retardo = atoi(argv[1]);
        if (retardo < 0) {
                fprintf(stderr, "Error: el retardo debe ser >= 0\n");
                exit(1);
        }//fin if

        num_choferes = atoi(argv[2]);
        if (num_choferes <= 0){
                fprintf(stderr, "Error: el numero de choferes debe ser > 0\n");
                exit(1);
        }//fin if

        // argumentos opcionales
        debug = 0; prio_PA = 0; prio_PD = 0;

        for (int i = 3; i < argc; i++) {
                if (strcmp(argv[i], "D") == 0) {
                        debug = 1;
                }//fin if
                else if (strcmp(argv[i], "PA") == 0) {
                        prio_PA = 1;
                }//fin else if
                else if (strcmp(argv[i], "PD") == 0) {
                        prio_PD = 1;
                }//fin else if
                else {
                        fprintf(stderr, "Error: argumento desconocido '%s'\n", argv[i]);
                        exit(1);
                }//fin else
        }//fin for

        if (prio_PA && prio_PD) {
                fprintf(stderr, "Error: PA y PD no pueden usarse a la vez\n");
                exit(1);
        }//fin if
}//fin funcion validar_argumentos

int primer_ajuste(HCoche hc) {
        return 0;
}//fin funcion primer_ajuste

int siguiente_ajuste(HCoche hc) {
        return -2; // de momento no queremos que funcione
}//fin funcion siguiente_ajuste

int mejor_ajuste(HCoche hc) {
        return -2; // de momento no queremos que funcione
}//fin funcion mejor_ajuste

int peor_ajuste(HCoche hc) {
        return -2; // de momento no queremos que funcione
}//fin funcion peor_ajuste

void limpiar(void) {
        //=======================
        if (debug) fprintf(stderr, "[D-IPC] Limpiando recursos\n");
        //=======================
        if (mem_base != NULL && mem_base != (char *)-1) {
                shmdt(mem_base);
        }//fin if
        if (id_mem   != -1) {
                shmctl(id_mem,   IPC_RMID, NULL);
        }//fin if
        if (id_sem   != -1) {
                semctl(id_sem, 0, IPC_RMID);
        }//fin if
        if (id_buzon != -1) {
                msgctl(id_buzon, IPC_RMID, NULL);
        }//fin if
}//fin funcion limpiar

void manejar_ctrlc(int signal) {
        //=======================
        if (debug) fprintf(stderr, "[D-SIG] Señal %d recibida\n", signal);
        //=======================
        terminar = 1; //la funcion de esto es que cuando se ejecute el programa parar los bucles de creacion de los hijos cuando se reciba ctrl-c para limpiar bien los procesos
        system("tput cnorm"); //esto lo que hace es volver a poner el cursor "normal", ya que cuando se ejecuta el programa a veces el cursor se queda en modo "escondido" esto lo que hace es cambiarle a modo mostar
}//fin funcion manejar_ctrlc

//-=-=-=-=-=-=-=-=-=-=- Funciones de Rellamada (Callbacks), para el apartado 8 -=-=-=-=-=-=-=-=-=-=-

//se ejecuta cuando la biblioteca confirma que el coche ha aparcado
void aparcar_commit(HCoche hc) {
        if (debug) fprintf(stderr, "[D-PKG:aparcar_commit] Coche %d aparcado en algoritmo [no implementado] (Commit)\n", hc);
        //aqui es donde levantarias el semaforo para el siguiente coche (mp->proxaparcar)
        //semop(id_sem, ...);
}//fin funcion aparcar_commit

//se ejecuta cuando el coche quiere moverse. debe bloquearse hasta que sea seguro
void permiso_avance(HCoche hc) {
        if (debug) fprintf(stderr, "[D-PKG:permiso_avance] Coche %d pidiendo permiso para avanzar...\n", hc);
        //De momento como dice el enunciado solo mensaje
        //en una version final aqui se usarian semaforos para evitar colisiones
}//fin funcion permiso_avance

void permiso_avance_commit(HCoche hc) {
        if (debug) fprintf(stderr, "[D-PKG:permiso_avance_commit] Coche %d ha avanzado con éxito.\n", hc);
}//fin funcion mi_permiso_avance_commit


void parking(){
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
        }//fin if

        // adjuntar la memoria al espacio de direcciones del proceso
        mem_base = (char *)shmat(id_mem, NULL, 0);
        if (mem_base == (char *)-1) {
                perror("shmat");
                limpiar();
                exit(1);
        }//fin ifj

        // mp apunta a la zona propia, justo detras de la de la biblioteca
        mp = (MEM_PROPIA *)(mem_base + tam);
        memset(mp, 0, sizeof(MEM_PROPIA));

        // el primer coche en aparcar en cada algoritmo es el numero 1
        for (int i = 0; i < NUM_ALGORITMOS; i++) {
                mp->proxAparcar[i] = 1;
        }//fin for

        // creacion e inicializacion de los semaforos
        inicializar_semaforos();

        //creacion del buzon
        inicializacion_buzones();

        // iniciar la simulacion con los valores leidos de los argumentos
        inicializar_simulacion(funciones);

        //limpieza de los recursos utilizados
        limpiar();
        system("tput cnorm"); //esto lo que hace es volver a poner el cursor "normal", ya que cuando se ejecuta el programa a veces el cursor se queda en modo "escondido" esto lo que hace es cambiarle a modo mostar

        finalizar_simulacion();
}//fin funcion parking

void inicializar_semaforos(){
        //creacion de los semaforos
        id_sem = semget(IPC_PRIVATE, nSem + NUM_SEM_PROPIOS, IPC_CREAT | 0600);
        if (id_sem == -1) {
                perror("semget");
                limpiar();
                exit(1);
        }//fin if

        // inicializar semaforos propios
        semctl(id_sem, IDX_SEM_CHOFER, SETVAL, 0);
        semctl(id_sem, IDX_SEM_MUTEX, SETVAL, 1);

        for (int i = 0; i < NUM_ALGORITMOS; i++) {
                semctl(id_sem, IDX_SEM_ORDEN(i), SETVAL, 1);
        }//fin for
}// incializar_semaforos

void inicializacion_buzones(){
        id_buzon = msgget(IPC_PRIVATE, IPC_CREAT | 0600);
        if (id_buzon == -1) {
                perror("msgget");
                limpiar();
                exit(1);
        }//fin if
}//fin funcion inicializacion_buzones

void inicializar_simulacion(TIPO_FUNCION_LLEGADA funciones[]){
        int resultado = PARKING_inicio(retardo, funciones, id_sem, id_buzon, id_mem, debug);

        //===========================================================
        if (debug) {
                fprintf(stderr, "[D-IPC] IPC creados -> SHM:%d SEM:%d MSG:%d\n", id_mem, id_sem, id_buzon);
                fprintf(stderr, "[D-PKG] PARKING_inicio retornó: %d\n", resultado);
                if (resultado == 0) {
                        fprintf(stderr, "[D-PKG] PARKING_inicio funciona correctamente\n");
                }//fin if
                else {
                        fprintf(stderr, "[D-PKG] PARKING_inicio falló\n");
                }//fin else
        }//fin if
        //===========================================================

        crear_chofer();

        PARKING_simulaciOn();// llamada a esta funcion desde el proceso padre, definicion funcion al final del codigo
}//fin funcion inicializar_simulacion

void crear_chofer(){
        // creación del proceso chofer
        pid_t pid_chofer = fork();

        // fallo en fork (creacion del proceso hijo)
        if (pid_chofer < 0) {
                perror("fork chofer");
                limpiar();
                exit(1);
        }//fin if

        // fork creó el proceso hijo
        if (pid_chofer == 0) {
                // el hijo ignora SIGINT, solo muere cuando el buzon desaparece
                signal(SIGINT, SIG_IGN);
                struct PARKING_mensajeBiblioteca msg;
                int alg_aux;


                /*hay que pasar la informacion a los callbacks (como
                    por ejemplo el algoritmo que sea), utilizo una
                    variable para pasarla por el puntero void *datos.
                */
                while (1) {
                        // si el buzon se limpia y msgrcv falla, salimos
                        if (msgrcv(id_buzon, &msg, sizeof(msg) - sizeof(long), 0, 0) == -1) {
                                break;
                        }//fin if
                        // imprime lo que llegó
                        if (debug) fprintf(stderr, "[D-CHOFER] tipo=%ld subtipo=%ld coche=%d\n", msg.tipo, msg.subtipo, msg.hCoche);

                        //subtipo para representar el indice del algoritmo (0 a 3) 
                        //NOTA BRAIS: EN el .h hay un int PARKING_getAlgoritmo(HCoche), por el nombre te diria que va aqui, no se con lo que tu tienes si también va, lo comento por que estuve mirando el .h
                        alg_aux = (int)msg.subtipo;

                        if (msg.subtipo == PARKING_MSGSUB_APARCAR) {
                                //==========================
                                if (debug) fprintf(stderr, "[D-CHOFER: aparcar] PID=%d -> Coche %d va a aparcar\n", getpid(), msg.hCoche);
                                //==========================
                                PARKING_aparcar(
                                        msg.hCoche,
                                        &alg_aux,
                                        aparcar_commit,
                                        permiso_avance,
                                        permiso_avance_commit
                                );
                        } else if (msg.subtipo == PARKING_MSGSUB_DESAPARCAR) {
                                //==========================
                                if (debug) fprintf(stderr, "[D-CHOFER: desaparcar] PID=%d -> Coche %d va a desaparcar\n", getpid(), msg.hCoche);
                                //==========================
                                PARKING_desaparcar(
                                        msg.hCoche,
                                        &alg_aux,
                                        permiso_avance,
                                        permiso_avance_commit
                                );
                        }//fin else if
                }//fin while
                if (debug) fprintf(stderr, "[D-CHOFER] PID=%d muriendo\n", getpid());

                exit(0);
        }//fin if
}//fin funcion crear_chofer

void finalizar_simulacion(){
        // esperar a que todos los hijos terminen antes de que el padre muera
        int status;
        pid_t pid_muerto;
        // esto luego seria mas sencillo pero para el mensaje de debug es necesario
        while ((pid_muerto = wait(&status)) > 0) {
                if (debug) fprintf(stderr, "[D-PADRE] Hijo PID=%d recogido, exit=%d\n",
                                pid_muerto, WEXITSTATUS(status));
        }//fin while

        if (debug) fprintf(stderr, "[D-PADRE] PID=%d muriendo\n", getpid());
}//fin funcion finalizar_simulacion

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
        validar_argumentos(argc, argv);

        parking();
        return 0;
} //fin funcion main

//========================================Definiciones funciones en el codigo========================================
/*
int PARKING_simulaciOn();
        El proceso padre debe llamar a esta función después de haber iniciado todo lo necesario para que 
        la simulación se lleve a cabo. Permanecerá dentro de la función hasta que la simulación acabe. No 
        obstante,  la  simulación  no  acabará  en  una  ejecución  normal  hasta  que  todos  los  coches 
        pendientes de desaparcar se hayan ido.
*/
