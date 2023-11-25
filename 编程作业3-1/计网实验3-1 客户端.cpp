#include <winsock2.h>
#include <iostream>
#include <fstream>
#include<ctime>
#include<cstdlib>
using namespace std;
//链接"Ws2_32.lib" 这个库
#pragma comment(lib,"ws2_32.lib")
#pragma warning(disable:4996)
/*---------------------------------给服务器发送数据-------------------------------------*/
//缓冲区大小
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
// 路由器端口
const int RouterPort = 8081;
//时间限制500毫秒
const int TIMEOUT = 500;
int timeout = TIMEOUT;
//输出数据报信息
void printDatagram(Datagram datagram) {
    cout << "Seq:  " << datagram.seq << "   ACK:  " << datagram.ACK << "  FIN:   " << datagram.FIN << endl;
    cout<<"  校验和:  " << datagram.checksum << "    数据长度：  " << datagram.length << endl;
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
//主动握手
bool active_shakehands(SOCKET& clientsocket, sockaddr_in& routerAddr, int routerAddrLen) {
    clock_t starttime = clock();
    while (true) {
        //超时连接不成功！
        if ((clock() - starttime) / CLOCKS_PER_SEC > maxtime) {
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
        // 接收数据
        if (recvfrom(clientsocket, (char*)&ReceiveData, sizeof(Datagram), 0, (struct sockaddr*)&routerAddr, &routerAddrLen) == SOCKET_ERROR) {
            continue;
        }
        break;
    }
    cout << "第一次握手成功" << endl;
    //计算校验和
    receive_calculate_Checksum(ReceiveData);
    //打印接收的数据报
    cout << "接收数据报： ";
    printDatagram(ReceiveData);
    //将限制时间延长
    timeout = INT_MAX;
    if (setsockopt(clientsocket, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout)) == SOCKET_ERROR) {
        cerr << "setsockopt failed" << endl;
        return false;
    }
    while (!(ReceiveData.ACK == 1 && ReceiveData.SYN == 1 && ReceiveData.ack == SendData.seq + 1 && ReceiveData.checksum == 0)) {
        //超时挥手失败！
        if ((clock() - starttime) / CLOCKS_PER_SEC > maxtime) {
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
    //将限制时间恢复正常
    timeout = TIMEOUT;
    if (setsockopt(clientsocket, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout)) == SOCKET_ERROR) {
        cerr << "setsockopt failed" << endl;
        return false;
    }
    while (true) {
        //超时连接不成功！
        if ((clock() - starttime) / CLOCKS_PER_SEC > maxtime) {
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
        // 接收数据
        if (recvfrom(clientsocket, (char*)&ReceiveData, sizeof(Datagram), 0, (struct sockaddr*)&routerAddr, &routerAddrLen) == SOCKET_ERROR) {
            continue;
        }
        break;
    }
    cout << "第三次握手成功" << endl;
    cout << "第四次握手成功" << endl;
    return true;
}
//传输文件
int transform_file(char filepath[], SOCKET& clientSocket, sockaddr_in& routerAddr, int& routerAddrLen) {
    cout << "现在开始发送  " << filepath << endl;
    // 以二进制模式打开文件（图片或文本文件）
    ifstream file(filepath, ios::binary);
    if (!file) {
        std::cerr << "Failed to open the file." << std::endl;
        closesocket(clientSocket);
        WSACleanup();
        return 0;
    }
    //文件总字节
    int totalBytes = 0;
    // 读取文件内容并分割成数据包
    int sequenceNumber = 0;
    //开始传输时间
    clock_t startTime = clock();
    //执行到到文件末尾（end of file，EOF）
    while (!file.eof()) {
        memset(&SendData, 0, sizeof(SendData));
        SendData.seq = sequenceNumber++;
        sequenceNumber = sequenceNumber % 2;
        //每次从文件中读取最多BUFFER_SIZE 字节的数据
        file.read(SendData.data, BUFFER_SIZE);
        int bytesToSend = static_cast<int>(file.gcount());
        totalBytes += bytesToSend;
        SendData.length = bytesToSend;
        //计算校验和
        send_calculate_Checksum(SendData);
        // 发送数据包
        while (true) {
            //打印发送的数据报
            cout << "发送数据报： ";
            printDatagram(SendData);
            if (sendto(clientSocket, (char*)&SendData, sizeof(SendData), 0, (SOCKADDR*)&routerAddr, sizeof(routerAddr)) == SOCKET_ERROR) {
                cerr << "发送错误!" << endl;
            }
            // 等待反馈  
            memset(&ReceiveData, 0, sizeof(ReceiveData));
            // 接收数据
            if (recvfrom(clientSocket, (char*)&ReceiveData, sizeof(Datagram), 0, (struct sockaddr*)&routerAddr, &routerAddrLen) == SOCKET_ERROR) {
                continue;
            }
            receive_calculate_Checksum(ReceiveData);
            //打印接收的数据报
            cout << "接收数据报： ";
            printDatagram(ReceiveData);
            //因为sequenceNumber已经变了，而它只有两个值，所以不等于这个，就等于之前那个
            if (ReceiveData.ACK != sequenceNumber && ReceiveData.checksum == 0) {
                break;
            }
        }
    }
    file.close();
    clock_t endTime = clock();
    double transferTime = static_cast<double>(endTime - startTime) / CLOCKS_PER_SEC;
    double throughput = (totalBytes / 1024) / transferTime; // 计算吞吐量（单位：KB/s）
    cout << filepath << "传输完毕" << endl;
    cout << "传输文件大小为： " << totalBytes <<"B"<<endl;
    cout << "传输时间为： " << transferTime <<"s"<< endl;
    cout << "吞吐量为： " << throughput << "KB/s"<<endl;
    //告诉对方文件传输完了
    while (true) {
        memset(&SendData, 0, sizeof(SendData));
        SendData.FIN = -1;
        send_calculate_Checksum(SendData);
        //打印发送的数据报
        cout << "发送数据报： ";
        printDatagram(SendData);
        sendto(clientSocket, (char*)&SendData, sizeof(SendData), 0, (SOCKADDR*)&routerAddr, sizeof(routerAddr));
        // 接收数据
        if (recvfrom(clientSocket, (char*)&ReceiveData, sizeof(Datagram), 0, (struct sockaddr*)&routerAddr, &routerAddrLen) == SOCKET_ERROR) {
            continue;
        }
        break;
    }
    return 1;
}
//主动挥手
bool active_wakehands(SOCKET& clientsocket, sockaddr_in& routerAddr, int routerAddrLen) {
    clock_t starttime = clock();
    while (true) {
        //超时挥手失败！
        if ((clock() - starttime) / CLOCKS_PER_SEC > maxtime) {
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
        // 接收数据
        if (recvfrom(clientsocket, (char*)&ReceiveData, sizeof(Datagram), 0, (struct sockaddr*)&routerAddr, &routerAddrLen) == SOCKET_ERROR) {
            continue;
        }
        break;
    }
    cout << "第一次挥手成功" << endl;
    //计算校验和
    receive_calculate_Checksum(ReceiveData);
    //打印接收的数据报
    cout << "接收数据报： ";
    printDatagram(ReceiveData);
    //将限制时间延长
    timeout = INT_MAX;
    if (setsockopt(clientsocket, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout)) == SOCKET_ERROR) {
        cerr << "setsockopt failed" << endl;
        return false;
    }
    while (!(ReceiveData.ACK == 1 && ReceiveData.ack == SendData.seq + 1 && ReceiveData.checksum == 0)) {
        //超时挥手失败！
        if ((clock() - starttime) / CLOCKS_PER_SEC > maxtime) {
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
        if ((clock() - starttime) / CLOCKS_PER_SEC > maxtime) {
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

    //将限制时间恢复正常
    timeout = TIMEOUT;
    if (setsockopt(clientsocket, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout)) == SOCKET_ERROR) {
        cerr << "setsockopt failed" << endl;
        return false;
    }
    while (true) {
        //超时挥手失败！
        if ((clock() - starttime) / CLOCKS_PER_SEC > maxtime) {
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
        // 接收数据
        if (recvfrom(clientsocket, (char*)&ReceiveData, sizeof(Datagram), 0, (struct sockaddr*)&routerAddr, &routerAddrLen) == SOCKET_ERROR) {
            continue;
        }
        break;
    }
    cout << "第四次挥手成功" << endl;
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
    SOCKET clientSocket = socket(AF_INET, SOCK_DGRAM, 0);
    if (clientSocket == INVALID_SOCKET) {
        cerr << "Failed to create socket" << endl;
        WSACleanup();
        return -1;
    }
    // 设置超时选项
    if (setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout)) == SOCKET_ERROR) {
        cerr << "setsockopt failed" << endl;
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
 
    if (transform_file(jpg1_path, clientSocket, routerAddr, routerAddrLen) == 0) {
        return -1;
    };
    if (transform_file(jpg2_path, clientSocket, routerAddr, routerAddrLen) == 0) {
        return -1;
    };
    if (transform_file(jpg3_path, clientSocket, routerAddr, routerAddrLen) == 0) {
        return -1;
    };
    if (transform_file(txt_path, clientSocket, routerAddr, routerAddrLen) == 0) {
        return -1;
    };
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
