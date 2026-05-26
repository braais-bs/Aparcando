/*
* Sistemas Operativos II Práctica Windows - Aparcando
* Primera Convocatoria - Curso 2025-2026
* Grupo: G08 - Brais Bértolo Senra, Juan Riego Vila
* Fecha:
*/

#include <windows.h>
#include <stdio.h>
#include "parking2.h"

// variables globales
int retardo, num_choferes, debug, prio_PA, prio_PD;

// declaracion de los prototipos de las funciones
void validar_argumentos(int argc, char *argv[]);
void ayudaPrograma(char *argv[]);
BOOL WINAPI CtrlHandler (DWORD CtrlType);
//void inicializar_simulacion(TIPO_FUNCION_LLEGADA funciones[]);
void inicializar_simulacion();
void parking();
int main (int argc, char *argv[]);

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

void ayudaPrograma(char *argv[]){
        printf("=====AYUDA PROGRAMA [%s]=====\n",argv[0]);
        printf("Ejemplo uso:\n");
        printf("\t%s [numero de retardo] [numero de choferes] [tipo de politica / debug]\n", argv[0]);
        printf("\t  - [numero de retardo]-> Tiene que ser mayor o igual a 0, no hay limites con la velocidad, cuanto mas bajo sea el numero mas lento se ejecutara.\n");
        printf("\t  - [numero de choferes]-> Tiene que ser mayor o igual a 0, no hay limites con la cantidad de procesos chofer.\n");
        printf("\t  - [tipo de politica]-> Hay tres tipos de politica:.\n");
        printf("\t\t + FIFO: Si no se introduce argumento de politica es el que se ejecutara.\n");
        printf("\t\t + PA: Prioridad al Aparcar, se manejaran los mensajes de tal forma que los coches den prioridad a aparcar.\n");
        printf("\t\t + PD: Prioridad al Desaparcar, se manejaran los mensajes de tal forma que los coches den prioridad a desaparcar.\n");
        printf("\t  - [debug]-> Se puede obtener por la salida de errores los mensajes de depuracion del programa durante la ejecucion, este argumento puede ser el 3ro o 4to al invocar el programa.\n");
}//fin funcion ayudaPrograma

/*
  Función de manejo de los avisos de cierre o terminación.
  
  https://docs.microsoft.com/es-es/windows/console/handlerroutine
  
  Si la función controla la señal de control, debe devolver true. 
  Si devuelve false, se usa la función de controlador siguiente en la 
  lista de controladores para este proceso.
*/
BOOL WINAPI CtrlHandler (DWORD CtrlType){
        BOOL res= TRUE;
        switch (CtrlType) {

                // Handle the CTRL-C signal.
                case CTRL_C_EVENT:
                printf ("Ctrl-C event\n\n");
                Beep (750, 300);
                res= TRUE;
                break;

                // CTRL-CLOSE: confirm that the user wants to exit.
                case CTRL_CLOSE_EVENT:
                printf ("Ctrl-Close event\n\n");
                Beep (600, 200);
                res= TRUE;
                break;

                // Pass other signals to the next handler.
                case CTRL_BREAK_EVENT:
                printf ("Ctrl-Break event\n\n");
                Beep (900, 200);
                res= FALSE;
                break;

                case CTRL_LOGOFF_EVENT:
                printf ("Ctrl-Logoff event\n\n");
                Beep (1000, 200);
                res= FALSE;
                break;

                case CTRL_SHUTDOWN_EVENT:
                printf ("Ctrl-Shutdown event\n\n");
                Beep (750, 500);
                res= FALSE;
                break;

                default:
                res= FALSE;
                break;
        }//fin switch

        return res;
}//fin funcion CtrlHandler

//void inicializar_simulacion(TIPO_FUNCION_LLEGADA funciones[]){
void inicializar_simulacion(){
        TIPO_FUNCION_LLEGADA funcionLlegada[1];
        TIPO_FUNCION_SALIDA funcionSalida[1];
        long intervalo = 0.0;
        bool d = 0;

        int resultado = PARKING2_inicio(funcionLlegada,funcionSalida, intervalo, d);
}//fin funcion inicializar_simulacion

void parking(){
        // array de 4 funciones, una por algoritmo (PRIMER, SIGUIENTE, MEJOR, PEOR)
//        TIPO_FUNCION_LLEGADA funciones[4] = {
//                primer_ajuste,
//                siguiente_ajuste,
//                mejor_ajuste,
//                peor_ajuste
//        };

        // iniciar la simulacion con los valores leidos de los argumentos
//        inicializar_simulacion(funciones);
        inicializar_simulacion();

//        finalizar_simulacion();
}//fin funcion parking

int main (int argc, char *argv[]) {
        //Parte validador de argumentos (tiene que estar aqui porque si no la variable debug no se cambia)
        validar_argumentos(argc, argv);
        if (debug){
                fprintf(stderr,"[DBG-Variable-Debug]Estado variable debug = %d\n",debug);
                fprintf(stderr,"NUM_VELOCIDAD: %d\n",retardo);
                fprintf(stderr,"NUM_CHOFERES: %d\n",num_choferes);
        }//fin if

        //Parte manejo sennal Control C
        BOOL added;

        added = SetConsoleCtrlHandler ((PHANDLER_ROUTINE) CtrlHandler,
                                        TRUE);
        if (added) {
                if(debug){
                        fprintf (stderr,"\n[Start Of ControlC Handler Output]");
                        fprintf (stderr,"\nThe Control Handler is installed.\n");
                        fprintf (stderr,"\n -- Now try pressing Ctrl+C or Ctrl+Break, or");
                        fprintf (stderr,"\n    try logging off or closing the console...\n");
                        fprintf (stderr,"\n(...waiting in a loop for events...)\n");
                        fprintf (stderr,"[End Of ControlC Handler Output]\n");
        /*
                        while (1) {
                                Sleep (500);
                        }//fin while
        */
                }//fin if
                else fprintf (stderr,"\nERROR[CtrlC]: Could not set controlC handler");
        }//fin if
        parking();


        return 0;
}//fin funcion main
