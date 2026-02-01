#include "ping.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <signal.h>

sint32 initsocket(void){
    sint32 sockfd;
    sint32 on=1;
    struct timeval tv;

    // 創建原始 ICMP socket
    sockfd=socket(AF_INET,SOCK_RAW,IPPROTO_ICMP);
    if(sockfd<0){
        if(errno==EPERM){
            fprintf(stderr,"socket: Operation not permitted. Need root privileges or CAP_NET_RAW capability.\n");
        }else{
            perror("socket");
        }
        return -1;
    }

    // 設置 IP_HDRINCL 選項，因為我們自己構建 IP 頭
    // 注意：在某些環境（如 WSL2）中，這可能導致發送失敗
    if(setsockopt(sockfd,IPPROTO_IP,IP_HDRINCL,&on,sizeof(on))<0){
        perror("setsockopt IP_HDRINCL");
        close(sockfd);
        return -1;
    }

    // 設置 SO_BROADCAST 允許發送廣播
    on=1;
    if(setsockopt(sockfd,SOL_SOCKET,SO_BROADCAST,&on,sizeof(on))<0){
        perror("setsockopt SO_BROADCAST");
        // 非致命錯誤，繼續
    }

    // 設置接收超時 2 秒
    tv.tv_sec=2;
    tv.tv_usec=0;
    if(setsockopt(sockfd,SOL_SOCKET,SO_RCVTIMEO,&tv,sizeof(tv))<0){
        perror("setsockopt SO_RCVTIMEO");
        close(sockfd);
        return -1;
    }

    return sockfd;
}

bool sendip(sint32 s,ip *pkt){
    int8 *raw;
    sint32 n;
    int16 pktsize;
    struct sockaddr_in dst;

    if (s<0 || !pkt){
        return false;
    }

    // 將應用層結構轉換為原始封包
    raw=evalip(pkt);
    if(!raw){
        return false;
    }

    // 計算封包大小
    pktsize=sizeof(struct s_rawip);
    if(pkt->payload){
        pktsize+=sizeof(struct s_rawicmp)+pkt->payload->size;
    }

    // 設置目標地址
    zero((int8*)&dst,sizeof(dst));
    dst.sin_family=AF_INET;
    dst.sin_addr.s_addr=pkt->dst;

    // 發送封包
    n=sendto(s,raw,pktsize,0,(struct sockaddr*)&dst,sizeof(dst));

    free(raw);

    if(n<0){
        perror("sendto");
        return false;
    }

    return true;
}

ip *mkip(type kind, const int8 *src,const int8 *dst,int16 id_,int16 *cntptr){
    int16 id;
    int16 size;
    ip *pkt;
    if (!kind || !src || !dst ){
        return (ip*)0;
    }

    if (id_) {
        id=id_;
    }
    else{
        id=*cntptr++;
    }


    size=sizeof(struct s_ip);
    pkt=(ip*)malloc(size);
    assert(pkt);
    zero((int8*)pkt,size);

    pkt->kind=kind;
    pkt->id=id;
    pkt->src=inet_addr((char*)src);
    pkt->dst=inet_addr((char*)dst);
    pkt->payload=(icmp*)0;

    if (!pkt->dst){
        free(pkt);
        return (ip*)0;
    }
    return pkt;
}
    
void showip(char *ident,ip *pkt){
    if (!pkt){
        return;
    }

    printf("(ip *)%s = {\n", ident);
    printf("  kind:\t 0x%.02hhx\n",(char)pkt->kind);
    printf("  id:\t 0x%.02hhx\n",pkt->id);
    printf("  src:\t %s\n",todotted(pkt->src));
    printf("  dst:\t %s\n",todotted(pkt->dst));
    printf("}\n");

    if (pkt->payload){
        show(pkt->payload);
    }

}

