/*
* Sistemas Operativos II Práctica Windows - Aparcando II
* Primera Convocatoria - Curso 2025-2026
* Grupo: G08 - Brais Bértolo Senra, Juan Riego Vila
*/

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "parking2.h"

// constantes
#define NUM_ALGORITMOS 4
#define TAM_PARKING 80

// variables globales
int retardo, debug;
volatile int terminar = 0;

// estructura para pasar datos a los hilos
typedef struct {
    HCoche hc;
    int algoritmo;
} DATOS_HILO;

// Estado de la acera y carril para cada algoritmo
int acera[NUM_ALGORITMOS][TAM_PARKING];
int carril[NUM_ALGORITMOS][TAM_PARKING];
int proxAparcar[NUM_ALGORITMOS];  // para siguiente_ajuste

// array para rastrear qué número de coche debe aparcar primero en cada algoritmo
int proxAparcarNum[NUM_ALGORITMOS];

// variables globales para sincronización
HANDLE hMutex[NUM_ALGORITMOS];      // mutex para cada algoritmo
HANDLE hOrden[NUM_ALGORITMOS];      // semáforo para el orden de aparcamiento
HANDLE hAvance[NUM_ALGORITMOS];     // semáforo para controlar avance de coches

// manejador de la DLL
HMODULE hDLL = NULL;

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

// ============================================
// DECLARACIÓN DE PROTOTIPOS
// ============================================
DWORD WINAPI hilo_aparcar(LPVOID param);
DWORD WINAPI hilo_desaparcar(LPVOID param);
void cargar_dll();
void eliminar_dll();
void inicializar_sincronizacion();
void liberar_sincronizacion();
void validar_argumentos(int argc, char* argv[]);
BOOL WINAPI CtrlHandler(DWORD CtrlType);
DWORD WINAPI hilo_principal_simulacion(LPVOID param);


// ============================================
// FUNCIONES DE SINCRONIZACIÓN (callbacks)
// ============================================

void aparcar_commit(HCoche hc) {
    if (debug) fprintf(stderr, "[D-CALLBACK:aparcar_commit] Coche %d aparcado\n", PARKING2_getNUmero(hc));
    
    int alg = PARKING2_getAlgoritmo(hc);
    // Permitir que aparque el siguiente coche
    ReleaseSemaphore(hOrden[alg], 1, NULL);
}

void permiso_avance(HCoche hc) {
    if (debug) fprintf(stderr, "[D-CALLBACK:permiso_avance] Coche %d pidiendo permiso para avanzar\n", PARKING2_getNUmero(hc));
    
    int alg = PARKING2_getAlgoritmo(hc);
    int X1 = PARKING2_getX(hc);
    int Y1 = PARKING2_getY(hc);
    int X2 = PARKING2_getX2(hc);
    int Y2 = PARKING2_getY2(hc);
    
    if (debug) fprintf(stderr, "[D-CALLBACK:permiso_avance] Coche %d intenta ir de (%d,%d) a (%d,%d)\n", 
                       PARKING2_getNUmero(hc), X1, Y1, X2, Y2);
    
    // Avance por el mismo carril (Y permanece igual)
    if (Y1 == 2 && Y2 == 2 && X2 >= 0 && X2 < TAM_PARKING) {
        while (1) {
            WaitForSingleObject(hMutex[alg], INFINITE);
            
            if (carril[alg][X2] == 0) {
                carril[alg][X2] = 1;
                ReleaseMutex(hMutex[alg]);
                break;
            }
            
            ReleaseMutex(hMutex[alg]);
            WaitForSingleObject(hAvance[alg], INFINITE);
        }
    }
    // Avance desde la acera a la carretera
    else if (Y1 < Y2 && Y2 == 2 && X2 >= 0 && X2 + PARKING2_getLongitud(hc) - 1 < TAM_PARKING) {
        while (1) {
            WaitForSingleObject(hMutex[alg], INFINITE);
            
            int puede_ocupar = 1;
            for (int i = X2; i < X2 + PARKING2_getLongitud(hc); i++) {
                if (carril[alg][i] != 0) {
                    puede_ocupar = 0;
                    break;
                }
            }
            
            if (puede_ocupar) {
                for (int i = X2; i < X2 + PARKING2_getLongitud(hc); i++) {
                    carril[alg][i] = 1;
                }
                ReleaseMutex(hMutex[alg]);
                break;
            }
            
            ReleaseMutex(hMutex[alg]);
            WaitForSingleObject(hAvance[alg], INFINITE);
        }
    }
}

