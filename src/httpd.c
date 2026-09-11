#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "common.h"

/*
 * Definition of an HTTP request:
 * Method SP Request-URI SP HTTP-Version CRLF
 * example: GET /api/v1/users?id=42 HTTP/1.1\r\n
*/
static int
execute_api(char *buffer, ssize_t bytes_recvd) {
    char *api;
    size_t slen = strlen("GET ");

    // If not a get method, ignore
    if (strncmp("GET ", buffer, slen) == 0) {
        api = buffer + slen;

        // inc & dec values by a default amount
        if (strncmp(API_VOL_UP, api, strlen(API_VOL_UP)) == 0) {
            pw_vol_inc(0);
            return 0;
        } else if (strncmp(API_VOL_DOWN, api, strlen(API_VOL_DOWN)) == 0) {
            pw_vol_dec(0);
            return 0;
        } else if (strncmp(API_TONE_UP, api, strlen(API_TONE_UP)) == 0) {
            pw_tone_inc(0);
            return 0;
        } else if (strncmp(API_TONE_DOWN, api, strlen(API_TONE_DOWN)) == 0) {
            pw_tone_dec(0);
            return 0;
        }
    }
    return -1;
}

// TODO: pass the port from a command line arg?
void *
httpd_start(void *arg) {
    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    char buffer[BUFFER_SIZE];

    struct cmd_args *carg = arg;
    
    // 1. Create the standard TCP socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("Socket creation failed");
        report_error(carg->efd, HTTPD_THREAD, errno);
        return NULL;
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
        perror("Bind failed");
        report_error(carg->efd, HTTPD_THREAD, errno);
        close(server_fd);
        return NULL;
    }

    // Listen for incoming connections (backlog queue size of 10)
    if (listen(server_fd, 10) < 0) {
        perror("Listen failed");
        report_error(carg->efd, HTTPD_THREAD, errno);
        close(server_fd);
        return NULL;
    }

    if (gethostname(buffer, (size_t)BUFFER_SIZE) != 0) {
        perror("gethostname failed");
        report_error(carg->efd, HTTPD_THREAD, errno);
        close(server_fd);
        return NULL;
    }
    /*
      Do we want to get the IP addr? Just for display
      int getaddrinfo(const char *restrict node,
                      const char *restrict service,
                      const struct addrinfo *restrict hints,
                      struct addrinfo **restrict res);
     */

    printf("HTTP Server is running on http://%s:%d\n", buffer, PORT);

    // loop accepting clients
    while (1) {
        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_addr_len);
        if (client_fd < 0) {
            perror("Accept failed");
            continue;
        }

        // Clear buffer and read the raw HTTP request text
        memset(buffer, 0, BUFFER_SIZE);
        ssize_t bytes_recvd = recv(client_fd, buffer, BUFFER_SIZE, 0);

        if (bytes_recvd > 0) {
            // TBD: handle the root request distinct from an API request
            // TBD: get root response from index.html

            // Display the received request header
            printf("--- Received Request ---\n%s\n------------------------\n", buffer);

            // If this fails, we ignore. TBD: display an error on the browser?
            execute_api(buffer, bytes_recvd);

            // Hardcoded raw text response: Status Line + Headers + Empty Line + HTML Body
            const char *http_response = 
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/html; charset=UTF-8\r\n"
                "Connection: close\r\n"
                "\r\n" // This blank line is required to separate headers from the body
                "<!DOCTYPE html>"
                "<html>"
                "<head><title>C HTTP Server</title></head>"
                "<body>"
                "<h1>Hello from my C HTTP Server!</h1>"
                "<p>This page was served entirely using native raw C sockets.</p>"
                "</body>"
                "</html>";

            // Send the response back to the client
            send(client_fd, http_response, strlen(http_response), 0);
        }

        close(client_fd);
    }

    // unreachable here but good practice
    close(server_fd);
    return 0;
}
