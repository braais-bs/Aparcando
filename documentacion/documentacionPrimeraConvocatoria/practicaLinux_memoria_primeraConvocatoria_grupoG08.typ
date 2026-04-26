//Document Settings
#set page(
    paper: "a4",
    margin: (x: 1.8cm, y: 1.5cm),
)
#set text(
//    font: "Times New Roman",
    size: 14pt,
    lang: "es"
)
#set par(
    justify: true,
    leading: 0.52em,
)

#set heading(numbering: "1. ")


//> Beginning of the paper
#v(5cm)
#set text(size: 30pt)
#align(center)[
  #set par(justify: false)
    *Trabajo Sistemas Operativos 2: parking.c práctica Linux*\
]

#v(4cm)
#set text(size: 24pt)
#grid(
  columns: (1fr),
  align(center)[
    Sistemas Operativos II GIISI\
    USAL\
    Grupo: G08\
    Alumno 1: Brais Bértolo Senra\
    Alumno 2: Juan Riego Vila\
  ]
)

#set text(size: 14pt)

#pagebreak()

#outline(title: auto)

#pagebreak()
= RECURSOS IPC USADOS, VALORES INICIALES Y SIGNIFICADO

== Buzón de Mensajes (`id_buzon`)
   - *Uso:* Comunicación entre la Biblioteca, el Gestor y los Chóferes.
   - *Mensajes tipo 100:* Peticiones originales enviadas por la biblioteca.
   - *Mensajes tipo 1 (Alta) y tipo 2 (Baja):* Peticiones renumeradas por el Gestor según la política (PA, PD o FIFO) para que el SO las ordene automáticamente.

== Memoria Compartida (`id_mem`)
   - *`acera[4][80]` y `carril[4][80]`:* Matrices para guardar la ocupación física del parking por cada algoritmo.
   - *`proxAparcar[4]`:* Vector para guardar el puntero circular necesario en el algoritmo de Siguiente Ajuste.

== Semáforos (`id_sem`)
   - *`SEM_MUTEX` (Valor inicial = 1):* Garantiza la exclusión mutua estricta al leer o escribir en la Memoria Compartida (`acera`, `carril`, `proxAparcar`).
   - *`SEM_ORDEN[4]` (Valor inicial = 1 para cada uno):* Array de 4 semáforos. Garantiza que no haya dos chóferes calculando huecos para el mismo algoritmo a la vez, evitando que se asigne el mismo sitio a dos coches distintos.
   - *`SEM_AVANCE[4]` (Valor inicial = 0):* Array de 4 semáforos usado en las funciones de `permiso_avance` para sincronizar los pasos de la simulación gráfica de la biblioteca.

=== PSEUDOCÓDIGO PROCESO GESTOR Y PROCESO  CHOFER
*Nota:* Se asume que la Biblioteca internamente hace un `Send(Buzon, msg, tipo=100)` cuando llega un coche nuevo o agota su tiempo.
#block(
  stroke: 1pt + gray,
  fill: luma(96%),
  inset: 10pt,
  radius: 6pt,
  width: 100%,
)[
  #set align(center)
  #scale(x: 80%, y: 80%, reflow: true)[
    ```text
       PROCESO GESTOR                                PROCESO CHÓFER
      ================                              ================
    Por_siempre_jamás                             Por_siempre_jamás
    {                                             {
       // Lee SOLO peticiones de la biblioteca       // Pide tipo -2: El SO le da primero los 
       Receive(Buzon, msg, tipo=100)                 // de tipo 1 y luego los de tipo 2
                                                     Receive(Buzon, msg, tipo=-2)
       // Renumera según política (PA/PD/FIFO)
       Si (cumple_prioridad)                         alg = obtener_algoritmo(msg.coche)
           msg.tipo = 1 // Alta prioridad
       Sino                                          // Evita que dos chóferes del mismo 
           msg.tipo = 2 // Baja prioridad            // algoritmo actúen a la vez
                                                     Wait(SEM_ORDEN[alg])
       // Reenvía a la cola para los chóferes
       Send(Buzon, msg)                              // Protege la memoria compartida
    }                                                Wait(SEM_MUTEX)

                                                     Si (msg.subtipo == APARCAR) {
                                                         pos = buscar_hueco(alg)
                                                         escribir_acera_shm(pos)
                                                     } Sino {
                                                         borrar_acera_shm(pos)
                                                     }

                                                     // Libera la memoria compartida
                                                     Signal(SEM_MUTEX)

                                                     // Llama a la biblioteca para animar
                                                     Ejecutar_Operacion_Biblioteca()

                                                     // Libera el turno de su algoritmo
                                                     Signal(SEM_ORDEN[alg])
                                                  }
    ```
  ]
]

== Función crear_chofer
Es la función que crea los procesos trabajadores para que los coches realicen la función de aparcar y desaparcar.

*Funcionamiento interno:*

- *Bifurcación:* Ejecuta la llamada al sistema fork().

- *Configuración del Hijo:* El proceso hijo resultante hereda el acceso a los semáforos, el buzón y la memoria compartida. Inmediatamente invoca a bucle_chofer(), donde entrará en su ciclo de trabajo.

- *Gestión del Padre:* El proceso padre almacena el PID del nuevo chófer en un array global. Esto es fundamental para poder realizar un `wait()` o enviar señales de terminación `(SIGTERM)` a todos los hijos cuando la simulación deba cerrarse definitivamente.

- *Importancia:* Abstrae la creación de procesos, permitiendo que el programa principal lance tantos chóferes como se haya especificado por línea de comandos de forma limpia y organizada.


