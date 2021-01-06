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

#define MY_PORT "19999"

void setCall(char* call, char* sharedMem){
    memcpy(sharedMem, call, sizeof(char));
}

void waitForCall(char* call, char* sharedMem){
            while(1){
            if(strcmp(sharedMem,call))
                break;
        }
}

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
        if (playerPlayfield[a][position[1] - '0' - 1] == 1)
            return 1;

    return -1;
}

void setField(char *message, int playerPlayfield[4][4], int numOfFields)
{
    char position[100];
    int x, y; // x -> A; y -> 1
    printf("%s", message);
    scanf("%s", position);
    while (strlen(position)>2||(x = letterToNumber(position[0])) == -1 || (y = position[1] - '0' - 1) > 4 || playerPlayfield[x][y] == 1 || isNear(position, playerPlayfield) != -1)
    {
        printf("---Bledne dane---\nZa maly odstep|zla liczba|zla cyfra|pole zajete\nWprowadz ponownie: ");
        scanf("%s", position);
    }
    playerPlayfield[x][y] = 1;
    
    //ustawiamy drugie pole z tą różnicą, że dwumasztowiec musi stać obok drugiego pola z 1 masztem
    if(numOfFields==2){
        scanf("%s", position);
        while ((x = letterToNumber(position[0])) == -1 || (y = position[1] - '0' - 1) > 4 || playerPlayfield[x][y] == 1 || isNear(position, playerPlayfield) != 1)
        {
            printf("Bledne pole lub zajete, wprowadz jeszcze raz: ");
            scanf("%s", position);
        }
    playerPlayfield[x][y] = 1;
    }
}

int sockfd, s, shmid;
char *shared_mem;

void clearAndExit(){
        close(s);
        close(sockfd);
        shmdt(shared_mem);
        shmctl(shmid, IPC_RMID, NULL);
        printf("\nShutdown process..\n");
        exit(0);
}

void exit_signal(int signo){
    if(signo == SIGINT){
        clearAndExit();
    }
        printf("Signal caught!");
    return;
}

int main(int argc, char *argv[])
{
    int playerPlayfield[4][4];
    memset(playerPlayfield, 0, sizeof(playerPlayfield[0][0]) * 16);

    key_t key = ftok("statki.c",65); 
    // shmget returns an identifier in shmid 
    shmid = shmget(key,1024,0666|IPC_CREAT); 
    // shmat to attach to shared memory 
    shared_mem = (char*) shmat(shmid,(void*)0,0); 

    struct addrinfo hints;
    struct addrinfo *result, *rp;
    char message[25];
    pid_t PID,ppid;
    ppid = getpid();
    PID = fork();
    
    setCall("pause",shared_mem);

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
        
        waitForCall("unpause", shared_mem);
        
        while (1)
        {
            //Pobieraj polecenia od użytkownika i dodaj nick jeśli został utworzony
            scanf("%s", message);
            if(strcmp(message,"<koniec>")==0){
                kill(ppid, SIGINT);
                clearAndExit();
            }

            strcat(message, "|");
            strcat(message, nickname);

            //wysyłamy dane do drugiego użytkownika
            sendto(sockfd, &message, sizeof(message), 0,
                   NULL, 0);
        }

        // close(sockfd);
    }
    else
    {
        sleep(1);

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

        // setField("1. jednomasztowiec: ", playerPlayfield, 1);
        // setField("2. jednomasztowiec: ", playerPlayfield, 1);
        // setField("3. dwumasztowiec: ", playerPlayfield, 2);
        
        // int x, y;
        // char tmp[4] = {'A', 'B', 'C', 'D'};
        // printf("\n---");
        
        // for(x=1;x<5;x++)
        //     printf("%d ",x);
        // printf("\n");
        // for (x = 0; x < 4; x++)
        // {
        //     printf("%c  ", tmp[x]);
        //     for (y = 0; y < 4; y++)
        //     {
        //         printf("%d ", playerPlayfield[x][y]);
        //     }
        //     printf("\n");
        // }

        setCall("unpause",shared_mem);
        signal(SIGINT,exit_signal);
        struct sockaddr_in from;
        socklen_t len = sizeof(from);
        while (1)
        {
            recvfrom(sockfd, &message, sizeof(message), 0, (struct sockaddr *)&from, &len);

            printf("[%s]: %s\n", inet_ntoa(from.sin_addr), message);
        }
    }

    //wysyłamy wiadomość ready, i oczekujemy na wiadomość ready
    //jeśli otrzymamy wiadomość ready, wysyłamy wiadmość start i rozpoczynamy
    //jeśli otrzymamy wiadomość start, to rozpoczynamy
    wait(NULL);
    return 0;
}