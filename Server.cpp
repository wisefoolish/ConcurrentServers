#include<WinSock2.h>
#include<Windows.h>
#include<stdio.h>
#include<process.h>
#include<tchar.h>
#include<vector>
#include<string>
#include<semaphore>
#include<WS2tcpip.h>
#pragma comment(lib,"Ws2_32.lib")

struct AcceptThreadParam
{
    bool* isExit;
    std::vector<SOCKET>* clientArray;
    HANDLE lock_client_arr;
};

unsigned int __stdcall AcceptThread(void* Param)
{
    AcceptThreadParam* param = (AcceptThreadParam*)Param;
    SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in serverAddress;
    serverAddress.sin_addr.S_un.S_addr = htonl(ADDR_ANY);
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(6000);

    bind(serverSocket, (sockaddr*)&serverAddress, sizeof(serverAddress));// 绑定

    listen(serverSocket, 1);// 监听
    while (!*(param->isExit))
    {
        sockaddr_in clientAddress = { 0 };
        int clientAddressLen = sizeof(clientAddress);
        SOCKET clientSocket = accept(serverSocket, (sockaddr*)&clientAddress, &clientAddressLen);
        char ip_address[64] = "";
        printf("接收到连接:%s\n", inet_ntop(AF_INET, (sockaddr*)&clientAddress.sin_addr, ip_address, 64));
        int iMode = 1;
        ioctlsocket(clientSocket, FIONBIO, (u_long*)&iMode);
        WaitForSingleObject(param->lock_client_arr, INFINITE);
        param->clientArray->push_back(clientSocket);
        ReleaseSemaphore(param->lock_client_arr, 1, NULL);
    }
    WaitForSingleObject(param->lock_client_arr, INFINITE);
    for (SOCKET sock : (*(param->clientArray)))closesocket(sock);
    (*(param->clientArray)).clear();
    ReleaseSemaphore(param->lock_client_arr, 1, NULL);
    closesocket(serverSocket);
    return 0;
}

struct KeyWord
{
    std::string send;
    SOCKET sock;
};

struct DealWithClientParam
{
    bool* isExit;
    std::vector<SOCKET>* clientArray;
    std::vector<KeyWord>* sendMessage;
    HANDLE lock_client_arr;
    HANDLE lock_send_msg;
};

unsigned int __stdcall DealWithClient(void* Param)
{
    DealWithClientParam* param = (DealWithClientParam*)Param;
    int index = 0;
    while (!*(param->isExit))
    {
        WaitForSingleObject(param->lock_client_arr, INFINITE);
        if(!param->clientArray->empty())
        {
            index = index % param->clientArray->size();
            SOCKET sock = (*(param->clientArray))[index];
            char buf[2048] = "";
            int retVal = recv(sock, buf, 2048, 0);
            if (retVal == SOCKET_ERROR)
            {
                int err = WSAGetLastError();
                if (err == WSAEWOULDBLOCK || err == WSAEINVAL)index++;
                else if (err == WSAEISCONN)index++;
                else
                {
                    printf("接收消息失败，连接断开\n");
                    closesocket(sock);
                    (*(param->clientArray)).erase((*(param->clientArray)).begin() + index);
                }
            }
            else if (retVal == 0)
            {
                printf("连接断开\n");
                closesocket(sock);
                (*(param->clientArray)).erase((*(param->clientArray)).begin() + index);
            }
            else
            {
                WaitForSingleObject(param->lock_send_msg, INFINITE);
                param->sendMessage->push_back({ std::string(buf),sock });
                printf("接收到消息:%s\t%llu\n", buf, param->sendMessage->size());
                ReleaseSemaphore(param->lock_send_msg, 1, NULL);
            }
        }
        ReleaseSemaphore(param->lock_client_arr, 1, NULL);
        Sleep(1);
    }
    return 0;
}

struct SendMessageParam
{
    bool* isExit;
    std::vector<KeyWord>* sendMessage;
    HANDLE lock_send_msg;
};

unsigned int __stdcall SendMessageToClient(void* Param)
{
    SendMessageParam* param = (SendMessageParam*)Param;
    int index = 0;
    while (!*(param->isExit))
    {
        WaitForSingleObject(param->lock_send_msg, INFINITE);
        if (!param->sendMessage->empty())
        {
            index = index % param->sendMessage->size();
            SOCKET sock = (*(param->sendMessage))[index].sock;
            std::string str_send = (*(param->sendMessage))[index].send;
            int retVal = send(sock, str_send.c_str(), str_send.length(), 0);
            if (retVal == SOCKET_ERROR)
            {
                int err = WSAGetLastError();
                if (err == WSAEWOULDBLOCK || err == WSAEINVAL)index++;
                else if (err == WSAEISCONN)index++;
                else
                {
                    printf("发送消息失败，连接断开\n");
                    (*(param->sendMessage)).erase((*(param->sendMessage)).begin() + index);
                }
            }
            else if (retVal == 0)
            {
                printf("连接断开\n");
                (*(param->sendMessage)).erase((*(param->sendMessage)).begin() + index);
            }
            else
            {
                (*(param->sendMessage)).erase((*(param->sendMessage)).begin() + index);
                printf("发送消息成功\t%llu\n", (*(param->sendMessage)).size());
            }
        }
        ReleaseSemaphore(param->lock_send_msg, 1, NULL);
        Sleep(1);       // 让出sendmessage队列
    }
    return 0;
}

int main()
{
    WSAData wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);

    std::vector<SOCKET> clientArray;
    std::vector<KeyWord> sendMessage;
    bool isExit = false;
    HANDLE lock_client_array = CreateSemaphore(NULL, 1, 1, _T("clientArray"));
    HANDLE lock_send_msg = CreateSemaphore(NULL, 1, 1, _T("sendMessage"));

    AcceptThreadParam AcceptParam;
    AcceptParam.clientArray = &clientArray;
    AcceptParam.isExit = &isExit;
    AcceptParam.lock_client_arr = lock_client_array;
    HANDLE accept = (HANDLE)_beginthreadex(NULL, 0, AcceptThread, &AcceptParam, 0, NULL);

    DealWithClientParam DealWithParam;
    DealWithParam.clientArray = &clientArray;
    DealWithParam.sendMessage = &sendMessage;
    DealWithParam.isExit = &isExit;
    DealWithParam.lock_client_arr = lock_client_array;
    DealWithParam.lock_send_msg = lock_send_msg;
    HANDLE deal_with = (HANDLE)_beginthreadex(NULL, 0, DealWithClient, &DealWithParam, 0, NULL);

    SendMessageParam SendParam;
    SendParam.isExit = &isExit;
    SendParam.lock_send_msg = lock_send_msg;
    SendParam.sendMessage = &sendMessage;
    HANDLE send_msg = (HANDLE)_beginthreadex(NULL, 0, SendMessageToClient, &SendParam, 0, NULL);

    getchar();
    isExit = true;
    SOCKET clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    sockaddr_in serveAddress;
    inet_pton(AF_INET, "127.0.0.1", &serveAddress.sin_addr);
    serveAddress.sin_port = htons(6000);
    serveAddress.sin_family = AF_INET;
    int retVal = connect(clientSocket, (sockaddr*)&serveAddress, sizeof(serveAddress));

    WaitForSingleObject(accept, INFINITE);
    WaitForSingleObject(deal_with, INFINITE);
    WaitForSingleObject(send_msg, INFINITE);
    CloseHandle(accept);
    CloseHandle(deal_with);
    CloseHandle(send_msg);
    CloseHandle(lock_client_array);
    CloseHandle(lock_send_msg);
    WSACleanup();
    return 0;
}
