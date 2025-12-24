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
} UIState;

// Initialise l'interface utilisateur
UIState* ui_init(int width, int height, const char* title);

// Libère les ressources de l'UI
void ui_destroy(UIState* state);

// Affiche la liste des fichiers
void ui_render(UIState* state, FileList* files, const char* current_path);

// Gère les événements de la fenêtre
bool ui_should_close(void);

#endif // UI_H
