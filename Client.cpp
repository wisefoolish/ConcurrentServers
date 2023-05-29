#include<WinSock2.h>
#include<Windows.h>
#include<stdio.h>
#include<WS2tcpip.h>
#pragma comment(lib,"Ws2_32.lib")

int main()
{
    WSAData wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);

    SOCKET clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    sockaddr_in serveAddress;
    inet_pton(AF_INET, "127.0.0.1", &serveAddress.sin_addr);
    serveAddress.sin_port = htons(6000);
    serveAddress.sin_family = AF_INET;
    printf("client is running...\n");
    int retVal = connect(clientSocket, (sockaddr*)&serveAddress, sizeof(serveAddress));
    while (true)
    {
        printf("请输入要发送的内容:");
        char sendBuf[2048] = "";
        scanf_s("%s", sendBuf, 2048);
        if (strcmp(sendBuf, "quit") == 0)break;
        retVal = send(clientSocket, sendBuf, strlen(sendBuf), 0);
        recv(clientSocket, sendBuf, 2048, 0);
        printf("%s\n", sendBuf);
    }

    closesocket(clientSocket);
    WSACleanup();
    return 0;
}
