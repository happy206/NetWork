#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <winsock2.h>
using namespace std;
//����"Ws2_32.lib" �����
#pragma comment(lib,"ws2_32.lib")


// �ͻ����б�
vector<SOCKET> clients;
//�ͻ�������б�
vector<int> clientid;
// ���ڱ����ͻ����б�Ļ�����
mutex mtx;
// �����������˿�
const int PORT = 8080;
//��������С
const int BUFFER_SIZE = 1024;
// ����ͻ�����ŵ�ȫ�ּ�����
int clientCounter = 0;


// ������������Ϣ���߳�
void ReceiveMessages(SOCKET client) {
	char buffer[BUFFER_SIZE];
    // Ѱ�ҿͻ��˵�����
	int clientIndex;
	{
		lock_guard<mutex> lock(mtx);
		auto it = find(clients.begin(), clients.end(), client);
		if (it != clients.end()) {
			// �ҵ����׽��֣�it ��ָ���ҵ�Ԫ�صĵ�����
			size_t index = distance(clients.begin(), it);
			clientIndex = clientid[index];
		}
	}
	while (true) {
		int bytesReceived = recv(client, buffer, sizeof(buffer), 0);
		if (bytesReceived <= 0) {
			// �ͻ��˶Ͽ�����
			lock_guard<mutex> lock(mtx);
			clients.erase(remove(clients.begin(), clients.end(), client), clients.end());
			clientid.erase(remove(clientid.begin(), clientid.end(), clientIndex), clientid.end());
			closesocket(client);
			cout << "Client " << clientIndex << "  �Ͽ�����" << endl;
			break;
		}
		// ���������ͻ�����ŵ���Ϣ
		string message = "Client " + to_string(clientIndex) + ": " + string(buffer, bytesReceived);
		lock_guard<mutex> lock(mtx);
		cout << message << endl;
		// �㲥��Ϣ�����пͻ��ˣ���������Ϣ�Ŀͻ��ˣ�
		for (SOCKET otherClient : clients) {
			if (otherClient != client) {
				send(otherClient, message.c_str(), message.size(), 0);
			}
		}
	}
}

// ���ͻ��˷������
void AssignClientIndex(SOCKET client) {

	// Ϊ�ͻ��˷���Ψһ�����
	int clientIndex = clientCounter++;
	lock_guard<mutex> lock(mtx);
	clients.push_back(client);
	clientid.push_back(clientIndex);
	// ��ͻ��˷��ͷ�������
	cout << "Client " << clientIndex << "  connected " << endl;
	string message = "You are assigned index " + to_string(clientIndex);
	send(client, message.c_str(), message.size(), 0);

}

int main()
{

	//��ʼ��WSA(WinSock)
	WORD sockVersion = MAKEWORD(2, 2); //����ʹ�� Winsock 2.2 �汾
	WSADATA wsaData;                   //WSADATA�ṹ������ĵ�ֵַ

	//�ɹ�ʱ�᷵��0��ʧ��ʱ���ط���Ĵ������ֵ
	if (WSAStartup(sockVersion, &wsaData) != 0)
	{
		cerr << "WSAStartup() error!" << endl;
		return 1;
	}

	//�����׽���
	SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP); //IPv4����ʽ�׽��֡�TCP
	if (serverSocket == INVALID_SOCKET) //INVALID_SOCKET: ��Ч�׽��ֵ�����ֵ
	{
		cerr << "Failed to create server socket." << endl;
		return 1;
	}

	//��IP�Ͷ˿�
	sockaddr_in serverAddr;//IPv4��ָ��������ʹ��struct sockaddr_in���͵ı���
	serverAddr.sin_family = AF_INET;     //IPv4
	serverAddr.sin_addr.S_un.S_addr = INADDR_ANY;//IP��ַ���ó�INADDR_ANY����ϵͳ�Զ���ȡ������IP��ַ
	serverAddr.sin_port = htons(PORT);//���ö˿ڡ�htons�������ֽ���ת��Ϊ�����ֽ�˳��
	//bind������һ����ַ���е��ض���ַ����socket��
	if (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
	{
		cerr << "Bind failed." << endl;
		return 1;
	}

	//��ʼ����
	if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR)
	{
		cout << "listen error !" << endl;
		return 1;
	}

	cout << "Server is listening on port " << PORT << endl;

	while (true) {
		// ���ܿͻ�������
		SOCKET client = accept(serverSocket, NULL, NULL);
		if (client == INVALID_SOCKET) {
			cerr << "Accept failed." << endl;
		}
		else {
			// ���ͻ��˷������
			AssignClientIndex(client);
			// ����һ�����߳�������ͻ�����Ϣ
			thread clientThread(ReceiveMessages, client);
			clientThread.detach();
		}
	}
	closesocket(serverSocket);
	WSACleanup();
	system("pause");
	return 0;
}



