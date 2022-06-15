#include <stdio.h>
#include <string.h>   
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>   
#include <arpa/inet.h>  
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <pthread.h>
#include <semaphore.h>
#include <dirent.h>
#include <fcntl.h>

#define PORT 7588
#define ADDR "127.0.0.1"
#define MAX_CLIENTS 100
#define BUFFER_SIZE 1024
#define MSG_QUEUE_SIZE 2048
#define MAX_REQUEST_PER_CLIENT 100
#define MUSIC_DB_PATH "music_db.txt"

struct server_info
{
    int socket_fd;
    struct sockaddr_in address;
};

struct package_t
{
    int socket_fd;
    char package[BUFFER_SIZE];
};

struct request_t
{
    int request_type;
    FILE *fp;
    int current_package;
    int total_packages;
};

struct client_state
{
    int socket_fd;
    int admin;
    struct request_t request_queue[MAX_REQUEST_PER_CLIENT];
};

struct client_state clients[MAX_CLIENTS];
struct package_t packeges[MSG_QUEUE_SIZE];

pthread_mutex_t lock;
pthread_mutex_t msg_queue_lock;

sem_t close_server;
sem_t empty_sem;                // Semafor care tine evidenta cate locuri libere sunt in coada de mesaje
sem_t full_sem;                 // Semafor care tine evidenta cate locuri ocupate sunt in coada de mesaje

int number_of_packages = 0;

void *connect_incoming_clients(void *args)
{
    int i;
    struct server_info *server= (struct server_info*)args;
    int address_len = sizeof(server->address);

    printf("Waiting for new clients\n");

    while (1)
    {
        int socket_fd = accept(server->socket_fd,(struct sockaddr *) &server->address, &address_len);

        if (socket_fd < 0)
        {
            // errno 11 = EAGAIN, adica nu s-a gasit client de conectat din cauza ca e socket non-blocant, deci trebuie sa reincercam
            if (errno != 11)
            {
                fprintf(stderr, "Failed to connect new client %d\n", errno);
                perror(NULL);
            }
        }
        else
        {
            // Facem socket-ul non-blocant ca sa putem verifica toti clientii pe rand daca au trimis ceva
            if(fcntl(socket_fd, F_SETFL, fcntl(socket_fd, F_GETFL) | O_NONBLOCK) < 0)
            {
                fprintf(stderr, "Socket flag setting error: %d\n", errno);
                perror(NULL);
                exit(EXIT_FAILURE);
            }

            for (i = 0; i < MAX_CLIENTS; i++)
            {
                pthread_mutex_lock(&lock);
                int tmp_socket_fd = clients[i].socket_fd;
                pthread_mutex_unlock(&lock);

                // Punem sub mutex in cazul in care alt fir de executie deconecteaza un client sa nu apara concurenta pe vectorul de socket-uri
                if (tmp_socket_fd == 0)
                {
                    pthread_mutex_lock(&lock);
                    clients[i].socket_fd = socket_fd;
                    pthread_mutex_unlock(&lock);

                    //fprintf(stderr, "i: %d, sock: %d\n", i, clients[i].socket_fd);  // De sters
                    printf("New client connected\n");
                    // THREAD CLIENT START

                    break;
                }
            }

            // In cazul asta tot vectorul de clienti e plin si nu mai avem loc de nimeni
            if (i == MAX_CLIENTS)
            {
                close(socket_fd);
                fprintf(stderr, "Server full, connection closed\n");
            }
        }

        sleep(0.01);
    }
}

// Converteste intr-un interval dintr-un array de caractere in numar intreg
int extract_number(char *package, int start, int end)
{
    int n = 0;
    int tmp = 1;

    for (int i = end - 1; i >= start; i--, tmp *= 10)
    {
        if (package[i] - '0' >= 0 && package[i] - '0' <= 9)
        {
            n = n + (package[i] - '0') * tmp;
        }
    }

    return n;
}

int find_free_request_slot(int client_index)
{
    int request_index;

    for (request_index = 0; request_index < MAX_REQUEST_PER_CLIENT; request_index++)
    {
        pthread_mutex_lock(&lock);
        int tmp_request_type = clients[client_index].request_queue[request_index].request_type;
        pthread_mutex_unlock(&lock);

        // Daca tipul de mesaj e setat pe -1, inseamna ca acolo e liber si putem pune o cerere
        if (tmp_request_type == -1)
        {
            break;
        }
    }

    return request_index;
}

