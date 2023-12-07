#include <../include/sync.h>
#include <../include/processes.h>
#include <../include/utility.h>
#include <../include/messages.h>
#include <../include/file-properties.h>

#include <dirent.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/sendfile.h>
#include <unistd.h>
#include <sys/msg.h>
#include <stdio.h>
#include <stdlib.h>

/*!
 * @brief synchronize is the main function for synchronization
 * It will build the lists (source and destination), then make a third list with differences, and apply differences to the destination
 * It must adapt to the parallel or not operation of the program.
 * @param the_config is a pointer to the configuration
 * @param p_context is a pointer to the processes context
 */
void synchronize(configuration_t *the_config, process_context_t *p_context) {
    // Logique de synchronisation

    // Construire les listes source et destination
    files_list_t source_list;
    files_list_t destination_list;
    make_files_lists_parallel(&source_list, &destination_list, the_config, p_context->msg_queue);

    // Créer une troisième liste avec les différences
    files_list_t differences_list;
    build_differences_list(&source_list, &destination_list, &differences_list);

    // Appliquer les différences à la destination
    apply_differences(&destination_list, &differences_list, the_config);
} 

/*!
 * @brief mismatch tests if two files with the same name (one in source, one in destination) are equal
 * @param lhd a files list entry from the source
 * @param rhd a files list entry from the destination
 * @has_md5 a value to enable or disable MD5 sum check
 * @return true if both files are not equal, false else
 */
bool mismatch(files_list_entry_t *lhd, files_list_entry_t *rhd, bool has_md5) {
    if (has_md5) {//Regade s'il y a un md5
        for (int i = 0; i < 16; ++i) {
            if (lhd->md5sum[i] != rhd->md5sum[i]) {
                return true;
            }
        }
    }  
    else {//S'il n'y en a pas : 
        FILE *file1 = fopen(lhd->path_and_name, "rb");//Ouvre 2 fichiers
        FILE *file2 = fopen(rhd->path_and_name, "rb");

        if (file1 == NULL || file2 == NULL) {//Vérifie que tous deux ne soient pas vide pour passer à la suite
            if (file1) fclose(file1);
            if (file2) fclose(file2);
            fprintf(stderr, "[MISMATCH TEST] : un des 2 fichier n'a pas pu être ouvert\n");
            exit(-1);
        }
        char char1, char2;

        while ((strcpy(char1, fgetc(file1))) != end_of_file && (strcpy(char2,fgetc(file2))) != end_of_file) {//Détecte s'il y a une différence
            if (strcmp(char1,char2) != 0) {
                return true;
                break; //Si c'est le cas, sort
            }
        }
        if ((char1 == EOF && char2 != EOF) || (char1 != EOF && char2 == EOF)) {//Regarde si la longueur des fichiers est différente
            return true;
        }
        fclose(file1);//Ferme les fichiers
        fclose(file2);
    }
    return false;
}

void make_files_list(files_list_t *list, char *target_path) {
    DIR *dir;//Ouverture répertoire
    struct dirent *entry;

    if ((dir = opendir(target_path)) == NULL) {//Problème d'ouverture
        perror("Unable to open directory");
        return;
    }

    while ((entry = readdir(dir)) != NULL) {//Parcours dans chaque élément présent dans le répertoire 
        char path[PATH_SIZE];
        if (concat_path(path, target_path, entry->d_name) == NULL) {
            perror("Unable to concatenate path");//En cas d'erreur, affiche ce message
            continue;
        }

        files_list_entry_t *new_entry = malloc(sizeof(files_list_entry_t));//Mémoire allouée pour une nouvelle entrée de la liste de fichiers
        if (new_entry == NULL) {
            perror("Unable to allocate memory for new file entry");//Erreur allocation mémoire
            continue;
        }
        //Ajout chemin du fichier à la liste des fichiers
        if (add_file_entry(list, path) != 0) {
            perror("Unable to add file entry to list");//Erreur ajout fichier
        }
    }

    closedir(dir);//Fermeture du répertoire
}
/*!
 * @brief make_files_lists_parallel makes both (src and dest) files list with parallel processing
 * @param src_list is a pointer to the source list to build
 * @param dst_list is a pointer to the destination list to build
 * @param the_config is a pointer to the program configuration
 * @param msg_queue is the id of the MQ used for communication
 */
