#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <poll.h>
#include <signal.h>
#include <ctype.h>
#include <time.h>

#define MAX_BUFFER 1024
#define MAX_NAME 16
socklen_t slen = sizeof(struct sockaddr_in);

char buffer[MAX_BUFFER];

// message struct
struct game_struct {
    int cmd; // always present (0 - game start, 1 - start acknowledged, 2 - game move, 3 - game quit)
    int number; // present only if cmd == 2
	char name[MAX_NAME]; // present only if cmd == 0 or 1
};

// connection
int sockfd;
struct sockaddr_in *client_addr;
struct addrinfo *result;

// game state
char client_name[MAX_NAME]; // client = remote player
char host_name[MAX_NAME]; // host = this player
int client_score;
int host_score;
int number;
int turn; // 0 for host, 1 for client, -1 for no game
int prev_player; // 0 for host, 1 for client

void cleanup(int sig) {
    close(sockfd);
    exit(sig ? EXIT_FAILURE : EXIT_SUCCESS);
}

int is_number(char *s) {
    if (s[0] == '\0' || s[0] == '\n') return 0;
    
    int i = 0;
    while (s[i] != '\0' && s[i] != '\n') {
        if (!isdigit(s[i]) && !(i == 0 && s[i] == '-')) {
            return 0;
        }
        i++;
    }
    return 1;
}

int min(int a, int b) {
    return a < b ? a : b;
}

// random int between [min, max]
int random_int(int min, int max) {
	return min + rand() % (max - min + 1);
}

// check if x is in range [min, max]
int in_range(int x, int min, int max) {
	return x >= min && x <= max;
}

void send_start() {
    printf("Rozpoczynam gre z %s.", inet_ntoa(client_addr->sin_addr));
    printf(" Napisz \"koniec\" by zakonczyc lub \"wynik\" by wyswietlic aktualny wynik.\n");

    struct game_struct msg = {
        .cmd = 0,
        .number = 0,
    };
    strcpy(msg.name, host_name);

    if (sendto(sockfd, &msg, sizeof(msg), 0, (struct sockaddr *)client_addr, slen) < 0) {
        fprintf(stderr, "Nie udalo sie dostarczyc wiadomosci, error:\n");
        perror("sendto");
        cleanup(EXIT_FAILURE);
    }

    printf("Propozycja gry wyslana.\n");
}

// gets input from stdin, evaluates it to game command and executes it
// sends game command to other player
// return 0 - quit loop, 1 - continue
int process_input() {
    struct game_struct msg = {
        .cmd = 0,
        .number = 0,
        .name = ""
    };

    fgets(buffer, MAX_BUFFER, stdin);

    if (strcmp(buffer, "koniec\n") == 0) {
        msg.cmd = 3;
        if (sendto(sockfd, &msg, sizeof(msg), 0, (struct sockaddr *)client_addr, slen) < 0) {
            fprintf(stderr, "Nie udalo sie dostarczyc wiadomosci, error:\n");
            perror("sendto");
            cleanup(EXIT_FAILURE);
        }
        return 0;
    }
    
    if (strcmp(buffer, "wynik\n") == 0) {
        if (turn == -1) {
            printf("Nie mozesz wyswietlic wyniku, poniewaz nie masz przeciwnika.\n");
            return 1;
        }

        printf("Ty %d : %d %s \n", host_score, client_score, client_name);
        return 1;
    }
    
    if (is_number(buffer)) {
        if (turn == -1) {
            printf("Nie mozesz podac wartosci, poniewaz nie masz przeciwnika.\n");
            return 1;
        }

        if (turn == 1) {
            printf("Teraz tura gracza %s, poczekaj na swoja kolej.\n", client_name);
            return 1;
        }

        int temp = atoi(buffer);

        if (in_range(temp, number + 1, min(number + 10, 50))) {
            number = temp;

            msg.cmd = 2;
            msg.number = number;

            if (sendto(sockfd, &msg, sizeof(msg), 0, (struct sockaddr *)client_addr, slen) < 0) {
                fprintf(stderr, "Nie udalo sie dostarczyc wiadomosci, error:\n");
                perror("sendto");
                cleanup(EXIT_FAILURE);
            }

            turn = 1;

            if (number == 50) {
                printf("Wygrana!\n");
                host_score++;
                prev_player = !prev_player;

                if (prev_player == 0) {
                    printf("Zaczynamy kolejna rozgrywke.\n");
                    number = random_int(1, 10);
                    printf("Losowa wartosc poczatkowa: %d, podaj kolejna wartosc.\n", number);
                    turn = 0;
                } else {
                    printf("Zaczynamy kolejna rozgrywke, poczekaj na swoja kolej.\n");
                }
            }
        } else {
            printf("Takiej wartosci nie mozesz wybrac!\n");
        }

        return 1;
    }

    printf("Niepoprawna komenda!\n");
    return 1;
}