// Seteaza campurile unui client pe valorile initiale, ca si cum ar fi liber
void set_client_to_default(int i)
{
    clients[i].socket_fd = 0;
    clients[i].admin = 0;

    // Daca request_type == -1 inseamna ca avem loc liber pentru o cerere de la clientul respectiv
    for (int j = 0; j < MAX_REQUEST_PER_CLIENT; j++)
    {
        clients[i].request_queue[j].request_type = -1;
    }
}

void set_request_type(int client_index, int request_index, int request_type)
{
    pthread_mutex_lock(&lock);
    clients[client_index].request_queue[request_index].request_type = request_type;
    pthread_mutex_unlock(&lock);
}

int get_request_type(int client_index, int request_index)
{
    int tmp_request_type;

    pthread_mutex_lock(&lock);
    tmp_request_type = clients[client_index].request_queue[request_index].request_type;
    pthread_mutex_unlock(&lock);

    return tmp_request_type;
}

void set_request_file_descriptor_to_rb(int  client_index, int request_index, char *path)
{
    FILE *tmp_fp = fopen(path, "rb");

    if (!tmp_fp)
    {
        fprintf(stderr, "Error while opening file for reading: %s | Err: %d", path, errno);
        perror(NULL);
        exit(EXIT_FAILURE);
    }
    fprintf(stderr, "client_index: %d | request_index: %d | fp: %d\n", client_index, request_index, tmp_fp);
    pthread_mutex_lock(&lock);
    clients[client_index].request_queue[request_index].fp = tmp_fp;
    pthread_mutex_unlock(&lock);
}

void set_request_file_descriptor_to_wb(int  client_index, int request_index, char *path)
{
    FILE *tmp_fp = fopen(path, "wb");

    if (!tmp_fp)
    {
        fprintf(stderr, "Error while opening file for reading: %s", path);
        exit(EXIT_FAILURE);
    }

    pthread_mutex_lock(&lock);
    clients[client_index].request_queue[request_index].fp = tmp_fp;
    pthread_mutex_unlock(&lock);
}

void close_file_descriptor(int client_index, int request_index)
{
    FILE *tmp_fp;

    pthread_mutex_lock(&lock);
    tmp_fp = clients[client_index].request_queue[request_index].fp;
    clients[client_index].request_queue[request_index].fp = NULL;
    pthread_mutex_unlock(&lock);

    fclose(tmp_fp);
}

FILE* get_file_descriptor(int client_index, int request_index)
{
    FILE *tmp_fp;

    pthread_mutex_lock(&lock);
    tmp_fp = clients[client_index].request_queue[request_index].fp;
    pthread_mutex_unlock(&lock);

    return tmp_fp;
}

int get_socket_file_descriptor(int client_index)
{
    int socket_fd;

    pthread_mutex_lock(&lock);
    socket_fd = clients[client_index].socket_fd;
    pthread_mutex_unlock(&lock);

    return socket_fd;
}

void put_int_to_char_in_buffer(char *buffer, int start, int end, int n)
{
   int digit;

    if (n == 0)
    {
        buffer[end - 1] = '0';

        for (int i = end - 2; i >= start; i--)
        {

            buffer[i] = '-';
        }

        return;
    }

    for (int i = end - 1; i >= start; i--, n /= 10)
    {
        if (n != 0)
        {
            digit = n % 10;
            buffer[i] = '0' + digit;
        }
        else
        {
            buffer[i] = '-';
        }
    }
}

void send_package(int socket_fd, char *buffer, int size)
{
    if (send(socket_fd, buffer, size, 0) == -1)
    {
        fprintf(stderr, "Error while sending the data: %d\n", errno);
        perror(NULL);
        exit(EXIT_FAILURE);
    }
}

int make_data_package(char *buffer, int client_index, int request_index, int package_type)
{
    int bytes_read;     // Mesajele de tip 5, 7 (cel care urmeaza sa fie trimis), are header de lungime 12
    int header_size = 12;
    FILE *tmp_fp;

    // Luam file descriptor-ul ca sa citim din fisier
    tmp_fp = get_file_descriptor(client_index, request_index);
    //fprintf(stderr, "%d\n", tmp_fp);

    // Citim primul pachet din fisier (poate sa fie si ultimul, in cazul in care baza de date este mica)
    bytes_read = fread(buffer + header_size, sizeof(char), BUFFER_SIZE - header_size, tmp_fp);
    //fprintf(stderr, "b\n");

    // Setam header-ul de la mesajul care urmeaza sa fie trimis
    put_int_to_char_in_buffer(buffer, 0, 4, package_type);
    //fprintf(stderr, "c\n");


    // Punem dimensiunea octetilor cititi de la poz 4 la 7 in buffer
    put_int_to_char_in_buffer(buffer, 4, 8, bytes_read);
    //fprintf(stderr, "d\n");


    // Punem ID-ul crererii de la poz 8 la 11, ID-ul este reprezentat de variabila request_index
    put_int_to_char_in_buffer(buffer, 8, 12, request_index);
    //fprintf(stderr, "e\n");


    return bytes_read;
}

