#include <iostream>
#include <string>
#include <cstdio>
#include <vector>
#include <sstream>


using namespace std;

vector<string> separarPalubrias(const string& input){
    istringstream linea_entrada(input);
    vector<string> comandos;
    string comando;
    while (linea_entrada >> comando) {
        comandos.push_back(comando);
    }
    return comandos;
}


int main(){
     string input;

    while (true){
        //Imprime el prompt
        cout << "uwu:$: ";
        getline(cin, input);


        //Cuando se apreta Enter
        if(input.empty()){
            continue;
        }
        //si el input es exit para salir
        if(input == "exit"){
            break;
        }

        vector<string> comandos;
        size_t pos = 0;
        string comando;

        while ((pos = input.find('|')) != string::npos) {
            comando = input.substr(0, pos);
            comandos.push_back(comando);
            input.erase(0, pos + 1);
        }
        comandos.push_back(input);

        // Imprimir los comandos separados por | TESTING
        cout << "Comandos extraídos:" << endl;
        for (size_t i = 0; i < comandos.size(); ++i) {
            cout << "Comando " << (i + 1) << ": '" << comandos[i] << "'" << endl;
        }
       



    }
    return 0;
    
}