int8 *evalip(ip *pkt){
    struct s_rawip rawpkt;
    struct s_rawip *rawptr;
    int16 check;
    int16 size;
    int8 *p,*ret;
    int8 protocol;
    int16 lengthle;
    int8 *icmpptr;

    if (!pkt){
        return (int8 *)0;
    }

    protocol=0;
    switch(pkt->kind){
        case L4icmp:
        protocol=1;
        break;
        case L4tcp:
        protocol=6;
        break;
        case L4udp:
        protocol=17;
        break;
        default:
        return (int8 *)0;
    }    

    rawpkt.checksum=0;
    rawpkt.dscp=0;
    rawpkt.dst=pkt->dst;
    rawpkt.ecn=0;
    rawpkt.flag=0;
    rawpkt.id=endian16(pkt->id);
    rawpkt.ihl=(sizeof(struct s_rawip)/4);

    lengthle=0;
    if (pkt->payload){
        lengthle=(rawpkt.ihl*4)+pkt->payload->size+sizeof(struct s_rawicmp);
        rawpkt.length=endian16(lengthle);
    }
    else{
        lengthle=(rawpkt.ihl*4);
        rawpkt.length=endian16(lengthle);
    }

    rawpkt.offset=0;
    rawpkt.protocol=protocol;
    rawpkt.src=pkt->src;
    rawpkt.ttl=250;
    rawpkt.version=4;

    if (lengthle%2){
        lengthle++;
    }

    size=sizeof(struct s_rawip);
    p=malloc(lengthle);
    ret=p;
    assert(p);
    zero(p,lengthle);
    copy(p,(int8*)&rawpkt,size);
    p+=size;

    if (pkt->payload){
        icmpptr=evalicmp(pkt->payload);
        if (icmpptr){
            int16 icmpsize=sizeof(struct s_rawicmp)+pkt->payload->size;
            copy(p,icmpptr,icmpsize);
            free(icmpptr);
        }
    }

    // IP 校驗和只計算 IP 頭部分，不包括 payload
    check=checksum(ret,sizeof(struct s_rawip));
    rawptr=(struct s_rawip *)ret;
    rawptr->checksum=check;

    return ret;

}

//0xaabb -> 0xbbaa
int16 endian16(int16 x){
    int a,b;
    int16 y;
    b=(x&0x00ff);
    a=((x&0xff00)>>8);
    y=(b<<8)|a;
    return y;
}

int16 checksum(int8 *pkt,int16 size){

    int16 *p;
    int32 acc,b;
    int16 carry,sum,n;
    int16 ret;
    acc=0;

    for(n=size,p=(int16*)pkt;n;n-=2,p++){
        b=*p;
        acc+=b;
    }
    carry=(acc&0xffff0000)>>16;//acc for deferred carry
    sum=(acc&0x0000ffff);

    ret=~(sum+carry); //1's complement of the sum
    
    return endian16(ret);
}

int8 *evalicmp(icmp *pkt){
    int8 *p,*ret;
    int16 size;
    struct s_rawicmp rawpkt;
    struct s_rawicmp *rawptr;
    int16 check;

    if (!pkt || !pkt->data) {
        return (int8 *)0;
    }

    switch(pkt->kind){
        case echo:
        rawpkt.type=8;
        rawpkt.code=0;
            break;
        case echoreply:
        rawpkt.type=0;
        rawpkt.code=0;
            break;
        default:
            return (int8 *)0;
            break;
    }

    rawpkt.checksum=0;
    rawpkt.identifier=endian16(pkt->identifier);
    rawpkt.sequence=endian16(pkt->sequence);
    size=sizeof(struct s_rawicmp)+pkt->size;
    if (size%2){
        size++;
    }

    //allocate memory for raw icmp packet
    p= (int8 *)malloc(size);
    ret=p;
    assert(p);
    zero((int8 *)p,size);

    //copy metadata to raw icmp packet
    copy(p,(int8 *)&rawpkt,sizeof(struct s_rawicmp));
    p+=sizeof(struct s_rawicmp);
    //copy data to raw icmp packet
    copy(p,pkt->data,pkt->size);

    //calculate checksum
    check=checksum(ret,size);
    
    //set checksum to raw icmp packet
    rawptr=(struct s_rawicmp *)ret;
    rawptr->checksum=check;

    return ret;
}

