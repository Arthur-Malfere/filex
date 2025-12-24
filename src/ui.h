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
    bool search_limit_reached; // Si la limite de résultats a été atteinte
    char* selected_file_path;  // Chemin du fichier sélectionné pour visualisation
    char* file_content;        // Contenu du fichier sélectionné
    bool is_binary_file;       // Si le fichier sélectionné est binaire
    long file_size;            // Taille du fichier sélectionné
    int file_scroll_offset;    // Offset de scroll pour le contenu du fichier
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

// Définit si la limite de résultats a été atteinte
void ui_set_search_limit_reached(UIState* state, bool reached);

// Gère les événements de la fenêtre
bool ui_should_close(void);

#endif // UI_H