void permiso_avance_commit(HCoche hc) {
    int alg = PARKING2_getAlgoritmo(hc);
    int X_anterior = PARKING2_getX2(hc);
    int Y_anterior = PARKING2_getY2(hc);
    int Y_actual = PARKING2_getY(hc);
    int longitud = PARKING2_getLongitud(hc);
    
    if (debug) fprintf(stderr, "[D-CALLBACK:permiso_avance_commit] Coche %d avanzó a (%d,%d), venía de (%d,%d)\n", 
                       PARKING2_getNUmero(hc), PARKING2_getX(hc), Y_actual, X_anterior, Y_anterior);
    
    // Libera si el coche desaparca del carril (de acera a carretera)
    if (Y_anterior == 1 && Y_actual == 2) {
        int pos = PARKING2_getPosiciOnEnAcera(hc);
        if (pos >= 0) {
            for (int i = pos; i < pos + longitud; i++) {
                acera[alg][i] = 0;
            }
        }
    }
    
    // Libera si el coche sale del carril para aparcar (de carretera a acera)
    if (Y_anterior == 2 && Y_actual == 1 && X_anterior >= 0 && X_anterior + longitud - 1 < TAM_PARKING) {
        WaitForSingleObject(hMutex[alg], INFINITE);
        for (int i = X_anterior; i < X_anterior + longitud; i++) {
            carril[alg][i] = 0;
        }
        ReleaseMutex(hMutex[alg]);
        
        ReleaseSemaphore(hAvance[alg], 1, NULL);
    }
    
    // Libera si el coche se mueve en horizontal en el mismo carril
    if (Y_anterior == 2 && Y_actual == 2 && X_anterior < TAM_PARKING) {
        int final_coche = X_anterior + longitud - 1;
        
        if (final_coche >= 0 && final_coche < TAM_PARKING) {
            WaitForSingleObject(hMutex[alg], INFINITE);
            carril[alg][final_coche] = 0;  // libera donde estaba
            ReleaseMutex(hMutex[alg]);
            
            ReleaseSemaphore(hAvance[alg], 1, NULL);
        }
    }
}

// ============================================
// ALGORITMOS DE BÚSQUEDA DE HUECO
// ============================================

int primer_ajuste(HCoche hc) {
    int longitud = PARKING2_getLongitud(hc);
    int huecoLibre = 0;
    int pos = -1;
    
    if (debug) fprintf(stderr, "[D-ALG:primer_ajuste] Coche %d - longitud=%d - buscando hueco\n", 
                       PARKING2_getNUmero(hc), longitud);
    
    for (int i = 0; i < TAM_PARKING; ) {
        if (acera[PRIMER_AJUSTE][i] == 0) {
            huecoLibre++;
            if (huecoLibre >= longitud) {
                pos = i - longitud + 1;
                break;
            }
            i++;
        } else {
            i += acera[PRIMER_AJUSTE][i];  // Optimización: saltar el coche
            huecoLibre = 0;
        }
    }
    
    if (pos >= 0) {
        for (int i = pos; i < pos + longitud; i++) {
            acera[PRIMER_AJUSTE][i] = longitud;
        }
    }
    
    if (debug) {
        (pos >= 0) ? fprintf(stderr, "[D-ALG:primer_ajuste] Coche %d -> hueco encontrado en pos=%d\n", 
                            PARKING2_getNUmero(hc), pos)
                   : fprintf(stderr, "[D-ALG:primer_ajuste] Coche %d -> sin hueco (pos=-1)\n", 
                            PARKING2_getNUmero(hc));
    }
    
    return pos;
}

