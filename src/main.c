#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "file_explorer.h"
#include "ui.h"

int main(int argc, char** argv) {
    char start_path[MAX_PATH_LENGTH];
    
    // Déterminer le chemin de départ
    if (argc > 1) {
        // Utiliser le chemin fourni en argument
        strncpy(start_path, argv[1], sizeof(start_path) - 1);
        start_path[sizeof(start_path) - 1] = '\0';
    } else {
        // Utiliser le répertoire courant
        if (getcwd(start_path, sizeof(start_path)) == NULL) {
            fprintf(stderr, "Erreur: impossible d'obtenir le répertoire courant\n");
            return 1;
        }
    }
    
    printf("Exploration de: %s\n", start_path);
    
    // Créer la liste de fichiers
    FileList* files = file_list_create();
    if (!files) {
        fprintf(stderr, "Erreur: impossible de créer la liste de fichiers\n");
        return 1;
    }
    
    // Explorer le répertoire de manière récursive
    if (!explore_directory(start_path, files, 0)) {
        fprintf(stderr, "Erreur lors de l'exploration du répertoire\n");
        file_list_destroy(files);
        return 1;
    }
    
    // Trier la liste
    file_list_sort(files);
    
    printf("Fichiers trouvés: %d\n", files->count);
    
    // Initialiser l'interface utilisateur
    UIState* ui = ui_init(1200, 800, "FileX - Explorateur de Fichiers");
    if (!ui) {
        fprintf(stderr, "Erreur: impossible d'initialiser l'interface\n");
        file_list_destroy(files);
        return 1;
    }
    
    // Boucle principale
    while (!ui_should_close()) {
        ui_render(ui, files, start_path);
    }
    
    // Nettoyage
    ui_destroy(ui);
    file_list_destroy(files);
    
    return 0;
}
