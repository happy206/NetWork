#include <winsock2.h>
#include <iostream>
#include <fstream>
#include<ctime>
#include<cstdlib>
using namespace std;
//����"Ws2_32.lib" �����
#pragma comment(lib,"ws2_32.lib")
//����4996����
#pragma warning(disable:4996)
/*----------------------�������Կͻ��˵�����----------------------------*/
// �����������˿�
const int PORT = 8080;
//��������С
const int BUFFER_SIZE = 10240;
//��������ʱ������5000����
const int TIMEOUT = 5000;
int timeout = TIMEOUT;
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
//��������
bool passive_shakehands(SOCKET& serversocket, sockaddr_in& routerAddr, int routerAddrLen) {
    clock_t starttime = clock();
    //������ʱ���ӳ�
    timeout = INT_MAX;
    if (setsockopt(serversocket, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout)) == SOCKET_ERROR) {
        cerr << "setsockopt failed" << endl;
        return false;
    }
    while (true) {
        //��ʱ���Ӳ��ɹ���
        if ((clock() - starttime) / CLOCKS_PER_SEC > maxtime) {
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
        if (!(ReceiveData.checksum == 0 && ReceiveData.SYN == 1)){
            continue;
        }
        break;
    }
    cout << "��һ�����ֳɹ�" << endl;
    //��ʱ�����ƻָ�����
    timeout = TIMEOUT;
    if (setsockopt(serversocket, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout)) == SOCKET_ERROR) {
        cerr << "setsockopt failed" << endl;
        return false;
    }
    while (true) {
        if ((clock() - starttime) / CLOCKS_PER_SEC > maxtime) {
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
        if (recvfrom(serversocket, (char*)&ReceiveData, sizeof(Datagram), 0, (struct sockaddr*)&routerAddr, &routerAddrLen) == SOCKET_ERROR) {
            continue;
        }
        cout << "�ڶ������ֳɹ�" << endl;
        break;
    }
    //����У���
    receive_calculate_Checksum(ReceiveData);
    //��ӡ���ܵ����ݱ�
    cout << "�������ݱ��� ";
    printDatagram(ReceiveData);
    //������ʱ���ӳ�
    timeout = INT_MAX;
    if (setsockopt(serversocket, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout)) == SOCKET_ERROR) {
        cerr << "setsockopt failed" << endl;
        return false;
    }
    while (!(ReceiveData.ACK == 1 && ReceiveData.seq == SendData.ack && ReceiveData.ack == SendData.seq + 1 && ReceiveData.checksum == 0)) {
        //��ʱ����ʧ�ܣ�
        if ((clock() - starttime) / CLOCKS_PER_SEC > maxtime) {
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
    if (sendto(serversocket, (char*)&SendData, sizeof(Datagram), 0, (struct sockaddr*)&routerAddr, routerAddrLen) == SOCKET_ERROR) {
        cerr << "���ʹ���" << endl;
    }
    cout << "���Ĵ����ֳɹ�" << endl;
    return true;  
}
//�����ļ�
int receive_file(char filepath[], SOCKET& serverSocket, sockaddr_in& routerAddr,int& routerAddrLen) {
    cout << "��ʼ����"<<filepath << endl;
    // ���ļ�����д����յ�����
    ofstream outputFile(filepath, ios::binary);
    if (!outputFile) {
        cerr << "Failed to open the output file." << endl;
        closesocket(serverSocket);
        WSACleanup();
        return 0;
    }
    // ���պ��������ݰ�
    int sequenceNumber = 0;
    while (true) {
        memset(&ReceiveData, 0, sizeof(Datagram));
        int receivedBytes = recvfrom(serverSocket, (char*)&ReceiveData, sizeof(ReceiveData), 0, (SOCKADDR*)&routerAddr, &routerAddrLen);
        if (receivedBytes == SOCKET_ERROR) {
            continue;
        }
        receive_calculate_Checksum(ReceiveData);
        //��ӡ���ܵ����ݱ�
        cout << "�������ݱ��� ";
        printDatagram(ReceiveData);
        if (ReceiveData.FIN == -1 && ReceiveData.checksum == 0) {
            break;
        }
        //���յ��ظ����ݱ�
        if (ReceiveData.seq != sequenceNumber && ReceiveData.checksum == 0) {
            //��Ҫ���͵��������
            memset(&SendData, 0, sizeof(Datagram));
            SendData.ACK = sequenceNumber;
            send_calculate_Checksum(SendData);
            //��ӡ���͵����ݱ�
            cout << "�������ݱ��� ";
            printDatagram(SendData);
            if (sendto(serverSocket, (char*)&SendData, sizeof(Datagram), 0, (struct sockaddr*)&routerAddr, routerAddrLen) == SOCKET_ERROR) {
                cerr << "���ʹ���" << endl;
            }
            continue;
        }
        if (ReceiveData.seq == sequenceNumber && !(ReceiveData.checksum == 0)) {
            //��Ҫ���͵��������
            memset(&SendData, 0, sizeof(Datagram));
            SendData.ACK = (sequenceNumber + 1) % 2;
            send_calculate_Checksum(SendData);
            //��ӡ���͵����ݱ�
            cout << "�������ݱ��� ";
            printDatagram(SendData);
            if (sendto(serverSocket, (char*)&SendData, sizeof(Datagram), 0, (struct sockaddr*)&routerAddr, routerAddrLen) == SOCKET_ERROR) {
                cerr << "���ʹ���" << endl;
            }
            continue;
        }
        outputFile.write(ReceiveData.data, ReceiveData.length);
        //��Ҫ���͵��������
        memset(&SendData, 0, sizeof(Datagram));
        SendData.ACK = sequenceNumber;
        send_calculate_Checksum(SendData);
        //��ӡ���͵����ݱ�
        cout << "�������ݱ��� ";
        printDatagram(SendData);
        if (sendto(serverSocket, (char*)&SendData, sizeof(Datagram), 0, (struct sockaddr*)&routerAddr, routerAddrLen) == SOCKET_ERROR) {
            cerr << "���ʹ���" << endl;
        }
        sequenceNumber = ++sequenceNumber % 2;
    }
    //��㷢�͸���Ϣ����������֪���ļ���������
    if (sendto(serverSocket, (char*)&SendData, sizeof(Datagram), 0, (struct sockaddr*)&routerAddr, routerAddrLen) == SOCKET_ERROR) {
        cerr << "���ʹ���" << endl;
    }
    outputFile.close();
    cout <<filepath<<" �ļ�������ϣ�" << endl;
    return 1;
}
//��������
bool passive_wakehands(SOCKET& serversocket, sockaddr_in& routerAddr, int routerAddrLen) {
    clock_t starttime = clock();
    //������ʱ���ӳ�
    timeout = INT_MAX;
    if (setsockopt(serversocket, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout)) == SOCKET_ERROR) {
        cerr << "setsockopt failed" << endl;
        return false;
    }
    while (true) {
        //��ʱ����ʧ�ܣ�
        if ((clock() - starttime) / CLOCKS_PER_SEC > maxtime) {
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
    //��ʱ�����ƻָ�����
    timeout = TIMEOUT;
    if (setsockopt(serversocket, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout)) == SOCKET_ERROR) {
        cerr << "setsockopt failed" << endl;
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
        if ((clock() - starttime) / CLOCKS_PER_SEC > maxtime) {
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
        if (recvfrom(serversocket, (char*)&ReceiveData, sizeof(Datagram), 0, (struct sockaddr*)&routerAddr, &routerAddrLen) == SOCKET_ERROR) {
            continue;
        }
        break;
    }
    cout << "�����λ��ֳɹ�" << endl;
    //����У���
    receive_calculate_Checksum(ReceiveData);
    //��ӡ���ܵ����ݱ�
    cout << "�������ݱ��� ";
    printDatagram(ReceiveData);
    while (!(ReceiveData.ACK == 1 && ReceiveData.seq == SendData.ack && ReceiveData.ack == SendData.seq + 1 && ReceiveData.checksum == 0)) {
        //��ʱ����ʧ�ܣ�
        if ((clock() - starttime) / CLOCKS_PER_SEC > maxtime) {
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
    // ���ó�ʱѡ��
    if (setsockopt(serverSocket, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout)) == SOCKET_ERROR) {
        cerr << "setsockopt failed" << endl;
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
    cout << "UDP server is listening on port " <<PORT<<"..." << endl;

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
