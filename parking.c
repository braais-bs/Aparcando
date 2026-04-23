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
#define TAM_PARKING 80
#define NUM_SEM_PROPIOS (2 + NUM_ALGORITMOS + NUM_ALGORITMOS) // NUM_ALGORITMOS dos veces, una para los de orden y otra para los de avance

#define IDX_SEM_CHOFER (nSem)
#define IDX_SEM_MUTEX (nSem + 1)
#define IDX_SEM_ORDEN(a) (nSem + 2 + (a))
#define IDX_SEM_AVANCE(a) (nSem + 2 + NUM_ALGORITMOS + (a))

volatile sig_atomic_t terminar = 0;
volatile sig_atomic_t turno_recibido = 0;

typedef struct {
        int acera[NUM_ALGORITMOS][TAM_PARKING];
        int carril[NUM_ALGORITMOS][TAM_PARKING];
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
void manejar_ctrlc(int signal);
void ayudaPrograma(char *argv[]);
void validar_argumentos(int argc, char *argv[]);
int primer_ajuste(HCoche hc);
int siguiente_ajuste(HCoche hc);
int mejor_ajuste(HCoche hc);
int peor_ajuste(HCoche hc);
void aparcar_commit(HCoche hc);
void permiso_avance(HCoche hc);
void permiso_avance_commit(HCoche hc);
int ocupar_carril_desaparcar(int alg, int X2, int longitud);
void vaciar_pos_acera(int pos, int longitud, int alg);
void inicializar_memoria_compartida();
void inicializacion_buzones();
void inicializar_semaforos();
void sem_esperar_turno(int idx_sem, int num_coche);
void sem_liberar_turno(int idx_sem, int num_coche);
void limpiar(void);
void crear_chofer();
void bucle_chofer();
void inicializar_simulacion(TIPO_FUNCION_LLEGADA funciones[]);
void finalizar_simulacion();
void parking();
int main(int argc, char *argv[]);


void manejar_ctrlc(int signal) {
        //=======================
        if (debug) fprintf(stderr, "[D-SIG] Señal %d recibida\n", signal);
        //=======================

        PARKING_fin(0);
        limpiar();
        finalizar_simulacion();  // wait de los hijos
        system("tput cup 30 0");
        system("tput cnorm");
        exit(0); // sale del programa
}//fin funcion manejar_ctrlc

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
        int longitud = PARKING_getLongitud(hc); // obtiene la longitud del coche
        int huecoLibre = 0; // contador de posiciones libres consecutivas (para saber si el coche entra en un hueco)
        int pos = -1; // posicion donde aparca (-1 es que no encontro hueco)

        if (debug) fprintf(stderr, "[D-ALG:primer_ajuste] Coche %d - longitud=%d - buscando hueco\n", PARKING_getNUmero(hc), longitud);

        for (int i = 0; i < TAM_PARKING; ) {
                if (mp->acera[PRIMER_AJUSTE][i] == 0) {
                        huecoLibre++; // suma uno al contador de posiciones libres seguidas
                        if (huecoLibre >= longitud) { // si el hueco libre (posiciones seguidas) es mayor o igual que la longitud del coche
                                pos = i - longitud + 1; // calcula el inicio del hueco
                                break;
                        }
                        i++;
                } else {
                        i += mp->acera[PRIMER_AJUSTE][i]; // NOTA BRAIS: esto es un aoptimizacion que meti ahora. antes hacia un for con i++ que iba posicion por posicion. Ahora, si encuentro un coche aparcado, salta las posiciones que ocupe ese coche (por eso de que en el coche para indicar que una posicion esta ocupada se guarda la longitud del coche)
                        huecoLibre = 0; // si mp->acera en algun momento no da 0, se reinicia el contador
                }
        }

        if (pos >= 0) { // si encontro hueco
                for (int i = pos; i < pos + longitud; i++) { // recorre las posiciones que ocupara el coche
                        mp->acera[PRIMER_AJUSTE][i] = longitud; // las marca como ocupadas (las marca con el numero de longitud del coche)
                }
        }

        if (debug) {
                if (pos >= 0)
                        fprintf(stderr, "[D-ALG:primer_ajuste] Coche %d -> hueco encontrado en pos=%d\n", PARKING_getNUmero(hc), pos);
                else
                        fprintf(stderr, "[D-ALG:primer_ajuste] Coche %d -> sin hueco (pos=-1)\n", PARKING_getNUmero(hc));
        }//fin if

        return pos; //
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


//-=-=-=-=-=-=-=-=-=-=- Funciones de Rellamada (Callbacks), para el apartado 8 -=-=-=-=-=-=-=-=-=-=-

//se ejecuta cuando la biblioteca confirma que el coche ha aparcado
void aparcar_commit(HCoche hc) {
        //==========================
        if (debug) fprintf(stderr, "[D-PKG:aparcar_commit] Coche %d aparcado\n", PARKING_getNUmero(hc));
        //==========================
        
        sem_liberar_turno(IDX_SEM_ORDEN(PARKING_getAlgoritmo(hc)), PARKING_getNUmero(hc));
}//fin funcion aparcar_commit

//se ejecuta cuando el coche quiere moverse. debe bloquearse hasta que sea seguro
void permiso_avance(HCoche hc) {
        if (debug) fprintf(stderr, "[D-PKG:permiso_avance] Coche %d pidiendo permiso para avanzar...\n", PARKING_getNUmero(hc));
        
        int X1 = PARKING_getX(hc);
        int Y1 = PARKING_getY(hc);
        int X2 = PARKING_getX2(hc);
        int Y2 = PARKING_getY2(hc);
        int alg = PARKING_getAlgoritmo(hc);

        //==========================
        if (debug) fprintf(stderr, "[D-PKG:permiso_avance] Movimiento que quiere hacer el coche %d: (%d,%d) a (%d,%d)\n", PARKING_getNUmero(hc), X1, Y1, X2, Y2);
        //==========================
        
        // avance por el mismo carril
        if (Y1 == 2 && Y2 == 2 && X2 >= 0 && X2 < TAM_PARKING) {
                struct sembuf op;
                while (1) {
                        op = (struct sembuf){IDX_SEM_MUTEX, -1, 0};
                        semop(id_sem, &op, 1);

                        if (mp->carril[alg][X2] == 0) {
                                mp->carril[alg][X2] = 1;
                                op = (struct sembuf){IDX_SEM_MUTEX, +1, 0};
                                semop(id_sem, &op, 1);
                                break;
                        }

                        op = (struct sembuf){IDX_SEM_MUTEX, +1, 0};
                        semop(id_sem, &op, 1);

                        op = (struct sembuf){IDX_SEM_AVANCE(alg), -1, 0};
                        semop(id_sem, &op, 1);
                }
        }
        // avance desde la acera a la carretera
        if (Y1 < Y2 && Y2 == 2 && X2 >= 0 && X2 + PARKING_getLongitud(hc) - 1 < TAM_PARKING) {
                struct sembuf op;
                while (1) {
                        op = (struct sembuf){IDX_SEM_MUTEX, -1, 0};
                        semop(id_sem, &op, 1);

                        if (ocupar_carril_desaparcar(alg, X2, PARKING_getLongitud(hc))) {
                                op = (struct sembuf){IDX_SEM_MUTEX, +1, 0};
                                semop(id_sem, &op, 1);
                                break;
                        }

                        op = (struct sembuf){IDX_SEM_MUTEX, +1, 0};
                        semop(id_sem, &op, 1);

                        op = (struct sembuf){IDX_SEM_AVANCE(alg), -1, 0};
                        semop(id_sem, &op, 1);
                }
        }
        
}//fin funcion permiso_avance

void permiso_avance_commit(HCoche hc) {
        int X_anterior = PARKING_getX2(hc); // de donde venia el coche
        int Y_anterior = PARKING_getY2(hc);
        int Y_actual = PARKING_getY(hc);
        int alg = PARKING_getAlgoritmo(hc);

        if (debug) fprintf(stderr, "[D-PKG:permiso_avance_commit] Coche %d avanzó a (%d,%d), venía de (%d,%d)\n", PARKING_getNUmero(hc), PARKING_getX(hc), Y_actual, X_anterior, Y_anterior);

        // libera si el coche desaparca del carril
        if (Y_anterior == 1 && Y_actual == 2) {
                vaciar_pos_acera(PARKING_getPosiciOnEnAcera(hc), PARKING_getLongitud(hc), alg);
        }

        // libera si el coche sale del carril para aparcar
        if (Y_anterior == 2 && Y_actual == 1 && X_anterior >= 0 && X_anterior + PARKING_getLongitud(hc) - 1 < TAM_PARKING) {
                struct sembuf op;

                op = (struct sembuf){IDX_SEM_MUTEX, -1, 0};
                semop(id_sem, &op, 1);
                for (int i = X_anterior; i < X_anterior + PARKING_getLongitud(hc); i++) {
                        mp->carril[alg][i] = 0;
                }
                op = (struct sembuf){IDX_SEM_MUTEX, +1, 0};
                semop(id_sem, &op, 1);

                op = (struct sembuf){IDX_SEM_AVANCE(alg), +1, 0};
                semop(id_sem, &op, 1);
        }

        // libera si el coche se mueve en horizontal en el mismo carril
        if (Y_anterior == 2 && Y_actual == 2 && X_anterior < TAM_PARKING) {
                int cola = X_anterior + PARKING_getLongitud(hc) - 1;

                struct sembuf op;
                op = (struct sembuf){IDX_SEM_MUTEX, -1, 0};
                semop(id_sem, &op, 1);
                mp->carril[alg][cola] = 0; // libera donde estaba
                op = (struct sembuf){IDX_SEM_MUTEX, +1, 0};
                semop(id_sem, &op, 1);

                // despertar a los que esperan esta posición
                op = (struct sembuf){IDX_SEM_AVANCE(alg), +1, 0};
                semop(id_sem, &op, 1);
        }//fin if
}//fin funcion mi_permiso_avance_commit

int ocupar_carril_desaparcar(int alg, int X2, int longitud) {
        // recorremos las posiciones que ocupara el coche en el carril
        for (int i = X2; i < X2 + longitud; i++) {
                if (mp->carril[alg][i] != 0) {
                        return 0; // si hay alguna ocupada devolvemos 0
                }
        }
        // si todas estan libres, las ocupamos
        for (int i = X2; i < X2 + longitud; i++) {
                mp->carril[alg][i] = 1;
        }
        return 1;
}//fin funcion ocupar_carril_desaparcar

void vaciar_pos_acera(int pos, int longitud, int alg) {
        if (pos >= 0) {
                for (int i = pos; i < pos + longitud; i++) { // recorre las posiciones que ocupara el coche
                        mp->acera[alg][i] = 0; // las marca como libres
                }//fin for
        }//fin if
}//fin funcion vaciar_pos_acera

void inicializar_memoria_compartida(){
        // obtener tamaño de la memoria compartida
        tam = PARKING_getTamaNoMemoriaCompartida();

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
}

void inicializacion_buzones(){
        id_buzon = msgget(IPC_PRIVATE, IPC_CREAT | 0600);
        if (id_buzon == -1) {
                perror("msgget");
                limpiar();
                exit(1);
        }//fin if
}//fin funcion inicializacion_buzones

void inicializar_semaforos(){
        // obtener numero de semaforos
        nSem = PARKING_getNSemAforos();

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
                semctl(id_sem, IDX_SEM_AVANCE(i), SETVAL, 0);
        }//fin for
}//fin funcion incializar_semaforos

