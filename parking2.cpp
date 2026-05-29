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
int proxAparcarNum[NUM_ALGORITMOS]; // permite saber el numero de coche que sera el siguiente en aparcar.

// sincronizacion para cada algoritmo.
HANDLE hMutex[NUM_ALGORITMOS];  // un acceso a la vez a los arrays de cada algoritmo
HANDLE hOrden[NUM_ALGORITMOS];  // para controlar el orden de aparcamiento
HANDLE hAvance[NUM_ALGORITMOS]; // para controlar que solo un coche avance a la vez

// manejador de la DLL
HMODULE hDLL = NULL;

// manejador para terminr la simulacion al 
HANDLE hEventoFin = NULL;

// punteros a funciones de la DLL
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
void liberar_sincronizacion();
void parking();
int main(int argc, char* argv[]);

// Prototipos de funciones de asignación de memoria (adaptados de Linux)
int primer_ajuste(HCoche hc);
int siguiente_ajuste(HCoche hc);
int mejor_ajuste(HCoche hc);
int peor_ajuste(HCoche hc);

// Prototipos de las manejadoras Callback registradas en la biblioteca
int llegada_primer_ajuste(HCoche hc);
int llegada_siguiente_ajuste(HCoche hc);
int llegada_mejor_ajuste(HCoche hc);
int llegada_peor_ajuste(HCoche hc);

int salida_primer_ajuste(HCoche hc);
int salida_siguiente_ajuste(HCoche hc);
int salida_mejor_ajuste(HCoche hc);
int salida_peor_ajuste(HCoche hc);

// Funciones secundarias de sincronización y commit
void aparcar_commit(HCoche hc);
void permiso_avance(HCoche hc);
void permiso_avance_commit(HCoche hc);
int ocupar_carril_desaparcar(int alg, int X2, int longitud);
void vaciar_pos_acera(int pos, int longitud, int alg);

// Funciones que ejecutarán los hilos creados de forma dinámica
DWORD WINAPI hilo_aparcar(LPVOID lpParam);
DWORD WINAPI hilo_desaparcar(LPVOID lpParam);


void cargar_dll() {
    hDLL = LoadLibrary("parking2.dll");
    if (hDLL == NULL) {
        fprintf(stderr, "ERROR[DLL]: No se pudo cargar parking2.dll (error %lu)\n", GetLastError());
        exit(1);
    }

    if (debug) fprintf(stderr, "[DLL] parking2.dll cargada correctamente\n");

    PARKING2_inicio = (int (*)(TIPO_FUNCION_LLEGADA*, TIPO_FUNCION_SALIDA*, long, int)) GetProcAddress(hDLL, "PARKING2_inicio");
    PARKING2_fin = (int (*)(void)) GetProcAddress(hDLL, "PARKING2_fin");
    PARKING2_aparcar = (int (*)(HCoche, void*, TIPO_FUNCION_APARCAR_COMMIT, TIPO_FUNCION_PERMISO_AVANCE, TIPO_FUNCION_PERMISO_AVANCE_COMMIT)) GetProcAddress(hDLL, "PARKING2_aparcar");
    PARKING2_desaparcar = (int (*)(HCoche, void*, TIPO_FUNCION_PERMISO_AVANCE, TIPO_FUNCION_PERMISO_AVANCE_COMMIT)) GetProcAddress(hDLL, "PARKING2_desaparcar");
    PARKING2_getNUmero = (int (*)(HCoche)) GetProcAddress(hDLL, "PARKING2_getNUmero");
    PARKING2_getLongitud = (int (*)(HCoche)) GetProcAddress(hDLL, "PARKING2_getLongitud");
    PARKING2_getPosiciOnEnAcera = (int (*)(HCoche)) GetProcAddress(hDLL, "PARKING2_getPosiciOnEnAcera");
    PARKING2_getTServ = (unsigned long (*)(HCoche)) GetProcAddress(hDLL, "PARKING2_getTServ");
    PARKING2_getColor = (int (*)(HCoche)) GetProcAddress(hDLL, "PARKING2_getColor");
    PARKING2_getDatos = (void* (*)(HCoche)) GetProcAddress(hDLL, "PARKING2_getDatos");
    PARKING2_getX = (int (*)(HCoche)) GetProcAddress(hDLL, "PARKING2_getX");
    PARKING2_getY = (int (*)(HCoche)) GetProcAddress(hDLL, "PARKING2_getY");
    PARKING2_getX2 = (int (*)(HCoche)) GetProcAddress(hDLL, "PARKING2_getX2");
    PARKING2_getY2 = (int (*)(HCoche)) GetProcAddress(hDLL, "PARKING2_getY2");
    PARKING2_getAlgoritmo = (int (*)(HCoche)) GetProcAddress(hDLL, "PARKING2_getAlgoritmo");
    PARKING2_isAceraOcupada = (int (*)(int, int)) GetProcAddress(hDLL, "PARKING2_isAceraOcupada");

    if (!PARKING2_inicio || !PARKING2_fin || !PARKING2_aparcar || !PARKING2_desaparcar ||
        !PARKING2_getNUmero || !PARKING2_getLongitud || !PARKING2_getPosiciOnEnAcera || !PARKING2_getTServ ||
        !PARKING2_getColor || !PARKING2_getDatos || !PARKING2_getX || !PARKING2_getY ||
        !PARKING2_getX2 || !PARKING2_getY2 || !PARKING2_getAlgoritmo || !PARKING2_isAceraOcupada) {
        fprintf(stderr, "ERROR[DLL]: No se encontro una funcion en la DLL (error %lu)\n", GetLastError());
        eliminar_dll();
        exit(1);
    }

    if (debug) fprintf(stderr, "[DLL] Todas las funciones fueron resueltas correctamente\n");
}

