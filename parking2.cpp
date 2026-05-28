/*
* Sistemas Operativos II Práctica Windows - Aparcando
* Primera Convocatoria - Curso 2025-2026
* Grupo: G08 - Brais Bértolo Senra, Juan Riego Vila
* Fecha:
*/

#include <windows.h>
#include <stdio.h>
#include "parking2.h"

// constantes
#define NUM_ALGORITMOS 4
#define TAM_PARKING 80

// variables globales
int retardo, debug;

// estas son las que teniamos en memoria compartida, ahora son variables globales
int acera[NUM_ALGORITMOS][TAM_PARKING];
int carril[NUM_ALGORITMOS][TAM_PARKING];
int proxAparcar[NUM_ALGORITMOS];
int proxAparcarNum[NUM_ALGORITMOS]; // esto permite saber el numero de coche que sera el siguiente en aparcar.
                                   // antes teniamos un semaforo que hacia numcoches + 1, parece ser que en windows no se puede. De todas maneras dejo esto y luego vamos mirando

// sincronizacion para cada algoritmo. Les pongo h al principio, no es obligatorio pero en win32 es convencion de que son manejadores (HANDLE)
HANDLE hMutex[NUM_ALGORITMOS]; // un acceso a la vez a los arrays de cada algoritmo
HANDLE hOrden[NUM_ALGORITMOS]; // para controlar el orden de aparcamiento
HANDLE hAvance[NUM_ALGORITMOS]; // para controlar que solo un coche avance a la vez

// manejador de la DLL
HMODULE hDLL = NULL;

// no se si me comería alguno, si eso vamos metiendo sobre la marcha si se usa alguno que no aparece
// punteros a funciones de la DLL (los pille del .h y del enunciado de la practica)
int (*PARKING2_inicio) (TIPO_FUNCION_LLEGADA*, TIPO_FUNCION_SALIDA*, long, int) = NULL;
int (*PARKING2_fin) (void) = NULL;
int (*PARKING2_aparcar) (HCoche, void*, TIPO_FUNCION_APARCAR_COMMIT, TIPO_FUNCION_PERMISO_AVANCE, TIPO_FUNCION_PERMISO_AVANCE_COMMIT) = NULL;
int (*PARKING2_desaparcar) (HCoche, void*, TIPO_FUNCION_PERMISO_AVANCE, TIPO_FUNCION_PERMISO_AVANCE_COMMIT) = NULL;
int (*PARKING2_getNUmero) (HCoche) = NULL;
int (*PARKING2_getLongitud) (HCoche) = NULL;
int (*PARKING2_getPosiciOnEnAcera) (HCoche) = NULL;
unsigned long (*PARKING2_getTServ) (HCoche) = NULL;
int (*PARKING2_getColor) (HCoche) = NULL;
void* (*PARKING2_getDatos) (HCoche) = NULL;
int (*PARKING2_getX) (HCoche) = NULL;
int (*PARKING2_getY) (HCoche) = NULL;
int (*PARKING2_getX2) (HCoche) = NULL;
int (*PARKING2_getY2) (HCoche) = NULL;
int (*PARKING2_getAlgoritmo) (HCoche) = NULL;
int (*PARKING2_isAceraOcupada) (int, int) = NULL;

// declaracion de los prototipos de las funciones
void cargar_dll();
void eliminar_dll();
void validar_argumentos(int argc, char* argv[]);
void ayudaPrograma(char* argv[]);
BOOL WINAPI CtrlHandler(DWORD CtrlType);
void inicializar_sincronizacion();
void eliminar_sincronizacion();
//void inicializar_simulacion(TIPO_FUNCION_LLEGADA funciones[]);
void inicializar_simulacion();
void parking();
int main(int argc, char* argv[]);