void make_files_lists_parallel(files_list_t *src_list, files_list_t *dst_list, configuration_t *the_config, int msg_queue) {
}

/*!
 * @brief copy_entry_to_destination copies a file from the source to the destination
 * It keeps access modes and mtime (@see utimensat)
 * Pay attention to the path so that the prefixes are not repeated from the source to the destination
 * Use sendfile to copy the file, mkdir to create the directory
 */
void copy_entry_to_destination(files_list_entry_t *source_entry, configuration_t *the_config) {
    // Vérifie si l'entrée source est valide
    if (source_entry == NULL || the_config == NULL) {
        fprintf(stderr, "Invalid source entry or configuration\n");
        return;
    }

    // Construit le chemin complet du fichier source
    char source_path[PATH_MAX];
    snprintf(source_path, sizeof(source_path), "%s/%s", the_config->source_path, source_entry->path);

    // Construit le chemin complet du fichier destination
    char dest_path[PATH_MAX];
    snprintf(dest_path, sizeof(dest_path), "%s/%s", the_config->destination_path, source_entry->path);

    // Vérifie si l'entrée est un répertoire
    if (source_entry->is_directory) {
        // Crée le répertoire de destination
        if (mkdir(dest_path, the_config->dir_mode) != 0) {
            perror("Error creating directory");
            return;
        }
    } else {
        // Ouvre le fichier source
        int source_fd = open(source_path, O_RDONLY);
        if (source_fd == -1) {
            perror("Error opening source file");
            return;
        }

        // Crée ou ouvre le fichier de destination
        int dest_fd = open(dest_path, O_WRONLY | O_CREAT | O_TRUNC, the_config->file_mode);
        if (dest_fd == -1) {
            perror("Error opening destination file");
            close(source_fd);
            return;
        }

        // Utilise sendfile pour copier le contenu du fichier
        ssize_t bytes_copied = sendfile(dest_fd, source_fd, NULL, source_entry->size);

        // Vérifie si la copie a réussi
        if (bytes_copied == -1) {
            perror("Error copying file");
        }

        // Ferme les fichier
        close(source_fd);
        close(dest_fd);
    }
}

/*!
 * @brief make_list lists files in a location (it recurses in directories)
 * It doesn't get files properties, only a list of paths
 * This function is used by make_files_list and make_files_list_parallel
 * @param list is a pointer to the list that will be built
 * @param target is the target dir whose content must be listed
 */
void make_list(files_list_t *list, char *target) {
    // Ouvre le répertoire spécifié par `target`
    DIR *dir = open_dir(target);

    // Si l'ouverture du répertoire échoue
    if (dir == NULL) {
        return;
    }

    // Parcours de chaque entrée (fichier ou sous-répertoire) dans le répertoire ouvert
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        // Construit le chemin complet du fichier ou sous-répertoire en concaténant `target` avec le nom de l'entrée
        char path[PATH_MAX];
        snprintf(path, sizeof(path), "%s/%s", target, entry->d_name);

        // Ajoute le chemin du fichier à la liste des fichiers
        add_file_entry(list, path);

        // Si l'entrée est un sous-répertoire, appelle la fonction make_list pour explorer le sous-répertoire
        if (entry->d_type == DT_DIR && strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            make_list(list, path);
        }
    }

    // Ferme le répertoire après traitement
    closedir(dir);
}

/*!
 * @brief open_dir opens a dir
 * @param path is the path to the dir
 * @return a pointer to a dir, NULL if it cannot be opened
 */
DIR *open_dir(char *path) {
      DIR *dir = opendir(path);
    if (dir == NULL) {
        perror("Unable to open directory"); 
    }
    return dir;
}

/*!
 * @brief get_next_entry returns the next entry in an already opened dir
 * @param dir is a pointer to the dir (as a result of opendir, @see open_dir)
 * @return a struct dirent pointer to the next relevant entry, NULL if none found (use it to stop iterating)
 * Relevant entries are all regular files and dir, except . and ..
 */
struct dirent *get_next_entry(DIR *dir) {
}