void eliminar_dll() {
    if (hDLL != NULL) {
        FreeLibrary(hDLL);
        hDLL = NULL;
        if (debug) fprintf(stderr, "[DLL] DLL eliminada correctamente\n");
    }
}

void validar_argumentos(int argc, char* argv[]) {
    if (argc < 2 || argc > 3) {
        fprintf(stderr, "Error: numero de argumentos incorrecto\n");
        ayudaPrograma(argv);
        exit(1);
    }

    retardo = atoi(argv[1]);
    if (retardo < 0) {
        fprintf(stderr, "Error: el retardo debe ser >= 0\n");
        exit(1);
    }

    debug = 0;
    if (argc == 3) {
        if (strcmp(argv[2], "D") == 0) {
            debug = 1;
        }
        else {
            fprintf(stderr, "Error: argumento desconocido '%s'\n", argv[2]);
            exit(1);
        }
    }
}

void ayudaPrograma(char* argv[]) {
    printf("=====AYUDA PROGRAMA [%s]=====\n", argv[0]);
    printf("Ejemplo uso:\n");
    printf("\t%s [numero de retardo] [debug]\n", argv[0]);
    printf("\t  - [numero de retardo]-> Tiene que ser mayor o igual a 0.\n");
    printf("\t  - [debug]-> Opcional, debe ser la letra D.\n");
}

BOOL WINAPI CtrlHandler(DWORD CtrlType) {
    BOOL res = TRUE;
    switch (CtrlType) {
    case CTRL_C_EVENT:
    case CTRL_CLOSE_EVENT:
        if (debug) fprintf(stderr, "Evento de finalizacion recibido.\n");
        if (hEventoFin) SetEvent(hEventoFin); // solo avisa, no limpia
        return TRUE;
    default:
        return FALSE;
    }
}

void inicializar_sincronizacion() {
    for (int i = 0; i < NUM_ALGORITMOS; i++) {
        hMutex[i] = CreateMutex(NULL, FALSE, NULL);
        if (hMutex[i] == NULL) {
            fprintf(stderr, "ERROR[SYNC]: No se pudo crear hMutex[%d] (error %lu)\n", i, GetLastError());
            exit(1);
        }

        hOrden[i] = CreateSemaphore(NULL, 1, 1, NULL);
        if (hOrden[i] == NULL) {
            fprintf(stderr, "ERROR[SYNC]: No se pudo crear hOrden[%d] (error %lu)\n", i, GetLastError());
            exit(1);
        }

        hAvance[i] = CreateSemaphore(NULL, 0, 999, NULL);
        if (hAvance[i] == NULL) {
            fprintf(stderr, "ERROR[SYNC]: No se pudo crear hAvance[%d] (error %lu)\n", i, GetLastError());
            exit(1);
        }

        proxAparcarNum[i] = 1;
        proxAparcar[i] = 0;
    }

    memset(acera, 0, sizeof(acera));
    memset(carril, 0, sizeof(carril));

    if (debug) fprintf(stderr, "[SYNC] Objetos de sincronizacion inicializados correctamente\n");
}

