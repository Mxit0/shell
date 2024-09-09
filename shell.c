#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/types.h>

#define DEFAULT_FAV_FILE "misfavoritos.txt"
#define MAX_FAVS 100

// Variable global para almacenar la ruta del archivo de favoritos
char fav_file_path[256] = DEFAULT_FAV_FILE;

// Estructura para almacenar comandos favoritos en memoria
typedef struct {
    char *commands[MAX_FAVS];
    int count;
} FavList;

FavList fav_list = { .count = 0 };

// Prototipos de funciones
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

    // Cargar favoritos del archivo al iniciar la shell
    favs_cargar();

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
    } else if (strncmp(line, "favs", 4) == 0) {
        // Manejar comando `favs`
        char *cmd = strtok(line, " "); // Esto debería ser "favs"
        char *subcmd = strtok(NULL, " "); // Esto debería ser el subcomando (crear, mostrar, etc.)

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
        // Verificar y crear el archivo de favoritos si no existe
        if (check_and_create_fav_file() == -1) {
            perror("Error creando archivo de favoritos");
        }

        parse_command(line, args);

        // Comando `exit` para salir
        if (strcmp(args[0], "exit") == 0) {
            // Guardar favoritos antes de salir
            favs_guardar();
            exit(EXIT_SUCCESS);
        }

        add_fav(line); // Agregar comando actual a favoritos si no está ya

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
            // Si no es el primer comando, redirige la entrada desde el pipe anterior
            if (i > 0) {
                dup2(pipefd[i - 1][0], STDIN_FILENO);
            }
            // Si no es el último comando, redirige la salida al pipe siguiente
            if (i < num_commands - 1) {
                dup2(pipefd[i][1], STDOUT_FILENO);
            }

            // Cierra todos los pipes
            for (int j = 0; j < num_commands - 1; j++) {
                close(pipefd[j][0]);
                close(pipefd[j][1]);
            }

            // Ejecuta el comando
            if (execvp(args[i][0], args[i]) == -1) {
                perror("Error ejecutando comando");
            }
            exit(EXIT_FAILURE);
        } else if (pid < 0) { // Error en fork
            perror("Error en fork");
            exit(EXIT_FAILURE);
        }
    }

    // Cierra todos los pipes en el proceso padre
    for (int i = 0; i < num_commands - 1; i++) {
        close(pipefd[i][0]);
        close(pipefd[i][1]);
    }

    // Espera a que todos los procesos hijos terminen
    for (int i = 0; i < num_commands; i++) {
        wait(NULL);
    }
}

// Crear el archivo de favoritos
void favs_crear(char *filepath) {
    favs_guardar();  // Guardar el estado actual antes de cambiar la ruta
    change_fav_file_path(filepath);
    favs_guardar();  // Guardar los favoritos en el nuevo archivo
}

// Mostrar los comandos favoritos
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

// Eliminar un comando de favoritos por su número
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

// Buscar un comando en la lista de favoritos
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

// Borrar todos los favoritos
void favs_borrar() {
    for (int i = 0; i < fav_list.count; i++) {
        free(fav_list.commands[i]);
    }
    fav_list.count = 0;
    printf("Todos los favoritos han sido eliminados.\n");
}

// Ejecutar un comando favorito por su número
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

// Cargar favoritos desde el archivo, evitando duplicados en fav_list
void favs_cargar() {
    FILE *file = fopen(fav_file_path, "r");
    if (file) {
        char line[256];
        while (fgets(line, sizeof(line), file)) {
            line[strcspn(line, "\n")] = 0;  // Eliminar salto de línea

            // Verificar si el comando ya está en la lista de favoritos (en memoria)
            int found = 0;
            for (int i = 0; i < fav_list.count; i++) {
                if (strcmp(fav_list.commands[i], line) == 0) {
                    found = 1;  // El comando ya está en la lista de favoritos
                    break;
                }
            }

            // Si el comando no está en fav_list, lo añadimos
            if (!found && fav_list.count < MAX_FAVS) {
                fav_list.commands[fav_list.count] = strdup(line);
                fav_list.count++;
            }
        }
        fclose(file);
        printf("Favoritos cargados desde archivo: %s\n", fav_file_path);
    } else {
        printf("No se puede abrir el archivo de favoritos.\n");
    }
}