int get_number_of_packets(int client_index, int request_index, int effective_msg_length)
{
    FILE *fp;
    int size;

    fp = get_file_descriptor(client_index, request_index);

    // Aflam dimensiunea fisierului(melodiei) tinta care trebuie trimis
    fseek(fp, 0, SEEK_END);
    size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    // Calculam cate pachete trebuie trimise pentru melodia tinta
    if (size % effective_msg_length == 0)
    {
        size = size / effective_msg_length;
    }
    else
    {
        size = size / effective_msg_length + 1;
    }

    return size;
}

int find_request_by_id(int client_index, int request_type)
{
    int request_index;
    int tmp;

    for (int request_index = 0; request_index < MAX_CLIENTS; request_index++)
    {
        pthread_mutex_lock(&lock);
        tmp = clients[client_index].request_queue[request_index].request_type;
        pthread_mutex_unlock(&lock);

        if (tmp == request_type)
        {
            break;
        }
    }

    return request_index;
}

void free_request_slot(int client_index, int request_index)
{
    pthread_mutex_lock(&lock);
    clients[client_index].request_queue[request_index].request_type = -1;
    pthread_mutex_unlock(&lock);
}

void make_type_four_package(char *buffer, int request_id, int size)
{
    // Punem pe primii 4 octeti ca e pachet de tipul 4
    put_int_to_char_in_buffer(buffer, 0, 4, 4);

    // Punem pe octetii [4, 7] numarul de pachete care urmeaza a fi trimise
    put_int_to_char_in_buffer(buffer, 4, 8, size);

    // Punem pe octetii [8, 11] ID-ul cererii
    put_int_to_char_in_buffer(buffer, 8, 12, request_id);
}

void get_melody_info(char *melody_path, int music_id, char *title, char *genre)
{
    FILE *fp = fopen(melody_path, "r");
    char *line = NULL;
    size_t line_length = 0;
    int bytes_read;
    int i, j;
    int current_music_id;

    if (!fp)
    {
        fprintf(stderr, "Error while opening file for reading: %s | %d\n", melody_path, errno);
        perror(NULL);
        exit(EXIT_FAILURE);
    }

    // Separam ID-ul, de titlu si gen, pana cand .ID-ul melodiei curente cititte din fisier este egal cu cel dat in functie
    // Campurile sunt despartite prin virgula, ca si la CSV
    while ((bytes_read = getline(&line, &line_length, fp)) != -1)
    {
        // Iteram pe linie pana gasim prima virgula; atunci se termina numarul
        for (i = 0; i < line_length; i++)
        {
            if (line[i] == ',')
            {
                break;
            }
        }

        // Extragem ID-ul din linia citita
        current_music_id = extract_number(line, 0, i);

        // Daca ID-urile corespund, atunci inseamna ca am gasit melodia cautata
        if (current_music_id == music_id)
        {
            // Intre prima si a doua virgula se afla titlul melodiei
            for (j = 0, i = i + 1; i < line_length; i++)
            {
                if (line[i] != ',')
                {
                    title[j++] = line[i];
                }
                else
                {
                    title[j] = '\0';
                    break;
                }
            }

            // De la a doua virgula pana la sfarsit se afla genul
            for (j = 0, i = i + 1; i < line_length; i++)
            {
                if (line[i] == '\n')
                {
                    break;
                }

                genre[j++] = line[i];
            }

            genre[j] = '\0';

            //fprintf(stderr, "genre: %s | title: %s", genre, title);

            break;
        }
    }

    if (line)
    {
        free(line);
    }

    fclose(fp);
}

void get_title_and_genre_from_package(char *package, char *title, char *genre)
{
    int i, j;

    // Extragem titlul
    // Primii 8 octeti contin tipul de mesaj si nr total de pachete
    for(i = 8, j = 0; ; i++, j++)
    {
        if (package[i] == '|')
        {
            title[j] = '\0';
            break;
        }

        title[j] = package[i];
    }

    // Extragem genul
    for(j = 0, i = i + 1; ; i++, j++)
    {
        if (package[i] == '\0')
        {
            genre[j] = '\0';
            break;
        }

        genre[j] = package[i];
    }
}

void to_lowercase(char *string)
{
    for (int i = 0; i < strlen(string) + 1; i++)
    {
        if (string[i] >= 'A' && string[i] <= 'Z')
        {
            string[i] += 32;
        }
    }
}

