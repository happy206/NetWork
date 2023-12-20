#include <winsock2.h>
#include <iostream>
#include <fstream>
#include <functional>
#include<ctime>
#include<cstdlib>
#include<vector>
#include<thread>
#include <mutex>
#include<Windows.h>
using namespace std;
//链接"Ws2_32.lib" 这个库
#pragma comment(lib,"ws2_32.lib")
#pragma warning(disable:4996)
/*---------------------------------给服务器发送数据-------------------------------------*/
//互斥锁
mutex coutmutex;         //保护cout
mutex mtx;               //用于保护发送窗口
//数据大小
const int BUFFER_SIZE = 10240;
//最大一次连接和断开时间
const int maxtime = 1800; //1800秒
//最大尝试连接和断开时间
const int MAXTIME = 3600;//3600秒
//文件路径
char jpg1_path[50] = "1.jpg";
char jpg2_path[50] = "2.jpg";
char jpg3_path[50] = "3.jpg";
char txt_path[50] = "helloworld.txt";
//定时器
struct Timer {
    clock_t start;
    bool on;
};
vector<Timer>timer;
bool closed ;
bool finished;
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
// 发送窗口大小
const int WindowSize = 12;
// 传输数据时的序号空间大小
const int MaxSeq = 100;
//发送窗口
vector<Datagram>SendDataArray;
// 发送的数据包
Datagram SendData;
// 路由器端口
const int RouterPort = 8081;
//时间限制500毫秒
const int TIMEOUT = 500;
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
        //若超出16位，即有进位
        if (checksum & 0xffff0000)
        {
            //将超出16位的部分置0
            checksum &= 0xffff;
            //加上进位
            checksum++;
        }
    }
    datagram.checksum = ~(checksum & 0xffff);
    //datagram.checksum = ~checksum;
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
//主动握手
bool active_shakehands(SOCKET& clientsocket, sockaddr_in& routerAddr, int routerAddrLen) {
    clock_t total_time = clock();
    while (true) {
        //超时连接不成功！
        if ((clock() - total_time) / CLOCKS_PER_SEC > maxtime) {
            return false;
        }
        //将要发送的数据清空
        memset(&SendData, 0, sizeof(Datagram));
        // 将SYN设置为1，表示要建立连接
        SendData.SYN = 1;
        //随机设定seq(1到100)
        SendData.seq = (rand() % 100) + 1;
        //计算校验和
        send_calculate_Checksum(SendData);
        //打印发送的数据报
        cout << "发送数据报： ";
        printDatagram(SendData);
        if (sendto(clientsocket, (char*)&SendData, sizeof(Datagram), 0, (struct sockaddr*)&routerAddr, routerAddrLen) == SOCKET_ERROR) {
            cerr << "发送错误！" << endl;
        }
        //将要接受的数据清空
        memset(&ReceiveData, 0, sizeof(Datagram));
        //定时器启动
        clock_t start = clock();
        bool istimeout = true;
        while (clock() - start <= TIMEOUT) {
            // 接收数据
            if (recvfrom(clientsocket, (char*)&ReceiveData, sizeof(Datagram), 0, (struct sockaddr*)&routerAddr, &routerAddrLen) <= 0) {
                continue;
            }
            istimeout = false;
            break;
        }
        if (!istimeout) {
            break;
        }
    }
    cout << "第一次握手成功" << endl;
    //计算校验和
    receive_calculate_Checksum(ReceiveData);
    //打印接收的数据报
    cout << "接收数据报： ";
    printDatagram(ReceiveData);
    //改为阻塞状态
    if (!setBlocking(clientsocket)) {
        return false;
    }
    while (!(ReceiveData.ACK == 1 && ReceiveData.SYN == 1 && ReceiveData.ack == SendData.seq + 1 && ReceiveData.checksum == 0)) {
        //超时握手失败！
        if ((clock() - total_time) / CLOCKS_PER_SEC > maxtime) {
            return false;
        }
        //将要接受的数据清空
        memset(&ReceiveData, 0, sizeof(Datagram));
        recvfrom(clientsocket, (char*)&ReceiveData, sizeof(Datagram), 0, (struct sockaddr*)&routerAddr, &routerAddrLen);
        //计算校验和
        receive_calculate_Checksum(ReceiveData);
        //打印接收的数据报
        cout << "接收数据报： ";
        printDatagram(ReceiveData);
    }
    cout << "第二次握手成功！" << endl;
    //改为非阻塞状态
    if (!setNonBlocking(clientsocket)) {
        return false;
    }
    while (true) {
        //超时连接不成功！
        if ((clock() - total_time) / CLOCKS_PER_SEC > maxtime) {
            return false;
        }
        //将要发送的数据清空
        memset(&SendData, 0, sizeof(Datagram));
        SendData.ACK = 1;
        SendData.seq = ReceiveData.ack;
        SendData.ack = ReceiveData.seq + 1;
        //计算校验和
        send_calculate_Checksum(SendData);
        //打印发送的数据报
        cout << "发送数据报： ";
        printDatagram(SendData);
        if (sendto(clientsocket, (char*)&SendData, sizeof(Datagram), 0, (struct sockaddr*)&routerAddr, routerAddrLen) == SOCKET_ERROR) {
            cerr << "发送错误！" << endl;
            return false;
        }
        //将要接受的数据清空
        memset(&ReceiveData, 0, sizeof(Datagram));
        //定时器启动
        clock_t start = clock();
        bool istimeout = true;
        while (clock() - start <= TIMEOUT) {
            // 接收数据
            if (recvfrom(clientsocket, (char*)&ReceiveData, sizeof(Datagram), 0, (struct sockaddr*)&routerAddr, &routerAddrLen) <= 0) {
                continue;
            }
            istimeout = false;
            break;
        }
        if (!istimeout) {
            break;
        }
    }
    cout << "第三次握手成功" << endl;
    cout << "第四次握手成功" << endl;
    return true;
}
//接收数据包线程
void receiveACK(SOCKET& clientSocket, sockaddr_in& routerAddr, int& routerAddrLen) {
    finished = false;
    {
        lock_guard<mutex> lock(coutmutex);
        cout << "接收线程开启!" << endl;
    }
    //等待ACK反馈
    while (true) {
        //清空接收数据  
        memset(&ReceiveData, 0, sizeof(ReceiveData));
        // 接收数据
        if (recvfrom(clientSocket, (char*)&ReceiveData, sizeof(Datagram), 0, (struct sockaddr*)&routerAddr, &routerAddrLen) <= 0) {
            if (finished) {
                //告诉对方文件传输完了
                clock_t start;
                while (true) {
                    memset(&SendData, 0, sizeof(SendData));
                    SendData.FIN = -1;
                    send_calculate_Checksum(SendData);
                    //打印发送的数据报
                    cout << "发送数据报： ";
                    printDatagram(SendData);
                    sendto(clientSocket, (char*)&SendData, sizeof(SendData), 0, (SOCKADDR*)&routerAddr, sizeof(routerAddr));
                    //定时器启动
                    start = clock();
                    bool istimeout = true;
                    while (clock() - start <= TIMEOUT) {
                        // 接收数据
                        if (recvfrom(clientSocket, (char*)&ReceiveData, sizeof(Datagram), 0, (struct sockaddr*)&routerAddr, &routerAddrLen) <= 0) {
                            continue;
                        }
                        istimeout = false;
                        break;
                    }
                    if (!istimeout) {
                        break;
                    }
                }
                break;
            }
            continue;
        }
        //计算校验和
        receive_calculate_Checksum(ReceiveData);
        {
            lock_guard<mutex> lock(coutmutex);
            //打印接收的数据报
            cout << "接收数据报： ";
            printDatagram(ReceiveData);
        }
        if (ReceiveData.checksum == 0 ) {
            if (ReceiveData.FIN != -1) {
                int base;
                int back;
                {
                    lock_guard<mutex> lock(mtx);
                    if (SendDataArray.size() == 0) {
                        continue;
                    }
                    base = SendDataArray.front().seq;
                    back = SendDataArray.back().seq;
                    if (base <= back) {
                        if (base <= ReceiveData.ACK && ReceiveData.ACK <= back) {
                            timer[ReceiveData.ACK - base].on = false;
                        }
                    }
                    else {
                        if (base <= ReceiveData.ACK) {
                            timer[ReceiveData.ACK - base].on = false;
                        }
                        if (ReceiveData.ACK <= back) {
                            timer[MaxSeq + ReceiveData.ACK - base].on = false;
                        }
                    }

                }
            }
        }
    }
    {
        lock_guard<mutex> lock(coutmutex);
        cout << "接收线程关闭！" << endl;
    }
    closed = true;
    return;
}
//检测并处理超时事件
void Handle_Timeout(SOCKET& clientSocket, sockaddr_in& routerAddr, int& routerAddrLen) {
    {
        lock_guard<mutex> lock(coutmutex);
        cout << "处理超时线程开启！" << endl;
    }
    closed = false;
    while (true) {
        //由接收线程将closed置为true
        if (closed) {
            break;
        }
        {
            lock_guard<mutex> lock(mtx);
            for (int i = 0; i < SendDataArray.size(); i++) {
                //超时重发
                if (timer[i].on && clock() - timer[i].start > TIMEOUT) {
                    lock_guard<mutex> lock(coutmutex);
                    cout << "分组" << SendDataArray[i].seq << "丢了！！！重新发送分组" << SendDataArray[i].seq << "及其之后的分组" << endl;
                    //打印发送的数据报
                    cout << "发送数据报： ";
                    printDatagram(SendDataArray[i]);
                    //将超时的分组重新发送
                    if (sendto(clientSocket, (char*)&SendDataArray[i], sizeof(SendDataArray[i]), 0, (SOCKADDR*)&routerAddr, sizeof(routerAddr)) == SOCKET_ERROR) {
                        cerr << "发送错误!" << endl;
                    }
                    timer[i].start = clock();
                }
            }
        }
        //让其他线程运行
        Sleep(1);
    }
    {
        lock_guard<mutex> lock(coutmutex);
        cout << "处理超时线程关闭！" << endl;
    }
    finished = false;
    return;
}
//传输文件
int transform_file(char filepath[], SOCKET& clientSocket, sockaddr_in& routerAddr, int& routerAddrLen) {
    cout << "现在开始发送  " << filepath << endl;
    // 以二进制模式打开文件（图片或文本文件）
    ifstream file(filepath, ios::binary);
    if (!file) {
        cerr << "Failed to open the file." << endl;
        closesocket(clientSocket);
        WSACleanup();
        return 0;
    }
    //文件总字节
    int totalBytes = 0;
    // 序列号
    int sequenceNumber = 0;  //从0开始
    // 初始化发送窗口
    SendDataArray.clear();
    timer.clear();
    //开始传输时间
    clock_t startTime = clock();
    //启动接收线程
    thread receiveACKThread(receiveACK, ref(clientSocket), ref(routerAddr), ref(routerAddrLen));
    receiveACKThread.detach();
    //启动检测并处理超时事件的线程
    thread Handle_TimeoutThread(Handle_Timeout, ref(clientSocket), ref(routerAddr), ref(routerAddrLen));
    Handle_TimeoutThread.detach();
    //执行到到文件末尾（end of file，EOF）
    while (true) {
        //发送数据
        while (SendDataArray.size() < WindowSize) {
            if (file.eof()) {
                break;
            }
            //清空要发送的数据
            memset(&SendData, 0, sizeof(SendData));
            //设置分组序号
            SendData.seq = sequenceNumber++;
            sequenceNumber = sequenceNumber % MaxSeq;
            //每次从文件中读取最多BUFFER_SIZE 字节的数据
            file.read(SendData.data, BUFFER_SIZE);
            int bytesToSend = static_cast<int>(file.gcount());
            totalBytes += bytesToSend;
            //设置数据长度
            SendData.length = bytesToSend;
            //计算校验和
            send_calculate_Checksum(SendData);
            int size; 
            int base;
            int back;
            //加入到窗口
            {
                lock_guard<mutex> lock(mtx);
                SendDataArray.push_back(SendData);
                Timer t = { clock(),true };
                timer.push_back(t);
                size = SendDataArray.size();
                base = SendDataArray.front().seq;
                back = SendDataArray.back().seq;
            }
            //打印发送的数据报
            {
                lock_guard<mutex> lock(coutmutex);
                cout << "发送数据报： ";
                printDatagram(SendData);
            }
            if (sendto(clientSocket, (char*)&SendData, sizeof(SendData), 0, (SOCKADDR*)&routerAddr, sizeof(routerAddr)) == SOCKET_ERROR) {
                cerr << "发送错误!" << endl;
            }
            {
                lock_guard<mutex> lock(coutmutex);
                cout << "发送窗口大小为：" << size<< endl;
                cout << "发送窗口的下沿为:" << base<< endl;
                cout << "发送窗口的上沿为:" << back<< endl;
            }

        }
        while (true) {
            lock_guard<mutex> lock(mtx);
            if (timer.size()>0 && !timer[0].on) {
                SendDataArray.erase(SendDataArray.begin());
                timer.erase(timer.begin());
            }
            else {
                break;
            }     
        }
        if (file.eof() && SendDataArray.size() == 0) {
            break;
        }
        Sleep(5);
    }
    file.close();
    clock_t endTime = clock();
    double transferTime = static_cast<double>(endTime - startTime) / CLOCKS_PER_SEC;
    double throughput = (totalBytes / 1024) / transferTime; // 计算吞吐量（单位：KB/s）
    cout << filepath << "传输完毕" << endl;
    cout << "传输文件大小为： " << totalBytes / 1024 << "KB" << endl;
    cout << "传输时间为： " << transferTime << "s" << endl;
    cout << "吞吐量为： " << throughput << "KB/s" << endl;
    finished = true;
    while (finished) {}
    return 1;
}
//主动挥手
bool active_wakehands(SOCKET& clientsocket, sockaddr_in& routerAddr, int routerAddrLen) {
    //改为非阻塞状态
    if (!setNonBlocking(clientsocket)) {
        return false;
    }
    clock_t total_time = clock();
    while (true) {
        //超时挥手失败！
        if ((clock() - total_time) / CLOCKS_PER_SEC > maxtime) {
            return false;
        }
        //将要发送的数据清空
        memset(&SendData, 0, sizeof(Datagram));
        // 将FIN设置为1，表示要d断开连接
        SendData.FIN = 1;
        //随机设定seq(1到100)
        SendData.seq = (rand() % 100) + 1;
        //计算校验和
        send_calculate_Checksum(SendData);
        //打印发送的数据报
        cout << "发送数据报： ";
        printDatagram(SendData);
        if (sendto(clientsocket, (char*)&SendData, sizeof(Datagram), 0, (struct sockaddr*)&routerAddr, routerAddrLen) == SOCKET_ERROR) {
            cerr << "发送错误！" << endl;
        }
        //将要接受的数据清空
        memset(&ReceiveData, 0, sizeof(Datagram));
        //定时器启动
        clock_t start = clock();
        bool istimeout = true;
        while (clock() - start <= TIMEOUT) {
            // 接收数据
            if (recvfrom(clientsocket, (char*)&ReceiveData, sizeof(Datagram), 0, (struct sockaddr*)&routerAddr, &routerAddrLen) <= 0) {
                continue;
            }
            istimeout = false;
            break;
        }
        if (!istimeout) {
            break;
        }
    }
    cout << "第一次挥手成功" << endl;
    //计算校验和
    receive_calculate_Checksum(ReceiveData);
    //打印接收的数据报
    cout << "接收数据报： ";
    printDatagram(ReceiveData);
    //改为阻塞状态
    if (!setBlocking(clientsocket)) {
        return false;
    }
    while (!(ReceiveData.ACK == 1 && ReceiveData.ack == SendData.seq + 1 && ReceiveData.checksum == 0)) {
        //超时挥手失败！
        if ((clock() - total_time) / CLOCKS_PER_SEC > maxtime) {
            return false;
        }
        //将要接受的数据清空
        memset(&ReceiveData, 0, sizeof(Datagram));
        recvfrom(clientsocket, (char*)&ReceiveData, sizeof(Datagram), 0, (struct sockaddr*)&routerAddr, &routerAddrLen);
        //计算校验和
        receive_calculate_Checksum(ReceiveData);
        //打印接收的数据报
        cout << "接收数据报： ";
        printDatagram(ReceiveData);
    }
    cout << "第二次挥手成功！" << endl;
    //将要接受的数据清空
    memset(&ReceiveData, 0, sizeof(Datagram));
    while (!(ReceiveData.ACK == 1 && ReceiveData.FIN == 1 && ReceiveData.ack == SendData.seq + 1 && ReceiveData.checksum == 0)) {
        //超时挥手失败！
        if ((clock() - total_time) / CLOCKS_PER_SEC > maxtime) {
            return false;
        }
        //将要接受的数据清空
        memset(&ReceiveData, 0, sizeof(Datagram));
        if (recvfrom(clientsocket, (char*)&ReceiveData, sizeof(Datagram), 0, (struct sockaddr*)&routerAddr, &routerAddrLen) == SOCKET_ERROR) {
            continue;
        }
        //计算校验和
        receive_calculate_Checksum(ReceiveData);
        //打印接受的数据报
        cout << "接收数据报： ";
        printDatagram(ReceiveData);
    }
    cout << "第三次挥手成功" << endl;
    //改为非阻塞状态
    if (!setNonBlocking(clientsocket)) {
        return false;
    }
    while (true) {
        //超时挥手失败！
        if ((clock() - total_time) / CLOCKS_PER_SEC > maxtime) {
            return false;
        }
        //将要发送的数据清空
        memset(&SendData, 0, sizeof(Datagram));
        SendData.ACK = 1;
        SendData.seq = ReceiveData.ack;
        SendData.ack = ReceiveData.seq + 1;
        //计算校验和
        send_calculate_Checksum(SendData);
        //打印发送的数据报
        cout << "发送数据报： ";
        printDatagram(SendData);
        if (sendto(clientsocket, (char*)&SendData, sizeof(Datagram), 0, (struct sockaddr*)&routerAddr, routerAddrLen) == SOCKET_ERROR) {
            cerr << "发送错误！" << endl;
        }
        //定时器启动
        clock_t start = clock();
        bool istimeout = true;
        while (clock() - start <= TIMEOUT) {
            // 接收数据
            if (recvfrom(clientsocket, (char*)&ReceiveData, sizeof(Datagram), 0, (struct sockaddr*)&routerAddr, &routerAddrLen) <= 0) {
                continue;
            }
            istimeout = false;
            break;
        }
        if (!istimeout) {
            break;
        }
    }
    cout << "第四次挥手成功" << endl;
    cout << "第五次挥手成功" << endl;
    return true;
}
bool choose_file(int file_number, SOCKET& clientSocket, sockaddr_in& routerAddr, int routerAddrLen) {
    switch (file_number) {
    case 1:
        if (transform_file(jpg1_path, clientSocket, routerAddr, routerAddrLen) == 0) {
            return false;
        };
        break;
    case 2:
        if (transform_file(jpg2_path, clientSocket, routerAddr, routerAddrLen) == 0) {
            return false;
        };
        break;
    case 3:
        if (transform_file(jpg3_path, clientSocket, routerAddr, routerAddrLen) == 0) {
            return false;
        };
        break;
    case 4:
        if (transform_file(txt_path, clientSocket, routerAddr, routerAddrLen) == 0) {
            return false;
        };
        break;
    }
    return true;
}
void send_number(int number, SOCKET& clientSocket, sockaddr_in& routerAddr, int routerAddrLen) {
    //计时器
    clock_t start;
    //将要发送的数据清空
    memset(&SendData, 0, sizeof(Datagram));
    SendData.SYN = number;
    send_calculate_Checksum(SendData);
    //发送数字
    while (true) {
        //打印发送的数据报
        cout << "发送数据报： ";
        printDatagram(SendData);
        if (sendto(clientSocket, (char*)&SendData, sizeof(Datagram), 0, (struct sockaddr*)&routerAddr, routerAddrLen) == SOCKET_ERROR) {
            cerr << "发送错误！" << endl;
        }
        //将要接受的数据清空
        memset(&ReceiveData, 0, sizeof(Datagram));
        //计时器启动
        start = clock();
        bool istimeout = true;
        while (clock() - start <= TIMEOUT) {
            // 接收数据
            if (recvfrom(clientSocket, (char*)&ReceiveData, sizeof(Datagram), 0, (struct sockaddr*)&routerAddr, &routerAddrLen) <= 0) {
                continue;
            }
            istimeout = false;
            break;
        }
        //计算校验和
        receive_calculate_Checksum(ReceiveData);
        if (!istimeout && ReceiveData.ACK == 1 && ReceiveData.checksum == 0) {
            break;
        }
    }
    //将要发送的数据清空
    memset(&SendData, 0, sizeof(Datagram));
    SendData.FIN = -1;
    while (true) {
        //打印发送的数据报
        cout << "发送数据报： ";
        printDatagram(SendData);
        if (sendto(clientSocket, (char*)&SendData, sizeof(Datagram), 0, (struct sockaddr*)&routerAddr, routerAddrLen) == SOCKET_ERROR) {
            cerr << "发送错误！" << endl;
        }
        //计时器启动
        start = clock();
        bool istimeout = true;
        while (clock() - start <= TIMEOUT) {
            // 接收数据
            if (recvfrom(clientSocket, (char*)&ReceiveData, sizeof(Datagram), 0, (struct sockaddr*)&routerAddr, &routerAddrLen) <= 0) {
                continue;
            }
            istimeout = false;
            break;
        }
        if (!istimeout) {
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
    SOCKET clientSocket = socket(AF_INET, SOCK_DGRAM, 0);
    if (clientSocket == INVALID_SOCKET) {
        cerr << "Failed to create socket" << endl;
        WSACleanup();
        return -1;
    }
    // 设置非阻塞状态
    if (!setNonBlocking(clientSocket)) {
        closesocket(clientSocket);
        WSACleanup();
        return -1;
    }
    // 设置路由器端地址信息
    sockaddr_in routerAddr;
    int routerAddrLen = sizeof(routerAddr);
    routerAddr.sin_family = AF_INET;
    routerAddr.sin_port = htons(RouterPort); // 路由器的端口
    routerAddr.sin_addr.s_addr = inet_addr("127.0.0.1"); // 路由器的IP地址

    clock_t startTime = clock();
    //主动发出握手
    while (!active_shakehands(clientSocket, routerAddr, sizeof(routerAddr))) {
        if ((clock() - startTime) / CLOCKS_PER_SEC > MAXTIME) {
            cout << "连接不成功！" << endl;
            // 关闭套接字和清理Winsock
            closesocket(clientSocket);
            WSACleanup();
            return -1;
        }
    }
    cout << "连接成功！" << endl;
    int number;
    int file_number;
    cout << "请输入你想传送文件的数量: " << endl;
    cin >> number;
    //发送想发送的文件数量
    send_number(number, clientSocket, routerAddr, routerAddrLen);
    for (int i = 1; i <= number; i++) {
        cout << "请输入你想传送的第" << i << "个文件: " << endl;
        cin >> file_number;
        //发送文件号
        send_number(file_number, clientSocket, routerAddr, routerAddrLen);
        if (!choose_file(file_number, clientSocket, routerAddr, routerAddrLen)) {
            return -1;
        }
    }
    startTime = clock();
    //主动挥手
    while (!active_wakehands(clientSocket, routerAddr, routerAddrLen)) {
        if ((clock() - startTime) / CLOCKS_PER_SEC > MAXTIME) {
            cout << "挥手不成功！" << endl;
            break;
        }
    }
    // 关闭套接字和清理Winsock
    closesocket(clientSocket);
    WSACleanup();
    cout << "已断开连接" << endl;
    system("pause");
    return 0;
}
