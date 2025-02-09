# shell

### Búsqueda en $PATH

#### ¿Cuáles son las diferencias entre la syscall execve(2) y la familia de wrappers proporcionados por la librería estándar de C (libc) exec(3)?

`execve(2)` es una syscall definida en `<unistd.h>` que recibe un path absoluto o relativo al current directory que apunta a un archivo binario ejecutable o un script, además recibe un vector de argumentos `argv` y un vector de variables de entorno con forma `KEY=VALUE`.

Por otro lado, la familia de funciones `exec(3)` de la libc, son wrappers implementados en C de la syscall `execve(2)`, donde su comportamiento es variable dependiendo de cuál wrapper se usó, usualmente denominadas por las letras que le siguen a `exec` y son comportamientos que se combinan:

- `p`: tiene en cuenta si se busca el binario a ejecutar en los directorios a donde apunta el `$PATH`, en lugar de pasarle un vector con los argumentos.

- `e`: permiten elegir qué variables de entorno pasarle al proceso a ejecutar mediante la estructura de strings `KEY=VALUE`. Las que no son de esta variante, le heredan todo el entorno al proceso a ejecutar.

- `v` o `l`: la forma de estructurar el argv a pasarle al objeto a ejecutar, si es un vector (letra `v`) con su último elemento en NULL, o si se le pasa una lista variádica `va_list` (letra `l`) con los argumentos, siendo el último elemento NULL.

#### ¿Puede la llamada a exec(3) fallar? ¿Cómo se comporta la implementación de la shell en ese caso?

`execve(2)`, y por ende también los wrappers de la familia `exec(3)`, pueden fallar, y al hacerlo retorna a la función invocadora con -1 de valor de retorno y el tipo de error guardado en `errno`. El comportamiento de qué hacer cuando ocurre la falla, está a manos de la función invocadora.

Esta implementación de la shell, en caso de fallar un `exec(3)` (que se ejecuta dentro de un proceso separado, previamente habiendo llamado a fork), libera recursos del heap y luego llama a la syscall `_exit(2)`, que a diferencia de la syscall `exit(2)`, no llama a las funciones de cleanup `atexit(3)` y `on_exit(3)`, y sí cierra file descriptors abiertos por el proceso, y tampoco hace un flush de los buffers de `stdio`, lo cual es de interés para no hacer flush de datos heredados del proceso padre que hayan quedado en los buffers, evitando así interferencia en lo impreso en la pantalla. El proceso padre que inició el fork, el cual es la shell misma, deja de estar bloqueado por el `wait(2)` y vuelve a pedir input al usuario.

### Flujo estándar

#### Funcionamiento de `2>&1`

El flujo de datos de output está compuesto por dos streams: el stream de `stdout` (también llamado _stream 1_) y el de `stderr` (también llamado _stream 2_). Cuando se hace el redireccionamiento a un archivo de `stdout` de la manera `>file.text` sería conceptualmente equivalente a hacer `1>file.text`, en el caso de `stderr` es evidente porque se hace con el número 2 `2>file.text`.
Lo que sucede al ejecutar `2>&1` es combinar el _stream 2_ en el _stream 1_, es decir, que el output de ambos streams se dirija a donde se dirige el _stream 1_ al momento de ejecutar la redirección. Esto quiere decir, que si el _stream 1_ se redirige antes de combinar los streams, el _stream 2_ se combinará y terminará en el lugar redirigido, y que si el _stream 1_ se redirige después de la combinación, el _stream 2_ mantendrá dirigiendo el contenido hacia donde apuntaba el _stream 1_ previo a la posterior redirección del mismo. Esto se ilustra con los siguientes ejemplos teniendo en cuenta la shell `bash(1)`:

```shell
$ ls -C /home /noexiste >out.txt 2>&1

$ cat out.txt
ls: cannot access '/noexiste': No such file or directory
/home:
username
```

Primero se establece la redirección del _stream 1_ hacia el archivo `out.txt`. Luego, se combina el _stream 2_ con el _stream 1_, y como el _stream 1_ apunta a `out.txt`, allí también terminará el output del _stream 2_.

Se presenta otro ejemplo:

