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
    int opt, cont = 0, id, i, j, k, l, songSize, requestId, octSize, packetsNo, actualSongSize, toSendSize;
    char data[dataSize], songTitle[50], songGenre[50], sentNameGenre[100] = "", auxSize[4], 
        auxRequestId[4], auxOct[4], songbytes[5000000], path[200], sentAudio[dataSize - 12];
    FILE *audio, *aux;
	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(7588);
	addr.sin_addr.s_addr = inet_addr("127.0.0.1");

	if(connect(clientSocket, (struct sockaddr *) &addr , sizeof(addr)) == 0){
		printf("Connection established\n");

        while(cont == 0){
            printf("\nChoose an option:\n1 - Listen to a song\n2 - Check uploads\n3 - Upload file\n");
            scanf("%d", &opt);
            switch (opt){
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
                    for(i = 0; i < songSize; i++){
                        if (recv(clientSocket, data, dataSize, 0) < 0){
                            fprintf(stderr, "Error reading data");
                        }
                        j = 0;
                        for(k = 4; k < 8; k++){
                            auxOct[j] = data[k];
                            j++;
                        }
                        octSize = atoi(auxOct);
                        fwrite(data+12, sizeof(char), octSize, audio);
                        snprintf(data, dataSize, "---%d---%d---%c", 6, requestId, data[11]);
                        if (send(clientSocket, data, 12, 0) == -1){
                            fprintf(stderr, "Error sending data\n");
                        }
                    }

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

                    if (send(clientSocket, data, 12, 0) == -1){
                        fprintf(stderr, "Error sending data\n");
                    }
                    break;

                case 3: //      /home/denisa/BloodMoon.mp3 /home/denisa/TooYoung.mp3 /home/denisa/moon.mp3
                    printf("\nThe title of the song: ");
                    scanf("%s", songTitle);
                    printf("\nThe genre of the song: ");
                    scanf("%s", songGenre);
                    strcpy(sentNameGenre, songTitle);
                    strcat(sentNameGenre, "|");
                    strcat(sentNameGenre, songGenre);

                    printf("\nSong path:");
                    scanf("%s", path);
                    audio = fopen(path, "rb");
                    aux = fopen("/home/denisa/Documents/PCD/proiect/aux.mp3", "wb");
                    fseek(audio, 0, SEEK_END);
                    octSize = ftell(audio);
                    fseek(audio, 0, SEEK_SET);
                    if(octSize % (dataSize-12) == 0)
                        packetsNo = octSize/(dataSize-12);
                    else
                        packetsNo = octSize/(dataSize-12)+1;
                    printf("Song size: %d", octSize);

                    if(packetsNo > 999){
                        snprintf(data, dataSize, "---%d%d%s", 3, packetsNo, sentNameGenre);
                    }else if(packetsNo > 99){
                        snprintf(data, dataSize, "---%d-%d%s", 3, packetsNo, sentNameGenre);
                    }else if(packetsNo > 9){
                        snprintf(data, dataSize, "---%d--%d%s", 3, packetsNo, sentNameGenre);
                    }else{
                        snprintf(data, dataSize, "---%d---%d%s", 3, packetsNo, sentNameGenre);
                    }
                    printf("\nPackets needed: %d", packetsNo);
                    if(send(clientSocket, data, strlen(data) + 1, 0) ==-1){
                        fprintf(stderr, "Error sending data");
                    }
                    if (recv(clientSocket, data, dataSize, 0) < 0){
                        fprintf(stderr, "Error reading data");
                    }
                    // printf("\nReceived package: \n");
                    // for(i = 0; i<8; i++){
                    //     printf("%c", data[i]);
                    // }

                    j = 0;
                    for(i = 4; i < 8; i++){
                        auxRequestId[j] = data[i];
                        j++;
                    }
                    requestId = atoi(auxRequestId);

                    fread(songbytes, sizeof(char), octSize, audio);
                    for(i = 0; i < packetsNo; i++){
                        if(octSize >= 1012){
                            toSendSize = 1012;
                        }else{
                            toSendSize = octSize;
                        }
                        octSize -= 1012;
                        memcpy(sentAudio, songbytes + (i*1012), toSendSize);
                        fwrite(sentAudio, sizeof(char), toSendSize, aux);

                        if(toSendSize > 999){ //4 positions
                            snprintf(data, dataSize, "---%d%d---%d", 5, toSendSize, requestId);
                        }else if(toSendSize > 99){ //3 positions
                            snprintf(data, dataSize, "---%d-%d---%d", 5, toSendSize, requestId);
                        }else if(toSendSize > 9){ //2 positions
                            snprintf(data, dataSize, "---%d--%d---%d", 5, toSendSize, requestId);
                        }else{ //1 position
                            snprintf(data, dataSize, "---%d---%d---%d", 5, toSendSize, requestId);
                        }
                        memcpy(data+12, sentAudio, toSendSize);
                        if(send(clientSocket, data, toSendSize + 12, 0) ==-1){
                            fprintf(stderr, "Error sending data");
                        }
                        if (recv(clientSocket, data, dataSize, 0) < 0){
                            fprintf(stderr, "Error reading data");
                        }
                    }
                    // printf("\nRecommended songs: \n");
                    // for(i = 4; i<strlen(data); i++){
                    //     printf("%c", data[i]);
                    // }

                    snprintf(data, dataSize, "---%d---%d---%c", 6, data[11], data[11]);
                    if (send(clientSocket, data, 12, 0) == -1){
                        fprintf(stderr, "Error sending data\n");
                    }

                    break;

            }

            printf("\nDo you want to continue?\n0 - Yes\n1 - No\n");
            scanf("%d", &cont);
        }
	}
}

/*
compiling:
$gcc $(pkg-config --cflags libvlc) -c ordinaryClientC.c -o client.o
$gcc client.o -o client $(pkg-config --libs libvlc)
*/