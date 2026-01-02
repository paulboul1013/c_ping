#include "ping.h"

int16 checksum(int8 *pkt,int16 size){

    int16 *p;
    int32 acc,b;
    int16 carry,sum,n;
    acc=0;

    for(n=size,p=(int16*)pkt;n;n-=2,p++){
        b=*p;
        acc+=b;
    }
    carry=(acc&0xffff0000)>>16;//acc for deferred carry
    sum=(acc&0x0000ffff);

    return ~(sum+carry); //1's complement of the sum
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

icmp *mkicmp(type kind,const int8 *data,int16 size){
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
    p->size=size;
    p->data=(int8 *)data;

    return p;

}

void showicmp(icmp *pkt){
    if(!pkt){
        return;
    }
    printf("kind:\t %s\nsize:\t %d\npayload:\n",(pkt->kind==echo)?"echo":"echo reply",(int)pkt->size);

    if (pkt->data){
        printhex(pkt->data,pkt->size,0);
        
    }
    printf("\n");

    return;
}

int main(int argc,char **argv){
    int8 *str;
    icmp *pkt;
    int8 *raw;
    int16 size;

    str=(int8 *)malloc(5);
    assert(str);
    zero(str,6);
    strncpy((char *)str,"Hello",5);

    pkt=mkicmp(echo,str,5);
    assert(pkt);
    showicmp(pkt);

    raw=evalicmp(pkt);
    assert(raw);
    size=sizeof(struct s_rawicmp)+pkt->size;

    printhex(raw,size,0);
    free(pkt->data);
    free(pkt);


    return 0;
}