int siguiente_ajuste(HCoche hc) {
    int longitud = PARKING2_getLongitud(hc);
    int huecoLibre = 0;
    int pos = -1;
    
    if (debug) fprintf(stderr, "[D-ALG:siguiente_ajuste] Coche %d - longitud=%d - buscando hueco\n", 
                       PARKING2_getNUmero(hc), longitud);
    
    int inicio = proxAparcar[SIGUIENTE_AJUSTE];
    
    // Retroceder al inicio del hueco si la posición está libre
    if (acera[SIGUIENTE_AJUSTE][inicio] == 0) {
        while (inicio > 0 && acera[SIGUIENTE_AJUSTE][inicio - 1] == 0) {
            inicio--;
        }
    }
    
    // Primera pasada: desde 'inicio' hasta el final
    for (int i = inicio; i < TAM_PARKING; ) {
        if (acera[SIGUIENTE_AJUSTE][i] == 0) {
            huecoLibre++;
            if (huecoLibre >= longitud) {
                pos = i - longitud + 1;
                break;
            }
            i++;
        } else {
            i += acera[SIGUIENTE_AJUSTE][i];
            huecoLibre = 0;
        }
    }
    
    // Segunda pasada: desde el principio hasta 'inicio'
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
            } else {
                i += acera[SIGUIENTE_AJUSTE][i];
                huecoLibre = 0;
            }
        }
    }
    
    // Guardar datos
    if (pos >= 0) {
        for (int i = pos; i < pos + longitud; i++) {
            acera[SIGUIENTE_AJUSTE][i] = longitud;
        }
        proxAparcar[SIGUIENTE_AJUSTE] = (pos + longitud) % TAM_PARKING;
    }
    
    if (debug) {
        (pos >= 0) ? fprintf(stderr, "[D-ALG:siguiente_ajuste] Coche %d -> hueco encontrado en pos=%d\n", 
                            PARKING2_getNUmero(hc), pos)
                   : fprintf(stderr, "[D-ALG:siguiente_ajuste] Coche %d -> sin hueco (pos=-1)\n", 
                            PARKING2_getNUmero(hc));
    }
    
    return pos;
}

int mejor_ajuste(HCoche hc) {
    int longitud = PARKING2_getLongitud(hc);
    int pos = -1;
    int huecoActual = 0;
    int inicioHuecoActual = -1;
    int mejorTamano = TAM_PARKING + 1;
    
    if (debug) fprintf(stderr, "[D-ALG:mejor_ajuste] Coche %d - longitud=%d\n", 
                       PARKING2_getNUmero(hc), longitud);
    
    for (int i = 0; i < TAM_PARKING; i++) {
        if (acera[MEJOR_AJUSTE][i] == 0) {
            if (huecoActual == 0)
                inicioHuecoActual = i;
            huecoActual++;
        } else {
            // Cuando se choca con un coche, evaluar el hueco que deja atrás
            if (huecoActual >= longitud) {
                if (huecoActual < mejorTamano) {
                    mejorTamano = huecoActual;
                    pos = inicioHuecoActual;
                }
            }
            huecoActual = 0;
        }
    }
    
    // Evaluar el último hueco si el array termina en 0
    if (huecoActual >= longitud) {
        if (huecoActual < mejorTamano) {
            mejorTamano = huecoActual;
            pos = inicioHuecoActual;
        }
    }
    
    // Guardar datos
    if (pos >= 0) {
        for (int i = pos; i < pos + longitud; i++)
            acera[MEJOR_AJUSTE][i] = longitud;
    }
    
    if (debug) {
        (pos >= 0) ? fprintf(stderr, "[D-ALG:mejor_ajuste] Coche %d -> hueco encontrado en pos=%d\n", 
                            PARKING2_getNUmero(hc), pos)
                   : fprintf(stderr, "[D-ALG:mejor_ajuste] Coche %d -> sin hueco (pos=-1)\n", 
                            PARKING2_getNUmero(hc));
    }
    
    return pos;
}

int peor_ajuste(HCoche hc) {
    int longitud = PARKING2_getLongitud(hc);
    int pos = -1;
    int huecoActual = 0;
    int inicioHuecoActual = -1;
    int peorTamano = -1;
    
    if (debug) fprintf(stderr, "[D-ALG:peor_ajuste] Coche %d - longitud=%d\n", 
                       PARKING2_getNUmero(hc), longitud);
    
    for (int i = 0; i < TAM_PARKING; i++) {
        if (acera[PEOR_AJUSTE][i] == 0) {
            if (huecoActual == 0)
                inicioHuecoActual = i;
            huecoActual++;
        } else {
            if (huecoActual >= longitud) {
                if (huecoActual > peorTamano) {
                    peorTamano = huecoActual;
                    pos = inicioHuecoActual;
                }
            }
            huecoActual = 0;
        }
    }
    
    if (huecoActual >= longitud) {
        if (huecoActual > peorTamano) {
            peorTamano = huecoActual;
            pos = inicioHuecoActual;
        }
    }
    
    if (pos >= 0) {
        for (int i = pos; i < pos + longitud; i++)
            acera[PEOR_AJUSTE][i] = longitud;
    }
    
    if (debug) {
        (pos >= 0) ? fprintf(stderr, "[D-ALG:peor_ajuste] Coche %d -> hueco encontrado en pos=%d\n", 
                            PARKING2_getNUmero(hc), pos)
                   : fprintf(stderr, "[D-ALG:peor_ajuste] Coche %d -> sin hueco (pos=-1)\n", 
                            PARKING2_getNUmero(hc));
    }
    
    return pos;
}