void cargar_dll() {
    // cargamos la DLL dinámicamente. Devuelve un manejador que es hDLL 
    hDLL = LoadLibrary("parking2.dll");

    // en caso de que falle el manejador valdrá NULL, por tanto mostramos el err9r
    if (hDLL == NULL) {
        fprintf(stderr, "ERROR[DLL]: No se pudo cargar parking2.dll (error %lu)\n", GetLastError());
        exit(1); // sin DLL no podemos seguir, por tanto salimos
    }//fin if

    if (debug)
        fprintf(stderr, "[DLL] parking2.dll cargada correctamente\n");

    // buscamos dentro las funciones del dll y creamos un puntero a cada una
    // Ade´más cambiamos el tipo de cada puntero para que coincida con el de la función

    // PARA ENTENDER MEJOR, SERIA ALGO ASÍ CON TODAS:

    // puntero a la funcion de la DLL = (tipo que devuelve la funcion (* que indica que es un puntero) (tipo de los parámetros de la función))
    PARKING2_inicio = (int (*)(TIPO_FUNCION_LLEGADA*, TIPO_FUNCION_SALIDA*, long, int))
        // busca la funcion dentro del DLL (que es hDLL) y obtine la direccion y busca la función PARKING2_inicio
        GetProcAddress(hDLL, "PARKING2_inicio");

    // estas serían igual que la anterior
    PARKING2_fin = (int (*)(void))
        GetProcAddress(hDLL, "PARKING2_fin");
    PARKING2_aparcar = (int (*)(HCoche, void*, TIPO_FUNCION_APARCAR_COMMIT, TIPO_FUNCION_PERMISO_AVANCE, TIPO_FUNCION_PERMISO_AVANCE_COMMIT))
        GetProcAddress(hDLL, "PARKING2_aparcar");
    PARKING2_desaparcar = (int (*)(HCoche, void*, TIPO_FUNCION_PERMISO_AVANCE, TIPO_FUNCION_PERMISO_AVANCE_COMMIT))
        GetProcAddress(hDLL, "PARKING2_desaparcar");
    PARKING2_getNUmero = (int (*)(HCoche))
        GetProcAddress(hDLL, "PARKING2_getNUmero");
    PARKING2_getLongitud = (int (*)(HCoche))
        GetProcAddress(hDLL, "PARKING2_getLongitud");
    PARKING2_getPosiciOnEnAcera = (int (*)(HCoche))
        GetProcAddress(hDLL, "PARKING2_getPosiciOnEnAcera");
    PARKING2_getTServ = (unsigned long (*)(HCoche))
        GetProcAddress(hDLL, "PARKING2_getTServ");
    PARKING2_getColor = (int (*)(HCoche))
        GetProcAddress(hDLL, "PARKING2_getColor");
    PARKING2_getDatos = (void* (*)(HCoche))
        GetProcAddress(hDLL, "PARKING2_getDatos");
    PARKING2_getX = (int (*)(HCoche))
        GetProcAddress(hDLL, "PARKING2_getX");
    PARKING2_getY = (int (*)(HCoche))
        GetProcAddress(hDLL, "PARKING2_getY");
    PARKING2_getX2 = (int (*)(HCoche))
        GetProcAddress(hDLL, "PARKING2_getX2");
    PARKING2_getY2 = (int (*)(HCoche))
        GetProcAddress(hDLL, "PARKING2_getY2");
    PARKING2_getAlgoritmo = (int (*)(HCoche))
        GetProcAddress(hDLL, "PARKING2_getAlgoritmo");
    PARKING2_isAceraOcupada = (int (*)(int, int))
        GetProcAddress(hDLL, "PARKING2_isAceraOcupada");

    // verificamos todas las funciones y si cualquiera falló eliminamos la DLL y saldriamos también del programa
    if (!PARKING2_inicio || !PARKING2_fin ||
        !PARKING2_aparcar || !PARKING2_desaparcar ||
        !PARKING2_getNUmero || !PARKING2_getLongitud ||
        !PARKING2_getPosiciOnEnAcera || !PARKING2_getTServ ||
        !PARKING2_getColor || !PARKING2_getDatos ||
        !PARKING2_getX || !PARKING2_getY ||
        !PARKING2_getX2 || !PARKING2_getY2 ||
        !PARKING2_getAlgoritmo || !PARKING2_isAceraOcupada) {
        fprintf(stderr, "ERROR[DLL]: No se encontro una funcion en la DLL (error %lu)\n", GetLastError());
        eliminar_dll();
        exit(1);
	}//fin if

        if (debug)
            fprintf(stderr, "[DLL] Todas las funciones fueron resueltas correctamente\n");
}//fin funcion cargar_dll


