#include <winsock2.h>
#include <iostream>
#include <fstream>
#include<ctime>
#include<cstdlib>
using namespace std;
//链接"Ws2_32.lib" 这个库
#pragma comment(lib,"ws2_32.lib")
//忽略4996错误
#pragma warning(disable:4996)
/*----------------------接收来自客户端的数据----------------------------*/
// 服务器监听端口
const int PORT = 8080;
//缓冲区大小
const int BUFFER_SIZE = 10240;
//接收数据时间限制5000毫秒
const int TIMEOUT = 5000;
int timeout = TIMEOUT;
//最大一次连接和断开时间
const int maxtime = 600; //600秒
//最大尝试连接和断开时间
const int MAXTIME = 3600;//3600秒
//文件路径
char jpg1_path[50] = "./1.jpg";
char jpg2_path[50] = "./2.jpg";
char jpg3_path[50] = "./3.jpg";
char txt_path[50] = "./helloworld.txt";
//数据报
struct Datagram {
    int ACK, SYN, FIN;
    //校验和
    unsigned short int checksum; //unsigned short int是16位
    int seq, ack;
    int length;
    char data[BUFFER_SIZE];
};
// 接收的数据
Datagram ReceiveData;
// 发送的数据
Datagram SendData;
//输出数据报信息
void printDatagram(Datagram datagram) {
    cout << "Seq:  " << datagram.seq << "   ACK:  " << datagram.ACK << "  FIN:   " << datagram.FIN << endl;
    cout << "  校验和:  " << datagram.checksum << "    数据长度：  " << datagram.length << endl;
}
//发送时计算校验和
void send_calculate_Checksum(Datagram& datagram) {
    unsigned short int* buff = (unsigned short int*) & datagram;
    int num = sizeof(Datagram) / sizeof(unsigned short int);
    datagram.checksum = 0;
    unsigned long int checksum = 0;
    while (num--)
    {
        checksum += *buff;
        buff++;
        if (checksum & 0xffff0000)
        {
            checksum &= 0xffff;
            checksum++;
        }
    }
    datagram.checksum = ~(checksum & 0xffff);
}
//接收时计算校验和
void receive_calculate_Checksum(Datagram& datagram) {
    unsigned short int* buff = (unsigned short int*) & datagram;
    int num = sizeof(Datagram) / sizeof(unsigned short int);
    unsigned long int checksum = 0;
    while (num--)
    {
        checksum += *buff++;
        if (checksum & 0xffff0000)
        {
            checksum &= 0xffff;
            checksum++;
        }
    }
    datagram.checksum = ~(checksum & 0xffff);
    /*datagram.checksum = ~checksum;*/
}
//被动握手
bool passive_shakehands(SOCKET& serversocket, sockaddr_in& routerAddr, int routerAddrLen) {
    clock_t starttime = clock();
    //将限制时间延长
    timeout = INT_MAX;
    if (setsockopt(serversocket, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout)) == SOCKET_ERROR) {
        cerr << "setsockopt failed" << endl;
        return false;
    }
    while (true) {
        //超时连接不成功！
        if ((clock() - starttime) / CLOCKS_PER_SEC > maxtime) {
            return false;
        }
        //将要接受的数据清空
        memset(&ReceiveData, 0, sizeof(Datagram));
        // 接收数据
        if (recvfrom(serversocket, (char*)&ReceiveData, sizeof(Datagram), 0, (struct sockaddr*)&routerAddr, &routerAddrLen) == SOCKET_ERROR) {
            continue;
        }
        //计算校验和
        receive_calculate_Checksum(ReceiveData);
        //打印接受的数据报
        cout << "接收数据报： ";
        printDatagram(ReceiveData);
        if (!(ReceiveData.checksum == 0 && ReceiveData.SYN == 1)){
            continue;
        }
        break;
    }
    cout << "第一次握手成功" << endl;
    //将时间限制恢复正常
    timeout = TIMEOUT;
    if (setsockopt(serversocket, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout)) == SOCKET_ERROR) {
        cerr << "setsockopt failed" << endl;
        return false;
    }
    while (true) {
        if ((clock() - starttime) / CLOCKS_PER_SEC > maxtime) {
            return false;
        }
        //将要发送的数据清空
        memset(&SendData, 0, sizeof(Datagram));
        SendData.SYN = 1;
        SendData.ACK = 1;
        SendData.seq = (rand() % 100) + 1;
        SendData.ack = ReceiveData.seq + 1;
        //计算校验和
        send_calculate_Checksum(SendData);
        //打印发送的数据报
        cout << "发送数据包： ";
        printDatagram(SendData);
        if (sendto(serversocket, (char*)&SendData, sizeof(Datagram), 0, (struct sockaddr*)&routerAddr, routerAddrLen) == SOCKET_ERROR) {
            cerr << "发送错误！" << endl;
        }
        //将要接受的数据清空
        memset(&ReceiveData, 0, sizeof(Datagram));
        if (recvfrom(serversocket, (char*)&ReceiveData, sizeof(Datagram), 0, (struct sockaddr*)&routerAddr, &routerAddrLen) == SOCKET_ERROR) {
            continue;
        }
        cout << "第二次握手成功" << endl;
        break;
    }
    //计算校验和
    receive_calculate_Checksum(ReceiveData);
    //打印接受的数据报
    cout << "接收数据报： ";
    printDatagram(ReceiveData);
    //将限制时间延长
    timeout = INT_MAX;
    if (setsockopt(serversocket, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout)) == SOCKET_ERROR) {
        cerr << "setsockopt failed" << endl;
        return false;
    }
    while (!(ReceiveData.ACK == 1 && ReceiveData.seq == SendData.ack && ReceiveData.ack == SendData.seq + 1 && ReceiveData.checksum == 0)) {
        //超时挥手失败！
        if ((clock() - starttime) / CLOCKS_PER_SEC > maxtime) {
            return false;
        }
        //将要接受的数据清空
        memset(&ReceiveData, 0, sizeof(Datagram));
        if (recvfrom(serversocket, (char*)&ReceiveData, sizeof(Datagram), 0, (struct sockaddr*)&routerAddr, &routerAddrLen) == SOCKET_ERROR) {
            continue;
        }
        //计算校验和
        receive_calculate_Checksum(ReceiveData);
        //打印接受的数据报
        cout << "接收数据报： ";
        printDatagram(ReceiveData);
    }
    cout << "第三次握手成功" << endl;
    //将要发送的数据清空
    memset(&SendData, 0, sizeof(Datagram));
    if (sendto(serversocket, (char*)&SendData, sizeof(Datagram), 0, (struct sockaddr*)&routerAddr, routerAddrLen) == SOCKET_ERROR) {
        cerr << "发送错误！" << endl;
    }
    cout << "第四次握手成功" << endl;
    return true;  
}
//接收文件
int receive_file(char filepath[], SOCKET& serverSocket, sockaddr_in& routerAddr,int& routerAddrLen) {
    cout << "开始接收"<<filepath << endl;
    // 打开文件用于写入接收的数据
    ofstream outputFile(filepath, ios::binary);
    if (!outputFile) {
        cerr << "Failed to open the output file." << endl;
        closesocket(serverSocket);
        WSACleanup();
        return 0;
    }
    // 接收和重组数据包
    int sequenceNumber = 0;
    while (true) {
        memset(&ReceiveData, 0, sizeof(Datagram));
        int receivedBytes = recvfrom(serverSocket, (char*)&ReceiveData, sizeof(ReceiveData), 0, (SOCKADDR*)&routerAddr, &routerAddrLen);
        if (receivedBytes == SOCKET_ERROR) {
            continue;
        }
        receive_calculate_Checksum(ReceiveData);
        //打印接受的数据报
        cout << "接收数据报： ";
        printDatagram(ReceiveData);
        if (ReceiveData.FIN == -1 && ReceiveData.checksum == 0) {
            break;
        }
        //接收到重复数据报
        if (ReceiveData.seq != sequenceNumber && ReceiveData.checksum == 0) {
            //将要发送的数据清空
            memset(&SendData, 0, sizeof(Datagram));
            SendData.ACK = sequenceNumber;
            send_calculate_Checksum(SendData);
            //打印发送的数据报
            cout << "发送数据报： ";
            printDatagram(SendData);
            if (sendto(serverSocket, (char*)&SendData, sizeof(Datagram), 0, (struct sockaddr*)&routerAddr, routerAddrLen) == SOCKET_ERROR) {
                cerr << "发送错误！" << endl;
            }
            continue;
        }
        if (ReceiveData.seq == sequenceNumber && !(ReceiveData.checksum == 0)) {
            //将要发送的数据清空
            memset(&SendData, 0, sizeof(Datagram));
            SendData.ACK = (sequenceNumber + 1) % 2;
            send_calculate_Checksum(SendData);
            //打印发送的数据报
            cout << "发送数据报： ";
            printDatagram(SendData);
            if (sendto(serverSocket, (char*)&SendData, sizeof(Datagram), 0, (struct sockaddr*)&routerAddr, routerAddrLen) == SOCKET_ERROR) {
                cerr << "发送错误！" << endl;
            }
            continue;
        }
        outputFile.write(ReceiveData.data, ReceiveData.length);
        //将要发送的数据清空
        memset(&SendData, 0, sizeof(Datagram));
        SendData.ACK = sequenceNumber;
        send_calculate_Checksum(SendData);
        //打印发送的数据报
        cout << "发送数据报： ";
        printDatagram(SendData);
        if (sendto(serverSocket, (char*)&SendData, sizeof(Datagram), 0, (struct sockaddr*)&routerAddr, routerAddrLen) == SOCKET_ERROR) {
            cerr << "发送错误！" << endl;
        }
        sequenceNumber = ++sequenceNumber % 2;
    }
    //随便发送个消息，告诉他我知道文件传输完了
    if (sendto(serverSocket, (char*)&SendData, sizeof(Datagram), 0, (struct sockaddr*)&routerAddr, routerAddrLen) == SOCKET_ERROR) {
        cerr << "发送错误！" << endl;
    }
    outputFile.close();
    cout <<filepath<<" 文件接收完毕！" << endl;
    return 1;
}
//被动挥手
bool passive_wakehands(SOCKET& serversocket, sockaddr_in& routerAddr, int routerAddrLen) {
    clock_t starttime = clock();
    //将限制时间延长
    timeout = INT_MAX;
    if (setsockopt(serversocket, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout)) == SOCKET_ERROR) {
        cerr << "setsockopt failed" << endl;
        return false;
    }
    while (true) {
        //超时挥手失败！
        if ((clock() - starttime) / CLOCKS_PER_SEC > maxtime) {
            return false;
        }
        //将要接受的数据清空
        memset(&ReceiveData, 0, sizeof(Datagram));
        // 接收数据
        if (recvfrom(serversocket, (char*)&ReceiveData, sizeof(Datagram), 0, (struct sockaddr*)&routerAddr, &routerAddrLen) == SOCKET_ERROR) {
            continue;
        }
        //计算校验和
        receive_calculate_Checksum(ReceiveData);
        //打印接受的数据报
        cout << "接收数据报： ";
        printDatagram(ReceiveData);
        if (!(ReceiveData.checksum == 0 && ReceiveData.FIN == 1)) {
            continue;
        }
        break;
    }
    cout << "第一次挥手成功" << endl;
    //将时间限制恢复正常
    timeout = TIMEOUT;
    if (setsockopt(serversocket, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout)) == SOCKET_ERROR) {
        cerr << "setsockopt failed" << endl;
        return false;
    }
    //将要发送的数据清空
    memset(&SendData, 0, sizeof(Datagram));
    SendData.ACK = 1;
    SendData.seq = (rand() % 100) + 1;
    SendData.ack = ReceiveData.seq + 1;
    //计算校验和
    send_calculate_Checksum(SendData);
    //打印发送的数据报
    cout << "发送数据包： ";
    printDatagram(SendData);
    if (sendto(serversocket, (char*)&SendData, sizeof(Datagram), 0, (struct sockaddr*)&routerAddr, routerAddrLen) == SOCKET_ERROR) {
        cerr << "发送错误！" << endl;
    }
    cout << "第二次挥手成功" << endl;
    while (true) {
        //超时挥手失败！
        if ((clock() - starttime) / CLOCKS_PER_SEC > maxtime) {
            return false;
        }
        //将要发送的数据清空
        memset(&SendData, 0, sizeof(Datagram));
        SendData.ACK = 1;
        SendData.FIN = 1;
        SendData.seq = (rand() % 100) + 1;
        SendData.ack = ReceiveData.seq + 1;
        //计算校验和
        send_calculate_Checksum(SendData);
        //打印发送的数据报
        cout << "发送数据包： ";
        printDatagram(SendData);
        if (sendto(serversocket, (char*)&SendData, sizeof(Datagram), 0, (struct sockaddr*)&routerAddr, routerAddrLen) == SOCKET_ERROR) {
            cerr << "发送错误！" << endl;
        }
        //将要接受的数据清空
        memset(&ReceiveData, 0, sizeof(Datagram));
        if (recvfrom(serversocket, (char*)&ReceiveData, sizeof(Datagram), 0, (struct sockaddr*)&routerAddr, &routerAddrLen) == SOCKET_ERROR) {
            continue;
        }
        break;
    }
    cout << "第三次挥手成功" << endl;
    //计算校验和
    receive_calculate_Checksum(ReceiveData);
    //打印接受的数据报
    cout << "接收数据报： ";
    printDatagram(ReceiveData);
    while (!(ReceiveData.ACK == 1 && ReceiveData.seq == SendData.ack && ReceiveData.ack == SendData.seq + 1 && ReceiveData.checksum == 0)) {
        //超时挥手失败！
        if ((clock() - starttime) / CLOCKS_PER_SEC > maxtime) {
            return false;
        }
        //将要接受的数据清空
        memset(&ReceiveData, 0, sizeof(Datagram));
        recvfrom(serversocket, (char*)&ReceiveData, sizeof(Datagram), 0, (struct sockaddr*)&routerAddr, &routerAddrLen);
        //计算校验和
        receive_calculate_Checksum(ReceiveData);
        //打印接收的数据报
        cout << "接收数据报： ";
        printDatagram(ReceiveData);
    }
    cout << "第四次挥手成功" << endl;
    //随便发送个消息，告诉他挥手结束
    if (sendto(serversocket, (char*)&SendData, sizeof(Datagram), 0, (struct sockaddr*)&routerAddr, routerAddrLen) == SOCKET_ERROR) {
        cerr << "发送错误！" << endl;
    }
    cout << "第五次挥手成功" << endl;
    return true;
}
int main() {
    //设置随机数种子
    srand(time(0));
    // 初始化Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        cerr << "WSAStartup failed" << endl;
        return -1;
    }
    // 创建UDP套接字
    SOCKET serverSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);//IPv4、支持数据报传输的套接字、UDP
    if (serverSocket == INVALID_SOCKET) {
        cerr << "Failed to create socket" << endl;
        WSACleanup();
        return -1;
    }
    // 设置超时选项
    if (setsockopt(serverSocket, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout)) == SOCKET_ERROR) {
        cerr << "setsockopt failed" << endl;
        return -1;
    }
    // 绑定套接字到指定端口
    sockaddr_in serverAddr;             //IPv4的指定方法是使用struct sockaddr_in类型的变量
    serverAddr.sin_family = AF_INET;    //IPv4
    serverAddr.sin_port = htons(PORT); // 服务器指定端口
    serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1"); //IP地址

    if (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        cerr << "Failed to bind socket" << endl;
        closesocket(serverSocket);
        WSACleanup();
        return -1;
    }
    cout << "UDP server is listening on port " <<PORT<<"..." << endl;

    sockaddr_in routerAddr;
    int routerAddrLen = sizeof(routerAddr);
    
    clock_t startTime = clock();
    
    //四次握手建立连接
    while (!passive_shakehands(serverSocket, routerAddr, routerAddrLen)) {
        if ((clock() - startTime) / CLOCKS_PER_SEC > MAXTIME) {
            cout << "连接不成功！" << endl;
            // 关闭套接字和清理Winsock
            closesocket(serverSocket);
            WSACleanup();
            return -1;
        }
    }
    cout << "连接成功！" << endl;

    if (receive_file(jpg1_path, serverSocket, routerAddr, routerAddrLen) == 0) {
        return -1;
    }
    if (receive_file(jpg2_path, serverSocket, routerAddr, routerAddrLen) == 0) {
        return -1;
    };
    if (receive_file(jpg3_path, serverSocket, routerAddr, routerAddrLen) == 0) {
        return -1;
    };
    if (receive_file(txt_path, serverSocket, routerAddr, routerAddrLen) == 0) {
        return -1;
    };
    startTime = clock();
    //五次挥手
    while (!passive_wakehands(serverSocket, routerAddr, routerAddrLen)) {
        if ((clock() - startTime) / CLOCKS_PER_SEC > MAXTIME) {
            cout << "挥手不成功！" << endl;
            break;
        }
    }
    // 关闭套接字和清理Winsock
    closesocket(serverSocket);
    WSACleanup();
    cout << "已断开连接" << endl;
    system("pause");
    return 0;
}