icmp *mkicmp(type kind,const int8 *data,int16 size,int16 identifier,int16 sequence){
    int16 n;
    icmp *p;

    if (!data || !size) {
        return (icmp*)0;
    }
    n=sizeof(struct s_icmp)+size;

    p=(icmp*)malloc((int)n);
    assert(p);

    zero((int8 *)p,n);

    p->kind=kind;
    p->identifier=identifier;
    p->sequence=sequence;
    p->size=size;
    p->data=(int8 *)data;

    return p;

}

void showicmp(char *ident,icmp *pkt){
    if(!pkt){
        return;
    }
    printf("(icmp *)%s = {\n",ident);
    printf("  kind:\t %s\n",(pkt->kind==echo)?"echo":"echo reply");
    printf("  size:\t %d\n",(int)pkt->size);


    printf("}\n");

    printf("payload:\n");
    if (pkt->data){
        printhex(pkt->data,pkt->size,0);

    }

    return;
}

icmp *recvicmp(sint32 sockfd,sint32 *ttl_out){
    int8 buf[2048];
    struct sockaddr_in src;
    socklen_t srclen;
    sint32 n;
    struct s_rawip *ipheader;
    struct s_rawicmp *icmpheader;
    int16 iphdrlen;
    icmp *pkt;
    int8 *payload;
    int16 payloadsize;

    if(!ttl_out){
        return (icmp*)0;
    }

    zero(buf,sizeof(buf));
    srclen=sizeof(src);

    // 接收封包
    n=recvfrom(sockfd,buf,sizeof(buf),0,(struct sockaddr*)&src,&srclen);
    if(n<0){
        if(errno==EAGAIN || errno==EWOULDBLOCK){
            // 超時
            return (icmp*)0;
        }
        perror("recvfrom");
        return (icmp*)0;
    }

    // 解析 IP 頭
    ipheader=(struct s_rawip*)buf;
    iphdrlen=(ipheader->ihl)*4;
    *ttl_out=(sint32)ipheader->ttl;

    // 解析 ICMP 頭
    icmpheader=(struct s_rawicmp*)(buf+iphdrlen);

    // 只處理 echo reply (type=0)
    if(icmpheader->type!=0){
        return (icmp*)0;
    }

    // 提取 payload
    payloadsize=n-iphdrlen-sizeof(struct s_rawicmp);
    if(payloadsize<=0){
        return (icmp*)0;
    }

    payload=(int8*)malloc(payloadsize);
    assert(payload);
    copy(payload,(int8*)(buf+iphdrlen+sizeof(struct s_rawicmp)),payloadsize);

    // 創建應用層 icmp 結構
    pkt=(icmp*)malloc(sizeof(struct s_icmp));
    assert(pkt);
    zero((int8*)pkt,sizeof(struct s_icmp));

    pkt->kind=echoreply;
    pkt->identifier=endian16(icmpheader->identifier);
    pkt->sequence=endian16(icmpheader->sequence);
    pkt->size=payloadsize;
    pkt->data=payload;

    return pkt;
}

// 統計結構
struct ping_stats {
    sint32 transmitted;
    sint32 received;
    double min_rtt;
    double max_rtt;
    double sum_rtt;
};

void init_stats(struct ping_stats *stats){
    stats->transmitted=0;
    stats->received=0;
    stats->min_rtt=999999.0;
    stats->max_rtt=0.0;
    stats->sum_rtt=0.0;
}

void update_stats(struct ping_stats *stats,double rtt){
    stats->received++;
    stats->sum_rtt+=rtt;
    if(rtt<stats->min_rtt){
        stats->min_rtt=rtt;
    }
    if(rtt>stats->max_rtt){
        stats->max_rtt=rtt;
    }
}

