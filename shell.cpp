#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

// Prototipos de funciones
void shell_loop();            // Bucle principal de la shell
void process_command(char *line); // Procesa el comando ingresado
void parse_command(char *line, char **args); // Parsea la línea de entrada
void execute_command(char **args); // Ejecuta el comando
void handle_pipe(char *line);    // Maneja comandos con pipes

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

// Maneja el pipe entre dos comandos
void handle_pipe(char *line) {
    char *commands[2];
    char *cmd1[10], *cmd2[10];
    int pipefd[2];

    commands[0] = strtok(line, "|");
    commands[1] = strtok(NULL, "|");

    parse_command(commands[0], cmd1);
    parse_command(commands[1], cmd2);

    if (pipe(pipefd) == -1) {
        perror("Error creando pipe");
        exit(EXIT_FAILURE);
    }

    pid_t pid1, pid2;
    pid1 = fork();
    if (pid1 == 0) { // Primer proceso
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[0]);
        close(pipefd[1]);
        if (execvp(cmd1[0], cmd1) == -1) {
            perror("Error ejecutando primer comando");
        }
        exit(EXIT_FAILURE);
    }

    pid2 = fork();
    if (pid2 == 0) { // Segundo proceso
        dup2(pipefd[0], STDIN_FILENO);
        close(pipefd[1]);
        close(pipefd[0]);
        if (execvp(cmd2[0], cmd2) == -1) {
            perror("Error ejecutando segundo comando");
        }
        exit(EXIT_FAILURE);
    }

    close(pipefd[0]);
    close(pipefd[1]);
    wait(NULL);
    wait(NULL);
}