// ============================================
// FUNCIONES DE LLEGADA
// ============================================

int funcion_llegada_primer_ajuste(HCoche hc) {
    if (debug) fprintf(stderr, "[D-LLEGADA:PA] Coche %d llegó\n", PARKING2_getNUmero(hc));
    
    int pos = primer_ajuste(hc);
    
    if (pos >= 0) {
        if (debug) fprintf(stderr, "[D-LLEGADA:PA] Coche %d puede aparcar en %d\n", PARKING2_getNUmero(hc), pos);
        
        // Crear hilo para aparcar el coche
        DATOS_HILO *datos = (DATOS_HILO*)malloc(sizeof(DATOS_HILO));
        datos->hc = hc;
        datos->algoritmo = PRIMER_AJUSTE;
        
        HANDLE hThread = CreateThread(NULL, 0, hilo_aparcar, (LPVOID)datos, 0, NULL);
        if (hThread != NULL) {
            CloseHandle(hThread);
        }
        
        return pos;
    } else {
        if (debug) fprintf(stderr, "[D-LLEGADA:PA] Coche %d encolado\n", PARKING2_getNUmero(hc));
        return -1;
    }
}

int funcion_llegada_siguiente_ajuste(HCoche hc) {
    if (debug) fprintf(stderr, "[D-LLEGADA:SA] Coche %d llegó\n", PARKING2_getNUmero(hc));
    
    int pos = siguiente_ajuste(hc);
    
    if (pos >= 0) {
        if (debug) fprintf(stderr, "[D-LLEGADA:SA] Coche %d puede aparcar en %d\n", PARKING2_getNUmero(hc), pos);
        
        DATOS_HILO *datos = (DATOS_HILO*)malloc(sizeof(DATOS_HILO));
        datos->hc = hc;
        datos->algoritmo = SIGUIENTE_AJUSTE;
        
        HANDLE hThread = CreateThread(NULL, 0, hilo_aparcar, (LPVOID)datos, 0, NULL);
        if (hThread != NULL) {
            CloseHandle(hThread);
        }
        
        return pos;
    } else {
        if (debug) fprintf(stderr, "[D-LLEGADA:SA] Coche %d encolado\n", PARKING2_getNUmero(hc));
        return -1;
    }
}

int funcion_llegada_mejor_ajuste(HCoche hc) {
    if (debug) fprintf(stderr, "[D-LLEGADA:MA] Coche %d llegó\n", PARKING2_getNUmero(hc));
    
    int pos = mejor_ajuste(hc);
    
    if (pos >= 0) {
        if (debug) fprintf(stderr, "[D-LLEGADA:MA] Coche %d puede aparcar en %d\n", PARKING2_getNUmero(hc), pos);
        
        DATOS_HILO *datos = (DATOS_HILO*)malloc(sizeof(DATOS_HILO));
        datos->hc = hc;
        datos->algoritmo = MEJOR_AJUSTE;
        
        HANDLE hThread = CreateThread(NULL, 0, hilo_aparcar, (LPVOID)datos, 0, NULL);
        if (hThread != NULL) {
            CloseHandle(hThread);
        }
        
        return pos;
    } else {
        if (debug) fprintf(stderr, "[D-LLEGADA:MA] Coche %d encolado\n", PARKING2_getNUmero(hc));
        return -1;
    }
}

