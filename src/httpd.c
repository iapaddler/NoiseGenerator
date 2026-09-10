#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "common.h"

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
        close(server_fd);
        report_error(carg->efd, HTTPD_THREAD, errno);
        return NULL;
    }

    // Listen for incoming connections (backlog queue size of 10)
    if (listen(server_fd, 10) < 0) {
        perror("Listen failed");
        close(server_fd);
        report_error(carg->efd, HTTPD_THREAD, errno);
        return NULL;
    }

    printf("HTTP Server is running on http://localhost:%d\n", PORT);

    // loop accepting clients
    while (1) {
        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_addr_len);
        if (client_fd < 0) {
            perror("Accept failed");
            continue;
        }

        // Clear buffer and read the raw HTTP request text
        memset(buffer, 0, BUFFER_SIZE);
        ssize_t bytes_read = read(client_fd, buffer, BUFFER_SIZE - 1);
        
        if (bytes_read > 0) {
            // Print the received request header to the terminal
            printf("--- Received Request ---\n%s\n------------------------\n", buffer);

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
            write(client_fd, http_response, strlen(http_response));
        }

        close(client_fd);
    }

    // unreachable here but good practice
    close(server_fd);
    return 0;
}