// Guardar los favoritos en el archivo, evitando duplicados
void favs_guardar() {
    FILE *file = fopen(fav_file_path, "r+");
    if (!file) {
        perror("Error abriendo archivo para guardar favoritos");
        return;
    }

    // Leer el contenido actual del archivo y almacenarlo en un arreglo temporal
    char line[256];
    char *existing_favs[MAX_FAVS];
    int existing_count = 0;

    while (fgets(line, sizeof(line), file)) {
        line[strcspn(line, "\n")] = 0;  // Eliminar salto de línea
        existing_favs[existing_count] = strdup(line);
        existing_count++;
    }

    // Verificar si el comando ya está en el archivo antes de escribir
    for (int i = 0; i < fav_list.count; i++) {
        int found = 0;
        for (int j = 0; j < existing_count; j++) {
            if (strcmp(fav_list.commands[i], existing_favs[j]) == 0) {
                found = 1;  // El comando ya está en el archivo
                break;
            }
        }

        if (!found) {
            // El comando no está en el archivo, escribirlo
            fprintf(file, "%s\n", fav_list.commands[i]);
        }
    }

    // Liberar la memoria de los comandos leídos
    for (int i = 0; i < existing_count; i++) {
        free(existing_favs[i]);
    }

    fclose(file);
    printf("Favoritos guardados en archivo: %s\n", fav_file_path);
}


// Agregar un comando a la lista de favoritos
void add_fav(char *cmd) {
    // Verificar si el comando ya está en favoritos (en memoria)
    for (int i = 0; i < fav_list.count; i++) {
        if (strcmp(fav_list.commands[i], cmd) == 0) {
            return; // El comando ya está en la lista de favoritos, no lo añadimos
        }
    }

    // Si no está en la lista de favoritos, lo agregamos
    if (fav_list.count < MAX_FAVS) {
        fav_list.commands[fav_list.count] = strdup(cmd);  // Copiar el comando a la lista
        fav_list.count++;
        printf("Comando añadido a favoritos (en memoria): %s\n", cmd);
    } else {
        printf("No se puede añadir el comando a favoritos, la lista está llena.\n");
    }
}

// Cambiar la ruta del archivo de favoritos
void change_fav_file_path(char *new_path) {
    strncpy(fav_file_path, new_path, sizeof(fav_file_path) - 1);
    fav_file_path[sizeof(fav_file_path) - 1] = '\0'; // Asegurar que la ruta termine en '\0'
    printf("Ruta del archivo de favoritos cambiada a: %s\n", fav_file_path);
}

// Verificar y crear el archivo de favoritos si no existe
int check_and_create_fav_file() {
    FILE *file = fopen(fav_file_path, "a");
    if (file) {
        fclose(file);
        return 0; // El archivo existe o fue creado
    }
    return -1; // Error al crear o abrir el archivo
}

// Crear un directorio si no existe
int create_directory_if_not_exists(const char *dir_path) {
    struct stat st = {0};
    if (stat(dir_path, &st) == -1) {
        if (mkdir(dir_path, 0700) == -1) {
            return -1; // Error al crear directorio
        }
    }
    return 0; // Directorio creado o ya existe
}

// Función para establecer un recordatorio en X segundos
void set_recordatorio(int seconds, char *message) {
    pid_t pid = fork();

    if (pid == 0) { // Proceso hijo
        sleep(seconds);
        printf("Recordatorio: %s\n", message);
        exit(0);
    } else if (pid < 0) {
        perror("Error creando proceso para recordatorio");
    }
}