void print_stats(struct ping_stats *stats,const int8 *target){
    double loss;
    double avg;

    printf("\n--- %s ping statistics ---\n",target);
    printf("%d packets transmitted, %d received, ",stats->transmitted,stats->received);

    if(stats->transmitted>0){
        loss=(double)(stats->transmitted-stats->received)*100.0/(double)stats->transmitted;
        printf("%.1f%% packet loss\n",loss);
    }else{
        printf("0.0%% packet loss\n");
    }

    if(stats->received>0){
        avg=stats->sum_rtt/(double)stats->received;
        printf("rtt min/avg/max = %.3f/%.3f/%.3f ms\n",
            stats->min_rtt,avg,stats->max_rtt);
    }
}

// Signal handler
volatile sig_atomic_t keep_running=1;

void sigint_handler(int signum){
    (void)signum;
    keep_running=0;
}

int main(int argc,char **argv){
    sint32 sockfd;
    int8 *target;
    int16 identifier;
    int16 sequence;
    int8 *payload;
    int16 payloadsize;
    icmp *icmppkt;
    ip *ippkt;
    struct ping_stats stats;
    timestamp send_time,recv_time;
    icmp *reply;
    sint32 ttl;
    double rtt;
    int16 ipcnt;
    int8 src_ip[16];

    // 檢查參數
    if(argc<2){
        fprintf(stderr,"Usage: %s <target_ip>\n",argv[0]);
        return 1;
    }

    target=(int8*)argv[1];

    // 初始化 socket
    sockfd=initsocket();
    if(sockfd<0){
        fprintf(stderr,"Failed to create socket. Need root privileges or CAP_NET_RAW capability.\n");
        return 1;
    }

    if(sockfd==0){
        fprintf(stderr,"Invalid socket fd.\n");
        return 1;
    }

    // 設置 signal handler
    signal(SIGINT,sigint_handler);

    // 初始化統計
    init_stats(&stats);

    // 設置識別符（使用 process ID）
    identifier=(int16)getpid();
    sequence=0;
    ipcnt=0;

    // 獲取本機 IP（簡化版，使用固定值）
    snprintf((char*)src_ip,sizeof(src_ip),"127.0.0.1");

    printf("PING %s 56 data bytes\n",target);

    // 主循環
    while(keep_running){
        sequence++;

        // 創建 payload（包含時間戳）
        payloadsize=56;
        payload=(int8*)malloc(payloadsize);
        assert(payload);
        zero(payload,payloadsize);

        // 記錄發送時間到 payload
        getnow(&send_time);
        copy(payload,(int8*)&send_time,sizeof(timestamp));

        // 創建 ICMP echo request
        icmppkt=mkicmp(echo,payload,payloadsize,identifier,sequence);
        assert(icmppkt);

        // 創建 IP 封包
        ippkt=mkip(L4icmp,src_ip,target,0,&ipcnt);
        if(!ippkt){
            fprintf(stderr,"Invalid target IP address\n");
            free(payload);
            free(icmppkt);
            break;
        }
        ippkt->payload=icmppkt;

        // 發送封包
        if(!sendip(sockfd,ippkt)){
            fprintf(stderr,"Failed to send packet\n");
            free(payload);
            free(icmppkt);
            free(ippkt);
            break;
        }
        stats.transmitted++;

        // 接收 reply
        reply=recvicmp(sockfd,&ttl);
        getnow(&recv_time);

        if(reply && reply->identifier==identifier && reply->sequence==sequence){
            // 從 reply payload 提取發送時間
            if(reply->size>=sizeof(timestamp)){
                copy((int8*)&send_time,reply->data,sizeof(timestamp));
                rtt=difftime_ms(&send_time,&recv_time);

                printf("64 bytes from %s: icmp_seq=%d ttl=%d time=%.3f ms\n",
                    target,sequence,ttl,rtt);

                update_stats(&stats,rtt);
            }

            free(reply->data);
            free(reply);
        }else{
            printf("Request timeout for icmp_seq %d\n",sequence);
        }

        // 清理
        free(payload);
        free(icmppkt);
        free(ippkt);

        // 等待 1 秒
        sleep(1);
    }

    // 顯示統計
    print_stats(&stats,target);

    // 關閉 socket
    close(sockfd);

    return 0;
}