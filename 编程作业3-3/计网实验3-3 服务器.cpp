#include <winsock2.h>
#include <iostream>
#include <fstream>
#include<ctime>
#include<cstdlib>
#include<vector>
#include<thread>
#include <mutex>
using namespace std;
//����"Ws2_32.lib" �����
#pragma comment(lib,"ws2_32.lib")
//����4996����
#pragma warning(disable:4996)
/*----------------------�������Կͻ��˵�����----------------------------*/
// �����������˿�
const int PORT = 8080;
//���ݴ�С
const int BUFFER_SIZE = 10240;
//������
mutex mtx;        //���ڱ������մ���
mutex coutmutex;  //����cout
// ��������ʱ����ſռ��С
const int MaxSeq = 100;
//���մ��ڴ�С
const int WindowSize = 12;
//��������ʱ������500����
const int TIMEOUT = 500;
//���һ�����ӺͶϿ�ʱ��
const int maxtime = 600; //600��
//��������ӺͶϿ�ʱ��
const int MAXTIME = 3600;//3600��
//�ļ�·��
char jpg1_path[50] = "./1.jpg";
char jpg2_path[50] = "./2.jpg";
char jpg3_path[50] = "./3.jpg";
char txt_path[50] = "./helloworld.txt";
//���ݱ�
struct Datagram {
    int ACK, SYN, FIN;
    //У���
    unsigned short int checksum; //unsigned short int��16λ
    int seq, ack;
    int length;
    char data[BUFFER_SIZE];
};
// ���յ�����
Datagram ReceiveData;
// ���մ���
vector<Datagram>ReceiveDataArray;
vector<int>ReceiveSeqArray;
// ��¼���մ��ڵķ����Ƿ��յ�
vector<int>isreceived;
// ���͵�����
Datagram SendData;
//������ݱ���Ϣ
void printDatagram(Datagram datagram) {
    cout << "Seq:  " << datagram.seq << "   ACK:  " << datagram.ACK << "  FIN:   " << datagram.FIN << endl;
    cout << "  У���:  " << datagram.checksum << "    ���ݳ��ȣ�  " << datagram.length << endl;
}
//����ʱ����У���
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
//����ʱ����У���
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
// ����SocketΪ������ģʽ
bool setNonBlocking(SOCKET socket) {
    u_long mode = 1;  // 1��ʾ��������0��ʾ����
    if (ioctlsocket(socket, FIONBIO, &mode) == SOCKET_ERROR) {
        cerr << "���÷�����ģʽʧ�ܣ� " << endl;
        return false;
    }
    return true;
}
// ����SocketΪ����ģʽ
bool setBlocking(SOCKET socket) {
    u_long mode = 0;  // 0��ʾ������1��ʾ������
    if (ioctlsocket(socket, FIONBIO, &mode) == SOCKET_ERROR) {
        cerr << "��������ģʽʧ�ܣ� " << endl;
        return false;
    }
    return true;
}
//��������
bool passive_shakehands(SOCKET& serversocket, sockaddr_in& routerAddr, int routerAddrLen) {
    clock_t total_time = clock();
    //����Ϊ����״̬
    if (!setBlocking(serversocket)) {
        return false;
    }
    while (true) {
        //��ʱ���Ӳ��ɹ���
        if ((clock() - total_time) / CLOCKS_PER_SEC > maxtime) {
            return false;
        }
        //��Ҫ���ܵ��������
        memset(&ReceiveData, 0, sizeof(Datagram));
        // ��������
        if (recvfrom(serversocket, (char*)&ReceiveData, sizeof(Datagram), 0, (struct sockaddr*)&routerAddr, &routerAddrLen) == SOCKET_ERROR) {
            continue;
        }
        //����У���
        receive_calculate_Checksum(ReceiveData);
        //��ӡ���ܵ����ݱ�
        cout << "�������ݱ��� ";
        printDatagram(ReceiveData);
        if (!(ReceiveData.checksum == 0 && ReceiveData.SYN == 1)) {
            continue;
        }
        break;
    }
    cout << "��һ�����ֳɹ�" << endl;
    //����Ϊ������״̬
    if (!setNonBlocking(serversocket)) {
        return false;
    }
    while (true) {
        if ((clock() - total_time) / CLOCKS_PER_SEC > maxtime) {
            return false;
        }
        //��Ҫ���͵��������
        memset(&SendData, 0, sizeof(Datagram));
        SendData.SYN = 1;
        SendData.ACK = 1;
        SendData.seq = (rand() % 100) + 1;
        SendData.ack = ReceiveData.seq + 1;
        //����У���
        send_calculate_Checksum(SendData);
        //��ӡ���͵����ݱ�
        cout << "�������ݰ��� ";
        printDatagram(SendData);
        if (sendto(serversocket, (char*)&SendData, sizeof(Datagram), 0, (struct sockaddr*)&routerAddr, routerAddrLen) == SOCKET_ERROR) {
            cerr << "���ʹ���" << endl;
        }
        //��Ҫ���ܵ��������
        memset(&ReceiveData, 0, sizeof(Datagram));
        //��ʱ������
        clock_t start = clock();
        bool istimeout = true;
        while (clock() - start <= TIMEOUT) {
            // ��������
            if (recvfrom(serversocket, (char*)&ReceiveData, sizeof(Datagram), 0, (struct sockaddr*)&routerAddr, &routerAddrLen) <= 0) {
                continue;
            }
            istimeout = false;
            break;
        }
        if (!istimeout) {
            cout << "�ڶ������ֳɹ�" << endl;
            break;
        }
    }
    //����У���
    receive_calculate_Checksum(ReceiveData);
    //��ӡ���ܵ����ݱ�
    cout << "�������ݱ��� ";
    printDatagram(ReceiveData);
    //����Ϊ����״̬
    if (!setBlocking(serversocket)) {
        return false;
    }
    while (!(ReceiveData.ACK == 1 && ReceiveData.seq == SendData.ack && ReceiveData.ack == SendData.seq + 1 && ReceiveData.checksum == 0)) {
        //��ʱ����ʧ�ܣ�
        if ((clock() - total_time) / CLOCKS_PER_SEC > maxtime) {
            return false;
        }
        //��Ҫ���ܵ��������
        memset(&ReceiveData, 0, sizeof(Datagram));
        if (recvfrom(serversocket, (char*)&ReceiveData, sizeof(Datagram), 0, (struct sockaddr*)&routerAddr, &routerAddrLen) == SOCKET_ERROR) {
            continue;
        }
        //����У���
        receive_calculate_Checksum(ReceiveData);
        //��ӡ���ܵ����ݱ�
        cout << "�������ݱ��� ";
        printDatagram(ReceiveData);
    }
    cout << "���������ֳɹ�" << endl;
    //��Ҫ���͵��������
    memset(&SendData, 0, sizeof(Datagram));
    //����һ���հ�
    if (sendto(serversocket, (char*)&SendData, sizeof(Datagram), 0, (struct sockaddr*)&routerAddr, routerAddrLen) == SOCKET_ERROR) {
        cerr << "���ʹ���" << endl;
    }
    cout << "���Ĵ����ֳɹ�" << endl;
    return true;
}
//�������麯��
void delivery_group(char filepath[],int& sequenceNumber) {
    cout << "���������߳̿���!" << endl;
    // ���ļ�����д����յ�����
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
                //��������
                outputFile.write(ReceiveDataArray.front().data, ReceiveDataArray.front().length);
                //���մ�����ǰ�ƶ�
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
                    cout << "���մ��ڴ�СΪ��" << ReceiveSeqArray.size() << endl;
                    cout << "���մ��ڵ�����Ϊ:" << ReceiveSeqArray.front()<< endl;
                    cout << "���մ��ڵ�����Ϊ:" << ReceiveSeqArray.back()<< endl;
                }
            }
        }
        if (flag == -1) {
            outputFile.close();
            {
                lock_guard<mutex> lock(coutmutex);
                cout << "���������̹߳ر�!" << endl;
            }
            break;
        }
    }
}
//�����ļ�
int receive_file(char filepath[], SOCKET& serverSocket, sockaddr_in& routerAddr, int& routerAddrLen) {
    cout << "��ʼ����" << filepath << endl;
    int sequenceNumber = -1;  //�涨���͵ĵ�һ�����Ϊ0
    //��ʼ�����մ���
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
    //���������߳�
    thread DeliveryThread(delivery_group, filepath, ref(sequenceNumber));
    DeliveryThread.detach();
    // ���ղ��������ݰ�
    while (true) {
        memset(&ReceiveData, 0, sizeof(Datagram));
        int receivedBytes = recvfrom(serverSocket, (char*)&ReceiveData, sizeof(ReceiveData), 0, (SOCKADDR*)&routerAddr, &routerAddrLen);
        if (receivedBytes == SOCKET_ERROR) {
            continue;
        }
        receive_calculate_Checksum(ReceiveData);
        {
            lock_guard<mutex> lock(coutmutex);
            //��ӡ���ܵ����ݱ�
            cout << "�������ݱ��� ";
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
        //�����ܵ����ݰ��Ƿ��ڽ��մ�����
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
        //��Ҫ���͵��������
        memset(&SendData, 0, sizeof(Datagram));
        SendData.ACK = ReceiveData.seq;
        send_calculate_Checksum(SendData);
        {
            lock_guard<mutex> lock(coutmutex);
            //��ӡ���͵����ݱ�
            cout << "�������ݱ��� ";
            printDatagram(SendData);
        }
        if (sendto(serverSocket, (char*)&SendData, sizeof(Datagram), 0, (struct sockaddr*)&routerAddr, routerAddrLen) == SOCKET_ERROR) {
            cerr << "���ʹ���" << endl;
        }
    }
    //��Ҫ���͵��������
    memset(&SendData, 0, sizeof(Datagram));
    SendData.FIN = -1;
    //���߿ͻ�����֪���ļ���������
    if (sendto(serverSocket, (char*)&SendData, sizeof(Datagram), 0, (struct sockaddr*)&routerAddr, routerAddrLen) == SOCKET_ERROR) {
        cerr << "���ʹ���" << endl;
    }
    cout << filepath << " �ļ�������ϣ�" << endl;
    return 1;
}
//��������
bool passive_wakehands(SOCKET& serversocket, sockaddr_in& routerAddr, int routerAddrLen) {
    clock_t total_time = clock();
    //����Ϊ����״̬
    if (!setBlocking(serversocket)) {
        return false;
    }
    while (true) {
        //��ʱ����ʧ�ܣ�
        if ((clock() - total_time) / CLOCKS_PER_SEC > maxtime) {
            return false;
        }
        //��Ҫ���ܵ��������
        memset(&ReceiveData, 0, sizeof(Datagram));
        // ��������
        if (recvfrom(serversocket, (char*)&ReceiveData, sizeof(Datagram), 0, (struct sockaddr*)&routerAddr, &routerAddrLen) == SOCKET_ERROR) {
            continue;
        }
        //����У���
        receive_calculate_Checksum(ReceiveData);
        //��ӡ���ܵ����ݱ�
        cout << "�������ݱ��� ";
        printDatagram(ReceiveData);
        if (!(ReceiveData.checksum == 0 && ReceiveData.FIN == 1)) {
            continue;
        }
        break;
    }
    cout << "��һ�λ��ֳɹ�" << endl;
    //����Ϊ������״̬
    if (!setNonBlocking(serversocket)) {
        return false;
    }
    //��Ҫ���͵��������
    memset(&SendData, 0, sizeof(Datagram));
    SendData.ACK = 1;
    SendData.seq = (rand() % 100) + 1;
    SendData.ack = ReceiveData.seq + 1;
    //����У���
    send_calculate_Checksum(SendData);
    //��ӡ���͵����ݱ�
    cout << "�������ݰ��� ";
    printDatagram(SendData);
    if (sendto(serversocket, (char*)&SendData, sizeof(Datagram), 0, (struct sockaddr*)&routerAddr, routerAddrLen) == SOCKET_ERROR) {
        cerr << "���ʹ���" << endl;
    }
    cout << "�ڶ��λ��ֳɹ�" << endl;
    while (true) {
        //��ʱ����ʧ�ܣ�
        if ((clock() - total_time) / CLOCKS_PER_SEC > maxtime) {
            return false;
        }
        //��Ҫ���͵��������
        memset(&SendData, 0, sizeof(Datagram));
        SendData.ACK = 1;
        SendData.FIN = 1;
        SendData.seq = (rand() % 100) + 1;
        SendData.ack = ReceiveData.seq + 1;
        //����У���
        send_calculate_Checksum(SendData);
        //��ӡ���͵����ݱ�
        cout << "�������ݰ��� ";
        printDatagram(SendData);
        if (sendto(serversocket, (char*)&SendData, sizeof(Datagram), 0, (struct sockaddr*)&routerAddr, routerAddrLen) == SOCKET_ERROR) {
            cerr << "���ʹ���" << endl;
        }
        //��Ҫ���ܵ��������
        memset(&ReceiveData, 0, sizeof(Datagram));
        //��ʱ������
        clock_t start = clock();
        bool istimeout = true;
        while (clock() - start <= TIMEOUT) {
            // ��������
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
    cout << "�����λ��ֳɹ�" << endl;
    //����У���
    receive_calculate_Checksum(ReceiveData);
    //��ӡ���ܵ����ݱ�
    cout << "�������ݱ��� ";
    printDatagram(ReceiveData);
    //����Ϊ����״̬
    if (!setBlocking(serversocket)) {
        return false;
    }
    while (!(ReceiveData.ACK == 1 && ReceiveData.seq == SendData.ack && ReceiveData.ack == SendData.seq + 1 && ReceiveData.checksum == 0)) {
        //��ʱ����ʧ�ܣ�
        if ((clock() - total_time) / CLOCKS_PER_SEC > maxtime) {
            return false;
        }
        //��Ҫ���ܵ��������
        memset(&ReceiveData, 0, sizeof(Datagram));
        recvfrom(serversocket, (char*)&ReceiveData, sizeof(Datagram), 0, (struct sockaddr*)&routerAddr, &routerAddrLen);
        //����У���
        receive_calculate_Checksum(ReceiveData);
        //��ӡ���յ����ݱ�
        cout << "�������ݱ��� ";
        printDatagram(ReceiveData);
    }
    cout << "���Ĵλ��ֳɹ�" << endl;
    //��㷢�͸���Ϣ�����������ֽ���
    if (sendto(serversocket, (char*)&SendData, sizeof(Datagram), 0, (struct sockaddr*)&routerAddr, routerAddrLen) == SOCKET_ERROR) {
        cerr << "���ʹ���" << endl;
    }
    cout << "����λ��ֳɹ�" << endl;
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
        //��Ҫ���ܵ��������
        memset(&ReceiveData, 0, sizeof(Datagram));
        recvfrom(serverSocket, (char*)&ReceiveData, sizeof(Datagram), 0, (struct sockaddr*)&routerAddr, &routerAddrLen);
        //����У���
        receive_calculate_Checksum(ReceiveData);
        //��ӡ���յ����ݱ�
        cout << "�������ݱ��� ";
        printDatagram(ReceiveData);
        if (ReceiveData.checksum == 0 && ReceiveData.FIN != -1) {
            number = ReceiveData.SYN;
            //��Ҫ���͵��������
            memset(&SendData, 0, sizeof(Datagram));
            SendData.ACK = 1;
            //����У���
            send_calculate_Checksum(SendData);
            //��ӡ���͵����ݱ�
            cout << "�������ݰ��� ";
            printDatagram(SendData);
            if (sendto(serverSocket, (char*)&SendData, sizeof(Datagram), 0, (struct sockaddr*)&routerAddr, routerAddrLen) == SOCKET_ERROR) {
                cerr << "���ʹ���" << endl;
            }
        }
        if (ReceiveData.checksum == 0 && ReceiveData.FIN == -1) {
            if (sendto(serverSocket, (char*)&SendData, sizeof(Datagram), 0, (struct sockaddr*)&routerAddr, routerAddrLen) == SOCKET_ERROR) {
                cerr << "���ʹ���" << endl;
            }
            break;
        }
    }
}
int main() {
    //�������������
    srand(time(0));
    // ��ʼ��Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        cerr << "WSAStartup failed" << endl;
        return -1;
    }
    // ����UDP�׽���
    SOCKET serverSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);//IPv4��֧�����ݱ�������׽��֡�UDP
    if (serverSocket == INVALID_SOCKET) {
        cerr << "Failed to create socket" << endl;
        WSACleanup();
        return -1;
    }
    // ����Ϊ������״̬
    if (!setNonBlocking(serverSocket)) {
        closesocket(serverSocket);
        WSACleanup();
        return -1;
    }
    // ���׽��ֵ�ָ���˿�
    sockaddr_in serverAddr;             //IPv4��ָ��������ʹ��struct sockaddr_in���͵ı���
    serverAddr.sin_family = AF_INET;    //IPv4
    serverAddr.sin_port = htons(PORT); // ������ָ���˿�
    serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1"); //IP��ַ

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

    //�Ĵ����ֽ�������
    while (!passive_shakehands(serverSocket, routerAddr, routerAddrLen)) {
        if ((clock() - startTime) / CLOCKS_PER_SEC > MAXTIME) {
            cout << "���Ӳ��ɹ���" << endl;
            // �ر��׽��ֺ�����Winsock
            closesocket(serverSocket);
            WSACleanup();
            return -1;
        }
    }
    cout << "���ӳɹ���" << endl;
    int number;
    receive_number(number, serverSocket, routerAddr, routerAddrLen);
    cout << "������" << number << "���ļ�" << endl;
    int file_number;
    for (int i = 1; i <= number; i++) {
        cout << "���ڽ��յ�" << i << "���ļ�" << endl;
        receive_number(file_number, serverSocket, routerAddr, routerAddrLen);
        cout << "���ܵ��ļ�Ϊ�ļ�" << file_number << endl;

        if (!choose_file(file_number, serverSocket, routerAddr, routerAddrLen)) {
            return -1;
        }
    }
    startTime = clock();
    //��λ���
    while (!passive_wakehands(serverSocket, routerAddr, routerAddrLen)) {
        if ((clock() - startTime) / CLOCKS_PER_SEC > MAXTIME) {
            cout << "���ֲ��ɹ���" << endl;
            break;
        }
    }
    // �ر��׽��ֺ�����Winsock
    closesocket(serverSocket);
    WSACleanup();
    cout << "�ѶϿ�����" << endl;
    system("pause");
    return 0;
}