```shell
$ ls -C /home /noexiste 2>&1 >out.txt
ls: cannot access '/noexiste': No such file or directory

$ cat out.txt
/home:
username
```

Aquí primero se combina el _stream 2_ con el _stream 1_, y como el _stream 1_ apunta a al output de la terminal, allí también terminará el output del _stream 2_. Luego se establece la redirección del _stream 1_ hacia el archivo `out.txt`, por lo que solamente su output terminará en el archivo, y no el del _stream 2_.

En la shell implementada, sin embargo, programáticamente primero se establecen primero las redirecciones a los archivos de los streams, y por último si se debe combinar o no el _stream 2_ en el _stream 1_. Por lo que el orden relativo entre las redirecciones y la combinación no se tiene en cuenta.

### Tuberías múltiples

#### Investigar qué ocurre con el exit code reportado por la shell si se ejecuta un pipe ¿Cambia en algo?

En caso de ejecutar un pipe, la única diferencia será que se muestra el exit code de cada proceso involucrado por separado. Esto se debe a que la shell termina ejecutando cada comando como si fuera un cmd de tipo exec, donde lo único que cambia es que se redirige el flujo estándar de estos procesos.
Si se ejecuta, por ejemplo, el comando

```shell
ls | grep -F hola | wc -l
```

La shell reporta el status de cada proceso de la siguiente manera:

```shell
Program: [ls ] exited, status: 0
Program: [grep -F hola ] exited, status: 1
Program: [wc -l] exited, status: 0
```

#### ¿Qué ocurre si, en un pipe, alguno de los comandos falla? Mostrar evidencia (e.g. salidas de terminal) de este comportamiento usando bash. Comparar con su implementación.

En caso de que alguno de los comandos dentro del pipe falle, los demás se ejecutarán de igual manera dado que todos corren en paralelo. La shell muestra el error producido por el proceso conflictivo, además de la salida de aquellos que no tuvieron un comportamiento inesperado.

Si ejecutamos, por ejemplo, el comando:

```shell
ls /asd | grep -F hola | wc -l
```

La salida en bash será:

```shell
ls: no se puede acceder a '/asd': No existe el archivo o el directorio
0
```

Podemos observar que a pesar de que el primer comando falla, igual se muestra la salida del wc. Si luego del comando ejecutamos:

```shell
echo "${PIPESTATUS[0]} ${PIPESTATUS[1]} ${PIPESTATUS[2]}"
```

Podemos ver los exit code de cada uno de los procesos.

```shell
2 1 0
```

El '2' es el código de error arrojado por el **ls**. El '1' es el código de salida del **grep**, que indica que no hubo coincidencias entre "hola" y la entrada recibida del ls (archivo vacío en este caso). El '0' es el código de salida del **wc**, indicando que se ejecutó exitosamente y en este caso devolvió como output 0. Queda en evidencia que por más que un error suceda, el resto de los comandos en el pipe se siguen ejecutando normalmente.

En nuestra implementación sucede lo mismo: si un proceso falla el resto se seguirá ejecutando de igual manera. Observamos que no es necesario ejecutar otro comando para conocer los exit code de cada proceso, sino que ya se ve reflejado en la salida.

```shell
$ ls /asd | grep -F hola | wc -l
ls: no se puede acceder a '/asd': No existe el archivo o el directorio
0
Program: [ls /asd ] exited, status: 2
Program: [grep -F hola ] exited, status: 1
Program: [wc -l] exited, status: 0
```

### Variables de entorno temporarias

#### ¿Por qué es necesario hacerlo luego de la llamada a fork(2)?

Entendiendo al proceso hijo, producido por llamar a `fork(2)`, como el que ejecutará una llamada a `exec`, es importante establecer los valores de las variables de entorno temporarias luego del fork para no modificar el valor de las variables de entorno del proceso padre. De otra manera, no serían temporarias al programa a ejecutar sino que le cambiaría el valor al nivel de la shell.

