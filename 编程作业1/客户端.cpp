#include <iostream>
#include <string>
#include <winsock2.h>
#include <WS2tcpip.h>
#include <thread>

using namespace std;
//����"Ws2_32.lib" �����
#pragma comment(lib,"ws2_32.lib")

// �����������˿�
const int PORT = 8080;  
//��������С
const int BUFFER_SIZE = 1024;

// �������ڽ��պ���ʾ��������Ϣ
void receiveAndDisplayMessages(SOCKET clientSocket) {
    char buffer[BUFFER_SIZE];
    while (true) {
        int bytesReceived = recv(clientSocket, buffer, BUFFER_SIZE, 0);
        if (bytesReceived > 0) {
            buffer[bytesReceived] = '\0'; // �ڽ��յ����ݺ���� null ��ֹ�ַ�
            cout <<  buffer << endl;
        }
    }
}

int main() {
    // ��ʼ��Winsock
    WSADATA wsData;
    if (WSAStartup(MAKEWORD(2, 2), &wsData) != 0) {
        cerr << "WSAStartup failed." << endl;
        return 1;
    }

    //�����׽���
    SOCKET clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (clientSocket == INVALID_SOCKET) {
        cerr << "Failed to create client socket." << endl;
        return 1;
    }

    
    sockaddr_in serverAddr;           //��������ַ��Ϣ
    serverAddr.sin_family = AF_INET;  //ʹ��IPv4��ַ��Ϣ
    // ʹ�� inet_pton �� IP��ַת��Ϊ��������ʽ
    const char* serverIP = "127.0.0.1";
    if (inet_pton(AF_INET, serverIP, &serverAddr.sin_addr) <= 0) {
        cerr << "Invalid server IP address." << endl;
        return 1;
    }
    serverAddr.sin_port = htons(PORT);//���˿ںŴ������ֽ���С���ֽ���ת��Ϊ�����ֽ��򣨴���ֽ���

    // ���ӵ�������
    if (connect(clientSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        cerr << "Failed to connect to the server." << endl;
        return 1;
    }

    cout << "Connected to the server." << endl;

    // ������Ϣ�����߳�
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

    // �رտͻ����׽���
    closesocket(clientSocket);

    // ���� Winsock
    WSACleanup();
    system("pause");
    return 0;
}