void liberar_sincronizacion() {
    for (int a = 0; a < NUM_ALGORITMOS; a++) {
        if (hMutex[a])  CloseHandle(hMutex[a]);
        if (hOrden[a])  CloseHandle(hOrden[a]);
        if (hAvance[a]) CloseHandle(hAvance[a]);
    }
    if (debug) fprintf(stderr, "[SYNC] Objetos de sincronizacion liberados correctamente\n");
}

// ==================== FUNCIONES DE HILOS DILIGENTES ====================

DWORD WINAPI hilo_aparcar(LPVOID lpParam) {
    HCoche hc = (HCoche)(INT_PTR)lpParam;
    int alg = PARKING2_getAlgoritmo(hc);

    // NOTA XIV: Control de orden secuencial estricto en el hilo hijo
    while (1) {
        WaitForSingleObject(hOrden[alg], INFINITE);

        WaitForSingleObject(hMutex[alg], INFINITE);
        int mi_turno = (PARKING2_getNUmero(hc) == proxAparcarNum[alg]);
        ReleaseMutex(hMutex[alg]);

        if (mi_turno) {
            // Es su turno: conserva el semáforo hOrden para bloquear a coches posteriores y entra
            break;
        }
        else {
            // No es su turno: libera temporalmente el semáforo y espera un poco
            ReleaseSemaphore(hOrden[alg], 1, NULL);
            Sleep(5);
        }
    }

    // Llama a la DLL para ejecutar la animación del aparcado físico
    PARKING2_aparcar(hc, NULL, aparcar_commit, permiso_avance, permiso_avance_commit);
    return 0;
}

DWORD WINAPI hilo_desaparcar(LPVOID lpParam) {
    HCoche hc = (HCoche)(INT_PTR)lpParam;
    // No requiere control de orden secuencial para iniciar la maniobra
    PARKING2_desaparcar(hc, NULL, permiso_avance, permiso_avance_commit);
    return 0;
}

// ==================== MANEJADORAS CALLBACKS (LLEGADA Y SALIDA) ====================

int llegada_primer_ajuste(HCoche hc) {
    int pos = -1;
    WaitForSingleObject(hMutex[PRIMER_AJUSTE], INFINITE);
    pos = primer_ajuste(hc);
    ReleaseMutex(hMutex[PRIMER_AJUSTE]);

    if (pos >= 0) {
        HANDLE hThread = CreateThread(NULL, 0, hilo_aparcar, (LPVOID)(INT_PTR)hc, 0, NULL);
        if (hThread) CloseHandle(hThread); // NOTA XIII: cerrar manejador al momento
    }
    return pos;
}

int llegada_siguiente_ajuste(HCoche hc) {
    int pos = -1;
    WaitForSingleObject(hMutex[SIGUIENTE_AJUSTE], INFINITE);
    pos = siguiente_ajuste(hc);
    ReleaseMutex(hMutex[SIGUIENTE_AJUSTE]);

    if (pos >= 0) {
        HANDLE hThread = CreateThread(NULL, 0, hilo_aparcar, (LPVOID)(INT_PTR)hc, 0, NULL);
        if (hThread) CloseHandle(hThread);
    }
    return pos;
}

int llegada_mejor_ajuste(HCoche hc) {
    int pos = -1;
    WaitForSingleObject(hMutex[MEJOR_AJUSTE], INFINITE);
    pos = mejor_ajuste(hc);
    ReleaseMutex(hMutex[MEJOR_AJUSTE]);

    if (pos >= 0) {
        HANDLE hThread = CreateThread(NULL, 0, hilo_aparcar, (LPVOID)(INT_PTR)hc, 0, NULL);
        if (hThread) CloseHandle(hThread);
    }
    return pos;
}

int llegada_peor_ajuste(HCoche hc) {
    int pos = -1;
    WaitForSingleObject(hMutex[PEOR_AJUSTE], INFINITE);
    pos = peor_ajuste(hc);
    ReleaseMutex(hMutex[PEOR_AJUSTE]);

    if (pos >= 0) {
        HANDLE hThread = CreateThread(NULL, 0, hilo_aparcar, (LPVOID)(INT_PTR)hc, 0, NULL);
        if (hThread) CloseHandle(hThread);
    }
    return pos;
}