int directory_exists(char *path)
{
    DIR* dir = opendir(path);

    if (dir)
    {
        return 1;
    }
    else if (errno == ENOENT)
    {
        return 0;
    }
    else
    {
        return -1;
    }
}

void create_directory(char *path)
{
    mkdir(path, 0700);
}

int get_next_melody_id(char *db_path)
{
    FILE *fp = fopen(db_path, "r");
    char *line = NULL;
    size_t line_length = 0;
    int bytes_read;
    int id = 0;

    if (!fp)
    {
        fprintf(stderr, "Error while opening file for reading: %s | %d\n", db_path, errno);
        perror(NULL);
        exit(EXIT_FAILURE);
    }

    // Separam ID-ul, de titlu si gen, pana cand .ID-ul melodiei curente cititte din fisier este egal cu cel dat in functie
    // Campurile sunt despartite prin virgula, ca si la CSV
    while ((bytes_read = getline(&line, &line_length, fp)) != -1)
    {
        id++;
    }

    if (line)
    {
        free(line);
    }

    fclose(fp);

    return id;
}

void update_music_db(char *db_path, char *genre, char *title)
{
    FILE *fp;
    int id;
    char db_record[512];

    // Cautam urmatorul ID valabil pe care sa il folosim in baza de date
    id = get_next_melody_id(db_path);

    // Facem linie noua de intrare in baza de date
    snprintf(db_record, 512, "%d,%s,%s\n", id, title, genre);

    fp = fopen(db_path, "a");

    if (!fp)
    {
        fprintf(stderr, "Error while opening file for writing: %s | Err: %d", db_path, errno);
        perror(NULL);
        exit(EXIT_FAILURE);
    }

    // Mergem lafialul fisierului ca sa scriem intrarea noua
    fseek(fp, 0, SEEK_END);

    // Scriem inregistrarea noua
    fwrite(db_record, sizeof(char), strlen(db_record), fp);

    fclose(fp);
}

void make_type_six_package(char *buffer, int request_id)
{
    put_int_to_char_in_buffer(buffer, 0, 4, 6);
    put_int_to_char_in_buffer(buffer, 4, 8, request_id);
}

int get_admin_status(int client_index)
{
    int admin_status;

    pthread_mutex_lock(&lock);
    admin_status = clients[client_index].admin;
    pthread_mutex_unlock(&lock);

    return admin_status;
}

void set_admin_status(int client_index, int status)
{
    pthread_mutex_lock(&lock);
    clients[client_index].admin = status;
    pthread_mutex_unlock(&lock);
}


int is_admin_connected()
{
    // Pentru fiecare client verificam daca este administrator
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (get_admin_status(i))
        {
            return 1;
        }
    }

    return 0;
}

void set_request_to_default(int client_index, int request_index)
{
    pthread_mutex_lock(&lock);
    clients[client_index].request_queue[request_index].request_type = -1;
    pthread_mutex_unlock(&lock);
}

int is_admin(int client_index)
{
    int status;

    pthread_mutex_lock(&lock);
    status = clients[client_index].admin;
    pthread_mutex_unlock(&lock);

    return status;
}

