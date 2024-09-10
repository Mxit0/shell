#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/types.h>

#define DEFAULT_FAV_FILE "misfavoritos.txt"

char fav_file_path[256] = DEFAULT_FAV_FILE;

typedef struct {
    char **commands;
    int count;
    int capacity;
} FavList;

FavList fav_list = { .commands = NULL, .count = 0, .capacity = 10 };

void shell_loop();
void process_command(char *line);
void parse_command(char *line, char **args);
void execute_command(char **args);
void handle_pipe(char *line);

void favs_crear(char *filepath);
void favs_mostrar();
void favs_eliminar(char *nums);
void favs_buscar(char *cmd);
void favs_borrar();
void favs_ejecutar(int num);
void favs_cargar();
void favs_guardar();
void add_fav(char *cmd);
void set_recordatorio(int seconds, char *message);
void change_fav_file_path(char *new_path);
int check_and_create_fav_file();
int create_directory_if_not_exists(const char *dir_path);

int main() {
    printf("Bienvenido a nuestra Shell. Escribe 'exit' para salir.\n");
    
    fav_list.commands = malloc(fav_list.capacity * sizeof(char *));
    if (!fav_list.commands) {
        perror("Error al asignar memoria para favoritos");
        exit(EXIT_FAILURE);
    }

    shell_loop();
    return 0;
}

void shell_loop() {
    char *line = NULL;
    size_t len = 0;

    favs_cargar();

    while (1) {
        printf("shell:$ ");
        if (getline(&line, &len, stdin) == -1) {
            perror("Error leyendo comando");
            free(line);
            exit(EXIT_FAILURE);
        }
        line[strcspn(line, "\n")] = 0;

        if (line[0] == '\0') {
            continue;
        }

        process_command(line);
    }

    free(line);
    favs_borrar();
}

void process_command(char *line) {
    char *args[100];

    if (strstr(line, "|")) {
        handle_pipe(line);
    } else if (strncmp(line, "favs", 4) == 0) {
        char *cmd = strtok(line, " ");
        char *subcmd = strtok(NULL, " ");

        if (subcmd) {
            if (strcmp(subcmd, "crear") == 0) {
                char *filepath = strtok(NULL, " ");
                if (filepath) {
                    favs_crear(filepath);
                } else {
                    printf("Ruta del archivo no proporcionada.\n");
                }
            } else if (strcmp(subcmd, "mostrar") == 0) {
                favs_mostrar();
            } else if (strcmp(subcmd, "eliminar") == 0) {
                char *nums = strtok(NULL, " ");
                if (nums) {
                    favs_eliminar(nums);
                } else {
                    printf("Números para eliminar no proporcionados.\n");
                }
            } else if (strcmp(subcmd, "buscar") == 0) {
                char *cmd = strtok(NULL, " ");
                if (cmd) {
                    favs_buscar(cmd);
                } else {
                    printf("Comando a buscar no proporcionado.\n");
                }
            } else if (strcmp(subcmd, "borrar") == 0) {
                favs_borrar();
            } else if (strcmp(subcmd, "ejecutar") == 0) {
                char *num_str = strtok(NULL, " ");
                if (num_str) {
                    int num = atoi(num_str);
                    favs_ejecutar(num);
                } else {
                    printf("Número para ejecutar no proporcionado.\n");
                }
            } else if (strcmp(subcmd, "cargar") == 0) {
                favs_cargar();
            } else if (strcmp(subcmd, "guardar") == 0) {
                favs_guardar();
            } else if (strcmp(subcmd, "cambiar") == 0) {
                char *new_path = strtok(NULL, " ");
                if (new_path) {
                    change_fav_file_path(new_path);
                } else {
                    printf("Nueva ruta no proporcionada.\n");
                }
            } else {
                printf("Comando `favs` desconocido: %s\n", subcmd);
            }
        } else {
            printf("Comando `favs` incompleto.\n");
        }
    } else if (strncmp(line, "set recordatorio", 16) == 0) {
        int seconds = atoi(strtok(line + 16, " "));
        char *message = strtok(NULL, "");
        if (message) {
            set_recordatorio(seconds, message);
        } else {
            printf("Mensaje para el recordatorio no proporcionado.\n");
        }
    } else {
        if (check_and_create_fav_file() == -1) {
            perror("Error creando archivo de favoritos");
        }

        parse_command(line, args);

        if (args[0] && strcmp(args[0], "exit") == 0) {
            favs_guardar();
            favs_borrar();
            exit(EXIT_SUCCESS);
        }

        add_fav(line);

        execute_command(args);
    }
}

