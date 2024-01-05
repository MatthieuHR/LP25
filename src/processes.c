#include "../include/processes.h"
#include "../include/messages.h"
#include "../include/file-properties.h"
#include "../include/sync.h"

#include <stdlib.h>
#include <unistd.h>
#include <sys/msg.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <signal.h>

/*!
 * @brief prepare prepares (only when parallel is enabled) the processes used for the synchronization.
 * @param the_config is a pointer to the program configuration
 * @param p_context is a pointer to the program processes context
 * @return 0 if all went good, -1 else
 */
int prepare(configuration_t *the_config, process_context_t *p_context) {
    printf("Preparing processes\n");
    // Check if parallel is enabled
    if (the_config->is_parallel == false) {
        printf("Parallel is enabled\n");

        key_t mq_key = ftok(".", 1);
        if (mq_key == -1) {
            errno = EACCES;
            return 1;
        }
        p_context->shared_key = mq_key;
        printf("Generated key: %d\n", mq_key);

        int mq_id = msgget(mq_key, IPC_CREAT | 0666);
        if (mq_id == -1) {
            errno = EACCES;
            return 1;
        }
        printf("Message queue ID: %d\n", mq_id);
        p_context->message_queue_id = mq_id;
        p_context->processes_count = 0;

        // Create lister process for source
        lister_configuration_t source_lister_config = {
                .my_recipient_id = 1,
                .my_receiver_id = 2,
                .analyzers_count = the_config->processes_count/2,
                .mq_key = mq_key
        };
        p_context->source_lister_pid = make_process(p_context, lister_process_loop, &source_lister_config);

        // Create lister process for destination
        lister_configuration_t destination_lister_config = {
                .my_recipient_id = 2,
                .my_receiver_id = 1,
                .analyzers_count = the_config->processes_count/2,
                .mq_key = mq_key
        };
        p_context->destination_lister_pid = make_process(p_context, lister_process_loop, &destination_lister_config);

        // Create analyzer process for source
        analyzer_configuration_t source_analyzer_config = {
                .my_recipient_id = p_context->source_lister_pid,
                .my_receiver_id = 2,
                .mq_key = mq_key,
                .use_md5 = the_config->uses_md5
        };
        p_context->source_analyzers_pids = malloc(sizeof(pid_t) * source_lister_config.analyzers_count/2);
        for (int i = 0; i < source_lister_config.analyzers_count; i++) {
            printf("Creating analyzer process %d\n", i);
            p_context->source_analyzers_pids[i] = make_process(p_context, analyzer_process_loop, &source_analyzer_config);
            printf("Analyzer process %d as %d pid\n", i, p_context->source_analyzers_pids[i]);
            p_context->processes_count++;
        }

        // Create analyzer process for destination
        analyzer_configuration_t destination_analyzer_config = {
                .my_recipient_id = p_context->destination_lister_pid,
                .my_receiver_id = 1,
                .mq_key = mq_key,
                .use_md5 = the_config->uses_md5
        };
        p_context->destination_analyzers_pids = malloc(sizeof(pid_t) * destination_lister_config.analyzers_count/2);
        for (int i = 0; i < destination_lister_config.analyzers_count; i++) {
            printf("Creating analyzer process %d\n", i);
            p_context->destination_analyzers_pids[i] = make_process(p_context, analyzer_process_loop, &destination_analyzer_config);
            printf("Analyzer process %d as %d pid\n", i, p_context->destination_analyzers_pids[i]);
            p_context->processes_count++;
        }


    }else{
        p_context = NULL;
    }
    printf("Processes prepared\n");
    return 0; // Success
}

/*!
 * @brief make_process creates a process and returns its PID to the parent
 * @param p_context is a pointer to the processes context
 * @param func is the function executed by the new process
 * @param parameters is a pointer to the parameters of func
 * @return the PID of the child process (it never returns in the child process)
 */
int make_process(process_context_t *p_context, process_loop_t func, void *parameters) {
    pid_t pid = fork();
    if (pid == -1) {
        errno = EAGAIN;
        return -1; // Failed to create child process
    } else if (pid == 0) {
        // Child process
        func(parameters);
        exit(0); // Exit child process
    } else {
        // Parent process
        return pid; // Return child process ID to the parent
    }
}

/*!
 * @brief lister_process_loop is the lister process function (@see make_process)
 * @param parameters is a pointer to its parameters, to be cast to a lister_configuration_t
 */
void lister_process_loop(void *parameters) {
}


/*!
 * @brief analyzer_process_loop is the analyzer process function
 * @param parameters is a pointer to its parameters, to be cast to an analyzer_configuration_t
 */
void analyzer_process_loop(void *parameters) {

}

/*!
 * @brief clean_processes cleans the processes by sending them a terminate command and waiting to the confirmation
 * @param the_config is a pointer to the program configuration
 * @param p_context is a pointer to the processes context
 */
void clean_processes(configuration_t *the_config, process_context_t *p_context) {

    printf("Cleaning processes\n");
    // Check pointers
    if (the_config == NULL || p_context == NULL) {
        fprintf(stderr, "Invalid pointers provided to clean_processes function.\n");
        errno = EINVAL;
        return;
    }

    printf("Cleaning processes\n");
    // Check if parallel is enabled
    if (the_config->is_parallel == false) {
        return;
    }

    // Send terminate command to all processes
    send_terminate_command(p_context->message_queue_id, MSG_TYPE_TO_SOURCE_LISTER);
    send_terminate_command(p_context->message_queue_id, MSG_TYPE_TO_DESTINATION_LISTER);

    for (int i = 0; i < p_context->processes_count/2; i++) {
        send_terminate_command(p_context->message_queue_id, MSG_TYPE_TO_SOURCE_ANALYZERS);
        send_terminate_command(p_context->message_queue_id, MSG_TYPE_TO_DESTINATION_ANALYZERS);
    }

    // Wait for termination
    printf("Waiting for termination\n");
    any_message_t msg;
    for (int i = 0; i < p_context->processes_count/2; i++) {
        msgrcv(p_context->message_queue_id, &msg, sizeof(any_message_t) - sizeof(long), MSG_TYPE_TO_MAIN, 0);
    }

    // Kill processes
    printf("Killing processes\n");
    kill(p_context->source_lister_pid, SIGTERM);
    kill(p_context->destination_lister_pid, SIGTERM);
    for (int i=0; i<p_context->processes_count/2; i++) {
        kill(p_context->source_analyzers_pids[i], SIGTERM);
        kill(p_context->destination_analyzers_pids[i], SIGTERM);
    }

    free(p_context->source_analyzers_pids);
    free(p_context->destination_analyzers_pids);

    if (msgctl(p_context->message_queue_id, IPC_RMID, NULL) == -1) {
        fprintf(stderr, "Error in removing message queue\n");
    }
}