#### En algunos de los wrappers de la familia de funciones de exec(3) (las que finalizan con la letra e), se les puede pasar un tercer argumento (o una lista de argumentos dependiendo del caso), con nuevas variables de entorno para la ejecución de ese proceso. Supongamos, entonces, que en vez de utilizar setenv(3) por cada una de las variables, se guardan en un arreglo y se lo coloca en el tercer argumento de una de las funciones de exec(3). ¿El comportamiento resultante es el mismo que en el primer caso? Explicar qué sucede y por qué. Describir brevemente (sin implementar) una posible implementación para que el comportamiento sea el mismo.

Como se mencionó anteriormente, los wrappers de `execve(2)` que terminan con la letra e, no heredan todas las variables de entorno del proceso invocador sino que tendrán sólo las variables definidas en el vector que tiene de argumento para este propósito, mientras que las que no terminan en e, heredan todas las variables de entorno ya que pasan a `execve(2)` la variable externa `environ`.

De esta manera, si dicho vector se armara solamente con las variables de entorno temporarias, el programa a ejecutar sólo tendrá acceso a esas variables de entorno, no al resto disponible del proceso invocador. Si se quisiera replicar el comportamiento de wrapper sin e junto con variables temporarias, con el uso de un wrapper que sí termina con e, sería necesario realizar los siguientes pasos:

1. Hacer un vector con la copia de la variable externa `environ`.
1. En el vector, reemplazar las variables existentes que se quieren sobreescribir con el valor de la variable de entorno temporaria, o crearla de no existir.
1. Utilizar este vector como argumento al wrapper exec que finaliza con la e.

### Pseudo-variables

#### Propósito de la variable mágica `?`

La variable mágica `?` guarda como contenido el exit code del último proceso ejecutado por la shell. Esto es útil para saber si el programa ejecutado terminó en un estado de éxito o de falla, y en este último caso, con qué código falló. Para ver su valor se puede ejecutar: `echo $?`

#### Investigar al menos otras tres variables mágicas estándar, y describir su propósito. Incluir un ejemplo de su uso en bash (u otra terminal similar).

Otras variables mágicas estándar son:

- `$$`: contiene el PID de la shell en la que se está corriendo el comando. En bash se puede ejecutar `echo $$` y se imprimirá por pantalla el PID.

- `$0`: se puede interpretar como el primer argumento del argv. Si se ejecuta desde la shell, como ser bash o zsh, como `echo $0` imprimirá la ubicación del binario de la shell misma, si se ejecuta desde un script imprimirá el nombre del archivo que contiene el script.

- `$-`: contiene en forma de string el set de configuraciones de la shell donde se corre el comando. Estas pueden ser: hashall (h) para buscar todos los comandos en el path, interactive (i), monitor (m) para controlar los jobs en foreground y background, braceexpand (B) para habilitar la expansión con llaves, histexpand (H) para habilitar que se puedan correr comandos del hisorial con `!numero_de_comando_en_historial`, stdin (s) para leer comandos desde stdin, entre otros. Se puede ejecutar en bash como `echo $-`

Algunas variables mágicas son más útiles en un script de bash que en la shell misma, por ejemplo la variable mágica `$#` que en esencia sería como un `argc -1`, representando la cantidad de argumentos pasados al shellscript. Se puede usar, por ejemplo, para saber si el script llegó a la cantidad mínima de parámetros de entrada de la siguiente manera:

```shell
#!/bin/bash

if [ $# -lt 2 ]; then
  echo "Error: se requieren al menos dos parámetros de entrada"
  exit 1
fi

# Resto del script que contaba con que al menos se tuvieran 2 parámetros de entrada
```

### Comandos built-in

#### ¿Entre cd y pwd, alguno de los dos se podría implementar sin necesidad de ser built-in? ¿Por qué? ¿Si la respuesta es sí, cuál es el motivo, entonces, de hacerlo como built-in? (para esta última pregunta pensar en los built-in como true y false)

El comando cd necesita ser un built-in de la shell, ya que es la única manera de cambiar el estado del proceso de la shell para que refleje que está situado en otro directorio. Si no fuera un built-in, la ejecución de una syscall como `chdir(2)` se daría en un proceso hijo de la shell, al estar en un archivo binario separado se ejecutaría con un exec, lo que resultaría en el cambio de directorio para dicho proceso pero no para la shell.