void parse_command(char *line, char **args) {
    int position = 0;
    char *token = strtok(line, " ");
    while (token != NULL && position < 99) {  // Limita el número de argumentos para evitar desbordamientos
        args[position++] = token;
        token = strtok(NULL, " ");
    }
    args[position] = NULL;
}

void execute_command(char **args) {
    pid_t pid, wpid;
    int status;

    pid = fork();
    if (pid == 0) {
        if (execvp(args[0], args) == -1) {
            perror("Error ejecutando comando");
        }
        exit(EXIT_FAILURE);
    } else if (pid < 0) {
        perror("Error en fork");
    } else {
        do {
            wpid = waitpid(pid, &status, WUNTRACED);
        } while (!WIFEXITED(status) && !WIFSIGNALED(status));
    }
}

void handle_pipe(char *line) {
    char *commands[100];
    char *args[100][10];
    int num_commands = 0;
    int pipefd[100][2];

    char *token = strtok(line, "|");
    while (token != NULL && num_commands < 100) {
        commands[num_commands++] = token;
        token = strtok(NULL, "|");
    }

    for (int i = 0; i < num_commands; i++) {
        parse_command(commands[i], args[i]);
    }

    for (int i = 0; i < num_commands - 1; i++) {
        if (pipe(pipefd[i]) == -1) {
            perror("Error creando pipe");
            exit(EXIT_FAILURE);
        }
    }

    // Añadir cada comando a favoritos antes de ejecutar
    for (int i = 0; i < num_commands; i++) {
        add_fav(commands[i]);
    }

    for (int i = 0; i < num_commands; i++) {
        pid_t pid = fork();

        if (pid == 0) {
            if (i > 0) {
                dup2(pipefd[i - 1][0], STDIN_FILENO);
            }
            if (i < num_commands - 1) {
                dup2(pipefd[i][1], STDOUT_FILENO);
            }

            for (int j = 0; j < num_commands - 1; j++) {
                close(pipefd[j][0]);
                close(pipefd[j][1]);
            }

            if (execvp(args[i][0], args[i]) == -1) {
                perror("Error ejecutando comando");
            }
            exit(EXIT_FAILURE);
        } else if (pid < 0) {
            perror("Error en fork");
            exit(EXIT_FAILURE);
        }
    }

    for (int i = 0; i < num_commands - 1; i++) {
        close(pipefd[i][0]);
        close(pipefd[i][1]);
    }

    for (int i = 0; i < num_commands; i++) {
        wait(NULL);
    }

    // Guardar favoritos después de ejecutar la tubería
    favs_guardar();
}

void favs_crear(char *filepath) {
    favs_guardar();
    change_fav_file_path(filepath);
    favs_cargar();
}

void favs_mostrar() {
    if (fav_list.count == 0) {
        printf("No hay comandos favoritos guardados.\n");
    } else {
        printf("Favoritos:\n");
        for (int i = 0; i < fav_list.count; i++) {
            printf("%d: %s\n", i + 1, fav_list.commands[i]);
        }
    }
}

void favs_eliminar(char *nums) {
    int num = atoi(nums);
    if (num < 1 || num > fav_list.count) {
        printf("Número inválido.\n");
        return;
    }
    free(fav_list.commands[num - 1]);
    for (int i = num - 1; i < fav_list.count - 1; i++) {
        fav_list.commands[i] = fav_list.commands[i + 1];
    }
    fav_list.count--;
    printf("Favorito %d eliminado.\n", num);
}

void favs_buscar(char *cmd) {
    int found = 0;
    for (int i = 0; i < fav_list.count; i++) {
        if (strstr(fav_list.commands[i], cmd)) {
            printf("Encontrado en favoritos [%d]: %s\n", i + 1, fav_list.commands[i]);
            found = 1;
        }
    }
    if (!found) {
        printf("Comando '%s' no encontrado en favoritos.\n", cmd);
    }
}

