#include <windows.h>
#include <stdio.h>
#include "parking2.h"


int main(){
        TIPO_FUNCION_LLEGADA funcionLlegada[1];
        TIPO_FUNCION_SALIDA funcionSalida[1];
        long intervalo = 0.0;
        bool d = 0;

        int resultado = PARKING2_inicio(funcionLlegada,funcionSalida, intervalo, d);
        Sleep(1000);
        return 0;
}//fin funcion main