=== PSEUDOCODIGO FUNCIÓN CREAR_CHOFER
#block(
  stroke: 1pt + gray,
  fill: luma(96%),
  inset: 10pt,
  radius: 6pt,
  width: 100%,
)[
  #set align(center)
  #scale(x: 80%, y: 80%, reflow: true)[
    ```text
       PROCESO CREACIÓN CHOFER
      =========================
Por_siempre_jamás
{
    // El SO extrae automáticamente primero los de tipo 1 y luego los de tipo 2
    Receive(Buzon, msg, tipo_solicitado=-2)

    alg = obtener_algoritmo(msg.coche)

    // 1. Bloqueo de Algoritmo: Evita que dos chóferes del mismo algoritmo colisionen
    Wait(SEM_ORDEN[alg])

        // 2. Bloqueo de Memoria: Exclusión mutua para la memoria compartida
        Wait(SEM_MUTEX)

            Si (msg.subtipo == APARCAR) {
                pos = buscar_hueco(alg)
                escribir_acera_shm(pos)
            } Sino {
                borrar_acera_shm(pos)
            }

        // Libera la memoria compartida
        Signal(SEM_MUTEX)

        // Llama a la biblioteca (la simulación visual no necesita mutex de memoria)
        Ejecutar_Operacion_Biblioteca()

    // Libera el turno para el siguiente chófer de su mismo algoritmo
    Signal(SEM_ORDEN[alg])
}
    ```
  ]
]

== Función proceso_gestor()
Esta función actúa como el cerebro logístico de la cola de mensajes. Su objetivo es transformar una cola FIFO estándar en una cola de prioridad dinámica.

*Funcionamiento interno:*

- *Intercepción:* Realiza un bloqueo en msgrcv esperando únicamente mensajes de tipo PARKING_MSG (tipo 100), que son los que genera la biblioteca original.

- *Lógica de Clasificación:* Mediante una estructura condicional, comprueba si la petición es de "Aparcar" o "Desaparcar". Cruza este dato con las banderas de política de la práctica (prio_PA o prio_PD).

- *Re-etiquetado:* Si el mensaje debe tener prioridad, cambia su campo msg.tipo a 1 (Alta). Si es la operación secundaria, le asigna el tipo 2 (Baja).

- *Inyección:* Devuelve el mensaje modificado al buzón con msgsnd.

- *Importancia:* Permite que los chóferes no tengan que "pensar" en las prioridades; simplemente piden el mensaje con el número de tipo más bajo al Sistema Operativo.

=== PSEUDOCODIGO FUNCIÓN PROCESO_GESTOR
#block(
  stroke: 1pt + gray,
  fill: luma(96%),
  inset: 10pt,
  radius: 6pt,
  width: 100%,
)[
  #set align(center)
  #scale(x: 80%, y: 80%, reflow: true)[
    ```text
Por_siempre_jamás
{
    // Espera bloqueado hasta recibir una petición original de la biblioteca
    Receive(Buzon, msg, tipo_solicitado=100)

    Si (la operacion coincide con la política de prioridad elegida)
        msg.tipo = 1 // Asigna Alta Prioridad
    Sino
        msg.tipo = 2 // Asigna Baja Prioridad

    // Reenvía el mensaje a la cola para que lo coja un chófer
    Send(Buzon, msg)
}
    ```
  ]
]


== Función proceso_avisador()
Encargada de la gestión del tiempo de la simulación.

*Funcionamiento interno:*

- *Temporización:* Utiliza alarm(30) para activar una señal SIGALRM en el sistema operativo pasados 30 segundos de ejecución, y luego pause() para mantener (no se si mantener es la mejor palabra, por si se te ocurre alguna que creas que es mejor) el proceso sin consumir CPU hasta que se active la señal.

- *Finalización Controlada:* Una vez agotado el tiempo, este proceso realiza la llamada a PARKING_fin(1). Esto le indica a la biblioteca que no acepte más coches nuevos, pero permite que los que están dentro terminen de salir.

- *Finalización mediante manejador:* Al interrumpirse el pause(), se salta automáticamente a manejar_alarma(), que llama a PARKING_fin(1) y al volver a la propia función, se ejecuta un exit(0) que termina el proceso, que es recogido por el padre después.

- *Importancia:* Garantiza que la práctica no sea infinita y que el cierre sea "suave", permitiendo que la acera se vacíe antes de destruir los recursos IPC.

=== PSEUDOCODIGO FUNCIÓN PROCESO_AVISADOR
#block(
  stroke: 1pt + gray,
  fill: luma(96%),
  inset: 10pt,
  radius: 6pt,
  width: 100%,
)[
  #set align(center)
  #scale(x: 80%, y: 80%, reflow: true)[
    ```text
{
    // Ignora la señal SIGNINT, para que durante la próxima ejecución no intervenga.
    Signal(SIGINT, SIG_IGN)

    // Llama a la biblioteca indicando una finalización "normal" (valor 1).
    // Esto hace que la biblioteca deje de generar coches nuevos, pero 
    // permite a los chóferes terminar de desaparcar los que ya están dentro.
    PARKING_fin(1)


    // Configura el manejador para capturar la señal SIGALARM.
    SignalAction(SIGALARM)

    // Programa el temporizador del sistema 30 segundos que es lo que dura una ejecución sin fallos.
    alarm(30)

    // Espera sin consumición de CPU
    pause()

    // Finaliza su ejecución. El proceso principal (padre) lo detectará
    // y sabrá que ha comenzado la fase final de la simulación.
    Exit()
}
    ```
  ]
]
//>End of the paper