int salida_primer_ajuste(HCoche hc) {
    HANDLE hThread = CreateThread(NULL, 0, hilo_desaparcar, (LPVOID)(INT_PTR)hc, 0, NULL);
    if (hThread) {
        CloseHandle(hThread);
        return 0;
    }
    return -1;
}

int salida_siguiente_ajuste(HCoche hc) {
    HANDLE hThread = CreateThread(NULL, 0, hilo_desaparcar, (LPVOID)(INT_PTR)hc, 0, NULL);
    if (hThread) {
        CloseHandle(hThread);
        return 0;
    }
    return -1;
}

int salida_mejor_ajuste(HCoche hc) {
    HANDLE hThread = CreateThread(NULL, 0, hilo_desaparcar, (LPVOID)(INT_PTR)hc, 0, NULL);
    if (hThread) {
        CloseHandle(hThread);
        return 0;
    }
    return -1;
}

int salida_peor_ajuste(HCoche hc) {
    HANDLE hThread = CreateThread(NULL, 0, hilo_desaparcar, (LPVOID)(INT_PTR)hc, 0, NULL);
    if (hThread) {
        CloseHandle(hThread);
        return 0;
    }
    return -1;
}

// ==================== ALGORITMOS DE ASIGNACIÓN ====================

int primer_ajuste(HCoche hc) {
    int longitud = PARKING2_getLongitud(hc);
    int huecoLibre = 0;
    int pos = -1;

    if (debug) fprintf(stderr, "[D-ALG:primer_ajuste] Coche %d - longitud=%d - buscando hueco\n", PARKING2_getNUmero(hc), longitud);

    for (int i = 0; i < TAM_PARKING; ) {
        if (acera[PRIMER_AJUSTE][i] == 0) {
            huecoLibre++;
            if (huecoLibre >= longitud) {
                pos = i - longitud + 1;
                break;
            }
            i++;
        }
        else {
			i += acera[PRIMER_AJUSTE][i]; // Optimización para saltar todos los espacios ocupados
            huecoLibre = 0;
        }
    }

    if (pos >= 0) {
        for (int i = pos; i < pos + longitud; i++) {
            acera[PRIMER_AJUSTE][i] = longitud;
        }
    }
    return pos;
}

int siguiente_ajuste(HCoche hc) {
    int longitud = PARKING2_getLongitud(hc);
    int huecoLibre = 0;
    int pos = -1;

    int inicio = proxAparcar[SIGUIENTE_AJUSTE];
    if (acera[SIGUIENTE_AJUSTE][inicio] == 0) {
        while (inicio > 0 && acera[SIGUIENTE_AJUSTE][inicio - 1] == 0) {
            inicio--;
        }
    }

    for (int i = inicio; i < TAM_PARKING; ) {
        if (acera[SIGUIENTE_AJUSTE][i] == 0) {
            huecoLibre++;
            if (huecoLibre >= longitud) {
                pos = i - longitud + 1;
                break;
            }
            i++;
        }
        else {
            i += acera[SIGUIENTE_AJUSTE][i];
            huecoLibre = 0;
        }
    }

    if (pos == -1) {
        huecoLibre = 0;
        for (int i = 0; i < inicio; ) {
            if (acera[SIGUIENTE_AJUSTE][i] == 0) {
                huecoLibre++;
                if (huecoLibre >= longitud) {
                    pos = i - longitud + 1;
                    break;
                }
                i++;
            }
            else {
                i += acera[SIGUIENTE_AJUSTE][i];
                huecoLibre = 0;
            }
        }
    }

    if (pos >= 0) {
        for (int i = pos; i < pos + longitud; i++) {
            acera[SIGUIENTE_AJUSTE][i] = longitud;
        }
        proxAparcar[SIGUIENTE_AJUSTE] = (pos + longitud) % TAM_PARKING;
    }
    return pos;
}

