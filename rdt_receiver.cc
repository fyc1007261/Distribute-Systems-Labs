/*
 * FILE: rdt_receiver.cc
 * DESCRIPTION: Reliable data transfer receiver.
 * NOTE: This implementation assumes there is no packet loss, corruption, or 
 *       reordering.  You will need to enhance it to deal with all these 
 *       situations.  In this implementation, the packet format is laid out as 
 *       the following:
 *       
 *       |<-  1 byte  ->|<-             the rest            ->|
 *       | payload size |<-             payload             ->|
 *
 *       The first byte of each packet indicates the size of the payload
 *       (excluding this single-byte header)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <map>
#include <set>
#include <string>

#include "rdt_struct.h"
#include "rdt_receiver.h"


#define LEN_CHECKSUM 2
#define LEN_MSGSEQ 4
#define LEN_PKTSEQ 4
#define LEN_PLDSIZE 1
#define WINDOW_SIZE 10
#define TIMEOUT 0.3
#define HEADER_SIZE (LEN_CHECKSUM + LEN_MSGSEQ + LEN_PKTSEQ + LEN_PLDSIZE)
#define PAYLOAD_SIZE (RDT_PKTSIZE - HEADER_SIZE)
#define MAX(a, b) (a>b?a:b)
#define MIN(a, b) (a<b?a:b)

struct message_info_buffer {
    int tot_size;
    int received_size;
    std::map<int, std::string> received_pkts;
    bool sent;
    message_info_buffer(): tot_size(-1), received_size(0), sent(false){
        received_pkts.clear();
    }
};

static std::map<unsigned int, struct message_info_buffer> messages_buffer;
static std::set<struct packet*> forfree;

static int tot_message = 0;

static bool check_checksum(struct packet *pkt){
    short sum = 0;
    short got;
    memcpy(&got, pkt->data, sizeof(short));
    short *temp = (short*)malloc(sizeof(short));
    for (int i=2; i<RDT_PKTSIZE; i+=2){
        memcpy(temp, pkt->data + i, sizeof(short));
        sum += *temp;
    }
    free(temp);
    return sum == got;
}

static void checksum(struct packet *pkt){
    short sum = 0;
    short *temp = (short*)malloc(sizeof(short));
    for (int i=2; i<RDT_PKTSIZE; i+=2){
        memcpy(temp, pkt->data + i, sizeof(short));
        sum += *temp;
    }
    memcpy(pkt->data, &sum, LEN_CHECKSUM);
    free(temp);
}

void Receiver_Init()
{
    fprintf(stdout, "At %.2fs: receiver initializing ...\n", GetSimulationTime());
    messages_buffer.clear();
}

void Receiver_Final()
{
    fprintf(stdout, "At %.2fs: receiver finalizing ...\n", GetSimulationTime());
    fprintf(stdout, "Total received messages:%d\n", tot_message);
    for (std::set<struct packet*>::iterator it = forfree.begin(); it!=forfree.end(); it++){
        free(*it);
    }
}

static void send_ack(unsigned int msg_seq, int pkt_seq){
    struct packet *pkt = (struct packet *)malloc(sizeof(struct packet));
    memset(pkt->data, 0, RDT_PKTSIZE);
    memcpy(pkt->data+LEN_CHECKSUM, &msg_seq, LEN_MSGSEQ);
    memcpy(pkt->data + LEN_CHECKSUM + LEN_MSGSEQ, &pkt_seq, LEN_PKTSEQ);
    checksum(pkt);
    Receiver_ToLowerLayer(pkt);
    forfree.insert(pkt);
}



static bool message_to_higher(unsigned int msg_seq){
    // finished, then to the next one;
    if (messages_buffer.find(msg_seq) == messages_buffer.end())
        return false;
    if (messages_buffer[msg_seq].sent) return false;
    for (unsigned int i=0; i<=tot_message+1; i++){
        if (i<msg_seq && messages_buffer[i].sent==false){
            return false;
        }
    }
    std::string mess("");
    if (messages_buffer[msg_seq].tot_size != messages_buffer[msg_seq].received_size){
        return false;
    }
    for (unsigned int i=0; i<messages_buffer[msg_seq].received_pkts.size(); ++i){
        mess += messages_buffer[msg_seq].received_pkts[i];
    }
    struct message *msg = (struct message *)malloc(sizeof(struct message));
    msg->size = messages_buffer[msg_seq].tot_size;
    msg->data = (char*)malloc(mess.size());
    strncpy(msg->data, mess.c_str(),mess.size());
    Receiver_ToUpperLayer(msg);
    // printf("received:%s\n", msg->data);
    fflush(stdout);
    // messages_buffer.erase(msg_seq);
    messages_buffer[msg_seq].sent = true;
    tot_message ++;
    return true;
}

/* event handler, called when a packet is passed from the lower layer at the 
   receiver */
void Receiver_FromLowerLayer(struct packet *pkt)
{
    // printf("received a packet\n");
    if (!check_checksum(pkt)){
        // printf("checksum fails.\n");
        return;
    }
    unsigned int msg_seq;
    unsigned char payload_size;
    int pkt_seq;
    char payload[PAYLOAD_SIZE];
    memset(payload, 0, PAYLOAD_SIZE);

    memcpy(&msg_seq, pkt->data+LEN_CHECKSUM, LEN_MSGSEQ);
    memcpy(&pkt_seq, pkt->data+LEN_CHECKSUM+LEN_MSGSEQ, LEN_PKTSEQ);
    memcpy(&payload_size, pkt->data+LEN_CHECKSUM+LEN_MSGSEQ+LEN_PKTSEQ, LEN_PLDSIZE);

    send_ack(msg_seq, pkt_seq);

    if (messages_buffer.find(msg_seq) != messages_buffer.end() && messages_buffer[msg_seq].received_pkts.find(pkt_seq) != messages_buffer[msg_seq].received_pkts.end())
        return;

    if (messages_buffer.find(msg_seq) == messages_buffer.end()){
        // add new message 
        messages_buffer.insert(std::make_pair(msg_seq, message_info_buffer()));
    }

    if (pkt_seq == 0){
        // different packet structure
        int msg_size;
        memcpy(&msg_size, pkt->data+LEN_CHECKSUM+LEN_MSGSEQ+LEN_PKTSEQ+LEN_PLDSIZE, 4);
        memcpy(payload, pkt->data+LEN_CHECKSUM+LEN_MSGSEQ+LEN_PKTSEQ+LEN_PLDSIZE+4, payload_size);
        std::string payload_str(payload);
        messages_buffer[msg_seq].tot_size = msg_size;
        messages_buffer[msg_seq].received_size += payload_size;
        messages_buffer[msg_seq].received_pkts.insert(std::make_pair(pkt_seq,payload_str));
    }
    else{
        memcpy(payload, pkt->data+LEN_CHECKSUM+LEN_MSGSEQ+LEN_PKTSEQ+LEN_PLDSIZE, payload_size);
        std::string payload_str(payload);
        messages_buffer[msg_seq].received_size += payload_size;
        messages_buffer[msg_seq].received_pkts.insert(std::make_pair(pkt_seq,payload_str));
    }

  
    if (message_to_higher(msg_seq)){
        for (unsigned int i = msg_seq; i<=tot_message+1; i++){
            if (!message_to_higher(i));
        }
    }
  
}
