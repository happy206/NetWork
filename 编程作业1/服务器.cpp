#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <winsock2.h>
using namespace std;
//链接"Ws2_32.lib" 这个库
#pragma comment(lib,"ws2_32.lib")


// 客户端列表
vector<SOCKET> clients;
//客户端序号列表
vector<int> clientid;
// 用于保护客户端列表的互斥锁
mutex mtx;
// 服务器监听端口
const int PORT = 8080;
//缓冲区大小
const int BUFFER_SIZE = 1024;
// 分配客户端序号的全局计数器
int clientCounter = 0;


// 服务器接收消息的线程
void ReceiveMessages(SOCKET client) {
	char buffer[BUFFER_SIZE];
    // 寻找客户端的索引
	int clientIndex;
	{
		lock_guard<mutex> lock(mtx);
		auto it = find(clients.begin(), clients.end(), client);
		if (it != clients.end()) {
			// 找到了套接字，it 是指向找到元素的迭代器
			size_t index = distance(clients.begin(), it);
			clientIndex = clientid[index];
		}
	}
	while (true) {
		int bytesReceived = recv(client, buffer, sizeof(buffer), 0);
		if (bytesReceived <= 0) {
			// 客户端断开连接
			lock_guard<mutex> lock(mtx);
			clients.erase(remove(clients.begin(), clients.end(), client), clients.end());
			clientid.erase(remove(clientid.begin(), clientid.end(), clientIndex), clientid.end());
			closesocket(client);
			cout << "Client " << clientIndex << "  断开连接" << endl;
			break;
		}
		// 构建包含客户端序号的消息
		string message = "Client " + to_string(clientIndex) + ": " + string(buffer, bytesReceived);
		lock_guard<mutex> lock(mtx);
		cout << message << endl;
		// 广播消息给所有客户端（除发送消息的客户端）
		for (SOCKET otherClient : clients) {
			if (otherClient != client) {
				send(otherClient, message.c_str(), message.size(), 0);
			}
		}
	}
}

// 给客户端分配序号
void AssignClientIndex(SOCKET client) {

	// 为客户端分配唯一的序号
	int clientIndex = clientCounter++;
	lock_guard<mutex> lock(mtx);
	clients.push_back(client);
	clientid.push_back(clientIndex);
	// 向客户端发送分配的序号
	cout << "Client " << clientIndex << "  connected " << endl;
	string message = "You are assigned index " + to_string(clientIndex);
	send(client, message.c_str(), message.size(), 0);

}

int main()
{

	//初始化WSA(WinSock)
	WORD sockVersion = MAKEWORD(2, 2); //请求使用 Winsock 2.2 版本
	WSADATA wsaData;                   //WSADATA结构体变量的地址值

	//成功时会返回0，失败时返回非零的错误代码值
	if (WSAStartup(sockVersion, &wsaData) != 0)
	{
		cerr << "WSAStartup() error!" << endl;
		return 1;
	}

	//创建套接字
	SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP); //IPv4、流式套接字、TCP
	if (serverSocket == INVALID_SOCKET) //INVALID_SOCKET: 无效套接字的特殊值
	{
		cerr << "Failed to create server socket." << endl;
		return 1;
	}

	//绑定IP和端口
	sockaddr_in serverAddr;//IPv4的指定方法是使用struct sockaddr_in类型的变量
	serverAddr.sin_family = AF_INET;     //IPv4
	serverAddr.sin_addr.S_un.S_addr = INADDR_ANY;//IP地址设置成INADDR_ANY，让系统自动获取本机的IP地址
	serverAddr.sin_port = htons(PORT);//设置端口。htons将主机字节序转换为网络字节顺序
	//bind函数把一个地址族中的特定地址赋给socket。
	if (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
	{
		cerr << "Bind failed." << endl;
		return 1;
	}

	//开始监听
	if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR)
	{
		cout << "listen error !" << endl;
		return 1;
	}

	cout << "Server is listening on port " << PORT << endl;

	while (true) {
		// 接受客户端连接
		SOCKET client = accept(serverSocket, NULL, NULL);
		if (client == INVALID_SOCKET) {
			cerr << "Accept failed." << endl;
		}
		else {
			// 给客户端分配序号
			AssignClientIndex(client);
			// 创建一个新线程来处理客户端消息
			thread clientThread(ReceiveMessages, client);
			clientThread.detach();
		}
	}
	closesocket(serverSocket);
	WSACleanup();
	system("pause");
	return 0;
}



