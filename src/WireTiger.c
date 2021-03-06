// by jibancanyang 

#include <time.h>
#include <pcap.h>
#include <pcap/pcap.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include "WireTiger.h"

#define n_short u_short

#define LOG

const char *timestamp_string(struct timeval ts);
void process_packet(u_char *args, const struct pcap_pkthdr *header, const u_char *packet); 
void print_payload(const u_char *payload, int len);
void print_hex_ascii_line(const u_char *payload, int len, int offset);


char * string = NULL;

int main(int argc, char **argv) {
    printf("Welcome to use WireTiger!!\n");
    int d_choosed = 0, f_choosed = 0, e_choosed = 0;
    char * interface = NULL;
    char * fileName = NULL;
    char * BPFExpression = NULL;

    char dashI[] = "-d";
    char dashR[] = "-f";
    char dashS[] = "-e";

    //命令解析
    int counter = 1;
    if (counter < argc && strcmp(dashI, argv[counter]) == 0) {
        d_choosed = 1;
        counter++;
        interface = argv[counter++];
    }
    if (counter < argc && strcmp(dashR, argv[counter]) == 0) {
        f_choosed = 1;
        counter++;
        fileName = argv[counter++];
    }
    if (counter < argc && strcmp(dashS, argv[counter]) == 0) {
        e_choosed = 1;
        counter++;
        string = argv[counter++];
    }
    if (counter < argc)
        BPFExpression = argv[counter];

#ifdef LOG
    puts("Run successfully!");
    printf ("Log::d_choosed = %d, interface = %s, f_choosed = %d, fileName = %s\n",d_choosed, interface, f_choosed, fileName);
    printf ("Log::e_choosed = %d, string = %s, Filter expression = %s\n", e_choosed, string, BPFExpression);
#endif

    char errbuf[PCAP_ERRBUF_SIZE];     
    pcap_t *handle;            

    bpf_u_int32 mask;         
    bpf_u_int32 net;          


    //分情况执行
    if (d_choosed == 1 && f_choosed == 1) {
        fprintf(stderr, "Error::The program either listens to the interface \"%s\" or reads packets from file \"%s\"\n",
                interface, fileName);
        exit(EXIT_FAILURE);
    }
    else if (f_choosed == 1) { //从文件读取分析包 
        handle = pcap_open_offline(fileName, errbuf);
        if (handle == NULL) {
            fprintf(stderr, "Error: Couldn't read the file: %s\n", errbuf);
            exit(EXIT_FAILURE);
        }
    }
    else { 
        if (!(d_choosed == 1)) { //从默认接口读取 
            interface = pcap_lookupdev(errbuf); //返回默认设备
            if (interface == NULL) {
                fprintf(stderr, "Error::Couldn't find default device: %s\n", errbuf);
                exit(EXIT_FAILURE);
            }
        }

        //获取该接口信息
        if (pcap_lookupnet(interface, &net, &mask, errbuf) == -1) {
            fprintf(stderr, "Error::Couldn't get netmask for device %s: %s\n", interface, errbuf);
            net = 0;
            mask = 0;
        }


        //进程句柄
        handle = pcap_open_live(interface, SNAP_LEN, 1, 1000, errbuf);
        if (handle == NULL) {
            fprintf(stderr, "Error::Couldn't open device %s: %s\n", interface, errbuf);
            exit(EXIT_FAILURE);
        }


        //以太网接口
        if (pcap_datalink(handle) != DLT_EN10MB) {
            fprintf(stderr, "Error::%s is not an Ethernet\n", interface);
            exit(EXIT_FAILURE);
        }

    }


    //表达式处理
    struct bpf_program fp;         
    if (BPFExpression != NULL) {
        if (pcap_compile(handle, &fp, BPFExpression, 0, net) == -1) {
            fprintf(stderr, "Error::Couldn't parse filter %s: %s\n", BPFExpression, pcap_geterr(handle));
            exit(EXIT_FAILURE);
        }
        if (pcap_setfilter(handle, &fp) == -1) {
            fprintf(stderr, "Error::Couldn't install filter %s: %s\n", BPFExpression, pcap_geterr(handle));
            exit(EXIT_FAILURE);
        }
    }

    pcap_loop(handle, 0, process_packet, NULL);

    if (BPFExpression != NULL)
        pcap_freecode(&fp);
    pcap_close(handle);

    return 0;
}


//时间信息 
const char *timestamp_string(struct timeval tv){
    char mbuff[64];
    static char buff[64];

    time_t time = (time_t)tv.tv_sec;
    strftime(mbuff, 20, "%Y-%m-%d %H:%M:%S", localtime(&time));
    snprintf(buff, sizeof buff, "%s.%06d", mbuff, (int)tv.tv_usec);
    return buff;
}

