#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

#define PORT 8080
#define BUFFER_SIZE 4096

typedef struct {
    int id;
    char name[100];
    char description[256];
    char due_date[11]; // YYYY-MM-DD
    char due_time[6];  // HH:MM
    char priority[10]; // High, Medium, Low
} Task;

typedef struct Node {
    Task task;
    struct Node* next;
} Node;

Node* head = NULL;
int last_id = 0;

void add_task(char* name, char* description, char* due_date, char* due_time, char* priority) {
    Node* new_node = (Node*)malloc(sizeof(Node));
    new_node->task.id = ++last_id;
    strcpy(new_node->task.name, name);
    strcpy(new_node->task.description, description);
    strcpy(new_node->task.due_date, due_date);
    strcpy(new_node->task.due_time, due_time);
    strcpy(new_node->task.priority, priority);
    new_node->next = head;
    head = new_node;
}

void delete_task(int id) {
    Node* current = head;
    Node* prev = NULL;

    while (current != NULL) {
        if (current->task.id == id) {
            if (prev == NULL) {
                head = current->next;
            } else {
                prev->next = current->next;
            }
            free(current);
            return;
        }
        prev = current;
        current = current->next;
    }
}

void modify_task(int id, char* name, char* description, char* due_date, char* due_time, char* priority) {
    Node* current = head;
    while (current != NULL) {
        if (current->task.id == id) {
            // Overwrite all fields since partial updates are handled by frontend sending full object
            strcpy(current->task.name, name);
            strcpy(current->task.description, description);
            strcpy(current->task.due_date, due_date);
            strcpy(current->task.due_time, due_time);
            strcpy(current->task.priority, priority);
            return;
        }
        current = current->next;
    }
}

void update_priorities() {
    time_t now = time(NULL);
    struct tm* local_time = localtime(&now);
    char current_date_str[11];
    strftime(current_date_str, 11, "%Y-%m-%d", local_time);

    Node* current = head;
    while (current != NULL) {
        // Parse due date
        struct tm due_tm = {0};
        // Very basic parsing for YYYY-MM-DD
        if (sscanf(current->task.due_date, "%d-%d-%d", &due_tm.tm_year, &due_tm.tm_mon, &due_tm.tm_mday) == 3) {
            due_tm.tm_year -= 1900;
            due_tm.tm_mon -= 1;
            due_tm.tm_sec = 0;
            due_tm.tm_min = 0;
            due_tm.tm_hour = 0;
            due_tm.tm_isdst = -1; // Let mktime determine DST

            // Add time if available (HH:MM)
            int h, m;
             if (sscanf(current->task.due_time, "%d:%d", &h, &m) == 2) {
                due_tm.tm_hour = h;
                due_tm.tm_min = m;
            }

            time_t due_time_t = mktime(&due_tm);
            double seconds_diff = difftime(due_time_t, now);
            double days_diff = seconds_diff / (60 * 60 * 24);

            if (days_diff <= 2) {
                strcpy(current->task.priority, "High");
            } else if (days_diff <= 6) {
                if (strcmp(current->task.priority, "High") != 0) { // Don't downgrade High
                    strcpy(current->task.priority, "Medium");
                }
            }
        }
        current = current->next;
    }
}

char* get_tasks_json() {
    update_priorities();
    
    // Estimate size (rough)
    int size = 1024;
    Node* current = head;
    while(current){
        size += 512;
        current = current->next;
    }
    
    char* json = (char*)malloc(size);
    strcpy(json, "[");
    current = head;
    while (current != NULL) {
        char buffer[512];
        sprintf(buffer, "{\"id\":%d, \"name\":\"%s\", \"description\":\"%s\", \"due_date\":\"%s\", \"due_time\":\"%s\", \"priority\":\"%s\"}",
                current->task.id, current->task.name, current->task.description, current->task.due_date, current->task.due_time, current->task.priority);
        strcat(json, buffer);
        if (current->next != NULL) {
            strcat(json, ",");
        }
        current = current->next;
    }
    strcat(json, "]");
    return json;
}

// Helper to extract JSON string value by key
void extract_json_value(char* body, const char* key, char* dest, int max_len) {
    char search_key[128];
    sprintf(search_key, "\"%s\":\"", key);
    char* p = strstr(body, search_key);
    if (p) {
        p += strlen(search_key);
        int i = 0;
        while (*p && i < max_len - 1) {
            if (*p == '"') break;
            if (*p == '\\') {
                p++;
                if (*p == '"') dest[i++] = '"';
                else if (*p == '\\') dest[i++] = '\\';
                else dest[i++] = *p; // fallback
                if (*p) p++;
            } else {
                dest[i++] = *p++;
            }
        }
        dest[i] = '\0';
    }
}

