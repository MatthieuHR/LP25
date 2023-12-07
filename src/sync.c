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

    if (source_entry == NULL || the_config == NULL) {
        // Gérer l'erreur (par exemple, imprimer un message et retourner)
        return;
    }

    // Construisez le chemin de destination en ajoutant le préfixe approprié
    char destination_path[MAX_PATH_LENGTH]; // Remplacez MAX_PATH_LENGTH par la longueur maximale attendue du chemin
    build_destination_path(source_entry->chemin_du_fichier, the_config->prefixe_destination, destination_path);

    // Vérifiez si le répertoire de destination existe, sinon créez-le
    if (access(destination_path, F_OK) == -1) {
        if (mkdir(destination_path, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) != 0) {
            // Gérer l'erreur de création du répertoire (par exemple, imprimer un message et retourner)
            return;
        }
    }
     // Ouvrez le fichier source en lecture
    int source_file = open(source_entry->chemin_du_fichier, O_RDONLY);
    if (source_file == -1) {
        // Gérer l'erreur d'ouverture du fichier source (par exemple, imprimer un message et retourner)
        return;
    }

    // Ouvrez (ou créez) le fichier destination en écriture
    int destination_file = open(destination_path, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (destination_file == -1) {
        // Gérer l'erreur d'ouverture du fichier destination (par exemple, imprimer un message et retourner)
        close(source_file);
        return;
    }

    // Utilisez sendfile pour copier le contenu du fichier source vers le fichier destination
    off_t offset = 0;
    sendfile(destination_file, source_file, &offset, source_entry->taille);

    // Fermez les fichiers
    close(source_file);
    close(destination_file);

    // Copiez les modes d'accès et mtime de la source à la destination
    copy_permissions_and_mtime(source_entry->chemin_du_fichier, destination_path);
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
}

/*!
 * @brief open_dir opens a dir
 * @param path is the path to the dir
 * @return a pointer to a dir, NULL if it cannot be opened
 */
DIR *open_dir(char *path) {
}

/*!
 * @brief get_next_entry returns the next entry in an already opened dir
 * @param dir is a pointer to the dir (as a result of opendir, @see open_dir)
 * @return a struct dirent pointer to the next relevant entry, NULL if none found (use it to stop iterating)
 * Relevant entries are all regular files and dir, except . and ..
 */
struct dirent *get_next_entry(DIR *dir) {
}