void favs_borrar() {
    for (int i = 0; i < fav_list.count; i++) {
        free(fav_list.commands[i]);
    }
    free(fav_list.commands);
    fav_list.commands = NULL;
    fav_list.count = 0;
    fav_list.capacity = 10;
    fav_list.commands = malloc(fav_list.capacity * sizeof(char *));
    if (!fav_list.commands) {
        perror("Error al reasignar memoria para favoritos");
        exit(EXIT_FAILURE);
    }
    printf("Todos los favoritos han sido eliminados.\n");
}

void favs_ejecutar(int num) {
    if (num < 1 || num > fav_list.count) {
        printf("Número de favorito inválido.\n");
        return;
    }
    char *cmd = fav_list.commands[num - 1];
    char *args[100];
    parse_command(cmd, args);
    execute_command(args);
}

void favs_cargar() {
    FILE *file = fopen(fav_file_path, "r");
    if (file) {
        char line[256];
        while (fgets(line, sizeof(line), file)) {
            line[strcspn(line, "\n")] = 0;

            int found = 0;
            for (int i = 0; i < fav_list.count; i++) {
                if (strcmp(fav_list.commands[i], line) == 0) {
                    found = 1;
                    break;
                }
            }

            if (!found) {
                if (fav_list.count >= fav_list.capacity) {
                    fav_list.capacity *= 2;
                    fav_list.commands = realloc(fav_list.commands, fav_list.capacity * sizeof(char *));
                    if (!fav_list.commands) {
                        perror("Error al reasignar memoria para favoritos");
                        exit(EXIT_FAILURE);
                    }
                }
                fav_list.commands[fav_list.count] = strdup(line);
                if (!fav_list.commands[fav_list.count]) {
                    perror("Error al duplicar cadena de favorito");
                    exit(EXIT_FAILURE);
                }
                fav_list.count++;
            }
        }
        fclose(file);
        printf("Favoritos cargados desde archivo: %s\n", fav_file_path);
    } else {
        printf("No se puede abrir el archivo de favoritos.\n");
    }
}

void favs_guardar() {
    FILE *file = fopen(fav_file_path, "w");
    if (!file) {
        perror("Error abriendo archivo para guardar favoritos");
        return;
    }

    for (int i = 0; i < fav_list.count; i++) {
        fprintf(file, "%s\n", fav_list.commands[i]);
    }

    fclose(file);
    printf("Favoritos guardados en archivo: %s\n", fav_file_path);
}

void add_fav(char *cmd) {
    for (int i = 0; i < fav_list.count; i++) {
        if (strcmp(fav_list.commands[i], cmd) == 0) {
            return;
        }
    }

    if (fav_list.count >= fav_list.capacity) {
        fav_list.capacity *= 2;
        fav_list.commands = realloc(fav_list.commands, fav_list.capacity * sizeof(char *));
        if (!fav_list.commands) {
            perror("Error al reasignar memoria para favoritos");
            exit(EXIT_FAILURE);
        }
    }

    fav_list.commands[fav_list.count] = strdup(cmd);
    if (!fav_list.commands[fav_list.count]) {
        perror("Error al duplicar cadena de favorito");
        exit(EXIT_FAILURE);
    }
    fav_list.count++;
    printf("Comando añadido a favoritos (en memoria): %s\n", cmd);
}

void change_fav_file_path(char *new_path) {
    strncpy(fav_file_path, new_path, sizeof(fav_file_path) - 1);
    fav_file_path[sizeof(fav_file_path) - 1] = '\0';
    printf("Ruta del archivo de favoritos cambiada a: %s\n", fav_file_path);
}

int check_and_create_fav_file() {
    FILE *file = fopen(fav_file_path, "a");
    if (file) {
        fclose(file);
        return 0;
    }
    return -1;
}

int create_directory_if_not_exists(const char *dir_path) {
    struct stat st = {0};
    if (stat(dir_path, &st) == -1) {
        if (mkdir(dir_path, 0700) == -1) {
            return -1;
        }
    }
    return 0;
}

void set_recordatorio(int seconds, char *message) {
    pid_t pid = fork();

    if (pid == 0) {
        sleep(seconds);
        printf("Recordatorio: %s\n", message);
        exit(0);
    } else if (pid < 0) {
        perror("Error creando proceso para recordatorio");
    }
}