void process_package(struct package_t package)
{
    struct client_state client;
    int package_header;
    int client_index;
    int request_index;
    int header_size;
    int bytes_read;
    int request_type;
    int client_socket;
    int music_id;
    int size;
    int client_socket_tmp;
    char buffer[BUFFER_SIZE];
    char path[516];
    char tmp_buff[256];
    char title[256], genre[256];
    FILE *tmp_fp;

    // Cautam care este clientul de la care a venit pachetul in functie de socket-ul pe care a venit mesajul
    // Daca socket_fd-ul de la pachet corespunde cu socket_fd-ul asociat unui client, atunci inseamna ca s-a gasit corespindenta
    for (client_index = 0; client_index < MAX_CLIENTS; client_index++)
    {
        pthread_mutex_lock(&lock);
        int tmp_socket_fd = clients[client_index].socket_fd;
        pthread_mutex_unlock(&lock);

        if (tmp_socket_fd == package.socket_fd)
        {
            break;
        }
    }

    // Extragem tipul mesajului de pe primii 4 octeti din pachet (din header)
    package_header = extract_number(package.package, 0, 4);

    switch (package_header)
    {
    case 1:
        fprintf(stderr, "In case 1\n");

        header_size = 12;
        request_index = find_free_request_slot(client_index);
        fprintf(stderr, "request_index: %d\n", request_index);

        // Daca e egal cu MAX_REQUEST_PER_CLIENT inseamna ca toate locurile de cereri noi sunt ocupate
        if (request_index < MAX_REQUEST_PER_CLIENT)
        {
            // Setam tipul de cerere cu header-ul de la pachet, ca sa stim ce cereri vor urma in continuare
            set_request_type(client_index, request_index, package_header);

            // Luam ID-ul melodiei de pe pozitiile 4 - 7
            music_id = extract_number(package.package, 4, 8);

            // Cautam care este titlul si genul pentru melodia cu ID-ul dat
            get_melody_info(MUSIC_DB_PATH, music_id, title, genre);

            // Contruim calea spre melodia geruta
            snprintf(path, 516, "%s/%s.mp3", genre, title);

            // Deschidem file descriptor-ul de la baza de date cu melodii
            set_request_file_descriptor_to_rb(client_index, request_index, path);
            //fprintf(stderr, "tttttt, %d\n", get_file_descriptor(client_index, request_index));

            // Salvam cate pachete are melodia care urmeaza sa fie trimisa
            size = get_number_of_packets(client_index, request_index, BUFFER_SIZE - header_size);

            // Construim pachetul de tip 4, cel care anunta clientul despre nr de pachete si ID cererii
            make_type_four_package(buffer, request_index, size);

            // Gasim socket-ul corespunzator clientului tinta si trimitem pachetul la client
            client_socket = get_socket_file_descriptor(client_index);
            /*char httpHeader[100000] = "HTTP/1.1 200 OK\r\n" "Content-Type: text/plain; charset=UTF-8\r\n\r\n";
            char auxbuf[2*BUFFER_SIZE];
            strcpy(auxbuf, httpHeader);
            strcat(auxbuf, buffer);*/
            //send_package(client_socket, auxbuf, bytes_read + header_size + strlen(httpHeader));
            send_package(client_socket, buffer, bytes_read + header_size);
        }

        break;

    case 2:
        fprintf(stderr, "In case 2\n");
        header_size = 12;
        request_index = find_free_request_slot(client_index);

        // Daca e egal cu MAX_REQUEST_PER_CLIENT inseamna ca toate locurile de cereri noi sunt ocupate
        if (request_index < MAX_REQUEST_PER_CLIENT)
        {
            // Setam tipul de cerere cu header-ul de la pachet, ca sa stim ce cereri vor urma in continuare
            set_request_type(client_index, request_index, package_header);

            // Deschidem file descriptor-ul de la baza de date cu melodii
            set_request_file_descriptor_to_rb(client_index, request_index, MUSIC_DB_PATH);
            char httpHeader[100000] = "HTTP/1.1 200 OK\r\n" "Content-Type: text/plain; charset=UTF-8\r\n\r\n";

            // Facem un pachet cu informatia care trebuie trimisa clientului
            bytes_read = make_data_package(buffer, client_index, request_index, 7);
            char auxbuf[2*BUFFER_SIZE];
            strcpy(auxbuf, httpHeader);
            strcat(auxbuf, buffer);

            // Daca s-a intors un numar mai mare decat 0 de octeti cititi din fisier, inseamna ca inca avem informatie acolo
            // Daca se intorc 0, inseamna ca am terminat de citit din fisier si inchidem file descriptorul
            if (bytes_read != 0)
            {
                // Gasim socket-ul corespunzator clientului tinta si trimitem pachetul la client
                client_socket = get_socket_file_descriptor(client_index);
                //send_package(client_socket, buffer, bytes_read + header_size);
                send_package(client_socket, auxbuf, bytes_read + header_size + strlen(httpHeader));
            }
            else
            {
                close_file_descriptor(client_index, request_index);
                free_request_slot(client_index, request_index);
            }
        }

        break;

    case 3:
        fprintf(stderr, "In case 3\n");
        header_size = 12;
        request_index = find_free_request_slot(client_index);

        // Daca e egal cu MAX_REQUEST_PER_CLIENT inseamna ca toate locurile de cereri noi sunt ocupate
        if (request_index < MAX_REQUEST_PER_CLIENT)
        {
            // Extragem titlul si genul din pachetul primit
            get_title_and_genre_from_package(package.package, title, genre);

            // Facem titlul si genul sa fie lowercase, altfel pot fi duplicate logic, dar diferite ca text
            to_lowercase(title);
            to_lowercase(genre);

            // Verificam daca exista directorul cu genul de muzica indicat in pachet
            // In cazul in care nu exista, il cream
            if (!directory_exists(genre))
            {
                create_directory(genre);
            }
            
            // Facem calea unde vom scrie melodia noua
            snprintf(path, 516, "%s/%s.mp3", genre, title);

            tmp_fp = fopen(path, "wb");

            if (!tmp_fp)
            {
                fprintf(stderr, "Error while opening file for writing: %s | Err: %d", path, errno);
                perror(NULL);
                exit(EXIT_FAILURE);
            }

            // Facem update bazei de date cu noua melodie care urmeaza sa se primeasca
            update_music_db(MUSIC_DB_PATH, genre, title);

            // Setam file descriptorul pe wb pentru ca urmeaza sa se primeasca melodia
            set_request_file_descriptor_to_wb(client_index, request_index, path);

            // Facem un mesaj de confirmare ca sa trimitem clientului
            make_type_six_package(buffer, request_index);

            // Gasim socket-ul corespunzator clientului tinta si trimitem pachetul la client
            client_socket = get_socket_file_descriptor(client_index);
            send_package(client_socket, buffer, 8);
        }

        break;
    case 5:
        header_size = 12;

        // Extragem numarul de octeti primiti din pachet
        bytes_read = extract_number(package.package, 4, 8);

        // Extragem ID cererii 
        request_index = extract_number(package.package, 8, 12);

        // Salvam file descriptor-ul corespunzator pachetului care a venit
        tmp_fp = get_file_descriptor(client_index, request_index);

        // Scriem in fisier pachetul primit
        fwrite(package.package + header_size, sizeof(char), bytes_read, tmp_fp);
        // for(int i = 0; i<bytes_read; i++)
        //     printf("%c", package.package[i+12]);}

        // Daca vine un pachet care nu e plin, inseamna ca e ultimul si putem inchide file descriptor-ul
        if (bytes_read < BUFFER_SIZE - header_size)
        {
            close_file_descriptor(client_index, request_index);
        }

        // Facem un mesaj de confirmare ca sa trimitem clientului
        make_type_six_package(buffer, request_index);

        // Gasim socket-ul corespunzator clientului tinta si trimitem pachetul la client
        client_socket = get_socket_file_descriptor(client_index);
        send_package(client_socket, buffer, 8);

        break;

    case 6:
        //fprintf(stderr, "In case 6\n");
        // Cautam care cerere ii este asociata clientului tinta in funtie de ID
        request_index = extract_number(package.package, 4, 8);
        //fprintf(stderr, "1\n");

        // Luam tipul de cerere de la clientul tinta, ca sa aflam la ce ii corespunde confirmarea
        request_type = get_request_type(client_index, request_index);
        //fprintf(stderr, "2\n");

        //fprintf(stderr, "request_type: %d, request_index: %d\n", request_type, request_index);

        if (request_type == 1)
        {
            // Facem un pachet cu informatia care trebuie trimisa clientului
            bytes_read = make_data_package(buffer, client_index, request_index, 5);
            //fprintf(stderr, "3\n");

            // Daca s-a intors un numar mai mare decat 0 de octeti cititi din fisier, inseamna ca inca avem informatie acolo
            // Daca se intorc 0, inseamna ca am terminat de citit din fisier si inchidem file descriptorul
            if (bytes_read != 0)
            {
                // Gasim socket-ul corespunzator clientului tinta si trimitem pachetul la client
                client_socket = get_socket_file_descriptor(client_index);
                //fprintf(stderr, "4\n");
                send_package(client_socket, buffer, bytes_read + header_size);
                //fprintf(stderr, "5\n");
            }
            else
            {
                close_file_descriptor(client_index, request_index);
                set_request_to_default(client_index, request_index);
                //fprintf(stderr, "59999\n");
            }
        }
        else if (request_type == 2)
        {
            // Facem un pachet cu informatia care trebuie trimisa clientului
            bytes_read = make_data_package(buffer, client_index, request_index, 7);

            // Daca s-a intors un numar mai mare decat 0 de octeti cititi din fisier, inseamna ca inca avem informatie acolo
            // Daca se intorc 0, inseamna ca am terminat de citit din fisier si inchidem file descriptorul
            if (bytes_read != 0)
            {
                // Gasim socket-ul corespunzator clientului tinta si trimitem pachetul la client
                client_socket = get_socket_file_descriptor(client_index);
                send_package(client_socket, buffer, bytes_read + header_size);
            }
            else
            {
                close_file_descriptor(client_index, request_index);
                set_request_to_default(client_index, request_index);
            }
        }

        break;

    case 8:
        if (is_admin(client_index))
        {
            fprintf(stderr, "Disconnecting clients\n");

            for (int i = 0; i < MAX_CLIENTS; i++)
            {
                client_socket = get_socket_file_descriptor(i);
                if (client_socket)
                {
                    close(client_socket);
                    set_client_to_default(i);
                }
            }

            fprintf(stderr, "Clients disconnected\n");
        }
        else
        {
            fprintf(stderr, "Unauthorised client tried to disconnect the clients\n");
        }

        break;

    case 9:

        // Verificam daca exista deja un admin conectat
        if (!is_admin_connected())
        {
            set_admin_status(client_index, 1);
            printf("Admin connected on socket: %d\n", get_socket_file_descriptor(client_index));
        }
        else
        {
            fprintf(stderr, "Admin already connected\n");
        }

        break;

    case 10:
        if (is_admin(client_index))
        {
            fprintf(stderr, "Server closed from admin\n");
            sem_post(&close_server);
        }
        else
        {
            fprintf(stderr, "Unauthorised client tried to close the server\n");
        }

        break;

    case 11:
        if (is_admin(client_index))
        {
            size = 0;
            client_socket = get_socket_file_descriptor(client_index);

            for (int i = 0; i < MAX_CLIENTS; i++)
            {
                client_socket_tmp = get_socket_file_descriptor(i);
                if (client_socket_tmp)
                {
                    size++;
                }
            }

            snprintf(tmp_buff, 256, "%d", size);
            send_package(client_socket, tmp_buff, strlen(tmp_buff) + 1);
        }

        break;

    default:
        fprintf(stderr, "Unknown request type\n");
        break;
    }
}