void eliminar_dll() {
        // si aun está cargado el DLL
        if (hDLL != NULL) {
                FreeLibrary(hDLL); // se elimina
                hDLL = NULL;
                if (debug) {
                        fprintf(stderr, "[DLL] DLL elimiada correctamente\n");
                }// fin if
        }//fin if
}//fin funcion descargar_dll

    void validar_argumentos(int argc, char* argv[]) {
        if (argc < 2 || argc > 3) {
            fprintf(stderr, "Error: numero de argumentos incorrecto\n");
            ayudaPrograma(argv);
            exit(1);
        }//fin if

        retardo = atoi(argv[1]);
        if (retardo < 0) {
            fprintf(stderr, "Error: el retardo debe ser >= 0\n");
            exit(1);
        }//fin if

        // argumento opcional de debug
        debug = 0;

        if (argc == 3) {
            if (strcmp(argv[2], "D") == 0) {
                debug = 1;
            }//fin if
            else {
                fprintf(stderr, "Error: argumento desconocido '%s'\n", argv[2]);
                exit(1);
            }//fin else
        }//fin if
    }//fin funcion validar_argumentos

    void ayudaPrograma(char* argv[]) {
        printf("=====AYUDA PROGRAMA [%s]=====\n", argv[0]);
        printf("Ejemplo uso:\n");
        printf("\t%s [numero de retardo] [debug]\n", argv[0]);
        printf("\t  - [numero de retardo]-> Tiene que ser mayor o igual a 0, no hay limites con la velocidad, cuanto mas bajo sea el numero mas lento se ejecutara.\n");
        printf("\t  - [debug]-> Se puede obtener por la salida de errores los mensajes de depuracion del programa durante la ejecucion, este argumento es opcional y debe ser la letra D.\n");
    }//fin funcion ayudaPrograma

    /*
      Función de manejo de los avisos de cierre o terminación.

      https://docs.microsoft.com/es-es/windows/console/handlerroutine

      Si la función controla la señal de control, debe devolver true.
      Si devuelve false, se usa la función de controlador siguiente en la
      lista de controladores para este proceso.
    */
    BOOL WINAPI CtrlHandler(DWORD CtrlType) {
        BOOL res = TRUE;
        switch (CtrlType) {

            // Handle the CTRL-C signal.
        case CTRL_C_EVENT:
            printf("Ctrl-C event\n\n");
            Beep(750, 300);
            eliminar_dll();
            res = TRUE;
            break;

            // CTRL-CLOSE: confirm that the user wants to exit.
        case CTRL_CLOSE_EVENT:
            printf("Ctrl-Close event\n\n");
            Beep(600, 200);
            res = TRUE;
            eliminar_dll();
            break;

            // Pass other signals to the next handler.
        case CTRL_BREAK_EVENT:
            printf("Ctrl-Break event\n\n");
            Beep(900, 200);
            res = FALSE;
            break;

        case CTRL_LOGOFF_EVENT:
            printf("Ctrl-Logoff event\n\n");
            Beep(1000, 200);
            res = FALSE;
            break;

        case CTRL_SHUTDOWN_EVENT:
            printf("Ctrl-Shutdown event\n\n");
            Beep(750, 500);
            res = FALSE;
            break;

        default:
            res = FALSE;
            break;
        }//fin switch

        return res;
    }//fin funcion CtrlHandler

