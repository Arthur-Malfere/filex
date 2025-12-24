#ifndef UI_H
#define UI_H

#include "file_explorer.h"
#include <raylib.h>

typedef struct {
    int scroll_offset;
    int selected_index;
    int window_width;
    int window_height;
    Font font;
    bool initialized;
    char* clicked_path;  // Chemin cliqué pour navigation
    bool go_back;        // Demande de retour au dossier parent
    char search_text[256]; // Texte de recherche
    bool search_active;    // Si la barre de recherche est active
    bool is_searching;     // Si on affiche des résultats de recherche récursive
} UIState;

// Initialise l'interface utilisateur
UIState* ui_init(int width, int height, const char* title);

// Libère les ressources de l'UI
void ui_destroy(UIState* state);

// Affiche la liste des fichiers et retourne le chemin cliqué ou NULL
void ui_render(UIState* state, FileList* files, const char* current_path);

// Récupère et réinitialise le chemin cliqué
char* ui_get_clicked_path(UIState* state);

// Vérifie si le bouton retour a été cliqué
bool ui_should_go_back(UIState* state);

// Vérifie si on est en mode recherche
bool ui_is_searching(UIState* state);

// Récupère le texte de recherche
const char* ui_get_search_text(UIState* state);

// Définit l'état de recherche
void ui_set_searching(UIState* state, bool searching);

// Gère les événements de la fenêtre
bool ui_should_close(void);

#endif // UI_H
