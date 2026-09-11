#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <poll.h>
#include <strings.h>

#include "common.h"

// Check if a worker thread wrote to the error pipe
static int
check_error(int efd) {
    int rc, numfds;
    struct pollfd fds[MAXFDS];

    bzero(fds, sizeof(fds));
    fds[0].fd = efd;
    fds[0].events = POLLIN;
    fds[0].revents = 0;
    numfds = 1;

    rc = poll(fds, numfds, -1);
    if ((rc <= 0) ||
        (fds[0].revents & POLLHUP) ||
        (fds[0].revents & POLLERR) ||
        (fds[0].revents & POLLNVAL)) {
        return 0;
    }

    // Error or timeout from poll;
    if (fds[0].revents == POLLIN) {
        // TODO: read the event object and report
        return 1;
    }

    return 0;
}

// Function used by worker threads to report an error
int
report_error(int efd, int id, int error) {
    struct error_event event = {
        .thread_id = id,
        .error = error
    };

    if (write(efd, &event, sizeof(event))) {
        perror("Write to error pipe");
        return -1;
    }

    return 0;
}

int
main(int argc, char *argv[])
{
    pthread_attr_t attr;
    pthread_t pwireid, httpid;
    int pipefd[2];

    if (pipe(pipefd) == -1) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    // read fd of thepipe set to non-blocking
    fcntl(pipefd[0], F_SETFL, O_NONBLOCK);
    
    struct cmd_args cargs = {
        .argc = &argc,
        .argv = &argv,
        .efd = pipefd[1]
    };


    // Hold for setting attrs later
    if (pthread_attr_init(&attr) != 0) {
        perror("ERROR: pthread_attr_init");
        exit(EXIT_FAILURE);
    }
    
    if (pthread_create(&pwireid, &attr, pwire_start, (void *)&cargs)) { 
        perror("ERROR: pthread_create");
        exit(EXIT_FAILURE);
    }

    if (pthread_create(&httpid, &attr, httpd_start, (void *)&cargs)) { 
        perror("ERROR: pthread_create");
        exit(EXIT_FAILURE);
    }

    if (pthread_attr_destroy(&attr) != 0) {
        perror("ERROR: pthread_attr_destroy");
        exit(EXIT_FAILURE);
    }
    
    printf("Running noise generator and management web server\n(Press Ctrl+C to exit)...\n");

    while (1) {
        if (check_error(cargs.efd) != 0) {
            fprintf(stderr,"ERROR: thread failure\n");
            exit(EXIT_FAILURE);
        }

        sleep(5);
    }
    
    return 0;
}