El comando pwd no es necesario que sea un built-in de la shell, aunque en shells como bash en distribuciones de Linux como Ubuntu se tiene una versión built-in y una versión en un archivo binario. Esto es debido a que, en principio, tanto el proceso de la shell como un proceso hijo corren en el mismo directorio, y una llama a una función como `getcwd(3)` estaría ejecutando esencialmente operaciones de lectura sobre el directorio para saber su ubicación, lo cual no afecta el estado de ninguno de los dos procesos. Si fuera built-in, se tendría un ahorro de recursos, ya que no haría falta hacer un fork y así crear un nuevo proceso para obtener la información del directorio actual.

### Procesos en segundo plano

#### Explicar detalladamente el mecanismo completo utilizado.

Al inicializar la shell (en sh.c), se utiliza `sigaction` para setear el manejo de la señal SIGCHLD con un handler custom. Este último invoca `waitpid` solamente para aquellos procesos hijos que se ejecutan en segundo plano mediante el process group id. Para esto último, se utilizó setpgid(0, 0) en el proceso shell y en los procesos hijos de la shell que se ejecutan en primer plano. Dicha llamada a setpgid utiliza el PID del calling process y setea el PGID igual al PID. 

El wait mencionado anteriormente se realiza con un flag, `WNOHANG` (return inmediatamente si ningún hijo ha terminado), que transforma a la operación en no bloqueante, permitiendo así que la shell siga recibiendo comandos del usuario. También imprime por pantalla el pid del proceso finalizado y su estado.

Luego, en cada iteración del ciclo que ejecuta cada comando (runcmd) se verifica si el comando actual es de primer o segundo plano. Para aquellos en primer plano se realiza un wait e imprime el estado, mientras que para los de segundo plano se imprime la información correspondiente con `print_back_info` y no se realiza ningún wait. Esto se debe a que el wait para dichos procesos será aquel implementado en el handler de la shell, como se mencionó anteriormente. De no ser así, la shell se bloquearía esperando a que termine y no se trataría de un proceso en segundo plano.

Los comandos marcados como "BACK" (background process) funcionan correctamente ya sea que posean una redirección de i/o o no, ya que dicha información es verificada con el parámetro "c" del struct backcmd, y se ejecutan las mismas funciones de los casos EXEC y REDIR respectivamente.

Debido a que el handler para SIGCHLD se ejecuta en el espacio de usuario, se utilizó un stack alternativo con `malloc` y `sigaltstack` para evitar bugs. Dicho stack es liberado antes de finalizar el programa.

Finalmente, para poder imprimir por pantalla de forma async signal safe los procesos de background, se escribe con formato en el file descriptor de stdout.

#### ¿Por qué es necesario el uso de señales?

En el caso de procesos en segundo plano, las señales son necesarias para que el shell sepa cuándo un proceso hijo ha terminado sin bloquear su ejecución. En particular, SIGCHLD se usa para notificar al shell cuando un proceso hijo (en segundo plano) termina.
Un handler de SIGCHLD permite manejar la señal de manera asíncrona, llamando a waitpid() con la opción `WNOHANG` para recolectar el estado del proceso terminado sin interrumpir la ejecución del shell. Esto evita procesos zombies y permite que el shell siga aceptando comandos mientras los procesos en segundo plano corren.

### Historial

#### ¿Cuál es la función de los parámetros MIN y TIME del modo no canónico? ¿Qué se logra en el ejemplo dado al establecer a MIN en 1 y a TIME en 0?

Al utilizar la shell en modo no canónico mediante la remoción de los flags `ECHO` y `ICANON`, y manipulando las configuraciones de la shell con funciones como `tcgetattr(3)` y `tcsetattr(3)`, se pueden cambiar dos parámetros de alta importancia para la interactividad de la shell: MIN y TIME. MIN (o VMIN) representa la cantidad mínima de caracteres necesaria para una lectura en modo no canónico, y TIME (o VTIME) representa el timeout en decisegundos para ejecutar una lectura en modo no canónico. Al setear MIN en 1 y TIME en 0, se está tomando control de la forma de leer los caracteres de stdin, deshabilitando el timeout y leyendo de forma inmediata de a un caracter por vez, lo que facilita el control del flujo en modo no canónico.