void *process_queue(void *args)
{
    struct package_t package;

    while (1)
    {
        sem_wait(&full_sem);   // Daca nu avem locuri libere in coada, asteptam pana se fac
        pthread_mutex_lock(&msg_queue_lock);
        package = packeges[0];
        //fprintf(stderr, "5\n");
        for (int i = 0; i < number_of_packages; i++)
        {
            packeges[i] = packeges[i + 1];
        }

        number_of_packages--;
        pthread_mutex_unlock(&msg_queue_lock);
        sem_post(&empty_sem);    // Cand apare un loc liber, atunci incrementam cate locuri pline sunt, ptr ca am adaugat un element

        //fprintf(stderr, "Socket: %d, Mesaj: %s\n", package.socket_fd, package.package);
        //fprintf(stderr, "6\n");

        // Procesarea efectiva a pachetului
        process_package(package);
    }

    sleep(0.01);
}

struct server_info init_server_connection()
{
    int opt = 1;
    struct server_info server;

    // Punem toate pozitiile din vectorul de socket-uri de clienti sa fie 0 ca sa stim care pozitii sunt libere
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        set_client_to_default(i);
    }

    //  Facem socket-ul serverului
    if((server.socket_fd = socket(AF_INET , SOCK_STREAM , 0)) == -1)
    {
        fprintf(stderr, "Socket creation failed: %d\n", errno);
        perror(NULL);
        exit(EXIT_FAILURE);
    }
    else
    {
        printf("Socket successfully created\n");
    }

    // Setam socket-ul sa accepte mai multi clienti
    if(setsockopt(server.socket_fd, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0)
    {
        fprintf(stderr, "Setsockopt failure: %d\n", errno);
        perror(NULL);
        exit(EXIT_FAILURE);
    }

    server.address.sin_family = AF_INET;
    server.address.sin_addr.s_addr = INADDR_ANY;
    server.address.sin_port = htons(PORT);

    // Facem bind intre adresa si socket
    if (bind(server.socket_fd, (struct sockaddr *)&server.address, sizeof(server.address)) == -1)
    {
        fprintf(stderr, "Failed to bind the socket: %d\n", errno);
        perror(NULL);
        exit(EXIT_FAILURE);
    }
    else
    {
        printf("Socket successfully bound\n");
    }

    // Facem listen si zicem cam cati clienti credem ca asteapta sa se conecteze - aici 4
    if (listen(server.socket_fd, 4) < 0)
    {
        fprintf(stderr, "Failure while listening: %d\n", errno);
        perror(NULL);
        exit(EXIT_FAILURE);
    }
    else
    {
        printf("Listening on port %d \n", PORT);
    }

    // Facem socket-ul non-blocant ca sa putem verifica toti clientii pe rand daca au trimis ceva
    if(fcntl(server.socket_fd, F_SETFL, fcntl(server.socket_fd, F_GETFL) | O_NONBLOCK) < 0)
    {
        fprintf(stderr, "Socket flag setting error: %d\n", errno);
        perror(NULL);
        exit(EXIT_FAILURE);
    }

    return server;
}

