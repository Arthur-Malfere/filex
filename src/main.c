#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libgen.h>
#include "file_explorer.h"
#include "ui.h"

// Fonction pour obtenir le dossier parent
static void get_parent_directory(const char* path, char* parent) {
    char temp[MAX_PATH_LENGTH];
    strncpy(temp, path, sizeof(temp) - 1);
    temp[sizeof(temp) - 1] = '\0';
    
    char* dir = dirname(temp);
    strncpy(parent, dir, MAX_PATH_LENGTH - 1);
    parent[MAX_PATH_LENGTH - 1] = '\0';
}

// Fonction pour charger le contenu d'un répertoire
static bool load_directory(const char* path, FileList* files) {
    file_list_clear(files);
    
    if (!explore_directory_shallow(path, files)) {
        return false;
    }
    
    file_list_sort(files);
    return true;
}

int main(int argc, char** argv) {
    char current_path[MAX_PATH_LENGTH];
    
    // Déterminer le chemin de départ
    if (argc > 1) {
        // Utiliser le chemin fourni en argument
        strncpy(current_path, argv[1], sizeof(current_path) - 1);
        current_path[sizeof(current_path) - 1] = '\0';
    } else {
        // Utiliser le répertoire courant
        if (getcwd(current_path, sizeof(current_path)) == NULL) {
            fprintf(stderr, "Erreur: impossible d'obtenir le répertoire courant\n");
            return 1;
        }
    }
    
    printf("Dossier initial: %s\n", current_path);
    
    // Créer la liste de fichiers
    FileList* files = file_list_create();
    if (!files) {
        fprintf(stderr, "Erreur: impossible de créer la liste de fichiers\n");
        return 1;
    }
    
    // Charger le contenu initial
    if (!load_directory(current_path, files)) {
        fprintf(stderr, "Erreur lors du chargement du répertoire\n");
        file_list_destroy(files);
        return 1;
    }
    
    printf("Fichiers trouvés: %d\n", files->count);
    
    // Initialiser l'interface utilisateur
    UIState* ui = ui_init(1200, 800, "FileX - Explorateur de Fichiers");
    if (!ui) {
        fprintf(stderr, "Erreur: impossible d'initialiser l'interface\n");
        file_list_destroy(files);
        return 1;
    }
    
    char previous_search[256] = "";
    
    // Boucle principale
    while (!ui_should_close()) {
        ui_render(ui, files, current_path);
        
        // Gérer la recherche récursive
        const char* search_text = ui_get_search_text(ui);
        if (search_text[0] != '\0' && strcmp(search_text, previous_search) != 0) {
            // Nouvelle recherche
            printf("Recherche récursive de '%s' dans %s...\n", search_text, current_path);
            file_list_clear(files);
            bool limit_reached = !search_files_recursive(current_path, search_text, files, 0);
            file_list_sort(files);
            ui_set_searching(ui, true);
            ui_set_search_limit_reached(ui, limit_reached);
            strncpy(previous_search, search_text, sizeof(previous_search) - 1);
            previous_search[sizeof(previous_search) - 1] = '\0';
            if (limit_reached) {
                printf("Limite de %d resultats atteinte\n", files->count);
            } else {
                printf("Resultats: %d\n", files->count);
            }
        } else if (search_text[0] == '\0' && previous_search[0] != '\0') {
            // Recherche annulée, recharger le dossier
            printf("Recherche annulee\n");
            load_directory(current_path, files);
            ui_set_searching(ui, false);
            ui_set_search_limit_reached(ui, false);
            previous_search[0] = '\0';
        }
        
        // Vérifier si un dossier a été cliqué
        char* clicked_path = ui_get_clicked_path(ui);
        if (clicked_path) {
            printf("Navigation vers: %s\n", clicked_path);
            strncpy(current_path, clicked_path, sizeof(current_path) - 1);
            current_path[sizeof(current_path) - 1] = '\0';
            free(clicked_path);
            
            // Recharger le contenu et annuler la recherche
            if (!load_directory(current_path, files)) {
                fprintf(stderr, "Erreur lors du chargement du répertoire\n");
            }
            ui_set_searching(ui, false);
            ui_set_search_limit_reached(ui, false);
            previous_search[0] = '\0';
        }
        
        // Vérifier si le bouton retour a été cliqué
        if (ui_should_go_back(ui)) {
            char parent[MAX_PATH_LENGTH];
            get_parent_directory(current_path, parent);
            
            // Vérifier qu'on ne remonte pas au-dessus de la racine
            if (strcmp(parent, current_path) != 0) {
                printf("Retour vers: %s\n", parent);
                strncpy(current_path, parent, sizeof(current_path) - 1);
                current_path[sizeof(current_path) - 1] = '\0';
                
                // Recharger le contenu et annuler la recherche
                if (!load_directory(current_path, files)) {
                    fprintf(stderr, "Erreur lors du chargement du répertoire\n");
                }
                ui_set_searching(ui, false);
                ui_set_search_limit_reached(ui, false);
                previous_search[0] = '\0';
            }
        }
    }
    
    // Nettoyage
    ui_destroy(ui);
    file_list_destroy(files);
    
    return 0;
}
