#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <pthread.h>
#include <signal.h>

#define BUF_SIZE 100
#define NAME_SIZE 20

void* send_msg(void* arg);
void* recv_msg(void* arg);
void error_handling(char* msg);
void check_weather_and_notify(int* sock);

char name[NAME_SIZE] = "[Default]";
char msg[BUF_SIZE];

int main(int argc, char* argv[]) {
    int sock;
    struct sockaddr_in serv_addr;
    pthread_t snd_thread, rcv_thread;
    void* thread_return;

    if (argc != 4) {
        printf("Usage : %s <IP> <port> <name>\n", argv[0]);
        exit(1);
    }

    sprintf(name, "%s", argv[3]);

    sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock == -1)
        error_handling("socket() error");

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
    serv_addr.sin_port = htons(atoi(argv[2]));

    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1)
        error_handling("connect() error");

    sprintf(msg, "[%s:PASSWD]", name);
    write(sock, msg, strlen(msg));

    pthread_create(&rcv_thread, NULL, recv_msg, (void*)&sock);
    pthread_create(&snd_thread, NULL, send_msg, (void*)&sock);

    pthread_join(snd_thread, &thread_return);
    pthread_join(rcv_thread, &thread_return);

    close(sock);
    return 0;
}

void* send_msg(void* arg) {
    int* sock = (int*)arg;
    int str_len;
    int ret;
    fd_set initset, newset;
    struct timeval tv;
    char name_msg[NAME_SIZE + BUF_SIZE + 2];

    FD_ZERO(&initset);
    FD_SET(STDIN_FILENO, &initset);

    fputs("Input a message! [ID]msg (Default ID:ALLMSG)\n", stdout);
    while (1) {
        memset(msg, 0, sizeof(msg));
        name_msg[0] = '\0';
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        newset = initset;
        ret = select(STDIN_FILENO + 1, &newset, NULL, NULL, &tv);
        if (FD_ISSET(STDIN_FILENO, &newset)) {
            fgets(msg, BUF_SIZE, stdin);
            if (!strncmp(msg, "quit\n", 5)) {
                *sock = -1;
                return NULL;
            }
            else if (msg[0] != '[') {
                strcat(name_msg, "[ALLMSG]");
                strcat(name_msg, msg);
            }
            else {
                strcpy(name_msg, msg);
            }
            if (write(*sock, name_msg, strlen(name_msg)) <= 0) {
                *sock = -1;
                return NULL;
            }
        }
        if (ret == 0) {
            if (*sock == -1)
                return NULL;
        }
    }
}

void* recv_msg(void* arg) {
    int* sock = (int*)arg;
    int str_len;
    char name_msg[NAME_SIZE + BUF_SIZE + 1];

    while (1) {
        memset(name_msg, 0x0, sizeof(name_msg));
        str_len = read(*sock, name_msg, NAME_SIZE + BUF_SIZE);
        if (str_len <= 0) {
            *sock = -1;
            return NULL;
        }
        name_msg[str_len] = 0;
        fputs(name_msg, stdout);

        if (!strncmp(name_msg, "[KKH_STM]MOVE@ON", 16)) {
            system("python3 api_weather.py");
            check_weather_and_notify(sock);
        }
    }
}

void check_weather_and_notify(int* sock) {
    FILE* file;
    char buffer[7001];
    size_t bytes_read;

    printf("Opening weather_forecast.txt...\n");
    file = fopen("weather_forecast.txt", "r");
    if (file == NULL) {
        perror("Failed to open weather_forecast.txt");
        return;
    }

    bytes_read = fread(buffer, 1, 7000, file);
    buffer[bytes_read] = '\0';
    fclose(file);

    if (strstr(buffer, "rain") != NULL) {
        printf("Rain detected\n");
        if (write(*sock, "[ALLMSG]RAIN@ON\n", 16) <= 0) {
            perror("Failed to send RAIN@ON");
            *sock = -1;
        }
    }
    else {
        printf("No rain detected\n");
        if (write(*sock, "[ALLMSG]RAIN@OFF\n", 17) <= 0) {
            perror("Failed to send RAIN@OFF");
            *sock = -1;
        }
    }
}

void error_handling(char* msg) {
    fputs(msg, stderr);
    fputc('\n', stderr);
    exit(1);
}