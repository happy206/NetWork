#include <winsock2.h>
#include <iostream>
#include <fstream>
#include<ctime>
#include<cstdlib>
#include<vector>
#include<thread>
#include <mutex>
using namespace std;
//链接"Ws2_32.lib" 这个库
#pragma comment(lib,"ws2_32.lib")
//忽略4996错误
#pragma warning(disable:4996)
/*----------------------接收来自客户端的数据----------------------------*/
// 服务器监听端口
const int PORT = 8080;
//数据大小
const int BUFFER_SIZE = 10240;
//互斥锁
mutex mtx;        //用于保护接收窗口
mutex coutmutex;  //保护cout
// 传输数据时的序号空间大小
const int MaxSeq = 100;
//接收窗口大小
const int WindowSize = 12;
//接收数据时间限制500毫秒
const int TIMEOUT = 500;
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
// 接收窗口
vector<Datagram>ReceiveDataArray;
vector<int>ReceiveSeqArray;
// 记录接收窗口的分组是否收到
vector<int>isreceived;
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
// 设置Socket为非阻塞模式
bool setNonBlocking(SOCKET socket) {
    u_long mode = 1;  // 1表示非阻塞，0表示阻塞
    if (ioctlsocket(socket, FIONBIO, &mode) == SOCKET_ERROR) {
        cerr << "设置非阻塞模式失败！ " << endl;
        return false;
    }
    return true;
}
// 设置Socket为阻塞模式
bool setBlocking(SOCKET socket) {
    u_long mode = 0;  // 0表示阻塞，1表示非阻塞
    if (ioctlsocket(socket, FIONBIO, &mode) == SOCKET_ERROR) {
        cerr << "设置阻塞模式失败！ " << endl;
        return false;
    }
    return true;
}
//被动握手
bool passive_shakehands(SOCKET& serversocket, sockaddr_in& routerAddr, int routerAddrLen) {
    clock_t total_time = clock();
    //设置为阻塞状态
    if (!setBlocking(serversocket)) {
        return false;
    }
    while (true) {
        //超时连接不成功！
        if ((clock() - total_time) / CLOCKS_PER_SEC > maxtime) {
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
        if (!(ReceiveData.checksum == 0 && ReceiveData.SYN == 1)) {
            continue;
        }
        break;
    }
    cout << "第一次握手成功" << endl;
    //设置为非阻塞状态
    if (!setNonBlocking(serversocket)) {
        return false;
    }
    while (true) {
        if ((clock() - total_time) / CLOCKS_PER_SEC > maxtime) {
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
        //定时器启动
        clock_t start = clock();
        bool istimeout = true;
        while (clock() - start <= TIMEOUT) {
            // 接收数据
            if (recvfrom(serversocket, (char*)&ReceiveData, sizeof(Datagram), 0, (struct sockaddr*)&routerAddr, &routerAddrLen) <= 0) {
                continue;
            }
            istimeout = false;
            break;
        }
        if (!istimeout) {
            cout << "第二次握手成功" << endl;
            break;
        }
    }
    //计算校验和
    receive_calculate_Checksum(ReceiveData);
    //打印接受的数据报
    cout << "接收数据报： ";
    printDatagram(ReceiveData);
    //设置为阻塞状态
    if (!setBlocking(serversocket)) {
        return false;
    }
    while (!(ReceiveData.ACK == 1 && ReceiveData.seq == SendData.ack && ReceiveData.ack == SendData.seq + 1 && ReceiveData.checksum == 0)) {
        //超时挥手失败！
        if ((clock() - total_time) / CLOCKS_PER_SEC > maxtime) {
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
    //发送一个空包
    if (sendto(serversocket, (char*)&SendData, sizeof(Datagram), 0, (struct sockaddr*)&routerAddr, routerAddrLen) == SOCKET_ERROR) {
        cerr << "发送错误！" << endl;
    }
    cout << "第四次握手成功" << endl;
    return true;
}
//交付分组函数
void delivery_group(char filepath[],int& sequenceNumber) {
    cout << "交付分组线程开启!" << endl;
    // 打开文件用于写入接收的数据
    ofstream outputFile(filepath, ios::binary);
    if (!outputFile) {
        cerr << "Failed to open the output file." << endl;
        return ;
    }
    int flag;
    while (true) {
        {
            lock_guard<mutex> lock(mtx);
            flag = isreceived[0];
            if (flag == 1) {
                //交付分组
                outputFile.write(ReceiveDataArray.front().data, ReceiveDataArray.front().length);
                //接收窗口向前移动
                ReceiveDataArray.erase(ReceiveDataArray.begin());
                Datagram temp;
                ReceiveDataArray.push_back(temp);
                isreceived.erase(isreceived.begin());
                isreceived.push_back(0);
                ReceiveSeqArray.erase(ReceiveSeqArray.begin());
                ++sequenceNumber;
                sequenceNumber %= MaxSeq;
                ReceiveSeqArray.push_back(sequenceNumber);
                {
                    lock_guard<mutex> lock(coutmutex);
                    cout << "接收窗口大小为：" << ReceiveSeqArray.size() << endl;
                    cout << "接收窗口的下沿为:" << ReceiveSeqArray.front()<< endl;
                    cout << "接收窗口的上沿为:" << ReceiveSeqArray.back()<< endl;
                }
            }
        }
        if (flag == -1) {
            outputFile.close();
            {
                lock_guard<mutex> lock(coutmutex);
                cout << "交付分组线程关闭!" << endl;
            }
            break;
        }
    }
}
//接收文件
int receive_file(char filepath[], SOCKET& serverSocket, sockaddr_in& routerAddr, int& routerAddrLen) {
    cout << "开始接收" << filepath << endl;
    int sequenceNumber = -1;  //规定发送的第一个序号为0
    //初始化接收窗口
    ReceiveSeqArray.clear();
    ReceiveDataArray.clear();
    isreceived.clear();
    while (ReceiveSeqArray.size() < WindowSize) {
        ReceiveSeqArray.push_back(++sequenceNumber);
        Datagram temp;
        ReceiveDataArray.push_back(temp);
        isreceived.push_back(0);
        if (sequenceNumber >= MaxSeq) {
            sequenceNumber %= MaxSeq;
        }
    }
    //启动交付线程
    thread DeliveryThread(delivery_group, filepath, ref(sequenceNumber));
    DeliveryThread.detach();
    // 接收并重组数据包
    while (true) {
        memset(&ReceiveData, 0, sizeof(Datagram));
        int receivedBytes = recvfrom(serverSocket, (char*)&ReceiveData, sizeof(ReceiveData), 0, (SOCKADDR*)&routerAddr, &routerAddrLen);
        if (receivedBytes == SOCKET_ERROR) {
            continue;
        }
        receive_calculate_Checksum(ReceiveData);
        {
            lock_guard<mutex> lock(coutmutex);
            //打印接受的数据报
            cout << "接收数据报： ";
            printDatagram(ReceiveData);
        }
        if (ReceiveData.FIN == -1 && ReceiveData.checksum == 0) {
            lock_guard<mutex> lock(mtx);
            isreceived[0] = -1;
            break;
        }
        if (ReceiveData.checksum != 0) {
            continue;
        }
        //检查接受的数据包是否在接收窗口内
        int tempseq;
        for (int i = 0; i < WindowSize; i++) {
            lock_guard<mutex> lock(mtx);
            tempseq = ReceiveSeqArray[i];
            if (tempseq == ReceiveData.seq) {
                ReceiveDataArray[i] = ReceiveData;
                isreceived[i] = 1;
                break;
            }
        }
        //将要发送的数据清空
        memset(&SendData, 0, sizeof(Datagram));
        SendData.ACK = ReceiveData.seq;
        send_calculate_Checksum(SendData);
        {
            lock_guard<mutex> lock(coutmutex);
            //打印发送的数据报
            cout << "发送数据报： ";
            printDatagram(SendData);
        }
        if (sendto(serverSocket, (char*)&SendData, sizeof(Datagram), 0, (struct sockaddr*)&routerAddr, routerAddrLen) == SOCKET_ERROR) {
            cerr << "发送错误！" << endl;
        }
    }
    //将要发送的数据清空
    memset(&SendData, 0, sizeof(Datagram));
    SendData.FIN = -1;
    //告诉客户端我知道文件传输完了
    if (sendto(serverSocket, (char*)&SendData, sizeof(Datagram), 0, (struct sockaddr*)&routerAddr, routerAddrLen) == SOCKET_ERROR) {
        cerr << "发送错误！" << endl;
    }
    cout << filepath << " 文件接收完毕！" << endl;
    return 1;
}
//被动挥手
bool passive_wakehands(SOCKET& serversocket, sockaddr_in& routerAddr, int routerAddrLen) {
    clock_t total_time = clock();
    //设置为阻塞状态
    if (!setBlocking(serversocket)) {
        return false;
    }
    while (true) {
        //超时挥手失败！
        if ((clock() - total_time) / CLOCKS_PER_SEC > maxtime) {
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
    //设置为非阻塞状态
    if (!setNonBlocking(serversocket)) {
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
        if ((clock() - total_time) / CLOCKS_PER_SEC > maxtime) {
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
        //定时器启动
        clock_t start = clock();
        bool istimeout = true;
        while (clock() - start <= TIMEOUT) {
            // 接收数据
            if (recvfrom(serversocket, (char*)&ReceiveData, sizeof(Datagram), 0, (struct sockaddr*)&routerAddr, &routerAddrLen) <= 0) {
                continue;
            }
            istimeout = false;
            break;
        }
        if (!istimeout) {
            break;
        }
    }
    cout << "第三次挥手成功" << endl;
    //计算校验和
    receive_calculate_Checksum(ReceiveData);
    //打印接受的数据报
    cout << "接收数据报： ";
    printDatagram(ReceiveData);
    //设置为阻塞状态
    if (!setBlocking(serversocket)) {
        return false;
    }
    while (!(ReceiveData.ACK == 1 && ReceiveData.seq == SendData.ack && ReceiveData.ack == SendData.seq + 1 && ReceiveData.checksum == 0)) {
        //超时挥手失败！
        if ((clock() - total_time) / CLOCKS_PER_SEC > maxtime) {
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
bool choose_file(int file_number, SOCKET& serverSocket, sockaddr_in& routerAddr, int routerAddrLen) {
    switch (file_number) {
    case 1:
        if (receive_file(jpg1_path, serverSocket, routerAddr, routerAddrLen) == 0) {
            return false;
        }
        break;
    case 2:
        if (receive_file(jpg2_path, serverSocket, routerAddr, routerAddrLen) == 0) {
            return false;
        };
        break;
    case 3:
        if (receive_file(jpg3_path, serverSocket, routerAddr, routerAddrLen) == 0) {
            return false;
        };
        break;
    case 4:
        if (receive_file(txt_path, serverSocket, routerAddr, routerAddrLen) == 0) {
            return false;
        };
        break;
    }
    return true;
}
void receive_number(int& number, SOCKET& serverSocket, sockaddr_in& routerAddr, int routerAddrLen) {
    while (true) {
        //将要接受的数据清空
        memset(&ReceiveData, 0, sizeof(Datagram));
        recvfrom(serverSocket, (char*)&ReceiveData, sizeof(Datagram), 0, (struct sockaddr*)&routerAddr, &routerAddrLen);
        //计算校验和
        receive_calculate_Checksum(ReceiveData);
        //打印接收的数据报
        cout << "接收数据报： ";
        printDatagram(ReceiveData);
        if (ReceiveData.checksum == 0 && ReceiveData.FIN != -1) {
            number = ReceiveData.SYN;
            //将要发送的数据清空
            memset(&SendData, 0, sizeof(Datagram));
            SendData.ACK = 1;
            //计算校验和
            send_calculate_Checksum(SendData);
            //打印发送的数据报
            cout << "发送数据包： ";
            printDatagram(SendData);
            if (sendto(serverSocket, (char*)&SendData, sizeof(Datagram), 0, (struct sockaddr*)&routerAddr, routerAddrLen) == SOCKET_ERROR) {
                cerr << "发送错误！" << endl;
            }
        }
        if (ReceiveData.checksum == 0 && ReceiveData.FIN == -1) {
            if (sendto(serverSocket, (char*)&SendData, sizeof(Datagram), 0, (struct sockaddr*)&routerAddr, routerAddrLen) == SOCKET_ERROR) {
                cerr << "发送错误！" << endl;
            }
            break;
        }
    }
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
    // 设置为非阻塞状态
    if (!setNonBlocking(serverSocket)) {
        closesocket(serverSocket);
        WSACleanup();
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
    cout << "UDP server is listening on port " << PORT << "..." << endl;

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
    int number;
    receive_number(number, serverSocket, routerAddr, routerAddrLen);
    cout << "将接收" << number << "个文件" << endl;
    int file_number;
    for (int i = 1; i <= number; i++) {
        cout << "现在接收第" << i << "个文件" << endl;
        receive_number(file_number, serverSocket, routerAddr, routerAddrLen);
        cout << "接受的文件为文件" << file_number << endl;

        if (!choose_file(file_number, serverSocket, routerAddr, routerAddrLen)) {
            return -1;
        }
    }
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
