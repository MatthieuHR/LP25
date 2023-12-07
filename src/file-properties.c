#include "file-properties.h"

#include <sys/stat.h>
#include <dirent.h>
#include <openssl/evp.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include "defines.h"
#include <fcntl.h>
#include <stdio.h>
#include "utility.h"
#include <stdbool.h>

int get_file_stats(files_list_entry_t *entry) {
    struct stat sb;
    char path = entry->path_and_name;
    if (lstat(path, &sb) == -1) {
        return -1;
    }

    entry->mtime.tv_nsec = sb.st_mtime;
    entry->size = sb.st_size;
    entry->mode = sb.st_mode;

    if (S_ISDIR(sb.st_mode)) {
        entry->entry_type = DOSSIER;
    } else if (S_ISREG(sb.st_mode)) {
        entry->entry_type = FICHIER;

        if (compute_file_md5(entry) == -1) {
            return -1;
        }
    } else {
        return -1;
    }

    return 0;
}
int compute_file_md5(files_list_entry_t *entry) {

    //Ouvre et vérifie si le fichier à été correctement ouvert.
    FILE *file = fopen(entry->path_and_name, "rb");
    if (!file) {
        perror("Impossible d'ouvrir le fichier");
        return -1;
    }

    EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
    const EVP_MD *md = EVP_md5(); // Algorithme MD5 de evp.h

    //Vérifie si les deux lignes du dessus ont réussi.
    if ((!mdctx || !md)||(1 != EVP_DigestInit_ex(mdctx, md, NULL))) {
        fclose(file);
        EVP_MD_CTX_free(mdctx);
        perror("Erreur dans l'initialisation de la somme MD5");
        return -1;
    }

    unsigned char buffer[1024];
    size_t bytes;
    while ((bytes = fread(buffer, 1, sizeof(buffer), file)) != 0) {
        if (1 != EVP_DigestUpdate(mdctx, buffer, bytes)) {
            printf("%s\n", mdctx);
            fclose(file);
            EVP_MD_CTX_free(mdctx);
            perror("Erreur dans la mise à jour de la somme MD5");
            return -1;
        }
    }
    unsigned int md_len;
    if (1 != EVP_DigestFinal_ex(mdctx, entry->md5sum, &md_len)) {
        fclose(file);
        EVP_MD_CTX_free(mdctx);
        perror("Erreur dans la finalisation de la somme MD5");
        return -1;
    }

    EVP_MD_CTX_free(mdctx);
    fclose(file);

    return 0;
}

bool directory_exists(char *path_to_dir) {
    struct stat sb;
    if (stat(path_to_dir, &sb) == 0 && S_ISDIR(sb.st_mode)) {
        return true;
    } else {
        return false;
    }
}

bool is_directory_writable(char *path_to_dir) {
    char test_file[PATH_SIZE];
    if (concat_path(test_file, path_to_dir, "testfile.tmp") == NULL) {
        return false;
    }

    FILE *file = fopen(test_file, "w");
    if (file != NULL) {
        fclose(file);
        remove(test_file);
        return true;
    } else {
        return false;
    }
}
