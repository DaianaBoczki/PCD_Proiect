import socket
import time
import vlc
import os

clientSocket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
port = 7588
clientSocket.connect(('127.0.0.1', port))
cont = 0
dataSize = 1024
auxRequestId = '0'*4
auxSize = '0'*4

while cont==0:
    opt = int(input("\nChoose an option:\n1 - Listen to a song\n2 - Check uploads\n3 - Upload file\n"))
    if opt == 1:
        actualSongSize = 0
        id = int(input("\nThe ID of the wanted song: "))
        songTitle = "song{songId}.mp3".format(songId = id)
        data = "---{requestId}---{songId}".format(requestId = 1, songId = id)
        clientSocket.send(data.encode())
        data = clientSocket.recv(dataSize)
        data = data.decode("utf-8", "ignore")
        print("\nReceived package: \n", data[0:12])

        j = 0
        auxSize = '0'*4
        for i in range(4, 8):
            auxSize += data[i]
            j+=1
        songSize = int(auxSize)
        print("Song size: ", songSize)

        requestId = data[11]
        data = "---6---{reqID}---{reqID}".format(reqID = requestId)
        clientSocket.send(data.encode())
        print("\nSent package: \n", data)

        audio = open(songTitle, "wb")
        for i in range(0, songSize):
            data = clientSocket.recv(dataSize)
            audio.write(data[12:])
            data = '0'*dataSize
            data = "---6---{reqID}---{reqID}".format(reqID = requestId)
            clientSocket.send(data.encode())

        audio.close()

        player = vlc.MediaPlayer(songTitle)
        player.play()
        time.sleep(30)
        player.stop()

    elif opt == 2:
        data = "---2"
        clientSocket.send(data.encode())
        print("\nSent package: \n", data)

        data = clientSocket.recv(dataSize)
        data = data.decode("utf-8", "ignore")

        print("\nThe available songs:\n", data[72:])

        requestId = data[11]
        data = "---6---{reqID}---{reqID}".format(reqID = requestId)
        clientSocket.send(data.encode())

    elif opt == 3: # /home/denisa/Bezos.mp3
        songTitle = input("\nThe title of the song: ")
        songGenre = input("\nThe genre of the song: ")
        sentNameGenre = songTitle + "|" + songGenre
        path = input("\nSong path:")
        audio = open(path, "rb")
        octSize = os.path.getsize(path)
        if octSize % (dataSize-12) == 0:
            packetsNo = octSize//(dataSize-12)
        else:
            packetsNo = octSize//(dataSize-12)+1
        print("Song size: ", octSize)
        data = '0'*dataSize

        if packetsNo > 999:
            data = "---3{packets}{name}".format(packets = packetsNo, name = sentNameGenre)
        elif packetsNo > 99:
            data = "---3-{packets}{name}".format(packets = packetsNo, name = sentNameGenre)
        elif packetsNo > 9:
            data = "---3--{packets}{name}".format(packets = packetsNo, name = sentNameGenre)
        else:
            data = "---3---{packets}{name}".format(packets = packetsNo, name = sentNameGenre)
        print("\nPackets needed: ", packetsNo)
        clientSocket.send(data.encode())
        data = clientSocket.recv(dataSize)
        data = data.decode("utf-8", "ignore")
        print("\nReceived package: \n", data[0:12])
        requestId = data[7]

        for i in range(0, packetsNo):
            if octSize >= 1012:
                toSendSize = 1012
            else:
                toSendSize = octSize
            octSize -= 1012
            sentAudio = audio.read(1012)

            if toSendSize > 999:
                data = "---5{size}---{id}".format(size = toSendSize, id = requestId)
            elif toSendSize > 99:
                data = "---5-{size}---{id}".format(size = toSendSize, id = requestId)
            elif toSendSize > 9:
                data = "---5--{size}---{id}".format(size = toSendSize, id = requestId)
            else:
                data = "---5---{size}---{id}".format(size = toSendSize, id = requestId)
            data = data.encode()
            data += sentAudio

            clientSocket.send(data)
            clientSocket.recv(dataSize)
        
        #print("\nRecommended songs: \n", data[4:])
        data = "---6---{reqID}---{reqID}".format(reqID = requestId)
        clientSocket.send(data.encode())
        audio.close()
    
    cont = int(input("\nDo you want to continue?\n0 - Yes\n1 - No\n"))


#installs needed:
# sudo apt install libasound-dev portaudio19-dev libportaudio2 libportaudiocpp0
# pip install pyaudio
# pip install pudub
# pip install python-vlc