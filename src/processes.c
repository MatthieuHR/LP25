#include <../include/processes.h>
#include <../include/messages.h>
#include <../include/file-properties.h>
#include <../include/sync.h>

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
    // Check if parallel is enabled
    if (the_config->is_parallel) {
        // Create lister process
        p_context->source_lister_pid = make_process(p_context, lister_process_loop, &the_config->processes_count);
        if (p_context->source_lister_pid == -1) {
            return -1; // Failed to create lister process
        }

        // Create analyzer process
        p_context->source_analyzers_pids = make_process(p_context, analyzer_process_loop, &the_config->processes_count);
        if (p_context->source_analyzers_pids == -1) {
            // Terminate lister process
            kill(p_context->source_lister_pid, SIGTERM);
            return -1; // Failed to create analyzer process
        }

        // Wait for both processes to initialize
        if (wait_for_processes(p_context) == -1) {
            // Terminate lister and analyzer processes
            kill(p_context->source_lister_pid, SIGTERM);
            kill(p_context->source_analyzers_pids, SIGTERM);
            return -1; // Failed to wait for processes
        }
    }

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
    // Do nothing if not parallel
    if (!the_config->is_parallel) {
        return;
    }

    // Send terminate
    send_terminate_signal(p_context->source_lister_pid);
    send_terminate_signal(p_context->source_analyzers_pids);

    // Wait for responses
    wait_for_process_termination(p_context->source_lister_pid);
    wait_for_process_termination(p_context->source_analyzers_pids);

    // Free allocated memory
    free(p_context->source_analyzers_pids);

    // Free the MQ
    msgctl(p_context->message_queue_id, IPC_RMID, NULL);
}
