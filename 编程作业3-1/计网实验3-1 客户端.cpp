#include <winsock2.h>
#include <iostream>
#include <fstream>
#include<ctime>
#include<cstdlib>
using namespace std;
//����"Ws2_32.lib" �����
#pragma comment(lib,"ws2_32.lib")
#pragma warning(disable:4996)
/*---------------------------------����������������-------------------------------------*/
//��������С
const int BUFFER_SIZE = 10240;
//���һ�����ӺͶϿ�ʱ��
const int maxtime = 1800; //1800��
//��������ӺͶϿ�ʱ��
const int MAXTIME = 3600;//3600��
//�ļ�·��
char jpg1_path[50] = "1.jpg";
char jpg2_path[50] = "2.jpg";
char jpg3_path[50] = "3.jpg";
char txt_path[50] = "helloworld.txt";
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
// ·�����˿�
const int RouterPort = 8081;
//ʱ������500����
const int TIMEOUT = 500;
int timeout = TIMEOUT;
//������ݱ���Ϣ
void printDatagram(Datagram datagram) {
    cout << "Seq:  " << datagram.seq << "   ACK:  " << datagram.ACK << "  FIN:   " << datagram.FIN << endl;
    cout<<"  У���:  " << datagram.checksum << "    ���ݳ��ȣ�  " << datagram.length << endl;
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
        //������16λ�����н�λ
        if (checksum & 0xffff0000)
        {
            //������16λ�Ĳ�����0
            checksum &= 0xffff;
            //���Ͻ�λ
            checksum++;
        }
    }
    datagram.checksum = ~(checksum & 0xffff);
    //datagram.checksum = ~checksum;
}
//��������
bool active_shakehands(SOCKET& clientsocket, sockaddr_in& routerAddr, int routerAddrLen) {
    clock_t starttime = clock();
    while (true) {
        //��ʱ���Ӳ��ɹ���
        if ((clock() - starttime) / CLOCKS_PER_SEC > maxtime) {
            return false;
        }
        //��Ҫ���͵��������
        memset(&SendData, 0, sizeof(Datagram));
        // ��SYN����Ϊ1����ʾҪ��������
        SendData.SYN = 1;
        //����趨seq(1��100)
        SendData.seq = (rand() % 100) + 1;
        //����У���
        send_calculate_Checksum(SendData);
        //��ӡ���͵����ݱ�
        cout << "�������ݱ��� ";
        printDatagram(SendData);
        if (sendto(clientsocket, (char*)&SendData, sizeof(Datagram), 0, (struct sockaddr*)&routerAddr, routerAddrLen) == SOCKET_ERROR) {
            cerr << "���ʹ���" << endl;
        }
        //��Ҫ���ܵ��������
        memset(&ReceiveData, 0, sizeof(Datagram));
        // ��������
        if (recvfrom(clientsocket, (char*)&ReceiveData, sizeof(Datagram), 0, (struct sockaddr*)&routerAddr, &routerAddrLen) == SOCKET_ERROR) {
            continue;
        }
        break;
    }
    cout << "��һ�����ֳɹ�" << endl;
    //����У���
    receive_calculate_Checksum(ReceiveData);
    //��ӡ���յ����ݱ�
    cout << "�������ݱ��� ";
    printDatagram(ReceiveData);
    //������ʱ���ӳ�
    timeout = INT_MAX;
    if (setsockopt(clientsocket, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout)) == SOCKET_ERROR) {
        cerr << "setsockopt failed" << endl;
        return false;
    }
    while (!(ReceiveData.ACK == 1 && ReceiveData.SYN == 1 && ReceiveData.ack == SendData.seq + 1 && ReceiveData.checksum == 0)) {
        //��ʱ����ʧ�ܣ�
        if ((clock() - starttime) / CLOCKS_PER_SEC > maxtime) {
            return false;
        }
        //��Ҫ���ܵ��������
        memset(&ReceiveData, 0, sizeof(Datagram));
        recvfrom(clientsocket, (char*)&ReceiveData, sizeof(Datagram), 0, (struct sockaddr*)&routerAddr, &routerAddrLen);
        //����У���
        receive_calculate_Checksum(ReceiveData);
        //��ӡ���յ����ݱ�
        cout << "�������ݱ��� ";
        printDatagram(ReceiveData);
    }
    cout << "�ڶ������ֳɹ���" << endl;
    //������ʱ��ָ�����
    timeout = TIMEOUT;
    if (setsockopt(clientsocket, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout)) == SOCKET_ERROR) {
        cerr << "setsockopt failed" << endl;
        return false;
    }
    while (true) {
        //��ʱ���Ӳ��ɹ���
        if ((clock() - starttime) / CLOCKS_PER_SEC > maxtime) {
            return false;
        }
        //��Ҫ���͵��������
        memset(&SendData, 0, sizeof(Datagram));
        SendData.ACK = 1;
        SendData.seq = ReceiveData.ack;
        SendData.ack = ReceiveData.seq + 1;
        //����У���
        send_calculate_Checksum(SendData);
        //��ӡ���͵����ݱ�
        cout << "�������ݱ��� ";
        printDatagram(SendData);
        if (sendto(clientsocket, (char*)&SendData, sizeof(Datagram), 0, (struct sockaddr*)&routerAddr, routerAddrLen) == SOCKET_ERROR) {
            cerr << "���ʹ���" << endl;
            return false;
        }
        //��Ҫ���ܵ��������
        memset(&ReceiveData, 0, sizeof(Datagram));
        // ��������
        if (recvfrom(clientsocket, (char*)&ReceiveData, sizeof(Datagram), 0, (struct sockaddr*)&routerAddr, &routerAddrLen) == SOCKET_ERROR) {
            continue;
        }
        break;
    }
    cout << "���������ֳɹ�" << endl;
    cout << "���Ĵ����ֳɹ�" << endl;
    return true;
}
//�����ļ�
int transform_file(char filepath[], SOCKET& clientSocket, sockaddr_in& routerAddr, int& routerAddrLen) {
    cout << "���ڿ�ʼ����  " << filepath << endl;
    // �Զ�����ģʽ���ļ���ͼƬ���ı��ļ���
    ifstream file(filepath, ios::binary);
    if (!file) {
        std::cerr << "Failed to open the file." << std::endl;
        closesocket(clientSocket);
        WSACleanup();
        return 0;
    }
    //�ļ����ֽ�
    int totalBytes = 0;
    // ��ȡ�ļ����ݲ��ָ�����ݰ�
    int sequenceNumber = 0;
    //��ʼ����ʱ��
    clock_t startTime = clock();
    //ִ�е����ļ�ĩβ��end of file��EOF��
    while (!file.eof()) {
        memset(&SendData, 0, sizeof(SendData));
        SendData.seq = sequenceNumber++;
        sequenceNumber = sequenceNumber % 2;
        //ÿ�δ��ļ��ж�ȡ���BUFFER_SIZE �ֽڵ�����
        file.read(SendData.data, BUFFER_SIZE);
        int bytesToSend = static_cast<int>(file.gcount());
        totalBytes += bytesToSend;
        SendData.length = bytesToSend;
        //����У���
        send_calculate_Checksum(SendData);
        // �������ݰ�
        while (true) {
            //��ӡ���͵����ݱ�
            cout << "�������ݱ��� ";
            printDatagram(SendData);
            if (sendto(clientSocket, (char*)&SendData, sizeof(SendData), 0, (SOCKADDR*)&routerAddr, sizeof(routerAddr)) == SOCKET_ERROR) {
                cerr << "���ʹ���!" << endl;
            }
            // �ȴ�����  
            memset(&ReceiveData, 0, sizeof(ReceiveData));
            // ��������
            if (recvfrom(clientSocket, (char*)&ReceiveData, sizeof(Datagram), 0, (struct sockaddr*)&routerAddr, &routerAddrLen) == SOCKET_ERROR) {
                continue;
            }
            receive_calculate_Checksum(ReceiveData);
            //��ӡ���յ����ݱ�
            cout << "�������ݱ��� ";
            printDatagram(ReceiveData);
            //��ΪsequenceNumber�Ѿ����ˣ�����ֻ������ֵ�����Բ�����������͵���֮ǰ�Ǹ�
            if (ReceiveData.ACK != sequenceNumber && ReceiveData.checksum == 0) {
                break;
            }
        }
    }
    file.close();
    clock_t endTime = clock();
    double transferTime = static_cast<double>(endTime - startTime) / CLOCKS_PER_SEC;
    double throughput = (totalBytes / 1024) / transferTime; // ��������������λ��KB/s��
    cout << filepath << "�������" << endl;
    cout << "�����ļ���СΪ�� " << totalBytes <<"B"<<endl;
    cout << "����ʱ��Ϊ�� " << transferTime <<"s"<< endl;
    cout << "������Ϊ�� " << throughput << "KB/s"<<endl;
    //���߶Է��ļ���������
    while (true) {
        memset(&SendData, 0, sizeof(SendData));
        SendData.FIN = -1;
        send_calculate_Checksum(SendData);
        //��ӡ���͵����ݱ�
        cout << "�������ݱ��� ";
        printDatagram(SendData);
        sendto(clientSocket, (char*)&SendData, sizeof(SendData), 0, (SOCKADDR*)&routerAddr, sizeof(routerAddr));
        // ��������
        if (recvfrom(clientSocket, (char*)&ReceiveData, sizeof(Datagram), 0, (struct sockaddr*)&routerAddr, &routerAddrLen) == SOCKET_ERROR) {
            continue;
        }
        break;
    }
    return 1;
}
//��������
bool active_wakehands(SOCKET& clientsocket, sockaddr_in& routerAddr, int routerAddrLen) {
    clock_t starttime = clock();
    while (true) {
        //��ʱ����ʧ�ܣ�
        if ((clock() - starttime) / CLOCKS_PER_SEC > maxtime) {
            return false;
        }
        //��Ҫ���͵��������
        memset(&SendData, 0, sizeof(Datagram));
        // ��FIN����Ϊ1����ʾҪd�Ͽ�����
        SendData.FIN = 1;
        //����趨seq(1��100)
        SendData.seq = (rand() % 100) + 1;
        //����У���
        send_calculate_Checksum(SendData);
        //��ӡ���͵����ݱ�
        cout << "�������ݱ��� ";
        printDatagram(SendData);
        if (sendto(clientsocket, (char*)&SendData, sizeof(Datagram), 0, (struct sockaddr*)&routerAddr, routerAddrLen) == SOCKET_ERROR) {
            cerr << "���ʹ���" << endl;
        }
        //��Ҫ���ܵ��������
        memset(&ReceiveData, 0, sizeof(Datagram));
        // ��������
        if (recvfrom(clientsocket, (char*)&ReceiveData, sizeof(Datagram), 0, (struct sockaddr*)&routerAddr, &routerAddrLen) == SOCKET_ERROR) {
            continue;
        }
        break;
    }
    cout << "��һ�λ��ֳɹ�" << endl;
    //����У���
    receive_calculate_Checksum(ReceiveData);
    //��ӡ���յ����ݱ�
    cout << "�������ݱ��� ";
    printDatagram(ReceiveData);
    //������ʱ���ӳ�
    timeout = INT_MAX;
    if (setsockopt(clientsocket, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout)) == SOCKET_ERROR) {
        cerr << "setsockopt failed" << endl;
        return false;
    }
    while (!(ReceiveData.ACK == 1 && ReceiveData.ack == SendData.seq + 1 && ReceiveData.checksum == 0)) {
        //��ʱ����ʧ�ܣ�
        if ((clock() - starttime) / CLOCKS_PER_SEC > maxtime) {
            return false;
        }
        //��Ҫ���ܵ��������
        memset(&ReceiveData, 0, sizeof(Datagram));
        recvfrom(clientsocket, (char*)&ReceiveData, sizeof(Datagram), 0, (struct sockaddr*)&routerAddr, &routerAddrLen);
        //����У���
        receive_calculate_Checksum(ReceiveData);
        //��ӡ���յ����ݱ�
        cout << "�������ݱ��� ";
        printDatagram(ReceiveData);
    }
    cout << "�ڶ��λ��ֳɹ���" << endl;
    //��Ҫ���ܵ��������
    memset(&ReceiveData, 0, sizeof(Datagram));
    while (!(ReceiveData.ACK == 1 && ReceiveData.FIN == 1 && ReceiveData.ack == SendData.seq + 1 && ReceiveData.checksum == 0)) {
        //��ʱ����ʧ�ܣ�
        if ((clock() - starttime) / CLOCKS_PER_SEC > maxtime) {
            return false;
        }
        //��Ҫ���ܵ��������
        memset(&ReceiveData, 0, sizeof(Datagram));
        if (recvfrom(clientsocket, (char*)&ReceiveData, sizeof(Datagram), 0, (struct sockaddr*)&routerAddr, &routerAddrLen) == SOCKET_ERROR) {
            continue;
        }
        //����У���
        receive_calculate_Checksum(ReceiveData);
        //��ӡ���ܵ����ݱ�
        cout << "�������ݱ��� ";
        printDatagram(ReceiveData);
    }
    cout << "�����λ��ֳɹ�" << endl;

    //������ʱ��ָ�����
    timeout = TIMEOUT;
    if (setsockopt(clientsocket, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout)) == SOCKET_ERROR) {
        cerr << "setsockopt failed" << endl;
        return false;
    }
    while (true) {
        //��ʱ����ʧ�ܣ�
        if ((clock() - starttime) / CLOCKS_PER_SEC > maxtime) {
            return false;
        }
        //��Ҫ���͵��������
        memset(&SendData, 0, sizeof(Datagram));
        SendData.ACK = 1;
        SendData.seq = ReceiveData.ack;
        SendData.ack = ReceiveData.seq + 1;
        //����У���
        send_calculate_Checksum(SendData);
        //��ӡ���͵����ݱ�
        cout << "�������ݱ��� ";
        printDatagram(SendData);
        if (sendto(clientsocket, (char*)&SendData, sizeof(Datagram), 0, (struct sockaddr*)&routerAddr, routerAddrLen) == SOCKET_ERROR) {
            cerr << "���ʹ���" << endl;
        }
        // ��������
        if (recvfrom(clientsocket, (char*)&ReceiveData, sizeof(Datagram), 0, (struct sockaddr*)&routerAddr, &routerAddrLen) == SOCKET_ERROR) {
            continue;
        }
        break;
    }
    cout << "���Ĵλ��ֳɹ�" << endl;
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
    SOCKET clientSocket = socket(AF_INET, SOCK_DGRAM, 0);
    if (clientSocket == INVALID_SOCKET) {
        cerr << "Failed to create socket" << endl;
        WSACleanup();
        return -1;
    }
    // ���ó�ʱѡ��
    if (setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout)) == SOCKET_ERROR) {
        cerr << "setsockopt failed" << endl;
        return -1;
    }
    // ����·�����˵�ַ��Ϣ
    sockaddr_in routerAddr;
    int routerAddrLen = sizeof(routerAddr);
    routerAddr.sin_family = AF_INET;
    routerAddr.sin_port = htons(RouterPort); // ·�����Ķ˿�
    routerAddr.sin_addr.s_addr = inet_addr("127.0.0.1"); // ·������IP��ַ


    clock_t startTime = clock();
    //������������
    while (!active_shakehands(clientSocket, routerAddr, sizeof(routerAddr))) {
        if ((clock() - startTime) / CLOCKS_PER_SEC > MAXTIME) {
            cout << "���Ӳ��ɹ���" << endl;
            // �ر��׽��ֺ�����Winsock
            closesocket(clientSocket);
            WSACleanup();
            return -1;
        }
    }
    cout << "���ӳɹ���" << endl;
 
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
    //��������
    while (!active_wakehands(clientSocket, routerAddr, routerAddrLen)) {
        if ((clock() - startTime) / CLOCKS_PER_SEC > MAXTIME) {
            cout << "���ֲ��ɹ���" << endl;
            break;
        }
    }
    // �ر��׽��ֺ�����Winsock
    closesocket(clientSocket);
    WSACleanup();
    cout << "�ѶϿ�����" << endl;
    system("pause");
    return 0;
}
