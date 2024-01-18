#include <iostream>
#include <string>
#include <winsock2.h>
#include <WS2tcpip.h>
#include <thread>

using namespace std;
//链接"Ws2_32.lib" 这个库
#pragma comment(lib,"ws2_32.lib")

// 服务器监听端口
const int PORT = 8080;  
//缓冲区大小
const int BUFFER_SIZE = 1024;

// 函数用于接收和显示服务器消息
void receiveAndDisplayMessages(SOCKET clientSocket) {
    char buffer[BUFFER_SIZE];
    while (true) {
        int bytesReceived = recv(clientSocket, buffer, BUFFER_SIZE, 0);
        if (bytesReceived > 0) {
            buffer[bytesReceived] = '\0'; // 在接收的数据后添加 null 终止字符
            cout <<  buffer << endl;
        }
    }
}

int main() {
    // 初始化Winsock
    WSADATA wsData;
    if (WSAStartup(MAKEWORD(2, 2), &wsData) != 0) {
        cerr << "WSAStartup failed." << endl;
        return 1;
    }

    //创建套接字
    SOCKET clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (clientSocket == INVALID_SOCKET) {
        cerr << "Failed to create client socket." << endl;
        return 1;
    }

    
    sockaddr_in serverAddr;           //服务器地址信息
    serverAddr.sin_family = AF_INET;  //使用IPv4地址信息
    // 使用 inet_pton 将 IP地址转化为二进制形式
    const char* serverIP = "127.0.0.1";
    if (inet_pton(AF_INET, serverIP, &serverAddr.sin_addr) <= 0) {
        cerr << "Invalid server IP address." << endl;
        return 1;
    }
    serverAddr.sin_port = htons(PORT);//将端口号从主机字节序（小端字节序）转换为网络字节序（大端字节序）

    // 连接到服务器
    if (connect(clientSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        cerr << "Failed to connect to the server." << endl;
        return 1;
    }

    cout << "Connected to the server." << endl;

    // 启动消息接收线程
    thread messageThread(receiveAndDisplayMessages, clientSocket);
    messageThread.detach();

    while (true) {
        string message;
        cout << "Enter your message (or type 'quit' to exit):\n";
        getline(cin, message);

        if (message == "quit") {
            break;
        }

        send(clientSocket, message.c_str(), message.size(), 0);
    }

    // 关闭客户端套接字
    closesocket(clientSocket);

    // 清理 Winsock
    WSACleanup();
    system("pause");
    return 0;
}
