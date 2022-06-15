#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <vlc/vlc.h>

#define dataSize 1024

int main(){
	int clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    int opt, cont = 0, id, i, j, k, l, songSize, requestId, octSize, actualSongSize;
    char data[dataSize], auxSize[4], auxRequestId[4], auxOct[4], songbytes[5000000], songTitle[50];
    FILE *audio;
	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(7588);
	addr.sin_addr.s_addr = inet_addr("127.0.0.1");

	if(connect(clientSocket, (struct sockaddr *) &addr , sizeof(addr)) == 0){
		printf("Connection established\n");
        printf("Connection established\n");
        if(send(clientSocket, "---9", 4, 0) == -1){
            fprintf(stderr, "Error while requesting administrator rights\n");
            exit(EXIT_FAILURE);
        }

        while(cont == 0){
            printf("\nChoose an option:\n1 - Listen to a song\n2 - Check uploads\n3 - Close server\n4 - Disconnect all clients\n5 - Get number of clients\n");            scanf("%d", &opt);
            switch (opt){
                case 0:
                    snprintf(data, dataSize, "---%d", 0);
                    if(send(clientSocket, data, strlen(data) + 1, 0) ==-1){
                        fprintf(stderr, "Error sending data");
                    }
                    break;
                case 1:
                    actualSongSize = 0;
                    printf("\nThe ID of the wanted song: ");
                    scanf("%d", &id);
                    sprintf(songTitle, "%s%d%s", "song", id, ".mp3");

                    snprintf(data, dataSize, "---%d---%d", 1, id);
                    if(send(clientSocket, data, strlen(data) + 1, 0) ==-1){
                        fprintf(stderr, "Error sending data");
                    }
                    if (recv(clientSocket, data, dataSize, 0) < 0){
                        fprintf(stderr, "Error reading data");
                    }

                    printf("\nReceived package: \n");
                    for(i = 0; i<12; i++){
                        printf("%c", data[i]);
                    }

                    j = 0;
                    for(i = 4; i < 8; i++){
                        auxSize[j] = data[i];
                        j++;
                    }
                    songSize = atoi(auxSize);
                    j = 0;
                    for(i = 8; i < 12; i++){
                        auxRequestId[j] = data[i];
                        j++;
                    }
                    requestId = atoi(auxRequestId);

                    snprintf(data, dataSize, "---%d---%d---%c", 6, requestId, data[11]);
                    printf("\nSent package: \n");
                    for(i = 0; i<12; i++){
                        printf("%c", data[i]);
                    }
                    if (send(clientSocket, data, 12, 0) == -1){
                        fprintf(stderr, "Error sending data\n");
                    }

                    l = 0;
                    audio = fopen(songTitle, "wb");
                    int tmp;
                    for(i = 0; i < songSize; i++){
                        if ((tmp = recv(clientSocket, data, dataSize, 0)) < 0){
                            fprintf(stderr, "Error reading data");
                        }
                        j = 0;
                        for(k = 4; k < 8; k++){
                            auxOct[j] = data[k];
                            j++;
                        }
                        octSize = atoi(auxOct);
                        fwrite(data+12, sizeof(char), tmp-12, audio);
                        snprintf(data, dataSize, "---%d---%d---%c", 6, requestId, data[11]);
                        if (send(clientSocket, data, 12, 0) == -1){
                            fprintf(stderr, "Error sending data\n");
                        }
                    }
                    fclose(audio);

                    libvlc_instance_t *inst;
                    libvlc_media_player_t *mp;
                    libvlc_media_t *m;

                    // load the vlc engine
                    inst = libvlc_new(0, NULL);

                    // create a new item
                    m = libvlc_media_new_path(inst, songTitle);

                    // create a media play playing environment
                    mp = libvlc_media_player_new_from_media(m);

                    // no need to keep the media now
                    libvlc_media_release(m);

                    // play the media_player
                    libvlc_media_player_play(mp);

                    sleep(30);

                    // stop playing
                    libvlc_media_player_stop(mp);

                    // free the media_player
                    libvlc_media_player_release(mp);

                    libvlc_release(inst);

                    break;

                case 2:
                    snprintf(data, dataSize, "---%d", 2);
                    if(send(clientSocket, data, strlen(data) + 1, 0) ==-1){
                        fprintf(stderr, "Error sending data");
                    }
                    if (recv(clientSocket, data, dataSize, 0) < 0){
                        fprintf(stderr, "Error reading data");
                    }

                    printf("\nReceived package: \n");
                    for(i = 0; i<12; i++){
                        printf("%c", data[i]);
                    }

                    printf("\nThe available songs: \n");
                    for(i = 12; i< strlen(data); i++){
                        printf("%c", data[i]);
                    }

                    snprintf(data, dataSize, "---%d---%c---%c", 6, data[11], data[11]);
                    printf("\nSent package: \n");
                    for(i = 0; i<12; i++){
                        printf("%c", data[i]);
                    }

                    if (send(clientSocket, data, 8, 0) == -1){
                        fprintf(stderr, "Error sending data\n");
                    }
                    break;

                case 3:
                    if(send(clientSocket, "--10", 4, 0) == -1){
                        fprintf(stderr, "Error sending data\n");
                        exit(EXIT_FAILURE);
                    }
                    goto exit_admin;
                    break;

                case 4:
                    if(send(clientSocket, "---8", 4, 0) == -1){
                        fprintf(stderr, "Error sending data\n");
                        exit(EXIT_FAILURE);
                    }
                    break;

                case 5:
                    if(send(clientSocket, "--11", 4, 0) == -1){
                        fprintf(stderr, "Error sending data\n");
                        exit(EXIT_FAILURE);
                    }

                    if (recv(clientSocket, data, dataSize, 0) < 0){
                        fprintf(stderr, "Error reading data");
                    }
                    fprintf(stderr, "There are %s clients connected\n", data);
                    break;
            }

            printf("\nDo you want to continue?\n0 - Yes\n1 - No\n");
            scanf("%d", &cont);
        }
	}

    exit_admin:
        printf("Server closed, closing admin\n");
}

/*
└──╼ $gcc $(pkg-config --cflags libvlc) -c administrator.c -o administrator.o
└──╼ $gcc administrator.o -o administrator $(pkg-config --libs libvlc)

*/