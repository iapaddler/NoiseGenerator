#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <string.h>
#include <syslog.h>

#include "common.h"

// TODO: pass the port from a command line arg?
static int
server_start(struct cmd_args *carg) {
    int server_fd;
    struct sockaddr_in server_addr;
    char host[BUFFER_SIZE];
    
    // 1. Create the standard TCP socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        syslog(LOG_ERR, "socket: %s", strerror(errno));
        report_error(carg->efd, HTTPD_THREAD, errno);
        return -1;
    }

    // Handy: allow immediate reuse of the port after stopping the server
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // Server properties
    bzero((char *) &server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY); // any network interface
    server_addr.sin_port = htons(PORT);              // port to network byte order

    // Bind to PORT
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        syslog(LOG_ERR, "bind: %s", strerror(errno));
        report_error(carg->efd, HTTPD_THREAD, errno);
        close(server_fd);
        return -1;
    }

    // Listen for incoming connections (backlog queue size of 10)
    if (listen(server_fd, 10) < 0) {
        syslog(LOG_ERR, "listen: %s", strerror(errno));
        report_error(carg->efd, HTTPD_THREAD, errno);
        close(server_fd);
        return -1;
    }

    if (gethostname(host, (size_t)BUFFER_SIZE) != 0) {
        syslog(LOG_ERR, "gethostname: %s", strerror(errno));
        report_error(carg->efd, HTTPD_THREAD, errno);
        close(server_fd);
        return -1;
    }
    /*
      Do we want to get the IP addr? Just for display
      int getaddrinfo(const char *restrict node,
                      const char *restrict service,
                      const struct addrinfo *restrict hints,
                      struct addrinfo **restrict res);
     */

    if (strcasestr((const char *)host, (const char *)"unoq") == NULL) {
        strncpy(speaker_name, RPI_SPEAKER_NAME, strlen(RPI_SPEAKER_NAME) + 1);
    } else {
        strncpy(speaker_name, UNOQ_SPEAKER_NAME, strlen(RPI_SPEAKER_NAME) + 1);
    }

    syslog(LOG_NOTICE, "HTTP Server is running on http://%s:%d\n", host, PORT);
    return server_fd;
}

/*
 * Definition of an HTTP request:
 * Method SP Request-URI SP HTTP-Version CRLF
 * example: GET /api/v1/users?id=42 HTTP/1.1\r\n
*/
static bool
execute_api(char *buffer, ssize_t bytes_recvd) {
    char *api;
    size_t slen = strlen("GET ");

    // If not a get method, ignore
    if (strncmp("GET ", buffer, slen) == 0) {
        api = buffer + slen;

        // inc & dec values by a default amount
        if (strncmp(API_VOL_UP, api, strlen(API_VOL_UP)) == 0) {
            pw_vol_inc(0);
            return TRUE;
        } else if (strncmp(API_VOL_DOWN, api, strlen(API_VOL_DOWN)) == 0) {
            pw_vol_dec(0);
            return TRUE;
        } else if (strncmp(API_TONE_UP, api, strlen(API_TONE_UP)) == 0) {
            pw_tone_inc(0);
            return TRUE;
        } else if (strncmp(API_TONE_DOWN, api, strlen(API_TONE_DOWN)) == 0) {
            pw_tone_dec(0);
            return TRUE;
        }
    }
    return FALSE;
}

void *
httpd_start(void *arg) {
    int server_fd;
    struct cmd_args *carg = arg;

    server_fd = server_start(carg);
    if (server_fd == -1) {
        return NULL;
    }

    // loop accepting clients
    while (1) {
        int client_fd;
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        char *http_response;
        char buffer[BUFFER_SIZE];

        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_addr_len);
        if (client_fd < 0) {
            syslog(LOG_ERR, "accept: %s", strerror(errno));
            continue;
        }

        // Clear buffer and read the raw HTTP request text
        memset(buffer, 0, BUFFER_SIZE);
        ssize_t bytes_recvd = recv(client_fd, buffer, BUFFER_SIZE, 0);

        if (bytes_recvd > 0) {
            // Display the received request header
            syslog(LOG_DEBUG, "--- Received Request ---\n%s\n------------------------\n", buffer);

            // If this fails, we ignore. TODO: display an error on the browser?
            if (execute_api(buffer, bytes_recvd) == TRUE) {

                // Found an API, return abbreviated HTML.
                if ((http_response = calloc(1, RESPONSE_SIZE)) == NULL) {
                    syslog(LOG_ERR, "calloc: %s", strerror(errno));
                    continue;
                }

                if (snprintf(http_response, (size_t)RESPONSE_SIZE,
                             "HTTP/1.1 204 No Content\r\nCache-Control: no-cache") < 0) {
                    syslog(LOG_ERR, "snprintf: %s", strerror(errno));
                    free(http_response);
                    continue;
                }
            } else {
                int ifd, rc;
                size_t isize, rsize;
                char *index_buf;
                struct stat statbuf;

                ifd = open("assets/index.html", O_RDONLY);
                if (ifd < 0) {
                    syslog(LOG_ERR, "open: %s", strerror(errno));
                    continue;
                }

                if (fstat(ifd, &statbuf) == -1) {
                    syslog(LOG_ERR, "fstat: %s", strerror(errno));
                    continue;
                }

                // index buffer is the size of index.html, http_response is index.html + header
                isize = statbuf.st_size + 4;
                rsize = statbuf.st_size + RESPONSE_SIZE;

                if ((index_buf = calloc(1, isize)) == NULL) {
                    syslog(LOG_ERR, "calloc: %s", strerror(errno));
                    close(ifd);
                    continue;
                }

                if ((http_response = calloc(1, rsize)) == NULL) {
                    syslog(LOG_ERR, "calloc: %s", strerror(errno));
                    close(ifd);
                    free(index_buf);
                    continue;
                }

                rc = read(ifd, index_buf, isize);
                if (rc <= 0) {
                    syslog(LOG_ERR, "read: %s", strerror(errno));
                    close(ifd);
                    free(index_buf);
                    free(http_response);
                    continue;
                }

                close(ifd);

                // Definition of an HTTP response: Status Line + Headers + Empty Line + HTML Body
                if (snprintf(http_response, rsize,
                             "HTTP/1.1 200 OK\r\n"
                             "Content-Type: text/html; charset=UTF-8\r\n"
                             "Connection: close\r\n"
                             "\r\n" // This blank line is required to separate headers from the body
                             "%s", index_buf) < 0) {
                    syslog(LOG_ERR, "snprintf: %s", strerror(errno));
                    free(index_buf);
                    free(http_response);
                    continue;
                }
                free(index_buf);
            }

            // Send the response back to the client
            send(client_fd, http_response, strlen(http_response), 0);
            free(http_response);
        }

        close(client_fd);
    }

    // unreachable here but good practice
    close(server_fd);
    return 0;
}