void *package_reader(void *args)
{
    char buffer[BUFFER_SIZE];
    int socket_fd = *((int *)args);
    int tmp_client_socket;

    //fprintf(stderr, "1\n");
    while(1)
    {
      //  fprintf(stderr, "2\n");
        // Pentru fiecare socket de client conectat, verificam daca a fost trimis un mesaj
        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            pthread_mutex_lock(&lock);
            tmp_client_socket = clients[i].socket_fd;
            pthread_mutex_unlock(&lock);

        //    fprintf(stderr, "3\n");

            if (tmp_client_socket != 0)
            {
                bzero(buffer, BUFFER_SIZE);
                int recv_res;

          //      fprintf(stderr, "4\n");
                if ((recv_res = recv(tmp_client_socket, buffer, BUFFER_SIZE, 0)) == -1)
                {
                    if (errno != EAGAIN)
                    {
                        fprintf(stderr, "Package reading failure %d\n", errno);
                        perror(NULL);
                        //exit(1);
                    }
                }
                else if (recv_res == 0)     // Daca e 0, s-a pierdut conexiunea la client
                {
                    fprintf(stderr, "Connection lost with client with socket: %d\n", tmp_client_socket);
                    close(get_socket_file_descriptor(i));
                    pthread_mutex_lock(&lock);
                    set_client_to_default(i);
                    pthread_mutex_unlock(&lock);
                }
                else
                {
                    //fprintf(stderr, "Socket: %d, Mesaj: %s\n", clients[i].socket_fd, buffer);
                    struct package_t package;

                    package.socket_fd = tmp_client_socket;   // Copiem socket-ul clientului care a trimis mesajul
                    memcpy(package.package, buffer, recv_res);  // Copiem mesajul de la client in struct

                    sem_wait(&empty_sem);   // Daca nu avem locuri libere in coada, asteptam pana se fac
                    pthread_mutex_lock(&msg_queue_lock);
                    packeges[number_of_packages++] = package;
                    pthread_mutex_unlock(&msg_queue_lock);
                    sem_post(&full_sem);    // Cand apare un loc liber, atunci incrementam cate locuri pline sunt, ptr ca am adaugat un element
                }
            }
        }

        sleep(0.01);
    }
}

