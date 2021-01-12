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
#define FILE_NAME "WisniewskiAdam_gra.c"
#define MSG_SIZE 50

//zmienne globalne które wymagają zamknięcia bądź są używane bezpośrednio w sighandler
int sockfd, s, shmid;
char *shared_mem;
char shootField[4][4];
int isMyTurn;
char command[MSG_SIZE];

// ustawiamy komunikat w pamięci współdzielonej
void setCall(char *call)
{
    memset(shared_mem, 0, MSG_SIZE); //zerujemy komunikat, aby mieć pewność że nie będzie wycieków pamięci
    memcpy(shared_mem, call, MSG_SIZE);
}

// czekamy na wskazany komunikat w pamięci współdzielonej
void waitForCall(char *call)
{
    while (1) //czekamy w pętli nieskończonej aż w pamięci współdzielonej nie będzie naszego komunikatu
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
    //sprawdzamy czy podana wartość spełnia ograniczenia,
    //następnie sprawdzamy czy pole +-1 w górę i w dół jest zajęte i zwracamy fałsz lub prawdę
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

    int nearFlag; //flaga która określa czy dany statek ma być obok pola zajętego
    if (id == 3) //jeśli jest to dwumasztowiec to musi byc obok drugiego
        nearFlag = 1;
    else
        nearFlag = -1;
    printf("%s", message); //wypisujemy informację
    scanf("%s", position);

    //sprawdzamy czy pole jest zajęte, czy dane są poprawnie wprowadzone i czy obok nie stoi już statek
    while (strlen(position) > 2 || (x = letterToNumber(position[0])) == -1 || (y = position[1] - '0') > 4 || playerPlayfield[x][y] == 1 || isNear(position, playerPlayfield) != nearFlag)
    {
        printf("---Bledne dane---\n---Za maly odstep|zla liczba|zla cyfra|pole zajete---\nWprowadz ponownie: ");
        scanf("%s", position);
    }
    y--; //Reprezentowana liczba jest zmniejszana o 1 aby reprezentować indeks tablicy
    playerPlayfield[x][y] = 1; //zapisujemy do zmiennej pomocniczej pozycje statków
    memcpy(playerShip[id], position, sizeof(position)); //zapamiętujemy pole wpisane przez użytkownika do tablicy charów
}

// przygotowujemy plansze do gry
void setUpPlayground(char playerShips[4][3])
{
    int tmp[4][4]; // Tworzymy tablice pomocniczą do sprawdzania poprawnosci ułożeń statków
    //zerujemy nasze tablice    
    memset(tmp, 0, sizeof(tmp[0][0]) * 16);
    memset(playerShips, ' ', sizeof(playerShips[0][0]) * 8);
    memset(shootField, ' ', sizeof(shootField[0][0]) * 16);
    //ustawiamy odpowiednie pola 
    setField("1. jednomasztowiec: ", playerShips, 0, tmp);
    setField("2. jednomasztowiec: ", playerShips, 1, tmp);
    setField("3. dwumasztowiec: ", playerShips, 2, tmp);
    setField("", playerShips, 3, tmp);
}

// wypisujemy plansze
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
    sleep(0.5);
    while (1)
    {
        
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
    //wysylamy informacje zawierajaca nick, czas rozpoczecia oraz komende uruchamiajaca
    memcpy(command, "start", sizeof("start"));
    char sendTime[12];
    sprintf(sendTime,"%ld",time(NULL));
    strcat(strcat(strcat(strcat(command, "|"), nickname),"|"),sendTime);
    printf("[Propozycja gry wysłana]\n");
    sendto(sockfd, &command, sizeof(command), 0, NULL, 0);
}

// przechwytujemy odpowiednie sygnały i przetwarzamy je
void sigHandler(int signo)
{
    if (signo == SIGINT)
    {
        //sprawdzamy komunikaty w pamieci wspoldzielonej i przetwarzamy je
        char *cmd = strtok(shared_mem, "|");
        if (strcmp(cmd, "wypisz") == 0) //wypisujemy plansze na ekran
        {
            printPlayground(shootField);
        }
        else if (strcmp(cmd, "myTurn") == 0) //ustawiamy turę na naszą
        {
            isMyTurn = 1; 
        }
        else if (strcmp(cmd, "sendMsg") == 0) //wysyłamy wiadomość do przeciwnika (sygnał trafienia bądź pudła)
        {
            char *tmp = strtok(NULL, ""); 
            strcpy(command, tmp);
            sendto(sockfd, &command, sizeof(command), 0,
                   NULL, 0);
        }
        else if (strcmp(cmd, "set") == 0) 
        //ustawiamy miejsce w naszej planszy do której strzelamy na pudło, 
        //jeśli przeciwnik poinformuje nas o trafieniu to później zastępujemy ten znak
        {
            char *tmp = strtok(NULL, "");
            setShootField('x', tmp, shootField);
        }
        else
        {
            //wychodzimy i sprzatamy po sobie
            clearAndExit();
        }

        setCall("EMPTY"); //po wykonaniu zawsze ustawiamy pamiec na brak polecen
    }
    return;
}

