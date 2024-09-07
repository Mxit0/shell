#include <iostream>
#include <string>
#include <vector>
#include <unistd.h>
#include <sys/wait.h>
#include <sstream>
#include <cstring>

// Prototipos de funciones
void shell_loop();                   // Bucle principal de la shell
void process_command(std::string line); // Procesa el comando ingresado
void parse_command(const std::string& line, std::vector<char*>& args); // Parsea la línea de entrada
void execute_command(std::vector<char*>& args); // Ejecuta el comando
void handle_pipe(const std::string& line);      // Maneja comandos con pipes

int main() {
    std::cout << "Bienvenido a nuestra Shell. Escribe 'exit' para salir." << std::endl;
    shell_loop();
    return 0;
}

// Función que ejecuta el bucle de la shell
void shell_loop() {
    std::string line;

    while (true) {
        std::cout << "shell:$ ";
        std::getline(std::cin, line); // Leer la línea desde el teclado

        if (line.empty()) {
            continue; // Si solo presiona enter, mostrar nuevamente el prompt
        }

        process_command(line); // Procesar el comando
    }
}

// Procesa el comando ingresado, manejando pipes o ejecutando el comando normal
void process_command(std::string line) {
    std::vector<char*> args;

    if (line.find('|') != std::string::npos) {
        handle_pipe(line); // Manejar comando con pipes
    } else {
        parse_command(line, args);

        // Comando `exit` para salir
        if (args[0] && strcmp(args[0], "exit") == 0) {
            exit(EXIT_SUCCESS);
        }

        execute_command(args);
    }
}

// Parsea el comando ingresado para obtener el comando y sus argumentos
void parse_command(const std::string& line, std::vector<char*>& args) {
    std::stringstream ss(line);
    std::string token;
    while (ss >> token) {
        char* arg = new char[token.size() + 1];
        std::strcpy(arg, token.c_str());
        args.push_back(arg);
    }
    args.push_back(nullptr); // Indicar el fin de los argumentos
}

// Ejecuta el comando ingresado utilizando fork() y execvp()
void execute_command(std::vector<char*>& args) {
    pid_t pid = fork();
    if (pid == 0) { // Proceso hijo
        if (execvp(args[0], args.data()) == -1) {
            perror("Error ejecutando comando");
        }
        exit(EXIT_FAILURE);
    } else if (pid < 0) { // Error en fork
        perror("Error en fork");
    } else { // Proceso padre
        int status;
        do {
            waitpid(pid, &status, WUNTRACED);
        } while (!WIFEXITED(status) && !WIFSIGNALED(status));
    }

    // Liberar memoria asignada para los argumentos
    for (char* arg : args) {
        delete[] arg;
    }
}

// Maneja el pipe entre dos comandos
void handle_pipe(const std::string& line) {
    std::vector<char*> cmd1, cmd2;
    size_t pipe_pos = line.find('|');
    std::string command1 = line.substr(0, pipe_pos);
    std::string command2 = line.substr(pipe_pos + 1);

    parse_command(command1, cmd1);
    parse_command(command2, cmd2);

    int pipefd[2];
    if (pipe(pipefd) == -1) {
        perror("Error creando pipe");
        exit(EXIT_FAILURE);
    }

    pid_t pid1 = fork();
    if (pid1 == 0) { // Primer proceso
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[0]);
        close(pipefd[1]);
        if (execvp(cmd1[0], cmd1.data()) == -1) {
            perror("Error ejecutando primer comando");
        }
        exit(EXIT_FAILURE);
    }

    pid_t pid2 = fork();
    if (pid2 == 0) { // Segundo proceso
        dup2(pipefd[0], STDIN_FILENO);
        close(pipefd[1]);
        close(pipefd[0]);
        if (execvp(cmd2[0], cmd2.data()) == -1) {
            perror("Error ejecutando segundo comando");
        }
        exit(EXIT_FAILURE);
    }

    close(pipefd[0]);
    close(pipefd[1]);
    wait(NULL);
    wait(NULL);

    // Liberar memoria asignada para los argumentos
    for (char* arg : cmd1) {
        delete[] arg;
    }
    for (char* arg : cmd2) {
        delete[] arg;
    }
}
