#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

// Prototipos de funciones
void shell_loop();
void process_command(char *line);
void parse_command(char *line, char **args);
void execute_command(char **args);
void handle_pipe(char *line);

// Función principal (main)
int main() {
    printf("Bienvenido a nuestra Shell. Escribe 'exit' para salir.\n");
    shell_loop();
    return 0;
}

// Función que ejecuta el bucle de la shell
void shell_loop() {
    char *line = NULL;
    size_t len = 0;

    while (1) {
        printf("shell:$ ");
        if (getline(&line, &len, stdin) == -1) {
            perror("Error leyendo comando");
            exit(EXIT_FAILURE);
        }
        line[strcspn(line, "\n")] = 0; // Quitar el salto de línea

        if (line[0] == '\0') {
            continue; // Si solo presiona enter, mostrar nuevamente el prompt
        }

        process_command(line); // Procesar el comando
    }

    free(line);
}

// Procesa el comando ingresado, manejando pipes o ejecutando el comando normal
void process_command(char *line) {
    char *args[100];

    if (strstr(line, "|")) {
        handle_pipe(line); // Manejar comando con pipes
    } else {
        parse_command(line, args);

        // Comando `exit` para salir
        if (strcmp(args[0], "exit") == 0) {
            exit(EXIT_SUCCESS);
        }

        execute_command(args);
    }
}

// Parsea el comando ingresado para obtener el comando y sus argumentos
void parse_command(char *line, char **args) {
    int position = 0;
    char *token;

    token = strtok(line, " ");
    while (token != NULL) {
        args[position] = token;
        position++;
        token = strtok(NULL, " ");
    }
    args[position] = NULL; // Indicar el fin de los argumentos
}

// Ejecuta el comando ingresado utilizando fork() y execvp()
void execute_command(char **args) {
    pid_t pid, wpid;
    int status;

    pid = fork();
    if (pid == 0) { // Proceso hijo
        if (execvp(args[0], args) == -1) {
            perror("Error ejecutando comando");
        }
        exit(EXIT_FAILURE);
    } else if (pid < 0) { // Error en fork
        perror("Error en fork");
    } else { // Proceso padre
        do {
            wpid = waitpid(pid, &status, WUNTRACED);
        } while (!WIFEXITED(status) && !WIFSIGNALED(status));
    }
}

// Maneja múltiples pipes entre varios comandos
void handle_pipe(char *line) {
    char *commands[100];
    char *args[100][10];
    int num_commands = 0;
    int pipefd[100][2];

    // Divide el input en varios comandos usando el delimitador '|'
    char *token = strtok(line, "|");
    while (token != NULL) {
        commands[num_commands] = token;
        token = strtok(NULL, "|");
        num_commands++;
    }

    // Parsea cada comando y sus argumentos
    for (int i = 0; i < num_commands; i++) {
        parse_command(commands[i], args[i]);
    }

    // Crea pipes necesarios
    for (int i = 0; i < num_commands - 1; i++) {
        if (pipe(pipefd[i]) == -1) {
            perror("Error creando pipe");
            exit(EXIT_FAILURE);
        }
    }

    // Itera sobre cada comando y crea un proceso hijo para ejecutarlo
    for (int i = 0; i < num_commands; i++) {
        pid_t pid = fork();

        if (pid == 0) { // Proceso hijo
            // Si no es el primer comando, redirige la entrada del pipe anterior
            if (i > 0) {
                dup2(pipefd[i - 1][0], STDIN_FILENO);
            }
            // Si no es el último comando, redirige la salida al siguiente pipe
            if (i < num_commands - 1) {
                dup2(pipefd[i][1], STDOUT_FILENO);
            }

            // Cierra los pipes
            for (int j = 0; j < num_commands - 1; j++) {
                close(pipefd[j][0]);
                close(pipefd[j][1]);
            }

            // Ejecuta el comando
            if (execvp(args[i][0], args[i]) == -1) {
                perror("Error ejecutando comando");
                exit(EXIT_FAILURE);
            }
        } else if (pid < 0) { // Error en fork
            perror("Error en fork");
            exit(EXIT_FAILURE);
        }
    }

    // Cerrar pipes en el proceso padre
    for (int i = 0; i < num_commands - 1; i++) {
        close(pipefd[i][0]);
        close(pipefd[i][1]);
    }

    // Esperar a que todos los procesos hijos terminen
    for (int i = 0; i < num_commands; i++) {
        wait(NULL);
    }
}