void inicializar_sincronizacion() {
        for (int i = 0; i < NUM_ALGORITMOS; i++) {
                // manejador del mutex = CreateMutex(
                //     NULL --> cualquier proceso puede operar sin restricciones sobre el semáforo
                //     FALSE --> hace que el mutex al principio quede libre y no lo tome ningun proceso hasta que empieze lasimulacion, y el primero al que le toque lo tomará
                //     NULL --> puedes darle un nombre para que los procesos accedan por el nombre, pero no es necesario)
                hMutex[i] = CreateMutex(NULL, FALSE, NULL);
                // si el manejador es NULL es porque hubo un fallo al crear el mutex
                if (hMutex[i] == NULL) {
                        fprintf(stderr, "ERROR[SYNC]: No se pudo crear hMutex[%d] (error %lu)\n", i, GetLastError());
                        exit(1);
                }//fin if
  
				// manejador del semaforo de orden = CreateSemaphore(
				//    NULL --> cualquier proceso puede operar sin restricciones sobre el semáforo
				//    1 --> el semáforo empieza con un recurso disponible, los recursos miden el numero de coches que pueden acceder
				//    1 --> número máximo de recursos que pueden llegar a estar disponibles. El semaforo de orden solo deja pasar al coche que le toca, por tanto 1
				//    NULL --> puedes darle un nombre para que los procesos accedan por el nombre, pero no es necesario)
                hOrden[i] = CreateSemaphore(NULL, 1, 1, NULL);
                if (hOrden[i] == NULL) {
                        fprintf(stderr, "ERROR[SYNC]: No se pudo crear hOrden[%d] (error %lu)\n", i, GetLastError());
                        exit(1);
                }//fin if

                // manejador del semaforo de avance = CreateSemaphore(
                //    NULL --> cualquier proceso puede operar sin restricciones sobre el semáforo
                //    0 --> el semáforo empieza con 0 recursos disponibles, esto permite que si un coche no ouede avanzar se duerma aqui
                //    999 --> número máximo de recursos que pueden llegar a estar disponibles. Puede ser que haya varios coches que quieran avanzar pero se tuvieran que dormir, asi que permitimos un numero a lto para evitar problemas
                //    NULL --> puedes darle un nombre para que los procesos accedan por el nombre, pero no es necesario)
                hAvance[i] = CreateSemaphore(NULL, 0, 999, NULL);
                hAvance[i] = CreateSemaphore(NULL, 0, 999, NULL);
                if (hAvance[i] == NULL) {
                        fprintf(stderr, "ERROR[SYNC]: No se pudo crear hAvance[%d] (error %lu)\n", i, GetLastError());
                        exit(1);
                }//fin if

                // el primer coche en aparcar en cada algoritmo es el numero 1
                // equivale a: mp->proxAparcar[i] = 1 en Linux
                proxAparcarNum[i] = 1;

                // siguiente_ajuste empieza a buscar desde la posicion 0
                proxAparcar[i] = 0;
        }//fin for


        // inicializamos las arrays de acera y carril a 0 (todo libre)
        memset(acera, 0, sizeof(acera));
        memset(carril, 0, sizeof(carril));

        if (debug)
                fprintf(stderr, "[SYNC] Objetos de sincronizacion inicializados correctamente\n");
}//fin funcion inicializar_sincronizacion

void liberar_sincronizacion() {
        for (int a = 0; a < NUM_ALGORITMOS; a++) {
                // CloseHandle cierra el maanejador y lo libera
                if (hMutex[a])  CloseHandle(hMutex[a]);
                if (hOrden[a])  CloseHandle(hOrden[a]);
                if (hAvance[a]) CloseHandle(hAvance[a]);
        }//fin for

        if (debug)
                fprintf(stderr, "[SYNC] Objetos de sincronizacion liberados correctamente\n");
}//fin funcion liberar_sincronizacion

    //void inicializar_simulacion(TIPO_FUNCION_LLEGADA funciones[]){
    void inicializar_simulacion() {
        TIPO_FUNCION_LLEGADA funcionLlegada[4];
        TIPO_FUNCION_SALIDA funcionSalida[4];
        long intervalo = 0.0;
        bool d = 0;

        int resultado = PARKING2_inicio(funcionLlegada, funcionSalida, intervalo, d);
    }//fin funcion inicializar_simulacion


    void parking() {
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

    int main(int argc, char* argv[]) {
        //Parte validador de argumentos (tiene que estar aqui porque si no la variable debug no se cambia)
        validar_argumentos(argc, argv);
        if (debug) {
            fprintf(stderr, "[DBG-Variable-Debug]Estado variable debug = %d\n", debug);
            fprintf(stderr, "NUM_VELOCIDAD: %d\n", retardo);
        }//fin if

        //Parte manejo sennal Control C
        BOOL added;

        added = SetConsoleCtrlHandler((PHANDLER_ROUTINE)CtrlHandler, TRUE);
        if (added) {
            if (debug) {
                fprintf(stderr, "\n[Inicio de la salida manejador Control+C]\n");
                fprintf(stderr, "El manejador de Control+C esta instalado.\n");
                fprintf(stderr, "\n -- Ahora prueba a pulsar Ctrl+C o Ctrl+Break, o");
                fprintf(stderr, "\n    intenta cerrar la consola...\n");
                fprintf(stderr, "\n(...esperando eventos...)\n");
                fprintf(stderr, "[Fin salida manejador Control+C]\n");
                /*
                                while (1) {
                                        Sleep (500);
                                }//fin while
                */
            }//fin if
        }
        else {
            fprintf(stderr, "\nERROR[CtrlC]: No se pudo instalar el manejador de Control+C\n");
        }//fin if

        // cargamos dinamicamente el dll
        cargar_dll();

        parking();

        // eliminamos el dll antes de terminar
        eliminar_dll();

        return 0;
    }//fin funcion main