// espera por su turno sin consumir CPU
void sem_esperar_turno(int idx_sem, int num_coche) {
        struct sembuf op;
        op.sem_num = idx_sem;
        op.sem_op  = -num_coche;
        op.sem_flg = 0;
        semop(id_sem, &op, 1);

        // restaurar el valor porque semop lo restó
        op.sem_op = +num_coche;
        semop(id_sem, &op, 1);
}

// permite pasar al siguiente coche
void sem_liberar_turno(int idx_sem, int num_coche) {
        semctl(id_sem, idx_sem, SETVAL, num_coche + 1);
}

void limpiar(void) {
        //=======================
        if (debug) fprintf(stderr, "[D-IPC] Limpiando recursos\n");
        //=======================
        if (mem_base != NULL && mem_base != (char *)-1) {
                shmdt(mem_base);
        }//fin if
        if (id_mem != -1) {
                shmctl(id_mem,   IPC_RMID, NULL);
        }//fin if
        if (id_sem != -1) {
                semctl(id_sem, 0, IPC_RMID);
        }//fin if
        if (id_buzon != -1) {
                msgctl(id_buzon, IPC_RMID, NULL);
        }//fin if
}//fin funcion limpiar

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
                bucle_chofer();
                if (debug) fprintf(stderr, "[D-CHOFER] PID=%d muriendo\n", getpid());
                exit(0);
        }//fin if
}//fin funcion crear_chofer