int main(int argc , char *argv[])
{
    pthread_t client_connection, reader, pk_processing;
    struct server_info server;

    // Initializam mutex-urile
    if (pthread_mutex_init(&lock, NULL) != 0)
    {
        fprintf(stderr, "Mutex initialization has failed: %d\n", errno);
        perror(NULL);
        exit(EXIT_FAILURE);
    }

    if (pthread_mutex_init(&msg_queue_lock, NULL) != 0)
    {
        fprintf(stderr, "Mutex initialization has failed: %d\n", errno);
        perror(NULL);
        exit(EXIT_FAILURE);
    }

    // Initializam semaforul pentru inchiderea serverului
    if (sem_init(&close_server, 0, 0) == -1)
    {
        fprintf(stderr, "Sempahore initialization has failed: %d\n", errno);
        perror(NULL);
        exit(EXIT_FAILURE);
    }

    // Initial toate locurile sunt libere in coada de mesaje
    if (sem_init(&empty_sem, 0, MSG_QUEUE_SIZE) == -1)
    {
        fprintf(stderr, "Sempahore initialization has failed: %d\n", errno);
        perror(NULL);
        exit(EXIT_FAILURE);
    }

    // Initial niciun loc nu e ocupat in coada de mesaje
    if (sem_init(&full_sem, 0, 0) == -1)
    {
        fprintf(stderr, "Sempahore initialization has failed: %d\n", errno);
        perror(NULL);
        exit(EXIT_FAILURE);
    }

    // Salvam informatiile despre server
    server = init_server_connection();

    // Pornim fir de executie care asteapta conextiuni noi de clienti
    pthread_create(&client_connection, NULL, connect_incoming_clients, &server);

    // Pornim fir de executie care citeste pachetele din buffer
    pthread_create(&reader, NULL, package_reader, &server.socket_fd);

    // Pornim fir de executie care proceseaza coada de mesaje
    pthread_create(&pk_processing, NULL, process_queue, NULL);

    // Asteptam pana se primeste comanda din admin ca se opreste serverul
    sem_wait(&close_server);

    // Distrugem mutex-urile si semafoarele
    pthread_mutex_destroy(&lock);
    pthread_mutex_destroy(&msg_queue_lock);

    sem_destroy(&empty_sem);
    sem_destroy(&full_sem);
    sem_destroy(&close_server);

    pthread_cancel(client_connection);
    pthread_cancel(reader);
    pthread_cancel(pk_processing);

    printf("Server closed\n");

    return 0;
}