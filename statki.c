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

//zmienne globalne które wymagają zamknięcia
int sockfd, s, shmid;
char *shared_mem;
char shootField[4][4];
int isMyTurn;
char command[30];

// ustawiamy komunikat w pamięci współdzielonej
void setCall(char *call)
{
    memset(shared_mem, 0, 30);
    memcpy(shared_mem, call, 30);
}

// czekamy na wskazany komunikat w pamięci współdzielonej
void waitForCall(char *call)
{
    while (1)
    {
        if (strcmp(shared_mem, call) == 0){
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
void setField(char *message, char playerShip[4][3], int id,  int playerPlayfield[4][4])
{
    char position[15]={};
    int x, y; // x -> A; y -> 1

    int nearFlag;
    if(id==3)
        nearFlag=1;
    else
        nearFlag=-1;
    printf("%s", message);
    scanf("%s", position);
    
    //sprawdzamy czy pole jest zajęte, czy dane są poprawnie wprowadzone i czy obok nie stoi już statek
    while (strlen(position) > 2 || (x = letterToNumber(position[0])) == -1 || (y = position[1] - '0') > 4 || playerPlayfield[x][y] == 1 || isNear(position, playerPlayfield) != nearFlag)
    {
        printf("---Bledne dane---\nZa maly odstep|zla liczba|zla cyfra|pole zajete\nWprowadz ponownie: ");
        scanf("%s", position);
    }
    y--;
    playerPlayfield[x][y] = 1;
    memcpy(playerShip[id],position,sizeof(position));
}

// przygotowujemy plansze do gry
void setUpPlayground(char playerShips[4][3])
{   
    int tmp[4][4];
    memset(tmp, 0, sizeof(tmp[0][0]) * 16);
    memset(playerShips, ' ', sizeof(playerShips[0][0]) * 8);

    setField("1. jednomasztowiec: ", playerShips, 0,tmp);
    setField("2. jednomasztowiec: ", playerShips, 1,tmp);
    setField("3. dwumasztowiec: ", playerShips, 2,tmp);
    setField("", playerShips, 3,tmp);
    //int k=0;
    // printf("start wypisania\n");
    // for(k=0;k<4;k++)
    //     printf("%d - %s\n",k,playerShips[k]);
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
        if (strcmp(shared_mem, "wypisz") == 0)
        {
            printPlayground(shootField);
        }
        else if (strcmp(shared_mem, "myTurn") == 0)
        {
            isMyTurn=1;
        }
        else if(strcmp(strtok(shared_mem,"|"), "sendMsg") == 0){
            char *tmp = strtok(NULL,"");
            strcpy(command,tmp);
            sendto(sockfd, &command, sizeof(command), 0,
                       NULL, 0);
        }
        else{
            clearAndExit();
        }
        
        setCall("EMPTY");
    }
    return;
}

int main(int argc, char *argv[])
{
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

    //Na razie plansza do której strzelamy jest pusta
    memset(shootField, ' ', sizeof(shootField) * 16);

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
        
        isMyTurn=0;
        printf("[Propozycja gry wysłana]\n");
        memcpy(command, "start", sizeof("start"));
        strcat(command, "|");
        strcat(command, nickname);
        sendto(sockfd, &command, sizeof(command), 0, NULL, 0);
        time_t current_time = time(NULL);
        sendto(sockfd, &current_time, sizeof(current_time), 0, NULL, 0);
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
                setCall(command);
                waitForCall("write");
                printf("[Propozycja gry wysłana]\n");
                continue;
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
                strcat(command, "|");
                strcat(command, nickname);
                sendto(sockfd, &command, sizeof(command), 0,
                       NULL, 0);
                kill(ppid, SIGINT);
                clearAndExit();
            }
            else if(letterToNumber(command[0])==-1 ||(command[1]-'0')>4){
                printf("---Bledny komunikat---\n");
            }
            else if(isMyTurn==1){

                strcat(command, "|");
                strcat(command, nickname);

                //wysyłamy dane do drugiego użytkownika
                sendto(sockfd, &command, sizeof(command), 0,
                       NULL, 0);
                isMyTurn=0;
            }
            else{
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
        int dwuMaszt=2, jednoMaszt=2;
        while (1)
        {
            recvfrom(sockfd, &command, sizeof(command), 0, (struct sockaddr *)&from, &len);
            //printf("DOSTALEM: %s\n",command);
            char *msg = strtok(command, "|");
            char *nick = strtok(NULL, "|");
            //printf("[%s (%s): %s] \n", nick, inet_ntoa(from.sin_addr), msg);
            if (strcmp(msg, "<koniec>") == 0)
            {
                setCall("newgame");
                printf("[%s (%s) zakonczyl gre, czy chcesz przygotowac nowa plansze?(t/n)]\n", nick, inet_ntoa(from.sin_addr));
                while (1)
                {
                    if (strcmp(shared_mem, "t") == 0)
                    {
                        setUpPlayground(playerShips);
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
            else if (strcmp(msg, "start") == 0)
            {
                time_t rival_time;
                recvfrom(sockfd, &rival_time, sizeof(rival_time), 0, (struct sockaddr *)&from, &len);
                if(ourTime<rival_time){
                    printf("[%s (%s) dolaczyl do gry, podal pole do strzalu]\n", nick, inet_ntoa(from.sin_addr));
                    //wysylamy informacje do dziecka o mozliwosci przesylania informacji
                    setCall("myTurn");
                    kill(PID, SIGINT);
                }
            }
            else if (strcmp(msg, "trafiony") == 0){
                printf("TRAFILES podaj kolejne pole\n");
                setCall("myTurn");
                kill(PID, SIGINT);
                continue;
            }
            else
            {
                int i=0;
                int hit=0;
                //jednomasztowce   
                for(i=0;i<2;i++){
                    if(strcmp(msg,playerShips[i])==0){
                        jednoMaszt--;
                        //printf("TRAFIL 1 MASZTOWIEC\n");
                        char tmp[]="sendMsg|trafiony|";
                        strcat(tmp, nick);
                        setCall(tmp);
                        kill(PID, SIGINT);
                        hit=1;
                    }
                }
                //dwumasztowce   
                for(i=2;i<4;i++){
                    if(strcmp(msg,playerShips[i])==0){
                        dwuMaszt--;
                        //printf("TRAFIL 2 MASZTOWIEC\n");
                        char tmp[] ="sendMsg|trafiony|";
                        strcat(tmp, nick);
                        setCall(tmp);
                        kill(PID, SIGINT);
                        hit=1;
                    }
                }

                if(hit==0){
                    printf("pudlo, teraz ty\n");
                    setCall("myTurn");
                    kill(PID, SIGINT);
                }
                else if(dwuMaszt==0&&jednoMaszt==0)
                    printf("KONIEC GRY\n");


                //printf("[%s (%s): %s] \n", nick, inet_ntoa(from.sin_addr), msg);
            }
        }
    }

    wait(NULL);
    return 0;
}