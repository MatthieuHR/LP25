project(Projet_LP25)

cmake_minimum_required(VERSION 2.7)


add_executable(Projet_LP25 main.c
        src/file-properties.c
        src/file-properties.c
        src/file-properties.c
        src/configuration.c
        src/configuration.c
        src/files-list.c
        src/messages.c
        src/utility.c
        src/sync.c
        src/processes.c
)

target_link_libraries(Projet_LP25 crypto ssl)