int main(int argc, char *argv[])
{
    if (argc > 2 && strlen(argv[2]) > 10) //sprawdzamy czy użytkownik wprowadził dodatkowy argument i czy mieści się w założonej wartości
    {
        fprintf(stderr, "Maksymalna dlugosc nicku to 10\n");
        exit(EXIT_FAILURE);
    }
    else if(argc<2)
    {
        fprintf(stderr, "Uzycie programu:\n./nazwa_prog 'host' 'nick (opcjonalne)'\n");
        exit(EXIT_FAILURE);
    }


    // tworzymy klucz pamieci współdzielonej bazujący na nazwie naszego pliku
    key_t key;
    if ((key = ftok(FILE_NAME, 65)) == -1)
    {
        perror("ftok");
        exit(1);
    }
    //tworzymy pamięć współdzieloną
    if ((shmid = shmget(key, 1024, 0666 | IPC_CREAT)) == -1)
    {
        perror("ftok");
        exit(1);
    }
    //dołączamy segment do pamięci
    shared_mem = (char *)shmat(shmid, (void *)0, 0);
    //przechwytujemy sygnały
    signal(SIGINT, sigHandler);

    struct addrinfo hints; //zmienna przehowująca wskazówki połączenia
    struct addrinfo *result, *rp;

    pid_t PID, ppid; 
    ppid = getpid(); //zapisujemy sobie swój pid potrzebny do wysyłania sygnałów kill()
    PID = fork();
    //tworzymy dziecko, które odpowiada za komunikację z przeciwnikiem
    //natomiast rodzic stale nasłuchuje i przetwarza odbierane dane
    if (PID < 0) //nie udało się utworzyć dziecka
    {
        fprintf(stderr, "fork(): error\n");
        exit(EXIT_FAILURE);
    }
    else if (PID == 0)
    {
        //dziecko obsługuje tylko przesył komunikatów do hosta
        memset(&hints, 0, sizeof(struct addrinfo));
        hints.ai_family = AF_INET; //ipv4
        hints.ai_socktype = SOCK_DGRAM; //przesyłanie datagramów
        hints.ai_flags = 0;
        hints.ai_protocol = 0;
        struct sockaddr_in *addr; 
        s = getaddrinfo(argv[1], MY_PORT, &hints, &result); // używamy getaddrinfo do zapisania listy hostów do result
        if (s != 0)
        {
            fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
            exit(EXIT_FAILURE);
        }
        //przechodzimy kolejno przez tablice result i sprawdzamy czy uda się nam nawiązać połączenie
        //jeśli się powiedzie używamy funkcji connect, w przeciwnym wypadku szukamy dalej
        for (rp = result; rp != NULL; rp = rp->ai_next) 
        {
            sockfd = socket(rp->ai_family, rp->ai_socktype,
                            rp->ai_protocol);
            if (sockfd == -1)
                continue;

            if (connect(sockfd, rp->ai_addr, rp->ai_addrlen) != -1)
            {
                addr = (struct sockaddr_in *)rp->ai_addr; //przypisujemy adres z którym udało się nam połączyć aby wyświetlić jego ip
                break;
            }

            close(sockfd);
        }

        if (rp == NULL) //jeśli nie dostaliśmy rezultatu wypisujemy błąd
        {
            fprintf(stderr, "Could not connect\n");
            exit(EXIT_FAILURE);
        }
        freeaddrinfo(result); //zwalniamy strukture pamięci

        char nickname[10];
        if (argc > 2) //jeśli uzytkownik nie podał argumentu ustawiamy nasz domyślny nick
            strncpy(nickname, argv[2], 10);
        else
            strcpy(nickname, "NN");
        //wypisujemy informacje poczatkowe
        printf("Rozpoczynam gre z %s. Napisz <koniec> by zakonczyc. Ustal polozenie swoich okretow:\n", inet_ntoa((struct in_addr)addr->sin_addr));
        setCall("ready"); //ustawiamy wiadomosc dla rodzica
        waitForCall("unpause"); //czekamy az rodzic pozwoli nam dzialac
        sendStartFrame(nickname); //wysyłamy funkcję startującą do przeciwnika i czeakmy
        isMyTurn = 0; //czekamy na swoją ture

        while (1)
        {
            //Zerujemy naszą zmienną aby nie było w niej poprzednich poleceń
            memset(command, 0, sizeof(command));

            scanf("%s", command); //słuchamy użytkownika
            if (strcmp(shared_mem, "newgame") == 0) //jeśli jest ustawiony sygnał nowej gry
            {
                while (strcmp(command, "t") && strcmp(command, "n")) //pobieramy znak od uzytkownika dopoki nie poda n lub t
                {
                    printf("--Bledna wartosc--\n");
                    memset(command, 0, sizeof(command));
                    scanf("%s", command);
                }
                sleep(0.5); //usypiamy aby rodzic odpowiednio się zsynchronizował
                setCall(command); //do pamieci zapisujemy wybrana przez nas opcje
                waitForCall("write"); //odblokowujemy odczyt rodzica
                sendStartFrame(nickname); //wysyłamy ramkę startu
            }
            else if (strcmp(command, "wypisz") == 0)
            {
                setCall(command); //ustawiamy i wysyłamy sygnał wypisania planszy
                kill(ppid, SIGINT);
                continue;
            }
            else if (strcmp(command, "<koniec>") == 0)
            {
                setCall("end");
                strcat(strcat(command, "|"), nickname);
                sendto(sockfd, &command, sizeof(command), 0, NULL, 0); //wysyłamy informację o zakonczeniu do przeciwnika
                kill(ppid, SIGINT); //wysyłamy sygnał do rodzica o zakonczeniu
                clearAndExit(); //wychodzimy z dziecka
            }
            else if (letterToNumber(command[0]) == -1 || (command[1] - '0') > 4) //sprawdzamy czy jest to poprawna wartosc pola
            {
                printf("---Bledny komunikat---\n");
            }
            else if (isMyTurn == 1) //jesli jest nasza tura
            {
                //wstawiamy znak 'x' w pole które strzelamy
                //jesli trafimy to rodzic zaktualizuje pole po uzyskaniu odpowiedniej wiadomosci
                char cmd[20] = "set|";
                strcat(cmd, command);
                setCall(cmd);
                kill(ppid, SIGINT); 

                //wysyłamy dane do drugiego użytkownika
                strcat(strcat(command, "|"), nickname);
                sendto(sockfd, &command, sizeof(command), 0,
                       NULL, 0);
                isMyTurn = 0;
            }
            else //to nie jest nasza tura, ale dane są poprawne
            {
                printf("---Czekaj na swoją ture---\n"); 
            }
        }
    }
    else
    {
        //Ustawiamy rodzica na nasłuch komunikatów,
        //rodzic zajmuje się również ich przetwarzaniem
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

        waitForCall("ready"); //czekamy na dziecko
        char playerShips[4][3]; //nasze pola statków
        setUpPlayground(playerShips);

        setCall("unpause"); //odblokowujemy dziecko
        sleep(0.5); //czekamy aby zsynchronizować działanie
        
        time_t ourTime = time(NULL); //zmienna przechowujaca nasz czas 
        struct sockaddr_in from; //dane przechowujace wlasciciela wiadomosci
        socklen_t len = sizeof(from); //dlugosc tych danych
        int doublebShips = 2, singleShip = 2; //ilosc naszych jedno oraz dwumasztowców
        int missed = 0; // zmienna reprezentująca czy byliśmy wstanie spudłować, aby wyświetlać odpowiednie komunikaty
        while (1)
        {
            recvfrom(sockfd, &command, sizeof(command), 0, (struct sockaddr *)&from, &len); //przyjmujemy wiadomości
            char *msg = strtok(command, "|"); // wyciągamy treść
            char *nick = strtok(NULL, "|");   // wyciągamy nick
            if (strcmp(msg, "<koniec>") == 0 || strcmp(msg, "win") == 0) 
            {
                //resetujemy naszą rozgrywke
                doublebShips = 2;
                singleShip = 2;
                missed = 0;
                if(strcmp(msg, "<koniec>") == 0 )
                    printf("[%s (%s) zakonczyl gre, czy chcesz przygotowac nowa plansze?(t/n)]\n", nick, inet_ntoa(from.sin_addr));
                else
                    printf("[Wygrales z %s (%s), czy chcesz przygotowac nowa plansze?(t/n)]\n", nick, inet_ntoa(from.sin_addr));
                
                newGame(PID, playerShips, &ourTime); //tworzymy nową gre
                continue;
            }
            else if (strcmp(msg, "start") == 0)
            {
                char* tmp = strtok(NULL, "|"); // zczytujemy czas wysłania wiadomości
                //w przypadku kiedy pierwszy gracz się ustawi, a następnie drugi gracz dopiero uruchomi program
                //wiadomość z rozpoczeciem rozgrywki przepada, wiec dzieki temu zabezpieczamy się zatrzymaniem się programu
                if(tmp==NULL) 
                    continue;
                time_t rival_time = strtol(tmp,NULL, 10);
                if (ourTime < rival_time) //zaczyna ten który pierwszy ustawił statki
                {
                    printf("[%s (%s) dolaczyl do gry, podaj pole do strzalu]\n", nick, inet_ntoa(from.sin_addr));
                    missed = 1; //oddaliśmy strzał wiec mieliśmy szanse spudłować
                    setCall("myTurn"); //ustawiamy naszą turę
                    kill(PID, SIGINT);
                }
            }
            else if (strcmp(msg, "trafiony") == 0)
            {
                missed = 1; //jeśli trafiliśmy to znów strzelamy więc znów możemy spudłować
                char *shipLen = nick;

                //przetwarzamy w jaki statek trafilismy
                if (strcmp(shipLen, "2") == 0)
                    shipLen = "trafiles dwumasztowiec";
                else if (strcmp(shipLen, "1") == 0)
                    shipLen = "zatopiles jednomasztowiec";
                else
                    shipLen = "zatopiles dwumasztowiec";

                char *pos = strtok(NULL, "|");
                //ustawiamy odpowiedni znak w naszym polu
                setShootField('Z', pos, shootField);
                nick = strtok(NULL, "|");
                printf("[%s (%s): %s, podaj kolejne pole]\n", nick, inet_ntoa(from.sin_addr), shipLen);
                setCall("myTurn"); //ustawiamy znow nasza ture
                kill(PID, SIGINT);
                continue;
            }
            else
            {
                int i = 0;
                int hit = 0;
                char *missMsg;
                if (missed == 1) //poprzednio strzelalismy
                    missMsg = "Pudlo, ";
                else
                    missMsg = "";
                //przetwarzamy czy zostal trafiony nasz statek
                //jednomasztowce
                for (i = 0; i < 2; i++)
                {
                    if (strcmp(msg, playerShips[i]) == 0)
                    {
                        singleShip--; //odejmujemy ilosc naszych statków
                        printf("[%s%s (%s) strzela %s - jednomasztowiec zatopiony]\n", missMsg, nick, inet_ntoa(from.sin_addr), msg);
                        char tmp[35] = "sendMsg|trafiony|1|";
                        strcat(strcat(strcat(tmp, msg), "|"), nick);
                        setCall(tmp); //wysyłamy do przeciwnika wiadomosc o trafieniu
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
                        if (doublebShips <= 0) //zatopiono nasz dwumasztowiec
                        {
                            printf("- dwumasztowiec zatopiony]\n");
                            strcat(tmp, "2-|"); //wysylamy komunikat o zatopieniu
                        }
                        else
                        {
                            printf("- dwumasztowiec trafiony]\n");
                            strcat(tmp, "2|");
                        }
                        strcat(strcat(strcat(tmp, msg), "|"), nick);
                        setCall(tmp); //wysyłamy odpowiedni komunikat
                        kill(PID, SIGINT);
                        hit = 1;
                        missed = 0;
                    }
                }

                if (hit == 0) //jesli przeciwnik nas nie trafił
                {
                    printf("[%s (%s) strzela %s pudlo, podaj pole do strzalu]\n", nick, inet_ntoa(from.sin_addr), msg);
                    setCall("myTurn"); //ustawiamy nasza ture
                    kill(PID, SIGINT);
                }
                else if (doublebShips <= 0 && singleShip <= 0)
                {
                    sleep(1); //synchronizujemy sie z hostem
                    char tmp[35] = "sendMsg|win|";
                    strcat(tmp, nick);
                    setCall(tmp); //wysyłamy wiadomosc do przeciwnika o jego wygranej
                    kill(PID, SIGINT);

                    //informujemy o przegranej
                    printf("[Przegrales z %s (%s), czy chcesz przygotowac nowa plansze?(t/n)]\n", nick, inet_ntoa(from.sin_addr));
                    sleep(1.5); //synchronizujemy sie z hostem
                    //przygotowujemy na nowo rozgrywke
                    doublebShips = 2;
                    singleShip = 2;
                    missed = 0;
                    newGame(PID, playerShips, &ourTime);
                }
            }
        }
    }
    wait(NULL); //zabezpieczenie przed osieroceniem dziecka
    return 0;
}