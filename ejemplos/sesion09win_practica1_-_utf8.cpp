
//---------------------------------------------------------------------------
// Sesion09 - Windows - Práctica 1
//---------------------------------------------------------------------------
/*
Hay que crear un programa que admita un único argumento de línea de órdenes. 
Su nombre será hijos. El argumento es un número entero>100. El programa 
comprobará que los argumentos pasados son correctos y, si no es así, emitirá 
el correspondiente aviso por el canal de error estándar. El proceso creará 
dos hilos de ejecución que ejecutarán una misma función fnHilo. La función 
fnHilo desreferenciará el puntero que se le ha pasado, para conseguir un 
carácter. A continuación, repetirá un bucle tantas veces como el argumento 
pasado al programa y, en cada iteración, imprimirá, con printf el carácter 
que se le ha pasado. Usad fflush para que el carácter se imprima 
inmediatamente. El primero de los hilos creado por el padre, recibirá como 
parámetro '-'. El segundo, '+'. El propio padre llamará (sin crear un hilo) 
a la función fnhilo, pasándole un cero ('0'). Ejecutad la práctica y ved 
cómo Windows da la CPU a los diferentes hilos, variando en cada ejecución 
el valor pasado al programa. Si se añade un Sleep(0) después del fflush, 
¿cómo afecta a la salida? Podéis ver la salida con más comodidad si la 
redirigís a un fichero desde la línea de órdenes.
*/
//---------------------------------------------------------------------------
#include <windows.h>
#include <stdio.h>

/*---*x/
//---------------------------------------------------------------------------
#define PERROR(a) \
    {             \
        LPVOID lpMsgBuf;                                      \
        FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |        \
                   FORMAT_MESSAGE_FROM_SYSTEM |               \
                   FORMAT_MESSAGE_IGNORE_INSERTS, NULL,       \
                   GetLastError(),                            \
                   MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), \
                   (LPTSTR) &lpMsgBuf,0,NULL );               \
        fprintf(stderr,"%s:%s\n",a,lpMsgBuf);                 \
        LocalFree( lpMsgBuf );                                \
    }  
//---*/
/*---*/
//---------------------------------------------------------------------------
#define PERROR perror
void perror( char * mensaje)
{
  LPVOID lpMsgBuf;
  FormatMessage( 
      FORMAT_MESSAGE_ALLOCATE_BUFFER |
      FORMAT_MESSAGE_FROM_SYSTEM |
      FORMAT_MESSAGE_IGNORE_INSERTS, 
      NULL,
      GetLastError(),
      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
      (LPTSTR) &lpMsgBuf,
      0,
      NULL 
  );
  fprintf( stderr, "%s:%s\n", mensaje, lpMsgBuf);
  LocalFree( lpMsgBuf);
}//perror
//---*/

//--------------------------------------------------------------
//Prototipo de la funcion de gestión del hilo...
DWORD WINAPI fnHilo(LPVOID parametro);

//--------------------------------------------------------------
// Variables globales (comunes a todos)
int veces= 0; //global.. !!ejem!!

//--------------------------------------------------------------
int main(int argc, char* argv[])
{
  char * letras= (char *)"-+0"; //0 y 1 para hilos, 2=padre.
  HANDLE hilo[2];
  DWORD hiloId[2];
  int i;
  
  //Obtener y Validar argumentos...
  if (argc < 1+1) {
    fprintf( stderr, "Falta Argumento...\n");
    exit(1);
  }
  veces= atoi( argv[1]);
  if (veces < 100) {
    fprintf( stderr, "Argumento debe ser >= 100...\n");
    exit(2);
  }
  
  //Crear los hilos...
  for (i= 0; i < 2; i++) {
    hilo[i]= CreateThread( 
        NULL, 0, 
        fnHilo, &letras[i], 
        0, &hiloId[i]
    );
    if (hilo[i] == NULL) {
        PERROR( "Hilo");
    }
  }//for
  
  //Llamar también a la función del hilo.
  fnHilo( &letras[2]);


  
  //Hacer que el principal pierda un tiempo antes de acabar, 
  //para dar tiempo de finalización a los hilos.
  Sleep( 1000);
  
  return 0;
}//main

//--------------------------------------------------------------
DWORD WINAPI fnHilo(LPVOID parametro)
{
    char caracter= *((char *)parametro);
    int i;

    for (i= 0; (i < veces); i++) {
      printf( "%c", caracter); fflush( stdout); 
      //Sleep(0);
      Sleep(1);
    }//for
    
    return 0;
}//fnHilo