int mejor_ajuste(HCoche hc) {
    int longitud = PARKING2_getLongitud(hc);
    int pos = -1;
    int huecoActual = 0;
    int inicioHuecoActual = -1;
    int mejorTamano = TAM_PARKING + 1;

    for (int i = 0; i < TAM_PARKING; i++) {
        if (acera[MEJOR_AJUSTE][i] == 0) {
            if (huecoActual == 0) inicioHuecoActual = i;
            huecoActual++;
        }
        else {
            if (huecoActual >= longitud && huecoActual < mejorTamano) {
                mejorTamano = huecoActual;
                pos = inicioHuecoActual;
            }
            huecoActual = 0;
        }
    }
    if (huecoActual >= longitud && huecoActual < mejorTamano) {
        pos = inicioHuecoActual;
    }

    if (pos >= 0) {
        for (int i = pos; i < pos + longitud; i++) acera[MEJOR_AJUSTE][i] = longitud;
    }
    return pos;
}

int peor_ajuste(HCoche hc) {
    int longitud = PARKING2_getLongitud(hc);
    int pos = -1;
    int huecoActual = 0;
    int inicioHuecoActual = -1;
    int peorTamano = -1;

    for (int i = 0; i < TAM_PARKING; i++) {
        if (acera[PEOR_AJUSTE][i] == 0) {
            if (huecoActual == 0) inicioHuecoActual = i;
            huecoActual++;
        }
        else {
            if (huecoActual >= longitud && huecoActual > peorTamano) {
                peorTamano = huecoActual;
                pos = inicioHuecoActual;
            }
            huecoActual = 0;
        }
    }
    if (huecoActual >= longitud && huecoActual > peorTamano) {
        pos = inicioHuecoActual;
    }

    if (pos >= 0) {
        for (int i = pos; i < pos + longitud; i++) acera[PEOR_AJUSTE][i] = longitud;
    }
    return pos;
}

// ==================== CALLBACKS DE AVANCE Y COMMIT ====================

void aparcar_commit(HCoche hc) {
    int alg = PARKING2_getAlgoritmo(hc);

    WaitForSingleObject(hMutex[alg], INFINITE);
    proxAparcarNum[alg]++; // Incrementa el secuencial de turnos
    ReleaseMutex(hMutex[alg]);

    if (debug) fprintf(stderr, "[D-PKG:aparcar_commit] Coche %d aparcado. Siguiente esperado: %d\n", PARKING2_getNUmero(hc), proxAparcarNum[alg]);

    // Da paso al siguiente hilo retenido en cola secuencial
    ReleaseSemaphore(hOrden[alg], 1, NULL);
}

void permiso_avance(HCoche hc) {
    int X1 = PARKING2_getX(hc);
    int Y1 = PARKING2_getY(hc);
    int X2 = PARKING2_getX2(hc);
    int Y2 = PARKING2_getY2(hc);
    int alg = PARKING2_getAlgoritmo(hc);
    int longitud = PARKING2_getLongitud(hc);

    // Avance regular por el propio carril
    if (Y1 == 2 && Y2 == 2 && X2 >= 0 && X2 < TAM_PARKING) {
        while (1) {
            WaitForSingleObject(hMutex[alg], INFINITE);
            if (carril[alg][X2] == 0) {
                carril[alg][X2] = 1;
                ReleaseMutex(hMutex[alg]);
                break;
            }
            ReleaseMutex(hMutex[alg]);
            WaitForSingleObject(hAvance[alg], INFINITE); // Bloqueo si está la carretera ocupada
        }
    }
    // Salida desde la acera a la carretera (Desaparcar)
    if (Y1 < Y2 && Y2 == 2 && X2 >= 0 && X2 + longitud - 1 < TAM_PARKING) {
        while (1) {
            WaitForSingleObject(hMutex[alg], INFINITE);
            if (ocupar_carril_desaparcar(alg, X2, longitud)) {
                ReleaseMutex(hMutex[alg]);
                break;
            }
            ReleaseMutex(hMutex[alg]);
            WaitForSingleObject(hAvance[alg], INFINITE);
        }
    }
}