int funcion_llegada_peor_ajuste(HCoche hc) {
    if (debug) fprintf(stderr, "[D-LLEGADA:PA] Coche %d llegó\n", PARKING2_getNUmero(hc));
    
    int pos = peor_ajuste(hc);
    
    if (pos >= 0) {
        if (debug) fprintf(stderr, "[D-LLEGADA:PA] Coche %d puede aparcar en %d\n", PARKING2_getNUmero(hc), pos);
        
        DATOS_HILO *datos = (DATOS_HILO*)malloc(sizeof(DATOS_HILO));
        datos->hc = hc;
        datos->algoritmo = PEOR_AJUSTE;
        
        HANDLE hThread = CreateThread(NULL, 0, hilo_aparcar, (LPVOID)datos, 0, NULL);
        if (hThread != NULL) {
            CloseHandle(hThread);
        }
        
        return pos;
    } else {
        if (debug) fprintf(stderr, "[D-LLEGADA:PA] Coche %d encolado\n", PARKING2_getNUmero(hc));
        return -1;
    }
}

// ============================================
// FUNCIONES DE SALIDA
// ============================================

int funcion_salida_primer_ajuste(HCoche hc) {
    if (debug) fprintf(stderr, "[D-SALIDA:PA] Coche %d debe salir\n", PARKING2_getNUmero(hc));
    
    DATOS_HILO *datos = (DATOS_HILO*)malloc(sizeof(DATOS_HILO));
    datos->hc = hc;
    datos->algoritmo = PRIMER_AJUSTE;
    
    HANDLE hThread = CreateThread(NULL, 0, hilo_desaparcar, (LPVOID)datos, 0, NULL);
    if (hThread != NULL) {
        CloseHandle(hThread);
    }
    
    return 0;
}

int funcion_salida_siguiente_ajuste(HCoche hc) {
    if (debug) fprintf(stderr, "[D-SALIDA:SA] Coche %d debe salir\n", PARKING2_getNUmero(hc));
    
    DATOS_HILO *datos = (DATOS_HILO*)malloc(sizeof(DATOS_HILO));
    datos->hc = hc;
    datos->algoritmo = SIGUIENTE_AJUSTE;
    
    HANDLE hThread = CreateThread(NULL, 0, hilo_desaparcar, (LPVOID)datos, 0, NULL);
    if (hThread != NULL) {
        CloseHandle(hThread);
    }
    
    return 0;
}

int funcion_salida_mejor_ajuste(HCoche hc) {
    if (debug) fprintf(stderr, "[D-SALIDA:MA] Coche %d debe salir\n", PARKING2_getNUmero(hc));
    
    DATOS_HILO *datos = (DATOS_HILO*)malloc(sizeof(DATOS_HILO));
    datos->hc = hc;
    datos->algoritmo = MEJOR_AJUSTE;
    
    HANDLE hThread = CreateThread(NULL, 0, hilo_desaparcar, (LPVOID)datos, 0, NULL);
    if (hThread != NULL) {
        CloseHandle(hThread);
    }
    
    return 0;
}

int funcion_salida_peor_ajuste(HCoche hc) {
    if (debug) fprintf(stderr, "[D-SALIDA:PA] Coche %d debe salir\n", PARKING2_getNUmero(hc));
    
    DATOS_HILO *datos = (DATOS_HILO*)malloc(sizeof(DATOS_HILO));
    datos->hc = hc;
    datos->algoritmo = PEOR_AJUSTE;
    
    HANDLE hThread = CreateThread(NULL, 0, hilo_desaparcar, (LPVOID)datos, 0, NULL);
    if (hThread != NULL) {
        CloseHandle(hThread);
    }
    
    return 0;
}

// ============================================
// FUNCIONES PARA HILOS
// ============================================

DWORD WINAPI hilo_aparcar(LPVOID param) {
    DATOS_HILO *datos = (DATOS_HILO*)param;
    HCoche hc = datos->hc;
    int alg = datos->algoritmo;
    
    if (debug) fprintf(stderr, "[D-HILO-APARCAR] Coche %d comenzando a aparcar (Hilo: %lu)\n", 
                       PARKING2_getNUmero(hc), GetCurrentThreadId());
    
    // Esperar a que sea el turno de este coche
    WaitForSingleObject(hOrden[alg], INFINITE);
    
    // Comprobar que le toca a este coche
    int numCoche = PARKING2_getNUmero(hc);
    if (numCoche != proxAparcarNum[alg]) {
        if (debug) fprintf(stderr, "[D-HILO-APARCAR] ERROR: Coche %d fuera de orden (esperaba %d)\n", 
                           numCoche, proxAparcarNum[alg]);
        ReleaseSemaphore(hOrden[alg], 1, NULL);
        free(datos);
        return 1;
    }
    
    proxAparcarNum[alg]++;
    
    if (debug) fprintf(stderr, "[D-HILO-APARCAR] Coche %d aparcando en orden correcto\n", numCoche);
    
    // Llamar a la función de aparcamiento de la DLL
    PARKING2_aparcar(hc, &alg, aparcar_commit, permiso_avance, permiso_avance_commit);
    
    if (debug) fprintf(stderr, "[D-HILO-APARCAR] Coche %d aparcado correctamente\n", PARKING2_getNUmero(hc));
    
    free(datos);
    return 0;
}

