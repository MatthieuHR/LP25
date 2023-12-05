#include <../include/configuration.h>

#include <stddef.h>
#include <stdlib.h>
#include <getopt.h>
#include <stdio.h>
#include <string.h>

typedef enum {DATE_SIZE_ONLY, NO_PARALLEL} long_opt_values;

/*!
 * @brief function display_help displays a brief manual for the program usage
 * @param my_name is the name of the binary file
 * This function is provided with its code, you don't have to implement nor modify it.
 */
void display_help(char *my_name) {
    printf("%s [options] source_dir destination_dir\n", my_name);
    printf("Options: \t-n <processes count>\tnumber of processes for file calculations\n");
    printf("         \t-h display help (this text)\n");
    printf("         \t--date_size_only disables MD5 calculation for files\n");
    printf("         \t--no-parallel disables parallel computing (cancels values of option -n)\n");
}

/*!
 * @brief init_configuration initializes the configuration with default values
 * @param the_config is a pointer to the configuration to be initialized
 */
void init_configuration(configuration_t *the_config) {  //j initialise le the_config
    strcpy(the_config->source,"");
    strcpy(the_config->destination,"");
    the_config->processes_count=0;
    the_config->is_parallel=false;
    the_config->uses_md5=false;
    
}

/*!
 * @brief set_configuration updates a configuration based on options and parameters passed to the program CLI
 * @param the_config is a pointer to the configuration to update
 * @param argc is the number of arguments to be processed
 * @param argv is an array of strings with the program parameters
 * @return -1 if configuration cannot succeed, 0 when ok
 */
int set_configuration(configuration_t *the_config, int argc, char *argv[]) {            //pas testé

    struct option my_opts[] = {                                            //je me suis inspiré du td avec getop pas encore testé
            {.name="date-size-only",.has_arg=0,.flag=0,.val='d'},
            {.name="number",.has_arg=1,.flag=0,.val='n'},
            {.name="no-parallel",.has_arg=0,.flag=0,.val='p'},
            {.name="dry-run",.has_arg=0,.flag=0,.val='r'},
            {.name="verbose",.has_arg=0,.flag=0,.val='v'},
            {.name=0,.has_arg=0,.flag=0,.val=0}, // last element must be zero
    };

    int opt=0;

    while ((opt = getopt_long(argc, argv, "n:dprv", my_opts, NULL)) != -1) {
        switch (opt) {
            case 'd':
                the_config->uses_md5=true;
                break;
            case 'n':
                the_config->processes_count = atoi(optarg);
                break;
            case 'p':
                the_config->is_parallel=true;
                break;
            case 'r':
                    //je voit pas quoi mettre
                break;
            case 'v':
                    //je voit pas quoi mettre
                break;
        }
    }

    if (optind + 2 != argc) {                                                            //verifie si 2 argument sinon renvoie a help 
        fprintf(stderr, "Error: source_dir and destination_dir are required.\n");
        display_help(argv[0]);
        return -1; // Configuration failed due to missing arguments
    }

    strncpy(the_config->source, argv[optind], sizeof(the_config->source) - 1);                    //copie les 2 arguments sources et destinations dans 
    the_config->source[sizeof(the_config->source) - 1] = '\0';                                    //les config->source et destnation 

    strncpy(the_config->destination, argv[optind + 1], sizeof(the_config->destination) - 1);
    the_config->destination[sizeof(the_config->destination) - 1] = '\0';

    return 0;

    
}
