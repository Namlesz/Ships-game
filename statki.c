#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <netdb.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/shm.h>

//definujemy port na którym będziemy pracować
#define MY_PORT "19999"

//zmienne globalne które wymagają zamknięcia
int sockfd, s, shmid;
char *shared_mem;

// ustawiamy komunikat w pamięci współdzielonej
void setCall(char *call)
{
    memcpy(shared_mem, call, 15);
}

// czekamy na wskazany komunikat w pamięci współdzielonej
void waitForCall(char *call)
{
    while (1)
    {
        if (strcmp(shared_mem, call) == 0)
            break;
    }
}

// zamieniamy znak na liczbę A->1, B->2, C->3, D->4
int letterToNumber(char letter)
{
    if (letter == 'A')
        return 0;
    else if (letter == 'B')
        return 1;
    else if (letter == 'C')
        return 2;
    else if (letter == 'D')
        return 3;
    else
        return -1;
}

// sprawdzamy czy pola bezpośrednio obok wskazanego, są zajęte
int isNear(char *position, char playerPlayfield[4][4])
{
    int a, b;

    b = position[1] - '0' - 1; //niezmienne
    a = letterToNumber(position[0]) - 1;
    if (a != -1)
        if (playerPlayfield[a][b] == '1')
            return 1;

    a = letterToNumber(position[0]) + 1;
    if (a != -1)
        if (playerPlayfield[a][b] == '1')
            return 1;

    a = letterToNumber(position[0]); //niezmienne
    b = position[1] - '0' - 2;
    if (a != -1 && b < 5)
        if (playerPlayfield[a][b] == '1')
            return 1;

    b = position[1] - '0';
    if (a != -1 && b < 5)
        if (playerPlayfield[a][b] == '1')
            return 1;

    return -1;
}

// waliduje i ustawia na wskazanym polu statek
void setField(char *message, char playerPlayfield[4][4], int numOfFields)
{
    char position[15];
    int x, y; // x -> A; y -> 1
    printf("%s", message);
    scanf("%s", position);
    //sprawdzamy czy pole jest zajęte, czy dane są poprawnie wprowadzone i czy obok nie stoi już statek
    while (strlen(position) > 2 || (x = letterToNumber(position[0])) == -1 || (y = position[1] - '0') > 4 || playerPlayfield[x][y] == '1' || isNear(position, playerPlayfield) != -1)
    {
        printf("---Bledne dane---\nZa maly odstep|zla liczba|zla cyfra|pole zajete\nWprowadz ponownie: ");
        scanf("%s", position);
    }
    y--;
    playerPlayfield[x][y] = '1';

    //ustawiamy drugie pole z tą różnicą, że dwumasztowiec musi stać obok drugiego pola z 1 masztem
    if (numOfFields == 2)
    {
        scanf("%s", position);
        while ((x = letterToNumber(position[0])) == -1 || (y = position[1] - '0') > 4 || playerPlayfield[x][y] == '1' || isNear(position, playerPlayfield) != 1)
        {
            printf("---Bledne dane---\nZa maly odstep|zla liczba|zla cyfra|pole zajete\nWprowadz ponownie: ");
            scanf("%s", position);
        }
        y--;
        playerPlayfield[x][y] = '1';
    }
}

// przygotowujemy plansze do gry
void setUpPlayground(char playerPlayfield[4][4]){
        memset(playerPlayfield, ' ', sizeof(playerPlayfield[0][0]) * 16);
        setField("1. jednomasztowiec: ", playerPlayfield, 1);
        setField("2. jednomasztowiec: ", playerPlayfield, 1);
        setField("3. dwumasztowiec: ", playerPlayfield, 2);
}

// wypisujemy wskazaną plansze
void printPlayground(char playground[4][4]){
        int i, j;
        char tmp[4] = {'A', 'B', 'C', 'D'};
        printf("\n.| ");

        for(i=1;i<5;i++)
            printf("%d ",i);

        printf("\n-|--------\n");
        
        for (i = 0; i < 4; i++)
        {
            printf("%c| ", tmp[i]);
            for (j = 0; j < 4; j++)
            {
                printf("%c ", playground[i][j]);
            }
            printf("\n");
        }
}

// zamykamy deskryptory i wychodzimy z programu
void clearAndExit()
{
    close(s);
    close(sockfd);
    shmdt(shared_mem);
    shmctl(shmid, IPC_RMID, NULL);
    exit(EXIT_SUCCESS);
}

void sigHandler(int signo)
{
    if (signo == SIGINT)
    {
        clearAndExit();
    }
    return;
}

