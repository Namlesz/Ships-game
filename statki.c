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
#include <time.h>

//definujemy port na którym będziemy pracować
#define MY_PORT "19999"
#define FILE_NAME "statki.c"
#define MSG_SIZE 50
//zmienne globalne które wymagają zamknięcia

int sockfd, s, shmid;
char *shared_mem;
char shootField[4][4];
int isMyTurn;
char command[MSG_SIZE];

// ustawiamy komunikat w pamięci współdzielonej
void setCall(char *call)
{
    memset(shared_mem, 0, MSG_SIZE);
    memcpy(shared_mem, call, MSG_SIZE);
}

// czekamy na wskazany komunikat w pamięci współdzielonej
void waitForCall(char *call)
{
    while (1)
    {
        if (strcmp(shared_mem, call) == 0)
        {
            setCall("EMPTY");
            break;
        }
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
int isNear(char *position, int playerPlayfield[4][4])
{
    int a, b;

    b = position[1] - '0' - 1; //niezmienne
    a = letterToNumber(position[0]) - 1;
    if (a != -1)
        if (playerPlayfield[a][b] == 1)
            return 1;

    a = letterToNumber(position[0]) + 1;
    if (a != -1)
        if (playerPlayfield[a][b] == 1)
            return 1;

    a = letterToNumber(position[0]); //niezmienne
    b = position[1] - '0' - 2;
    if (a != -1 && b < 5)
        if (playerPlayfield[a][b] == 1)
            return 1;

    b = position[1] - '0';
    if (a != -1 && b < 5)
        if (playerPlayfield[a][b] == 1)
            return 1;

    return -1;
}

// waliduje i ustawia na wskazanym polu statek
void setField(char *message, char playerShip[4][3], int id, int playerPlayfield[4][4])
{
    char position[15] = {};
    int x, y; // x -> A; y -> 1

    int nearFlag;
    if (id == 3)
        nearFlag = 1;
    else
        nearFlag = -1;
    printf("%s", message);
    scanf("%s", position);

    //sprawdzamy czy pole jest zajęte, czy dane są poprawnie wprowadzone i czy obok nie stoi już statek
    while (strlen(position) > 2 || (x = letterToNumber(position[0])) == -1 || (y = position[1] - '0') > 4 || playerPlayfield[x][y] == 1 || isNear(position, playerPlayfield) != nearFlag)
    {
        printf("---Bledne dane---\n---Za maly odstep|zla liczba|zla cyfra|pole zajete---\nWprowadz ponownie: ");
        scanf("%s", position);
    }
    y--;
    playerPlayfield[x][y] = 1;
    memcpy(playerShip[id], position, sizeof(position));
}

// przygotowujemy plansze do gry
void setUpPlayground(char playerShips[4][3])
{
    int tmp[4][4];
    memset(tmp, 0, sizeof(tmp[0][0]) * 16);
    memset(playerShips, ' ', sizeof(playerShips[0][0]) * 8);
    //Na razie plansza do której strzelamy jest pusta
    memset(shootField, ' ', sizeof(shootField[0][0]) * 16);

    setField("1. jednomasztowiec: ", playerShips, 0, tmp);
    setField("2. jednomasztowiec: ", playerShips, 1, tmp);
    setField("3. dwumasztowiec: ", playerShips, 2, tmp);
    setField("", playerShips, 3, tmp);
}

// wypisujemy wskazaną plansze
void printPlayground(char playground[4][4])
{
    int i, j;
    char tmp[4] = {'A', 'B', 'C', 'D'};
    printf("\n.| ");

    for (i = 1; i < 5; i++)
        printf("%d ", i);

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

// ustawiamy wskazane pole na wskazany znak
void setShootField(char c, char *pos, char shootField[4][4])
{
    shootField[letterToNumber(pos[0])][pos[1] - '0' - 1] = c;
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

// tworzymy nową gre
void newGame(pid_t PID, char playerShips[4][3], time_t * ourTime)
{
    setCall("newgame");
    while (1)
    {
        sleep(0.5);
        if (strcmp(shared_mem, "t") == 0)
        {
            setUpPlayground(playerShips);
            *ourTime = time(NULL);
            setCall("write");
            break;
        }
        else if (strcmp(shared_mem, "n") == 0)
        {
            kill(PID, SIGINT);
            clearAndExit();
        }
    }
    
}

// wysylamy informacje o rozpoczaciu rozgrywki
void sendStartFrame(char *nickname)
{
    printf("[Propozycja gry wysłana]\n");
    memcpy(command, "start", sizeof("start"));
    strcat(strcat(command, "|"), nickname);
    sendto(sockfd, &command, sizeof(command), 0, NULL, 0);
    time_t current_time = time(NULL);
    sendto(sockfd, &current_time, sizeof(current_time), 0, NULL, 0);
}

// przechwytujemy odpowiednie sygnały i przetwarzamy je
void sigHandler(int signo)
{
    if (signo == SIGINT)
    {
        char *cmd = strtok(shared_mem, "|");
        if (strcmp(cmd, "wypisz") == 0)
        {
            printPlayground(shootField);
        }
        else if (strcmp(cmd, "myTurn") == 0)
        {
            isMyTurn = 1;
        }
        else if (strcmp(cmd, "sendMsg") == 0)
        {
            char *tmp = strtok(NULL, "");
            strcpy(command, tmp);
            sendto(sockfd, &command, sizeof(command), 0,
                   NULL, 0);
        }
        else if (strcmp(cmd, "set") == 0)
        {
            char *tmp = strtok(NULL, "");
            setShootField('x', tmp, shootField);
        }
        else
        {
            clearAndExit();
        }

        setCall("EMPTY");
    }
    return;
}

int main(int argc, char *argv[])
{
    if (argc > 2 && strlen(argv[2]) > 10)
    {
        fprintf(stderr, "Maksymalna dlugosc nicku to 10\n");
        exit(EXIT_FAILURE);
    }
    // tworzymy pamięc współdzieloną
    key_t key;
    ;
    if ((key = ftok(FILE_NAME, 65)) == -1)
    {
        perror("ftok");
        exit(1);
    }

    if ((shmid = shmget(key, 1024, 0666 | IPC_CREAT)) == -1)
    {
        perror("ftok");
        exit(1);
    }
    shared_mem = (char *)shmat(shmid, (void *)0, 0);
    // przechwytujemy sygnały
    signal(SIGINT, sigHandler);

    struct addrinfo hints;
    struct addrinfo *result, *rp;

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
            strncpy(nickname, argv[2], 10);
        else
            strcpy(nickname, "NN");

        printf("Rozpoczynam gre z %s. Napisz <koniec> by zakonczyc. Ustal polozenie swoich okretow:\n", inet_ntoa((struct in_addr)addr->sin_addr));
        setCall("ready");
        waitForCall("unpause");
        sendStartFrame(nickname);
        isMyTurn = 0;

        while (1)
        {
            //Pobieraj polecenia od użytkownika i dodaj nick jeśli został utworzony
            memset(command, 0, sizeof(command));

            scanf("%s", command);
            if (strcmp(shared_mem, "newgame") == 0)
            {
                while (strcmp(command, "t") && strcmp(command, "n"))
                {
                    printf("--Bledna wartosc--\n");
                    memset(command, 0, sizeof(command));
                    scanf("%s", command);
                }
                sleep(0.5);
                setCall(command);
                waitForCall("write");
                sendStartFrame(nickname);
            }
            else if (strcmp(command, "wypisz") == 0)
            {
                setCall(command);
                kill(ppid, SIGINT);
                continue;
            }
            else if (strcmp(command, "<koniec>") == 0)
            {
                setCall("end");
                strcat(strcat(command, "|"), nickname);
                sendto(sockfd, &command, sizeof(command), 0,
                       NULL, 0);
                kill(ppid, SIGINT);
                clearAndExit();
            }
            else if (letterToNumber(command[0]) == -1 || (command[1] - '0') > 4)
            {
                printf("---Bledny komunikat---\n");
            }
            else if (isMyTurn == 1)
            {
                char cmd[20] = "set|";
                strcat(cmd, command);
                setCall(cmd);
                kill(ppid, SIGINT);

                strcat(strcat(command, "|"), nickname);

                //wysyłamy dane do drugiego użytkownika
                sendto(sockfd, &command, sizeof(command), 0,
                       NULL, 0);
                isMyTurn = 0;
            }
            else
            {
                printf("---Czekaj na swoją ture---\n");
            }
        }
    }
    else
    {

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

        char playerShips[4][3];
        setUpPlayground(playerShips);

        setCall("unpause");
        sleep(0.5);
        
        time_t ourTime = time(NULL);
        struct sockaddr_in from;
        socklen_t len = sizeof(from);
        int doublebShips = 2, singleShip = 2;
        int missed = 0;
        while (1)
        {
            recvfrom(sockfd, &command, sizeof(command), 0, (struct sockaddr *)&from, &len);
            char *msg = strtok(command, "|");
            char *nick = strtok(NULL, "|");
            if (strcmp(msg, "<koniec>") == 0 || strcmp(msg, "win") == 0)
            {
                doublebShips = 2;
                singleShip = 2;
                missed = 0;
                if(strcmp(msg, "<koniec>") == 0 )
                    printf("[%s (%s) zakonczyl gre, czy chcesz przygotowac nowa plansze?(t/n)]\n", nick, inet_ntoa(from.sin_addr));
                else
                    printf("[Wygrales z %s (%s), czy chcesz przygotowac nowa plansze?(t/n)]\n", nick, inet_ntoa(from.sin_addr));
                
                newGame(PID, playerShips, &ourTime);
                continue;
            }
            else if (strcmp(msg, "start") == 0)
            {
                time_t rival_time;
                recvfrom(sockfd, &rival_time, sizeof(rival_time), 0, (struct sockaddr *)&from, &len);
                if (ourTime < rival_time)
                {
                    printf("[%s (%s) dolaczyl do gry, podal pole do strzalu]\n", nick, inet_ntoa(from.sin_addr));
                    missed = 1;
                    setCall("myTurn");
                    kill(PID, SIGINT);
                }
            }
            else if (strcmp(msg, "trafiony") == 0)
            {
                missed = 1;
                char *shipLen = nick;

                if (strcmp(shipLen, "2") == 0)
                    shipLen = "trafiles dwumasztowiec";
                else if (strcmp(shipLen, "1") == 0)
                    shipLen = "zatopiles jednomasztowiec";
                else
                    shipLen = "zatopiles dwumasztowiec";
                char *pos = strtok(NULL, "|");
                setShootField('Z', pos, shootField);
                nick = strtok(NULL, "|");
                printf("[%s (%s): %s, podaj kolejne pole]\n", nick, inet_ntoa(from.sin_addr), shipLen);
                setCall("myTurn");
                kill(PID, SIGINT);
                continue;
            }
            else
            {
                int i = 0;
                int hit = 0;
                char *missMsg;
                if (missed == 1)
                    missMsg = "Pudlo, ";
                else
                    missMsg = "";
                //jednomasztowce
                for (i = 0; i < 2; i++)
                {
                    if (strcmp(msg, playerShips[i]) == 0)
                    {
                        singleShip--;
                        printf("[%s%s (%s) strzela %s - jednomasztowiec zatopiony]\n", missMsg, nick, inet_ntoa(from.sin_addr), msg);
                        char tmp[35] = "sendMsg|trafiony|1|";
                        strcat(strcat(strcat(tmp, msg), "|"), nick);
                        setCall(tmp);
                        kill(PID, SIGINT);
                        hit = 1;
                        missed = 0;
                    }
                }
                //dwumasztowce
                for (i = 2; i < 4; i++)
                {
                    if (strcmp(msg, playerShips[i]) == 0)
                    {
                        doublebShips--;
                        printf("[%s%s (%s) strzela %s ", missMsg, nick, inet_ntoa(from.sin_addr), msg);
                        char tmp[35] = "sendMsg|trafiony|";
                        if (doublebShips <= 0)
                        {
                            printf("- dwumasztowiec zatopiony]\n");
                            strcat(tmp, "2-|");
                        }
                        else
                        {
                            printf("- dwumasztowiec trafiony]\n");
                            strcat(tmp, "2|");
                        }
                        strcat(strcat(strcat(tmp, msg), "|"), nick);
                        setCall(tmp);
                        kill(PID, SIGINT);
                        hit = 1;
                        missed = 0;
                    }
                }

                if (hit == 0)
                {
                    printf("[%s (%s) strzela %s pudlo, podaj pole do strzalu]\n", nick, inet_ntoa(from.sin_addr), msg);
                    setCall("myTurn");
                    kill(PID, SIGINT);
                }
                else if (doublebShips <= 0 && singleShip <= 0)
                {
                    sleep(1);
                    char tmp[35] = "sendMsg|win|";
                    strcat(tmp, nick);
                    setCall(tmp);
                    kill(PID, SIGINT);

                    printf("[Przegrales z %s (%s), czy chcesz przygotowac nowa plansze?(t/n)]\n", nick, inet_ntoa(from.sin_addr));
                    sleep(1.5);
                    doublebShips = 2;
                    singleShip = 2;
                    missed = 0;
                    newGame(PID, playerShips, &ourTime);
                }
            }
        }
    }

    wait(NULL);
    return 0;
}