// recives game command from other player and executes it
void process_recive() {
    struct game_struct msg = {
        .cmd = 0,
        .number = 0,
        .name = ""
    };

    if (recvfrom(sockfd, &msg, sizeof(msg), 0, (struct sockaddr *)client_addr, &slen) < 0) {
        fprintf(stderr, "Nie udalo sie odebrac wiadomosci, error:\n");
        perror("recvfrom");
        cleanup(EXIT_FAILURE);
    }

    if (msg.cmd == 0) {
        if (msg.name[0] == '\0') {
            strcpy(client_name, inet_ntoa(client_addr->sin_addr));
        } else {
            strcpy(client_name, msg.name);
        }

        printf("%s dolaczyl do gry.\n", client_name);
        
        msg.cmd = 1;
        msg.number = 0;
        strcpy(msg.name, host_name);

        if (sendto(sockfd, &msg, sizeof(msg), 0, (struct sockaddr *)client_addr, slen) < 0) {
            fprintf(stderr, "Nie udalo sie dostarczyc wiadomosci, error:\n");
            perror("sendto");
            cleanup(EXIT_FAILURE);
        }

        host_score = 0;
        client_score = 0;
        prev_player = 0;

        number = random_int(1, 10);
        printf("Losowa wartosc poczatkowa: %d, podaj kolejna wartosc.\n", number);
        turn = 0;
        return;
    }

    if (msg.cmd == 1) {
        if (msg.name[0] == '\0') {
            strcpy(client_name, inet_ntoa(client_addr->sin_addr));
        } else {
            strcpy(client_name, msg.name);
        }

        printf("%s dolaczyl do gry.\n", client_name);

        turn = 1;
        return;
    }
       
    if (msg.cmd == 2) {
        number = msg.number;

        if (number == 50) {
            printf("%s podal wartosc %d.\nPrzegrana!\n", client_name, number);
            client_score++;
            prev_player = !prev_player;

            if (prev_player == 0) {
                printf("Zaczynamy kolejna rozgrywke.\n");
                number = random_int(1, 10);
                printf("Losowa wartosc poczatkowa: %d, podaj kolejna wartosc.\n", number);
                turn = 0;
            } else {
                printf("Zaczynamy kolejna rozgrywke, poczekaj na swoja kolej.\n");
                turn = 1;
            }
        } else {
            printf("%s podal wartosc %d, podaj kolejna wartosc.\n", client_name, number);
            turn = 0;
        }

        return;
    }

    if (msg.cmd == 3) {
        printf("%s zakonczyl gre, mozesz poczekac na kolejnego gracza.\n", client_name);
        printf("Gra zakonczyla sie wynikiem:\nTy %d : %d %s\n", host_score, client_score, client_name);
        turn = -1;
        return;
    }
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        // debug is second port number for testing on one machine
        fprintf(stderr, "Sposob uzycia: %s <IP> <port> [nickname] [debug]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    printf("Gra w 50, wersja A.\n");

    srand(time(NULL));

    char *ip = argv[1];
    char* my_port = argv[2];
    char *nickname = argc > 3 ? argv[3] : "";
    char* port = (argc > 4) ? argv[4] : argv[2];

    turn = -1;
    prev_player = 1;

    if (strlen(nickname) > MAX_NAME - 1) {
        fprintf(stderr, "Nazwa nie moze byc dluzsza niz %d znakow.\n", MAX_NAME - 1);
        exit(EXIT_FAILURE);
    }

    strcpy(host_name, nickname);

    struct addrinfo hints;
	struct addrinfo *rp;
    int err;

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_INET;    
	hints.ai_socktype = SOCK_DGRAM; 
	hints.ai_flags = AI_PASSIVE;

	if ((err = getaddrinfo(NULL, my_port, &hints, &result)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(err));
		exit(EXIT_FAILURE);
	}

	for(rp = result; rp != NULL; rp = rp->ai_next) {
		sockfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if (sockfd == -1) continue;
		if (bind(sockfd, rp->ai_addr, rp->ai_addrlen) == 0) break;
		close(sockfd);
	}

	if (rp == NULL) {
		fprintf(stderr, "Nie udalo sie zbindowac gniazda\n");
		exit(EXIT_FAILURE);
	}

    freeaddrinfo(result);
    signal(SIGINT, cleanup);

    if (fcntl(sockfd, F_SETFL, O_NONBLOCK) < 0) {
        perror("fcntl");
        cleanup(EXIT_FAILURE);
    }

	if ((err = getaddrinfo(ip, port, &hints, &result)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(err));
		cleanup(EXIT_FAILURE);
	}

    client_addr = (struct sockaddr_in *)result->ai_addr;

    struct pollfd fds[2];
    fds[0].fd = STDIN_FILENO;
    fds[0].events = POLLIN;
    fds[1].fd = sockfd;
    fds[1].events = POLLIN;
   
    // game start
    send_start();

    while (1) {
        if (poll(fds, 2, -1) < 0) {
            perror("poll");
            cleanup(EXIT_FAILURE);
        }

        // stdin
        if (fds[0].revents & POLLIN) {
            if (process_input() == 0) break;
        }
        
        // socket
        if (fds[1].revents & POLLIN) {
            process_recive();
        }
    }

    freeaddrinfo(result);
    cleanup(EXIT_SUCCESS);
}