void permiso_avance_commit(HCoche hc) {
    int X_anterior = PARKING2_getX2(hc);
    int Y_anterior = PARKING2_getY2(hc);
    int Y_actual = PARKING2_getY(hc);
    int alg = PARKING2_getAlgoritmo(hc);
    int longitud = PARKING2_getLongitud(hc);

    // El coche se movió de la acera al carril (libera el hueco físico de la acera)
    if (Y_anterior == 1 && Y_actual == 2) {
        WaitForSingleObject(hMutex[alg], INFINITE);
        vaciar_pos_acera(PARKING2_getPosiciOnEnAcera(hc), longitud, alg);
        ReleaseMutex(hMutex[alg]);
    }

    // El coche se introduce en la acera para estacionar (libera el carril que usaba)
    if (Y_anterior == 2 && Y_actual == 1 && X_anterior >= 0 && X_anterior + longitud - 1 < TAM_PARKING) {
        WaitForSingleObject(hMutex[alg], INFINITE);
        for (int i = X_anterior; i < X_anterior + longitud; i++) {
            carril[alg][i] = 0;
        }
        ReleaseMutex(hMutex[alg]);
        ReleaseSemaphore(hAvance[alg], 1, NULL); // Avisa a los hilos en espera en el carril
    }

    // El coche avanzó una celda hacia adelante dentro de la calzada
    if (Y_anterior == 2 && Y_actual == 2 && X_anterior < TAM_PARKING) {
        int final_coche = X_anterior + longitud - 1;
        if (final_coche >= 0 && final_coche < TAM_PARKING) {
            WaitForSingleObject(hMutex[alg], INFINITE);
            carril[alg][final_coche] = 0; // vacía la cola trasera
            ReleaseMutex(hMutex[alg]);
            ReleaseSemaphore(hAvance[alg], 1, NULL);
        }
    }
}

int ocupar_carril_desaparcar(int alg, int X2, int longitud) {
    for (int i = X2; i < X2 + longitud; i++) {
        if (carril[alg][i] != 0) return 0;
    }
    for (int i = X2; i < X2 + longitud; i++) {
        carril[alg][i] = 1;
    }
    return 1;
}

void vaciar_pos_acera(int pos, int longitud, int alg) {
    if (pos >= 0) {
        for (int i = pos; i < pos + longitud; i++) {
            acera[alg][i] = 0;
        }
    }
}

// ==================== INICIALIZACIÓN Y PROGRAMA PRINCIPAL ====================

void parking() {
    TIPO_FUNCION_LLEGADA funcionesLlegada[4] = {
        llegada_primer_ajuste,
        llegada_siguiente_ajuste,
        llegada_mejor_ajuste,
        llegada_peor_ajuste
    };
    TIPO_FUNCION_SALIDA funcionesSalida[4] = {
        salida_primer_ajuste,
        salida_siguiente_ajuste,
        salida_mejor_ajuste,
        salida_peor_ajuste
    };

    // crea el evento de fin antes de iniciar la simulacion
    hEventoFin = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (hEventoFin == NULL) {
        fprintf(stderr, "ERROR: No se pudo crear hEventoFin\n");
        return;
    }

    inicializar_sincronizacion();

    // invoca la ejecución concurrente de la biblioteca. Bloqueará hasta finalizar el escenario
    int resultado = PARKING2_inicio(funcionesLlegada, funcionesSalida, retardo, debug);

    if (debug) fprintf(stderr, "[SIMULACION] PARKING2_inicio retorno con codigo: %d\n", resultado);

    if (resultado == -1) {
        fprintf(stderr, "ERROR: PARKING2_inicio fallo\n");
        liberar_sincronizacion();
        return;
    }
    
    // duerme 30s o si no hasta que se reciba la señal ctrl c (hEventoFin)
    DWORD motivo = WaitForSingleObject(hEventoFin, 30000);

    if (motivo == WAIT_TIMEOUT) {
        PARKING2_fin();        // 30s: ordenado, espera a la DLL
        liberar_sincronizacion();
    }
    else {
        liberar_sincronizacion(); // Ctrl+C: inmediato, sin esperar a la DLL
    }

    CloseHandle(hEventoFin);
    hEventoFin = NULL;
}


int main(int argc, char* argv[]) {
    validar_argumentos(argc, argv);
    if (debug) {
        fprintf(stderr, "[DBG-Variable-Debug] Estado variable debug = %d\n", debug);
        fprintf(stderr, "NUM_VELOCIDAD: %d\n", retardo);
    }

    // Instalación del capturador de interrupción Control+C nativo de consola Windows
    SetConsoleCtrlHandler((PHANDLER_ROUTINE)CtrlHandler, TRUE);

    cargar_dll();
    parking();
    eliminar_dll();

    return 0;
}
