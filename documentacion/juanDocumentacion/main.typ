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
    *Trabajo Sistemas Operativos 2: parking.c*\
]

#v(4cm)
#set text(size: 24pt)
#grid(
  columns: (1fr),
  align(center)[
    Sistemas Operativos II GIISI\
    USAL\
    Alumno: Juan Riego Vila\
  ]
)

#set text(size: 14pt)

#pagebreak()

#outline(title: auto)

#pagebreak()

= Cambios realizados el dia 31 de Marzo de 2026
== Hora: *12:34* cambios documentados:
He añadido una función de ayuda para que cuando se ejecute el programa sin argumentos esta salga aclarar los argumentos que tienes que introducir para el correcto funcionamiento del programa, esta se llama "ayudaPrograma()" y hay que pasarle como argumento el parámetro `argv` para que utiliza el nombre del archivo.

== Hora: *13:25* cambios documentados:
He añadido la forma para capturar cuando el usuario utiliza Ctrl-C, y he formateado el código de una forma que le es más agradable al profesor para cuando lo lea y tal.

Las señales funcionan de la siguiente forma dentro de la nueva función parking la cual ahora es la principal, ya que todo en la función main suele llevar a errores entonces lo he sacado todo y metido en otra función, bien pues esa funciona ahora se llama parking, dentro de esta al principio hay un bloque de código que lo único que hace es mediante las señales que se mandan al sistema operativo capturar Ctrl-C para su manejo, nada mas ha cambiado en esta función principal a parte de la justificación del texto para que todo se pueda leer mas fácil.
#block(
  stroke: 1pt + gray,
  fill: luma(96%),
  inset: 10pt,
  radius: 6pt,
  width: 100%,
)[
```C
        [...]
        struct sigaction sa_ctrlc;
        memset(&sa_ctrlc, 0, sizeof(sa_ctrlc));
        sa_ctrlc.sa_handler = manejar_ctrlc;
        sigaction(SIGINT, &sa_ctrlc, NULL);
        sigaction(SIGTERM, &sa_ctrlc, NULL);
        [...]
```
]

Luego he introducido una función llamada `manejar_ctrlc()`, la cual mediante la señal que hemos capturado en la función `parking()` cambiar el valor de la variable "terminar" la cual por ahora no sirve para nada ya que no tenemos hijos ni padres que terminar.

#block(
  stroke: 1pt + gray,
  fill: luma(96%),
  inset: 10pt,
  radius: 6pt,
  width: 100%,
)[
```C
        [...]
void manejar_ctrlc(int signal) {
        terminar = 1; //la funcion de esto es que cuando se ejecute el programa parar los bucles de creacion de los hijos cuando se reciba ctrl-c para limpiar bien los procesos
        system("tput cnorm"); //esto lo que hace es volver a poner el cursor "normal", ya que cuando se ejecuta el programa a veces el cursor se queda en modo "escondido" esto lo que hace es cambiarle a modo mostar
}//fin funcion manejar_ctrlc
        [...]
```
]

//
Y luego he introducido una parte del código que llama a 'system' mediante la librería "stdlib.h" gracias a esto se puede recuperar el cursor ya que a veces cuando 'matabas' al proceso en medio de la ejecución este te 'ocultaba' el cursor en la terminal, esta misma linea también ha sido agregada al final de la función `parking` para lo mismo.
#block(
  stroke: 1pt + gray,
  fill: luma(96%),
  inset: 10pt,
  radius: 6pt,
  width: 100%,
)[
```C
        [...]
        system("tput cnorm"); //esto lo que hace es volver a poner el cursor "normal", ya que cuando se ejecuta el programa a veces el cursor se queda en modo "escondido" esto lo que hace es cambiarle a modo mostar
        [...]
```
]
#pagebreak()

//>End of the paper