int main(int argc, char *argv[])
{
    // tworzymy pamięc współdzieloną 
    key_t key; ;
    if ((key = ftok("statki.c", 65)) == -1) {
        perror("ftok");
        exit(1);
    }

    if ((shmid = shmget(key, 1024, 0666 | IPC_CREAT)) == -1) {
        perror("ftok");
        exit(1);
    }
    shared_mem = (char *)shmat(shmid, (void *)0, 0);
    
    // przechwytujemy sygnały
    signal(SIGINT, sigHandler);

    struct addrinfo hints;
    struct addrinfo *result, *rp;
    char command[30];
    
    pid_t PID, ppid;
    ppid = getpid();
    PID = fork();
    //tworzymy dziecko, które odpowiada za komunikację z przeciwnikiem
    //natomiast rodzic stale nasłuchuje i przetwarza odbierane dane
    if (PID < 0)
    {
        fprintf(stderr, "fork(): error\n");
        exit(EXIT_FAILURE);
    }
    else if (PID == 0)
    {
        //ustawiamy dziecko na wysył komunikatów
        memset(&hints, 0, sizeof(struct addrinfo));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_DGRAM;
        hints.ai_flags = 0;
        hints.ai_protocol = 0;
        struct sockaddr_in *addr;
        s = getaddrinfo(argv[1], MY_PORT, &hints, &result);
        if (s != 0)
        {
            fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
            exit(EXIT_FAILURE);
        }

        for (rp = result; rp != NULL; rp = rp->ai_next)
        {
            sockfd = socket(rp->ai_family, rp->ai_socktype,
                            rp->ai_protocol);
            if (sockfd == -1)
                continue;

            if (connect(sockfd, rp->ai_addr, rp->ai_addrlen) != -1)
            {
                addr = (struct sockaddr_in *)rp->ai_addr;
                break;
            }

            close(sockfd);
        }

        if (rp == NULL)
        {
            fprintf(stderr, "Could not connect\n");
            exit(EXIT_FAILURE);
        }
        freeaddrinfo(result);

        char nickname[10];
        if (argc > 2)
        {
            strcpy(nickname, argv[2]);
        }
        else
        {
            strcpy(nickname, "NN");
        }
        printf("Rozpoczynam gre z %s. Napisz <koniec> by zakonczyc. Ustal polozenie swoich okretow:\n", inet_ntoa((struct in_addr)addr->sin_addr));
        setCall("ready");
        waitForCall("unpause");

        while (1)
        {
            //Pobieraj polecenia od użytkownika i dodaj nick jeśli został utworzony
            memset(command, 0, sizeof(command));

            scanf("%s", command);

            if (strcmp(command, "t") == 0 || strcmp(command, "n") == 0)
            {
                setCall(command);
                waitForCall("write");
            }

            char tmp[10];
            memcpy(tmp, command, 8);

            strcat(command, "|");
            strcat(command, nickname);

            //wysyłamy dane do drugiego użytkownika
            sendto(sockfd, &command, sizeof(command), 0,
                   NULL, 0);

            if (strcmp(tmp, "<koniec>") == 0)
            {
                kill(ppid, SIGINT);
                clearAndExit();
            }
        }

        // close(sockfd);
    }
    else
    {
        char playerPlayfield[4][4];

        //ustawiamy rodzica na nasłuch komunikatów
        memset(&hints, 0, sizeof(struct addrinfo));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_DGRAM;
        hints.ai_flags = AI_PASSIVE;
        hints.ai_protocol = 0;
        hints.ai_canonname = NULL;
        hints.ai_addr = NULL;
        hints.ai_next = NULL;

        s = getaddrinfo((const char *)NULL, MY_PORT, &hints, &result);
        if (s != 0)
        {
            fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
            exit(EXIT_FAILURE);
        }

        for (rp = result; rp != NULL; rp = rp->ai_next)
        {
            sockfd = socket(rp->ai_family, rp->ai_socktype,
                            rp->ai_protocol);
            if (sockfd == -1)
                continue;

            if (bind(sockfd, rp->ai_addr, rp->ai_addrlen) == 0)
                break;

            close(sockfd);
        }

        freeaddrinfo(result);
        // ustawiamy statki
        waitForCall("ready");

        setUpPlayground(playerPlayfield);
        printf("TWOJE USTAWIENIE STATKÓW:");
        printPlayground(playerPlayfield);

        setCall("unpause");
        struct sockaddr_in from;
        socklen_t len = sizeof(from);
        while (1)
        {
            recvfrom(sockfd, &command, sizeof(command), 0, (struct sockaddr *)&from, &len);

            char *msg = strtok(command, "|");
            char *nick = strtok(NULL, "|");
            if (strcmp(msg, "<koniec>") == 0)
            {
                printf("[%s (%s) zakonczyl gre, czy chcesz przygotowac nowa plansze?(t/n)]\n", nick, inet_ntoa(from.sin_addr));
                while (1)
                {
                    if (strcmp(shared_mem, "t") == 0)
                    {
                        setUpPlayground(playerPlayfield);
                        printf("TWOJE USTAWIENIE STATKÓW:");
                        printPlayground(playerPlayfield);
                        setCall("write");
                        break;
                    }
                    else if (strcmp(shared_mem, "n") == 0)
                    {
                        kill(PID, SIGINT);
                        clearAndExit();
                    }
                }
                continue;
            }

            printf("[%s (%s): %s] \n", nick, inet_ntoa(from.sin_addr), msg);
        }
    }

    wait(NULL);
    return 0;
}