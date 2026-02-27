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
static bool load_directory(const char* path, FileList* files, bool show_hidden, DirectoryCache* cache) {
    // Vérifier le cache d'abord
    FileList* cached = cache_get(cache, path, show_hidden);
    if (cached) {
        printf("Cache hit pour %s\n", path);
        file_list_clear(files);
        // Copier les entrées du cache
        for (int i = 0; i < cached->count && i < MAX_FILES; i++) {
            if (files->count >= files->capacity) {
                int new_capacity = files->capacity * 2;
                if (new_capacity > MAX_FILES) new_capacity = MAX_FILES;
                FileEntry* new_entries = (FileEntry*)realloc(files->entries, sizeof(FileEntry) * new_capacity);
                if (!new_entries) break;
                files->entries = new_entries;
                files->capacity = new_capacity;
            }
            files->entries[files->count++] = cached->entries[i];
        }
        return true;
    }
    
    // Pas dans le cache, charger depuis le disque
    printf("Cache miss pour %s\n", path);
    file_list_clear(files);
    
    if (!explore_directory_shallow(path, files, show_hidden)) {
        return false;
    }
    
    file_list_sort(files);
    
    // Ajouter au cache (créer une copie)
    FileList* to_cache = file_list_create();
    if (to_cache) {
        for (int i = 0; i < files->count; i++) {
            if (to_cache->count >= to_cache->capacity) {
                int new_capacity = to_cache->capacity * 2;
                if (new_capacity > MAX_FILES) new_capacity = MAX_FILES;
                FileEntry* new_entries = (FileEntry*)realloc(to_cache->entries, sizeof(FileEntry) * new_capacity);
                if (!new_entries) break;
                to_cache->entries = new_entries;
                to_cache->capacity = new_capacity;
            }
            to_cache->entries[to_cache->count++] = files->entries[i];
        }
        cache_put(cache, path, to_cache, show_hidden);
    }
    
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
    
    // Créer le cache
    DirectoryCache* cache = cache_create();
    if (!cache) {
        fprintf(stderr, "Erreur: impossible de créer le cache\n");
        return 1;
    }
    
    // Créer la recherche asynchrone
    AsyncSearch* async_search = async_search_create();
    if (!async_search) {
        fprintf(stderr, "Erreur: impossible de créer la recherche asynchrone\n");
        cache_destroy(cache);
        return 1;
    }
    
    // Créer la liste de fichiers
    FileList* files = file_list_create();
    if (!files) {
        fprintf(stderr, "Erreur: impossible de créer la liste de fichiers\n");
        async_search_destroy(async_search);
        cache_destroy(cache);
        return 1;
    }
    
    // Charger le contenu initial
    if (!load_directory(current_path, files, false, cache)) {
        fprintf(stderr, "Erreur lors du chargement du répertoire\n");
        file_list_destroy(files);
        async_search_destroy(async_search);
        cache_destroy(cache);
        return 1;
    }
    
    printf("Fichiers trouvés: %d\n", files->count);
    
    // Initialiser l'interface utilisateur
    UIState* ui = ui_init(1200, 800, "FileX - Explorateur de Fichiers");
    if (!ui) {
        fprintf(stderr, "Erreur: impossible d'initialiser l'interface\n");
        file_list_destroy(files);
        async_search_destroy(async_search);
        cache_destroy(cache);
        return 1;
    }
    
    char previous_search[256] = "";
    bool prev_show_hidden = false;
    bool prev_search_by_content = false;
    char last_message[256] = "";
    bool search_in_progress = false;
    
    // Boucle principale
    while (!ui_should_close()) {
        ui_render(ui, files, current_path);

        bool current_show_hidden = ui_get_show_hidden(ui);
        bool current_search_by_content = ui_get_search_by_content(ui);

        // Mettre à jour les statistiques de recherche si en cours
        if (search_in_progress) {
            SearchStatus status = async_search_status(async_search);
            
            if (status == SEARCH_RUNNING) {
                // Obtenir les statistiques de progression
                int files_scanned, dirs_scanned, files_matched;
                double elapsed_time;
                async_search_get_progress(async_search, &files_scanned, &dirs_scanned, &files_matched, &elapsed_time);
                ui_set_search_stats(ui, files_scanned, dirs_scanned, files_matched, elapsed_time);
                
                // Récupérer et afficher les résultats intermédiaires
                FileList* peek = async_search_peek_results(async_search);
                if (peek && peek->count > files->count) {
                    // Copier les nouveaux résultats
                    file_list_destroy(files);
                    files = peek;
                }
            } else if (status == SEARCH_COMPLETED) {
                // Recherche terminée
                bool limit_reached;
                FileList* results = async_search_get_results(async_search, &limit_reached);
                if (results) {
                    file_list_destroy(files);
                    files = results;
                    ui_set_search_limit_reached(ui, limit_reached);
                    
                    // Obtenir les statistiques finales
                    int files_scanned, dirs_scanned, files_matched;
                    double elapsed_time;
                    async_search_get_progress(async_search, &files_scanned, &dirs_scanned, &files_matched, &elapsed_time);
                    ui_set_search_stats(ui, files_scanned, dirs_scanned, files_matched, elapsed_time);
                    
                    printf("Recherche terminee: %d resultats en %.1fs\n", files->count, elapsed_time);
                    printf("Fichiers scannes: %d, Dossiers: %d\n", files_scanned, dirs_scanned);
                    if (limit_reached) {
                        printf("Limite de resultats atteinte\n");
                    }
                }
                search_in_progress = false;
            }
        }

        // Création de fichiers/dossiers
        if (ui_creation_confirmed(ui)) {
            const char* name = ui_get_creation_name(ui);
            CreateType type = ui_get_creation_type(ui);
            bool ok = false;
            if (type == CREATE_DIRECTORY) {
                ok = create_directory(current_path, name);
            } else if (type == CREATE_FILE) {
                ok = create_file(current_path, name);
            }
            if (ok) {
                snprintf(last_message, sizeof(last_message), "Creé: %s", name);
                // Recharger la vue courante
                if (ui_is_searching(ui) && ui_get_search_text(ui)[0] != '\0') {
                    // Relancer la recherche asynchrone
                    async_search_start(async_search, current_path, ui_get_search_text(ui), current_search_by_content, current_show_hidden);
                    search_in_progress = true;
                } else {
                    load_directory(current_path, files, current_show_hidden, cache);
                }
            } else {
                snprintf(last_message, sizeof(last_message), "Echec creation: %s", name);
            }
            ui_clear_creation_request(ui);
        }
        
        // Gérer la recherche récursive
        const char* search_text = ui_get_search_text(ui);
        bool search_params_changed = (strcmp(search_text, previous_search) != 0) || 
                                     (current_search_by_content != prev_search_by_content);
        
        if (search_text[0] != '\0' && search_params_changed) {
            // Nouvelle recherche - annuler l'ancienne si en cours
            if (search_in_progress) {
                async_search_cancel(async_search);
                search_in_progress = false;
            }
            
            // Démarrer une nouvelle recherche asynchrone
            printf("Recherche %s de '%s' dans %s...\n", 
                   current_search_by_content ? "par contenu" : "par nom",
                   search_text, current_path);
            
            async_search_start(async_search, current_path, search_text, current_search_by_content, current_show_hidden);
            search_in_progress = true;
            ui_set_searching(ui, true);
            
            strncpy(previous_search, search_text, sizeof(previous_search) - 1);
            previous_search[sizeof(previous_search) - 1] = '\0';
            prev_search_by_content = current_search_by_content;
        } else if (search_text[0] == '\0' && previous_search[0] != '\0') {
            // Recherche annulée
            if (search_in_progress) {
                async_search_cancel(async_search);
                search_in_progress = false;
            }
            printf("Recherche annulee\n");
            load_directory(current_path, files, current_show_hidden, cache);
            ui_set_searching(ui, false);
            ui_set_search_limit_reached(ui, false);
            previous_search[0] = '\0';
        }

        // Recharger si l'option d'affichage des fichiers cachés a changé
        if (current_show_hidden != prev_show_hidden) {
            if (search_text[0] != '\0' && !search_in_progress) {
                // Relancer la recherche avec le nouveau paramètre
                async_search_start(async_search, current_path, search_text, current_search_by_content, current_show_hidden);
                search_in_progress = true;
                ui_set_searching(ui, true);
            } else if (search_text[0] == '\0') {
                load_directory(current_path, files, current_show_hidden, cache);
                ui_set_searching(ui, false);
                ui_set_search_limit_reached(ui, false);
            }
            prev_show_hidden = current_show_hidden;
        }
        
        // Vérifier si un dossier a été cliqué
        char* clicked_path = ui_get_clicked_path(ui);
        if (clicked_path) {
            printf("Navigation vers: %s\n", clicked_path);
            
            // Annuler la recherche en cours
            if (search_in_progress) {
                async_search_cancel(async_search);
                search_in_progress = false;
            }
            
            strncpy(current_path, clicked_path, sizeof(current_path) - 1);
            current_path[sizeof(current_path) - 1] = '\0';
            free(clicked_path);
            
            // Recharger le contenu et annuler la recherche
            if (!load_directory(current_path, files, current_show_hidden, cache)) {
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
                
                // Annuler la recherche en cours
                if (search_in_progress) {
                    async_search_cancel(async_search);
                    search_in_progress = false;
                }
                
                strncpy(current_path, parent, sizeof(current_path) - 1);
                current_path[sizeof(current_path) - 1] = '\0';
                
                // Recharger le contenu et annuler la recherche
                if (!load_directory(current_path, files, current_show_hidden, cache)) {
                    fprintf(stderr, "Erreur lors du chargement du répertoire\n");
                }
                ui_set_searching(ui, false);
                ui_set_search_limit_reached(ui, false);
                previous_search[0] = '\0';
            }
        }
    }
    
    // Nettoyage
    async_search_destroy(async_search);
    cache_destroy(cache);
    ui_destroy(ui);
    file_list_destroy(files);
    
    return 0;
}
