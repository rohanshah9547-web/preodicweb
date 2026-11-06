
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #define CLOSESOCKET closesocket
    #define STRCASECMP _stricmp
    typedef SOCKET socket_t;
#else
    #include <unistd.h>
    #include <strings.h>
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #define CLOSESOCKET close
    #define STRCASECMP strcasecmp
    typedef int socket_t;
#endif

#include "elements.h"

#define BUFFER_SIZE 8192
#define JSON_SIZE 50000

void sendResponse(socket_t client, const char *status,
                  const char *type, const char *body) {
    char header[512];

    snprintf(header, sizeof(header),
        "HTTP/1.1 %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Connection: close\r\n\r\n",
        status, type, strlen(body));

    send(client, header, (int)strlen(header), 0);
    send(client, body, (int)strlen(body), 0);
}

void sendFile(socket_t client, const char *path,
              const char *type) {
    FILE *file = fopen(path, "rb");

    if (!file) {
        sendResponse(client, "404 Not Found",
                     "text/plain", "File not found");
        return;
    }

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    rewind(file);

    char *data = malloc((size_t)size + 1);

    if (!data) {
        fclose(file);
        sendResponse(client, "500 Internal Server Error",
                     "text/plain", "Memory error");
        return;
    }

    size_t readCount = fread(data, 1, (size_t)size, file);
    fclose(file);
    data[readCount] = '\0';

    sendResponse(client, "200 OK", type, data);
    free(data);
}

void sendAllElements(socket_t client) {
    char *json = malloc(JSON_SIZE);

    if (!json) {
        sendResponse(client, "500 Internal Server Error",
                     "text/plain", "Memory error");
        return;
    }

    size_t used = 0;
    used += snprintf(json + used, JSON_SIZE - used, "[");

    for (int i = 0; i < SIZE; i++) {
        int written = snprintf(json + used, JSON_SIZE - used,
            "%s{\"atomicNumber\":%d,\"name\":\"%s\","
            "\"symbol\":\"%s\",\"atomicMass\":%.4f,"
            "\"category\":\"%s\"}",
            i ? "," : "",
            elements[i].atomicNumber,
            elements[i].name,
            elements[i].symbol,
            elements[i].atomicMass,
            elements[i].category);

        if (written < 0 ||
            (size_t)written >= JSON_SIZE - used) {
            free(json);
            sendResponse(client, "500 Internal Server Error",
                         "text/plain", "JSON buffer too small");
            return;
        }

        used += (size_t)written;
    }

    snprintf(json + used, JSON_SIZE - used, "]");
    sendResponse(client, "200 OK", "application/json", json);
    free(json);
}

/* Decode spaces and other URL-encoded characters. */
void decodeURL(char *text) {
    char *src = text;
    char *dst = text;

    while (*src) {
        if (*src == '+' ) {
            *dst++ = ' ';
            src++;
        } else if (*src == '%' &&
                   isxdigit((unsigned char)src[1]) &&
                   isxdigit((unsigned char)src[2])) {
            char hex[3] = {src[1], src[2], '\0'};
            *dst++ = (char)strtol(hex, NULL, 16);
            src += 3;
        } else {
            *dst++ = *src++;
        }
    }

    *dst = '\0';
}

void searchElement(socket_t client, char *query){
    decodeURL(query);

    for (int i = 0; i < SIZE; i++) {
        char number[12];
        snprintf(number, sizeof(number), "%d",
                 elements[i].atomicNumber);

        if (strcmp(query, number) == 0 ||
            STRCASECMP(query, elements[i].name) == 0 ||
            STRCASECMP(query, elements[i].symbol) == 0) {

            char json[512];

            snprintf(json, sizeof(json),
                "{\"atomicNumber\":%d,\"name\":\"%s\","
                "\"symbol\":\"%s\",\"atomicMass\":%.4f,"
                "\"category\":\"%s\"}",
                elements[i].atomicNumber,
                elements[i].name,
                elements[i].symbol,
                elements[i].atomicMass,
                elements[i].category);

            sendResponse(client, "200 OK",
                         "application/json", json);
            return;
        }
    }

    sendResponse(client, "404 Not Found",
                 "application/json",
                 "{\"error\":\"Element not found\"}");
}
int binarySearchByNumber(int number) {
    int low = 0;
    int high = SIZE - 1;

    while (low <= high) {
        int mid = low + (high - low) / 2;

        if (elements[mid].atomicNumber == number) {
            return mid;
        }

        if (elements[mid].atomicNumber < number) {
            low = mid + 1;
        } else {
            high = mid - 1;
        }
    }

    return -1;
}
void bubbleSortByMass(struct Element sorted[SIZE]) {
    int i, j;
    struct Element temp;

    // Copy the original array
    for (i = 0; i < SIZE; i++) {
        sorted[i] = elements[i];
    }

    // Bubble Sort by atomic mass
    for (i = 0; i < SIZE - 1; i++) {
        for (j = 0; j < SIZE - i - 1; j++) {
            if (sorted[j].atomicMass >
                sorted[j + 1].atomicMass) {

                temp = sorted[j];
                sorted[j] = sorted[j + 1];
                sorted[j + 1] = temp;
            }
        }
    }
}
void bubbleSortByName(struct Element sorted[SIZE]) {
    int i, j;
    struct Element temp;

    // Copy original elements
    for (i = 0; i < SIZE; i++) {
        sorted[i] = elements[i];
    }

    // Bubble Sort alphabetically
    for (i = 0; i < SIZE - 1; i++) {
        for (j = 0; j < SIZE - i - 1; j++) {

            if (strcmp(sorted[j].name,
                       sorted[j + 1].name) > 0) {

                temp = sorted[j];
                sorted[j] = sorted[j + 1];
                sorted[j + 1] = temp;
            }
        }
    }
}

