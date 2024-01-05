#include "include/sync.h"
#include "include/configuration.h"
#include "include/file-properties.h"
#include "include/processes.h"

#include <stdio.h>
#include <assert.h>
#include <unistd.h>

/*!
 * @brief main function, calling all the mechanics of the program
 * @param argc its number of arguments, including its own name
 * @param argv the array of arguments
 * @return 0 in case of success, -1 else
 * Function is already provided with full implementation, you **shall not** modify it.
 */
int main(int argc, char *argv[]) {
    argc = 5;
    argv[0] = "/home/matth/vscode/LP25/cmake-build-debug/Projet_LP25";
    argv[1] = "/home/matth/vscode/LP25/Test";
    argv[2] = "/home/matth/vscode/LP25/Result";
    argv[3] = "-n6";
    // Check parameters:
    // - source and destination are provided
    // - source exists and can be read
    // - destination exists and can be written OR doesn't exist but can be created
    // - other options with getopt (see instructions)
    configuration_t my_config;
    init_configuration(&my_config);
    if (set_configuration(&my_config, argc, argv) == -1) {
        printf("Configuration failed\nAborting\n");
        return -1;
    }

    printf("Configuration:\n");
    printf("Source: %s\n", my_config.source);
    printf("Destination: %s\n", my_config.destination);
    printf("Processes count: %d\n", my_config.processes_count);
    printf("Parallel: %s\n", my_config.is_parallel ? "true" : "false");
    printf("MD5: %s\n", my_config.uses_md5 ? "true" : "false");
    printf("Verbose: %s\n", my_config.verbose ? "true" : "false");
    printf("Dry run: %s\n", my_config.dry_run ? "true" : "false");

    // Check directories
    if (!directory_exists(my_config.source) || !directory_exists(my_config.destination)) {
        printf("Either source or destination directory do not exist\nAborting\n");
        return -1;
    }
    // Is destination writable?
    if (!is_directory_writable(my_config.destination)) {
        printf("Destination directory %s is not writable\n", my_config.destination);
        return -1;
    }

    // Prepare (fork, MQ) if parallel
    process_context_t processes_context;
    if(prepare(&my_config, &processes_context) == 1){ perror("prepare");}

    printf("Processes context:\n");
    printf("Processes count: %d\n", processes_context.processes_count);
    printf("Main process PID: %d\n", processes_context.main_process_pid);
    printf("Source lister PID: %d\n", processes_context.source_lister_pid);
    printf("Destination lister PID: %d\n", processes_context.destination_lister_pid);
    printf("Shared key: %d\n", processes_context.shared_key);
    printf("Message queue ID: %d\n", processes_context.message_queue_id);
    printf("Source analyzers PIDs: ");
    for (int i = 0; i < processes_context.processes_count/2; i++) {
        printf("%d ", processes_context.source_analyzers_pids[i]);
    }
    printf("\n");
    printf("Destination analyzers PIDs: ");
    for (int i = 0; i < processes_context.processes_count/2; i++) {
        printf("%d ", processes_context.destination_analyzers_pids[i]);
    }
    printf("\n");


    // Run synchronize:
    synchronize(&my_config, &processes_context);
    
    // Clean resources
    clean_processes(&my_config, &processes_context);

    return 0;
}