//包内数据以16进制显示
void print_hex_ascii_line(const u_char *payload, int len, int offset) {
    int i;
    int gap;
    const u_char *ch;
    ch = payload;
    for(i = 0; i < len; i++) {
        printf("%02X ", *ch);
        ch++;
        if (i == 7)
            printf(" ");
    }
    if (len < 8)
        printf(" ");
    
    if (len < 16) {
        gap = 16 - len;
        for (i = 0; i < gap; i++) {
            printf("   ");
        }
    }
    printf("   ");
    ch = payload;
    for(i = 0; i < len; i++) {
        if (isprint(*ch))
            printf("%c", *ch);
        else
            printf(".");
        ch++;
    }
    printf("\n");
    return;
}


void print_payload(const u_char *payload, int len) {
    int len_rem = len;
    int line_width = 16;            
    int line_len;
    int offset = 0;                
    const u_char *ch = payload;

    if (len <= 0)
        return;

    if (len <= line_width) {
        print_hex_ascii_line(ch, len, offset);
        return;
    }

    for ( ;; ) {
        line_len = line_width % len_rem;
        print_hex_ascii_line(ch, line_len, offset);
        len_rem = len_rem - line_len;
        ch = ch + line_len;
        offset = offset + line_width;
        if (len_rem <= line_width) {
            print_hex_ascii_line(ch, len_rem, offset);
            break;
        }
    }
    return;
}

//数据包信息，回掉函数
void process_packet(u_char *args, const struct pcap_pkthdr *header, const u_char *packet) {
    
    const struct ethernet_header *ethernet;  /* 以太网头 */
    const struct ip_header *ip;              /*  IP  */
    const struct tcp_header *tcp;            /* TCP */
    const char *payload;                     /* Packet payload */

    int size_ip;
    int size_tcp;
    int size_payload;
        
    ethernet = (struct ethernet_header*)(packet);
    ip = (struct ip_header*)(packet + SIZE_ETHERNET);
    size_ip = IP_HL(ip)*4;

    
    switch(ip->ip_p) {
        case IPPROTO_TCP:
            tcp = (struct tcp_header*)(packet + SIZE_ETHERNET + size_ip);
            size_tcp = TH_OFF(tcp)*4;
            if (size_tcp < 20) {
                fprintf(stderr, "Invalid TCP header length: %u bytes\n", size_tcp);
                exit(EXIT_FAILURE);
            }
            payload = (char *)(packet + SIZE_ETHERNET + size_ip + size_tcp);
            size_payload = ntohs(ip->ip_len) - (size_ip + size_tcp);
            if ((string == NULL) || 
                ((string != NULL) && (size_payload > 0) && (strstr(payload, string) != NULL))) {
                printf("\nTime:%s \n", timestamp_string(header->ts));
                printf("Mac road:  ");
                printf("%02X:%02X:%02X:%02X:%02X:%02X",
                (unsigned)ethernet->ether_shost[0],
                (unsigned)ethernet->ether_shost[1],
                (unsigned)ethernet->ether_shost[2],
                (unsigned)ethernet->ether_shost[3],
                (unsigned)ethernet->ether_shost[4],
                (unsigned)ethernet->ether_shost[5]);
                printf(" -> ");
                printf("%02X:%02X:%02X:%02X:%02X:%02X ",
                (unsigned)ethernet->ether_dhost[0],
                (unsigned)ethernet->ether_dhost[1],
                (unsigned)ethernet->ether_dhost[2],
                (unsigned)ethernet->ether_dhost[3],
                (unsigned)ethernet->ether_dhost[4],
                (unsigned)ethernet->ether_dhost[5]);

                printf("\ntype 0x%04x\n ", ntohs(ethernet->ether_type));
                printf("len %d\n", header->len);
                printf("ip road: %s:%d -> %s:%d\n", inet_ntoa(ip->ip_src), ntohs(tcp->th_sport), 
                        inet_ntoa(ip->ip_dst), ntohs(tcp->th_dport));
                puts("");
                print_payload((u_char*)payload, size_payload);
                printf("TCP \n");
            }
            break;
        case IPPROTO_UDP:
            break;

        default:
            // COMMON
            printf("\nTime:%s \n", timestamp_string(header->ts));
            printf("Mac road:  ");
            printf("%02X:%02X:%02X:%02X:%02X:%02X",
                    (unsigned)ethernet->ether_shost[0],
                    (unsigned)ethernet->ether_shost[1],
                    (unsigned)ethernet->ether_shost[2],
                    (unsigned)ethernet->ether_shost[3],
                    (unsigned)ethernet->ether_shost[4],
                    (unsigned)ethernet->ether_shost[5]);

            printf(" -> ");
            printf("%02X:%02X:%02X:%02X:%02X:%02X ",
                    (unsigned)ethernet->ether_dhost[0],
                    (unsigned)ethernet->ether_dhost[1],
                    (unsigned)ethernet->ether_dhost[2],
                    (unsigned)ethernet->ether_dhost[3],
                    (unsigned)ethernet->ether_dhost[4],
                    (unsigned)ethernet->ether_dhost[5]);

            printf("\ntype 0x%04x\n", ntohs(ethernet->ether_type));
            printf("len %d\n", header->len);
            printf("ip road: %s -> %s ", inet_ntoa(ip->ip_src), inet_ntoa(ip->ip_dst));
            puts("");
            print_payload((u_char*)payload, 128);
            printf("OTHER \n");
            break;
    }
    return;
}
