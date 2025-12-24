#include "ui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FONT_SIZE 20
#define LINE_HEIGHT 25
#define PADDING 10
#define INDENT_SIZE 20

UIState* ui_init(int width, int height, const char* title) {
    UIState* state = (UIState*)malloc(sizeof(UIState));
    if (!state) return NULL;
    
    state->window_width = width;
    state->window_height = height;
    state->scroll_offset = 0;
    state->selected_index = -1;
    state->clicked_path = NULL;
    state->go_back = false;
    state->initialized = false;
    
    InitWindow(width, height, title);
    SetTargetFPS(60);
    
    state->font = GetFontDefault();
    state->initialized = true;
    
    return state;
}

void ui_destroy(UIState* state) {
    if (state && state->initialized) {
        if (state->clicked_path) {
            free(state->clicked_path);
        }
        CloseWindow();
        free(state);
    }
}

static void format_size(long bytes, char* buffer, size_t buffer_size) {
    if (bytes < 1024) {
        snprintf(buffer, buffer_size, "%ld B", bytes);
    } else if (bytes < 1024 * 1024) {
        snprintf(buffer, buffer_size, "%.1f KB", bytes / 1024.0);
    } else if (bytes < 1024 * 1024 * 1024) {
        snprintf(buffer, buffer_size, "%.1f MB", bytes / (1024.0 * 1024.0));
    } else {
        snprintf(buffer, buffer_size, "%.1f GB", bytes / (1024.0 * 1024.0 * 1024.0));
    }
}

static Color get_file_color(FileType type) {
    return type == FILE_TYPE_DIRECTORY ? BLUE : DARKGRAY;
}

void ui_render(UIState* state, FileList* files, const char* current_path) {
    if (!state || !files) return;
    
    // Réinitialiser le chemin cliqué et go_back
    if (state->clicked_path) {
        free(state->clicked_path);
        state->clicked_path = NULL;
    }
    state->go_back = false;
    
    // Gestion du scroll avec la molette
    float wheel = GetMouseWheelMove();
    if (wheel != 0) {
        state->scroll_offset -= (int)(wheel * LINE_HEIGHT * 3);
        if (state->scroll_offset < 0) {
            state->scroll_offset = 0;
        }
        
        int max_scroll = (files->count * LINE_HEIGHT) - state->window_height + 150;
        if (max_scroll < 0) max_scroll = 0;
        if (state->scroll_offset > max_scroll) {
            state->scroll_offset = max_scroll;
        }
    }
    
    BeginDrawing();
    ClearBackground(RAYWHITE);
    
    // En-tête avec bouton retour
    DrawRectangle(0, 0, state->window_width, 40, DARKGRAY);
    DrawText("Explorateur de Fichiers", PADDING + 100, 10, FONT_SIZE, WHITE);
    
    // Bouton retour
    Rectangle back_button = {PADDING, 8, 80, 24};
    Color back_color = LIGHTGRAY;
    if (CheckCollisionPointRec(GetMousePosition(), back_button)) {
        back_color = GRAY;
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            state->go_back = true;
        }
    }
    DrawRectangleRec(back_button, back_color);
    DrawText("< Retour", PADDING + 5, 12, 16, DARKGRAY);
    
    // Chemin actuel
    DrawRectangle(0, 40, state->window_width, 35, LIGHTGRAY);
    char path_display[512];
    snprintf(path_display, sizeof(path_display), "Chemin: %s", current_path);
    DrawText(path_display, PADDING, 48, 18, DARKGRAY);
    
    // Statistiques
    DrawRectangle(0, 75, state->window_width, 25, GRAY);
    char stats[256];
    int dir_count = 0, file_count = 0;
    for (int i = 0; i < files->count; i++) {
        if (files->entries[i].type == FILE_TYPE_DIRECTORY) {
            dir_count++;
        } else {
            file_count++;
        }
    }
    snprintf(stats, sizeof(stats), "%d dossiers, %d fichiers", dir_count, file_count);
    DrawText(stats, PADDING, 80, 16, WHITE);
    
    // Zone de défilement
    int content_y = 100;
    int y = content_y - state->scroll_offset;
    
    // Dessiner les fichiers
    for (int i = 0; i < files->count; i++) {
        FileEntry* entry = &files->entries[i];
        
        // Ne dessiner que les éléments visibles
        if (y >= content_y - LINE_HEIGHT && y < state->window_height) {
            int x = PADDING + (entry->depth * INDENT_SIZE);
            
            // Fond de sélection
            Color bg_color = BLANK;
            Rectangle item_rect = { 0, (float)y, (float)state->window_width, LINE_HEIGHT };
            
            if (CheckCollisionPointRec(GetMousePosition(), item_rect)) {
                bg_color = Fade(SKYBLUE, 0.2f);
                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                    state->selected_index = i;
                    // Si c'est un dossier, on navigue dedans
                    if (entry->type == FILE_TYPE_DIRECTORY) {
                        state->clicked_path = (char*)malloc(strlen(entry->path) + 1);
                        if (state->clicked_path) {
                            strcpy(state->clicked_path, entry->path);
                        }
                    }
                }
            }
            
            if (i == state->selected_index) {
                bg_color = Fade(SKYBLUE, 0.4f);
            }
            
            DrawRectangleRec(item_rect, bg_color);
            
            // Icône
            const char* icon = entry->type == FILE_TYPE_DIRECTORY ? "📁" : "📄";
            DrawText(icon, x, y + 3, FONT_SIZE, get_file_color(entry->type));
            
            // Nom
            DrawText(entry->name, x + 35, y + 3, FONT_SIZE - 2, BLACK);
            
            // Taille (seulement pour les fichiers)
            if (entry->type == FILE_TYPE_FILE) {
                char size_str[64];
                format_size(entry->size, size_str, sizeof(size_str));
                int size_width = MeasureText(size_str, FONT_SIZE - 4);
                DrawText(size_str, state->window_width - size_width - PADDING, y + 5, FONT_SIZE - 4, DARKGRAY);
            } else {
                // Flèche pour les dossiers
                DrawText(">", state->window_width - 30, y + 3, FONT_SIZE, DARKGRAY);
            }
        }
        
        y += LINE_HEIGHT;
    }
    
    // Instructions
    const char* instructions = "Cliquez sur un dossier pour y entrer | Molette pour defiler | ESC pour quitter";
    int text_width = MeasureText(instructions, 14);
    DrawText(instructions, state->window_width - text_width - PADDING, state->window_height - 25, 14, DARKGRAY);
    
    EndDrawing();
}

char* ui_get_clicked_path(UIState* state) {
    if (!state) return NULL;
    
    char* path = state->clicked_path;
    state->clicked_path = NULL;
    return path;
}

bool ui_should_go_back(UIState* state) {
    return state ? state->go_back : false;
}

bool ui_should_close(void) {
    return WindowShouldClose();
}
