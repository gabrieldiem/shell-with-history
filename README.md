# shell

Repositorio para el esqueleto del [TP: shell](https://fisop.github.io/website/tps/shell) del curso Mendez-Fresia de **Sistemas Operativos (7508) - FIUBA**

## Integrantes

- Victoria Avalos - 108434
- Julieta Perez Goldstein - 107997
- Walter Gabriel Diem - 105618
- Gonzalo Ranzani - 105933

## Challenge resuelto: HISTORY

Features soportadas:

- Comando `history [n]` con `n` siendo el argumento opcional para imprimir los últimos _n_ comandos, y si se ejecuta sin argumentos opcionales, es decir, sólo `history`, se imprimen todos los comandos del historial.
- Las teclas <kbd>↑</kbd> (arrow up) y <kbd>↓</kbd> (arrow down) para moverse entre los comandos del historial y simplemente presionar enter para re-ejecutarlos.
- La tecla <kbd>Backspace</kbd> para borrar comandos de hasta una línea de largo.
- La combinación de teclas <kbd>Ctrl</kbd> + <kbd>D</kbd> termina la ejecución de la shell.
- Variable de entorno `HISTFILE`. Por defecto, al iniciar la shell, esta variable apunta al path resuelto del path `~/.fisop_history`. Se expande como otras variables ejecutando, por ejemplo, `echo $HISTFILE`. Para cambiar su valor se puede ejecutar `HISTFILE=./.new_histfile history 2`, como si se tratara de una variable temporaria. Si `~/.fisop_history` no existe, se crea al iniciar la shell, si existiese, se cargan todos los comandos para poder seguir usando comandos de una sesión anterior. Este archivo usa el formato estándar para guardar comandos de historial, es decir, el comando seguido de un salto de línea y nada más. Cada vez que se ejecuta un comando, este se guarda en el historial.
- Designadores de eventos `!!` y `!-n`. Donde al escribir `!!` y presionar la tecla <kbd>Espacio</kbd> o <kbd>Tab</kbd>, se reemplaza por el último comando ejecutado, o no se reemplaza por nada si el historial está vacío. Análogamente, al escribir `!-n` y presionar <kbd>Espacio</kbd> o <kbd>Tab</kbd> pero con el comando correspondiente a retroceder _n_ comandos hacia atrás.

## Respuestas teóricas

Utilizar el archivo `shell.md` provisto en el repositorio

## Compilar

```bash
make
```

## Pruebas

- Ejecutar todas las pruebas

```bash
make test
```

- Ejecutar una **única** prueba

```bash
make test-TEST_NAME
```

Por ejemplo:

```bash
make test-env_empty_variable
```

Cada identificador de una prueba se muestra entre paréntesis `(TEST_NAME)` al lado de cada _test_ cuando se ejecutan todas las pruebas.

```
=== Temporary files will be stored in: /tmp/tmp0l10br1k-shell-test ===

PASS 1/26: cd . and cd .. work correctly by checking pwd (no prompt) (cd_back)
PASS 2/26: cd works correctly by checking pwd (no prompt) (cd_basic)
PASS 3/26: cd with no arguments takes you home (/proc/sys :D) (cd_home)
PASS 4/26: empty variables are not substituted (env_empty_variable)
...
```

## Ejecutar

```bash
./sh
```

## Linter

```bash
make format
```

Para efectivamente subir los cambios producidos por `make format`, hay que hacer `git add .` y `git commit`.