void bucle_chofer(){
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
                alg_aux = PARKING_getAlgoritmo(msg.hCoche);

                if (msg.subtipo == PARKING_MSGSUB_APARCAR) {
                        //==========================
                        if (debug) {
                            fprintf(stderr, "[D-CHOFER: aparcar] PID=%d -> Coche %d quiere aparcar\n", getpid(), msg.hCoche);
                            fprintf(stderr, "[DBUZON-CHOFER: aparcar] PID=%d -> Coche %d: msg_tipo = %li, msg_subtipo = %li\n", getpid(), msg.hCoche, msg.tipo, msg.subtipo);
                        }//fin if
                        //==========================

                        // esperar a que sea el turno de este coche
                        sem_esperar_turno(IDX_SEM_ORDEN(alg_aux), PARKING_getNUmero(msg.hCoche));

                        //==========================
                        if (debug) {
                                fprintf(stderr, "[D-CHOFER: aparcar] PID=%d -> Coche %d ya puede aparcar\n", getpid(), msg.hCoche);
                                fprintf(stderr, "[DBUZON-CHOFER: aparcar] PID=%d -> Coche %d: msg_tipo = %li, msg_subtipo = %li\n", getpid(), msg.hCoche, msg.tipo, msg.subtipo);
                        }//fin if
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
                        if (debug){
                                fprintf(stderr, "[D-CHOFER: desaparcar] PID=%d -> Coche %d va a desaparcar\n", getpid(), msg.hCoche);
                                fprintf(stderr, "[DBUZON-CHOFER: desaparcar] PID=%d -> Coche %d: msg_tipo = %li, msg_subtipo = %li\n", getpid(), msg.hCoche, msg.tipo, msg.subtipo);
                        }//fin if
                        //==========================

                        PARKING_desaparcar(
                                msg.hCoche,
                                &alg_aux,
                                permiso_avance,
                                permiso_avance_commit
                        );
                }//fin else if
        }//fin while
}//fin funcion bucle_chofer


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

        //creacion de los choferes
        for(int i=0; i < num_choferes; i++){
                crear_chofer();
        }//fin for

        PARKING_simulaciOn();// llamada a esta funcion desde el proceso padre, definicion funcion al final del codigo
}//fin funcion inicializar_simulacion

