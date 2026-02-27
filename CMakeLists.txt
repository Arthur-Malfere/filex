cmake_minimum_required(VERSION 3.10.0)
project(filex VERSION 0.1.0 LANGUAGES C)

# Homebrew installs a CMake config package for raylib in /opt/homebrew/opt/raylib
# Append the prefix so find_package can locate it without manual link flags.
list(APPEND CMAKE_PREFIX_PATH "/opt/homebrew/opt/raylib")

find_package(raylib 5.0 REQUIRED CONFIG)

# Fichiers sources
set(SOURCES
    src/main.c
    src/file_explorer.c
    src/ui.c
)

add_executable(filex ${SOURCES})

# Trouver pthread
find_package(Threads REQUIRED)

target_link_libraries(filex raylib Threads::Threads)

# Définir le répertoire d'include
target_include_directories(filex PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)