DWORD WINAPI hilo_desaparcar(LPVOID param) {
    DATOS_HILO *datos = (DATOS_HILO*)param;
    HCoche hc = datos->hc;
    int alg = datos->algoritmo;
    
    if (debug) fprintf(stderr, "[D-HILO-DESAPARCAR] Coche %d comenzando a desaparcar (Hilo: %lu)\n", 
                       PARKING2_getNUmero(hc), GetCurrentThreadId());
    
    // Llamar a la función de desaparcamiento de la DLL
    PARKING2_desaparcar(hc, &alg, permiso_avance, permiso_avance_commit);
    
    if (debug) fprintf(stderr, "[D-HILO-DESAPARCAR] Coche %d desaparcado correctamente\n", PARKING2_getNUmero(hc));
    
    free(datos);
    return 0;
}

// ============================================
// CARGAR/DESCARGAR DLL
// ============================================

void cargar_dll() {
    hDLL = LoadLibrary("parking2.dll");
    if (hDLL == NULL) {
        fprintf(stderr, "ERROR[DLL]: No se pudo cargar parking2.dll (error %lu)\n", GetLastError());
        exit(1);
    }
    
    if (debug)
        fprintf(stderr, "[DLL] parking2.dll cargada correctamente\n");
    
    // Resolver funciones
    PARKING2_inicio = (int (*)(TIPO_FUNCION_LLEGADA*, TIPO_FUNCION_SALIDA*, long, int))
        GetProcAddress(hDLL, "PARKING2_inicio");
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
    
    if (!PARKING2_inicio || !PARKING2_fin || !PARKING2_aparcar || !PARKING2_desaparcar ||
        !PARKING2_getNUmero || !PARKING2_getLongitud || !PARKING2_getPosiciOnEnAcera || 
        !PARKING2_getTServ || !PARKING2_getColor || !PARKING2_getDatos ||
        !PARKING2_getX || !PARKING2_getY || !PARKING2_getX2 || !PARKING2_getY2 ||
        !PARKING2_getAlgoritmo || !PARKING2_isAceraOcupada) {
        fprintf(stderr, "ERROR[DLL]: No se encontraron funciones en la DLL (error %lu)\n", GetLastError());
        eliminar_dll();
        exit(1);
    }
    
    if (debug)
        fprintf(stderr, "[DLL] Todas las funciones fueron resueltas correctamente\n");
}

void eliminar_dll() {
    if (hDLL != NULL) {
        FreeLibrary(hDLL);
        hDLL = NULL;
        if (debug) {
            fprintf(stderr, "[DLL] DLL eliminada correctamente\n");
        }
    }
}

// ============================================
// SINCRONIZACIÓN
// ============================================

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
    
    // Inicializar arrays de acera y carril
    memset(acera, 0, sizeof(acera));
    memset(carril, 0, sizeof(carril));
    
    if (debug)
        fprintf(stderr, "[SYNC] Objetos de sincronización inicializados correctamente\n");
}

void liberar_sincronizacion() {
    for (int i = 0; i < NUM_ALGORITMOS; i++) {
        if (hMutex[i]) CloseHandle(hMutex[i]);
        if (hOrden[i]) CloseHandle(hOrden[i]);
        if (hAvance[i]) CloseHandle(hAvance[i]);
    }
    
    if (debug)
        fprintf(stderr, "[SYNC] Objetos de sincronización liberados correctamente\n");
}

// ============================================
// CONTROL DE CTRL+C
// ============================================

BOOL WINAPI CtrlHandler(DWORD CtrlType) {
    BOOL res = TRUE;
    switch (CtrlType) {
        case CTRL_C_EVENT:
            printf("Ctrl-C event\n\n");
            Beep(750, 300);
            terminar = 1;
            PARKING2_fin();
            eliminar_dll();
            res = TRUE;
            break;
        case CTRL_CLOSE_EVENT:
            printf("Ctrl-Close event\n\n");
            Beep(600, 200);
            terminar = 1;
            PARKING2_fin();
            eliminar_dll();
            res = TRUE;
            break;
        default:
            res = FALSE;
            break;
    }
    return res;
}