void parse_body_and_action(char* body, int is_update) {
    char name[100] = "", desc[256] = "", date[11] = "", time_str[6] = "", priority[10] = "Low";
    int id = -1;
    
    char* p;
    // Parse ID if present (supports numeric or string)
    if ((p = strstr(body, "\"id\":"))) {
        p += 5; // skip "id":
        if (*p == '"') {
            char id_str[20];
            extract_json_value(body, "id", id_str, 20);
            id = atoi(id_str);
        } else {
             sscanf(p, "%d", &id);
        }
    }

    // Improved parsing for string fields
    extract_json_value(body, "name", name, 100);
    extract_json_value(body, "description", desc, 256);
    extract_json_value(body, "due_date", date, 11);
    extract_json_value(body, "due_time", time_str, 6);
    extract_json_value(body, "priority", priority, 10);

    if (is_update && id != -1) {
        modify_task(id, name, desc, date, time_str, priority);
    } else {
        add_task(name, desc, date, time_str, priority);
    }
}

const char* get_mime_type(const char* path) {
    const char* ext = strrchr(path, '.');
    if (!ext) return "text/plain";
    if (strcmp(ext, ".html") == 0) return "text/html";
    if (strcmp(ext, ".css") == 0) return "text/css";
    if (strcmp(ext, ".js") == 0) return "application/javascript";
    return "text/plain";
}

void handle_client(SOCKET client_socket) {
    char buffer[BUFFER_SIZE];
    int valread = recv(client_socket, buffer, BUFFER_SIZE, 0);
    if (valread <= 0) {
        closesocket(client_socket);
        return;
    }
    
    buffer[valread] = '\0';
    
    char method[10], full_path[100], protocol[10];
    sscanf(buffer, "%s %s %s", method, full_path, protocol);
    
    // Split path and query
    char path[100];
    char* query = strchr(full_path, '?');
    if (query) {
        strncpy(path, full_path, query - full_path);
        path[query - full_path] = '\0';
        query++; // skip '?'
    } else {
        strcpy(path, full_path);
    }

    if (strcmp(method, "GET") == 0) {
        if (strcmp(path, "/tasks") == 0) {
            char* json = get_tasks_json();
            char response[BUFFER_SIZE + 4096]; // Careful with size
            sprintf(response, "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n%s", json);
            send(client_socket, response, strlen(response), 0);
            free(json);
        } else {
            // Serve static files
            char filepath[256] = ".";
            if (strcmp(path, "/") == 0) {
                strcat(filepath, "/index.html");
            } else {
                strcat(filepath, path);
            }
            
            FILE* f = fopen(filepath, "rb");
            if (f) {
                fseek(f, 0, SEEK_END);
                long fsize = ftell(f);
                fseek(f, 0, SEEK_SET);
                
                char* fcontent = (char*)malloc(fsize + 1);
                fread(fcontent, 1, fsize, f);
                fclose(f);
                fcontent[fsize] = 0;
                
                char header[512];
                sprintf(header, "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length: %ld\r\n\r\n", get_mime_type(filepath), fsize);
                send(client_socket, header, strlen(header), 0);
                send(client_socket, fcontent, fsize, 0);
                free(fcontent);
            } else {
                char* not_found = "HTTP/1.1 404 Not Found\r\n\r\n404 Not Found";
                send(client_socket, not_found, strlen(not_found), 0);
            }
        }
    } else if (strcmp(method, "POST") == 0 && strcmp(path, "/tasks") == 0) {
        // Find body
        char* body = strstr(buffer, "\r\n\r\n");
        if (body) {
            body += 4;
            parse_body_and_action(body, 0);
            char* response = "HTTP/1.1 201 Created\r\n\r\nCreated";
            send(client_socket, response, strlen(response), 0);
        }
    } else if (strcmp(method, "PUT") == 0 && strcmp(path, "/tasks") == 0) {
        // Find body
        char* body = strstr(buffer, "\r\n\r\n");
        if (body) {
            body += 4;
            parse_body_and_action(body, 1); // 1 = modify
            char* response = "HTTP/1.1 200 OK\r\n\r\nUpdated";
            send(client_socket, response, strlen(response), 0);
        }
    } else if (strcmp(method, "DELETE") == 0 && strcmp(path, "/tasks") == 0) {
        // Parse ID from query (id=123)
        if (query) {
            int id = -1;
            if (sscanf(query, "id=%d", &id) == 1) {
                delete_task(id);
                char* response = "HTTP/1.1 200 OK\r\n\r\nDeleted";
                send(client_socket, response, strlen(response), 0);
            } else {
                 char* response = "HTTP/1.1 400 Bad Request\r\n\r\nInvalid ID";
                 send(client_socket, response, strlen(response), 0);
            }
        }
    }
    
    closesocket(client_socket);
}

int main() {
    WSADATA wsa;
    SOCKET server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        printf("Failed. Error Code : %d", WSAGetLastError());
        return 1;
    }

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
        printf("Could not create socket : %d", WSAGetLastError());
        return 1;
    }

    // Set socket options to reuse address/port to avoid "Bind failed" on restart
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt));

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) == SOCKET_ERROR) {
        printf("Bind failed with error code : %d", WSAGetLastError());
        return 1;
    }

    listen(server_fd, 3);
    printf("Server started on port %d\n", PORT);

    while (1) {
        new_socket = accept(server_fd, (struct sockaddr *)&address, &addrlen);
        if (new_socket == INVALID_SOCKET) {
            printf("accept failed with error code : %d", WSAGetLastError());
            continue; // Don't exit, just try next
        }
        handle_client(new_socket);
    }

    closesocket(server_fd);
    WSACleanup();
    return 0;
}