void finalizar_simulacion(){
        // si la terminación es normal (pasaron 30s) se destruye el buzón para que los hijos salgan de msgrcv
        if (id_buzon != -1) {
                msgctl(id_buzon, IPC_RMID, NULL);
                id_buzon = -1;
        }//fin if
        
        // esperar a que todos los hijos terminen antes de que el padre muera
        int status;
        pid_t pid_muerto;
        // esto luego seria mas sencillo pero para el mensaje de debug es necesario
        while ((pid_muerto = wait(&status)) > 0) {
                if (debug) fprintf(stderr, "[D-PADRE] Hijo PID=%d recogido, exit=%d\n", pid_muerto, WEXITSTATUS(status));
        }//fin while

        if (debug) fprintf(stderr, "[D-PADRE] PID=%d muriendo\n", getpid());
}//fin funcion finalizar_simulacion

void parking(){
        // array de 4 funciones, una por algoritmo (PRIMER, SIGUIENTE, MEJOR, PEOR)
        TIPO_FUNCION_LLEGADA funciones[4] = {
                primer_ajuste,
                siguiente_ajuste,
                mejor_ajuste,
                peor_ajuste
        };

        //creacion de la memoria compartida
        inicializar_memoria_compartida();

        // creacion e inicializacion de los semaforos
        inicializar_semaforos();

        //creacion del buzon
        inicializacion_buzones();

        // iniciar la simulacion con los valores leidos de los argumentos
        inicializar_simulacion(funciones);

        finalizar_simulacion();
}//fin funcion parking



//Luego si se me va te lo dejo aqui por si lo lees yo creo que main deberiamos de vaciarlo porque de main solo tendrian que haber llamadas a fuciones y tal
int main(int argc, char *argv[]) {
        //configurar señales
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

        if (debug){
                fprintf(stderr,"NUM_VELOCIDAD: %d\n",retardo);
                fprintf(stderr,"NUM_CHOFERES: %d\n",num_choferes);
        }//fin if

        parking();

        //limpieza de los recursos utilizados
        limpiar();
        system("tput cnorm"); //esto lo que hace es volver a poner el cursor "normal", ya que cuando se ejecuta el programa a veces el cursor se queda en modo "escondido" esto lo que hace es cambiarle a modo mostar

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