// ============================================
// VALIDACIÓN DE ARGUMENTOS
// ============================================

void validar_argumentos(int argc, char* argv[]) {
    if (argc < 2 || argc > 3) {
        fprintf(stderr, "Error: numero de argumentos incorrecto\n");
        printf("=====AYUDA PROGRAMA [%s]=====\n", argv[0]);
        printf("Ejemplo uso:\n");
        printf("\t%s [numero de retardo] [debug]\n", argv[0]);
        printf("\t  - [numero de retardo]-> Tiene que ser mayor o igual a 0, no hay limites con la velocidad, cuanto mas bajo sea el numero mas lento se ejecutara.\n");
        printf("\t  - [debug]-> Se puede obtener por la salida de errores los mensajes de depuracion del programa durante la ejecucion, este argumento es opcional y debe ser la letra D.\n");
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
        } else {
            fprintf(stderr, "Error: argumento desconocido '%s'\n", argv[2]);
            exit(1);
        }
    }
}

// ============================================
// HILO PRINCIPAL - DORMIR 30 SEGUNDOS
// ============================================

DWORD WINAPI hilo_principal_simulacion(LPVOID param) {
    Sleep(30000); // 30 segundos
    
    if (debug)
        fprintf(stderr, "[MAIN] 30 segundos terminados, finalizando simulación\n");
    
    terminar = 1;
    PARKING2_fin();
    
    return 0;
}

// ============================================
// MAIN
// ============================================

int main(int argc, char* argv[]) {
    validar_argumentos(argc, argv);
    
    if (debug) {
        fprintf(stderr, "[DBG] NUM_VELOCIDAD: %d\n", retardo);
    }
    
    // Registrar controlador de Ctrl+C
    BOOL added = SetConsoleCtrlHandler((PHANDLER_ROUTINE)CtrlHandler, TRUE);
    if (added) {
        if (debug) {
            fprintf(stderr, "\n[Inicio de la salida manejador Control+C]\n");
            fprintf(stderr, "El manejador de Control+C esta instalado.\n");
            fprintf(stderr, "\n -- Ahora prueba a pulsar Ctrl+C o Ctrl+Break, o");
            fprintf(stderr, "\n    intenta cerrar la consola...\n");
            fprintf(stderr, "\n(...esperando eventos...)\n");
            fprintf(stderr, "[Fin salida manejador Control+C]\n");
        }
    } else {
        fprintf(stderr, "\nERROR[CtrlC]: No se pudo instalar el manejador de Control+C\n");
    }
    
    // Cargar DLL
    cargar_dll();
    
    // Inicializar sincronización
    inicializar_sincronizacion();
    
    // Preparar callbacks
    TIPO_FUNCION_LLEGADA funciones_llegada[NUM_ALGORITMOS] = {
        funcion_llegada_primer_ajuste,
        funcion_llegada_siguiente_ajuste,
        funcion_llegada_mejor_ajuste,
        funcion_llegada_peor_ajuste
    };
    
    TIPO_FUNCION_SALIDA funciones_salida[NUM_ALGORITMOS] = {
        funcion_salida_primer_ajuste,
        funcion_salida_siguiente_ajuste,
        funcion_salida_mejor_ajuste,
        funcion_salida_peor_ajuste
    };
    
    // Iniciar simulación
    int resultado = PARKING2_inicio(funciones_llegada, funciones_salida, retardo, debug);
    
    if (resultado == -1) {
        fprintf(stderr, "ERROR: PARKING2_inicio falló\n");
        eliminar_dll();
        liberar_sincronizacion();
        exit(1);
    }
    
    if (debug)
        fprintf(stderr, "[D-IPC] PARKING2_inicio retornó: %d\n", resultado);
    
    // Crear hilo que dormirá 30 segundos
    HANDLE hThread = CreateThread(NULL, 0, hilo_principal_simulacion, NULL, 0, NULL);
    if (hThread) {
        WaitForSingleObject(hThread, INFINITE);
        CloseHandle(hThread);
    }
    
    // Cleanup
    liberar_sincronizacion();
    eliminar_dll();
    
    if (debug)
        fprintf(stderr, "[D-PADRE] PID=%lu muriendo\n", GetCurrentProcessId());
    
    return 0;
}