void handleRequest(socket_t client, char *request) {
    char path[1024] = "/";
    char *start = strchr(request, ' ');

    if (!start) {
        sendResponse(client, "400 Bad Request",
                     "text/plain", "Bad request");
        return;
    }

    start++;

    char *end = strchr(start, ' ');

    if (!end) {
        sendResponse(client, "400 Bad Request",
                     "text/plain", "Bad request");
        return;
    }

    size_t length = (size_t)(end - start);

    if (length >= sizeof(path)) {
        sendResponse(client, "414 URI Too Long",
                     "text/plain", "URL too long");
        return;
    }

    memcpy(path, start, length);
    path[length] = '\0';

    if (strncmp(request, "GET ", 4) != 0) {
        sendResponse(client, "405 Method Not Allowed",
                     "text/plain", "GET only");
        return;
    }

    if (strcmp(path, "/api/elements") == 0) {
        sendAllElements(client);
    } else if (strncmp(path, "/api/search?q=", 14) == 0) {
        char query[128];
        snprintf(query, sizeof(query), "%s", path + 14);
        char *amp = strchr(query, '&');
        if (amp) *amp = '\0';

        if (strlen(query) == 0) {
            sendResponse(client, "400 Bad Request",
                         "application/json",
                         "{\"error\":\"Enter a search term\"}");
        } else {
            searchElement(client, query);
        }
        } else if (strcmp(path, "/api/sort?by=mass") == 0) {
    struct Element sorted[SIZE];
    bubbleSortByMass(sorted);

    char json[16384];
    size_t used = 0;

    used += snprintf(json + used, sizeof(json) - used, "[");

    for (int i = 0; i < SIZE; i++) {
        int written = snprintf(
            json + used, sizeof(json) - used,
            "%s{\"atomicNumber\":%d,\"name\":\"%s\","
            "\"symbol\":\"%s\",\"atomicMass\":%.4f,"
            "\"category\":\"%s\"}",
            i == 0 ? "" : ",",
            sorted[i].atomicNumber,
            sorted[i].name,
            sorted[i].symbol,
            sorted[i].atomicMass,
            sorted[i].category
        );

        if (written < 0 || (size_t)written >= sizeof(json) - used) {
            sendResponse(client, "500 Internal Server Error",
                         "application/json", "{\"error\":\"Response too large\"}");
            return;
        }

        used += (size_t)written;
    }

    snprintf(json + used, sizeof(json) - used, "]");
    sendResponse(client, "200 OK", "application/json", json);


        } else if (strncmp(path, "/api/binary-search?n=", 21) == 0) {
    int number = atoi(path + 21);
    int index = binarySearchByNumber(number);

    if (index >= 0) {
        char json[512];
        snprintf(json, sizeof(json),
            "{\"atomicNumber\":%d,\"name\":\"%s\","
            "\"symbol\":\"%s\",\"atomicMass\":%.4f,"
            "\"category\":\"%s\"}",
            elements[index].atomicNumber,
            elements[index].name,
            elements[index].symbol,
            elements[index].atomicMass,
            elements[index].category);

        sendResponse(client, "200 OK", "application/json", json);
    } else {
        sendResponse(client, "404 Not Found", "application/json",
            "{\"error\":\"Element not found\"}");
    }

    } else if (strcmp(path, "/") == 0 ||
               strcmp(path, "/index.html") == 0) {
        sendFile(client, "public/index.html", "text/html");
    } else if (strcmp(path, "/style.css") == 0) {
        sendFile(client, "public/style.css", "text/css");
    } else if (strcmp(path, "/script.js") == 0) {
        sendFile(client, "public/script.js",
                 "text/javascript");
    } else {
        sendResponse(client, "404 Not Found",
                     "text/plain", "Page not found");
    }
}

int main(void) {
#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        printf("Could not initialize Windows sockets.\n");
        return 1;
    }
#endif

    int port = 8080;
    char *portText = getenv("PORT");

    if (portText && atoi(portText) > 0) {
        port = atoi(portText);
    }

    socket_t server = socket(AF_INET, SOCK_STREAM, 0);

#ifdef _WIN32
    if (server == INVALID_SOCKET) {
#else
    if (server < 0) {
#endif
        printf("Could not create socket.\n");
        return 1;
    }

    int reuse = 1;
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR,
               (const char *)&reuse, sizeof(reuse));

    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons((unsigned short)port);

    if (bind(server, (struct sockaddr *)&address,
             sizeof(address)) < 0) {
        printf("Could not bind port %d.\n", port);
        CLOSESOCKET(server);
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }

    if (listen(server, 10) < 0) {
        printf("Could not start listening.\n");
        CLOSESOCKET(server);
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }

    printf("Periodic Table server running on port %d\n", port);

    while (1) {
        socket_t client = accept(server, NULL, NULL);

#ifdef _WIN32
        if (client == INVALID_SOCKET) continue;
#else
        if (client < 0) continue;
#endif

        char request[BUFFER_SIZE];
        int received = recv(client, request,
                            sizeof(request) - 1, 0);

        if (received > 0) {
            request[received] = '\0';
            handleRequest(client, request);
        }

        CLOSESOCKET(client);

    }

    CLOSESOCKET(server);

#ifdef _WIN32
    WSACleanup();
#endif

    return 0;
}

