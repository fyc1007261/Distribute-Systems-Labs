/*
 * FILE: rdt_sender.cc
 * DESCRIPTION: Reliable data transfer sender.
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
/*
 *  new design:
 *  checksum(2 byte) + message_seq#(4 byte) + packet_seq#(4 bytes) + payload_size(1 byte) + contents(117 *  bytes)
 * 
 * 
*/
#define LEN_CHECKSUM 2
#define LEN_MSGSEQ 4
#define LEN_PKTSEQ 4
#define LEN_PLDSIZE 1

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <map>
#include <set>
#include <queue>
#include <string>
#include <pthread.h>
#include <unistd.h>


#include "rdt_struct.h"
#include "rdt_sender.h"

#define WINDOW_SIZE 10
#define TIMEOUT 0.3
#define HEADER_SIZE (LEN_CHECKSUM + LEN_MSGSEQ + LEN_PKTSEQ + LEN_PLDSIZE)
#define PAYLOAD_SIZE (RDT_PKTSIZE - HEADER_SIZE)

#define MAX(a, b) (a > b ? a : b)
#define MIN(a, b) (a < b ? a : b)

static std::map<long, struct pkt_info> send_buffer_map;
static int tot_msg = 0;
static int tot_pkt = 0;


static int nack;
// static pthread_mutex_t mutex;

struct pkt_info
{
    struct packet *pkt;
    bool acked;
    pkt_info() : pkt(NULL), acked(false) {}
};

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
static void checksum(struct packet *pkt)
{
    short sum = 0;
    short *temp = (short *)malloc(sizeof(short));
    for (int i = 2; i < RDT_PKTSIZE; i += 2)
    {
        memcpy(temp, pkt->data + i, sizeof(short));
        sum += *temp;
    }
    memcpy(pkt->data, &sum, LEN_CHECKSUM);
    free(temp);
}

static int waiting;
void flush_outof_queue(){
    if (waiting!=0 || send_buffer_map.size()==0) return;
    int i = 0;
    waiting = 0;
    // printf("begin loop, size=%d\n", send_buffer_map.size());
    for (auto it = send_buffer_map.begin(); it!=send_buffer_map.end() && i<WINDOW_SIZE; i++, it++){
        // printf("size=%d\n", send_buffer_map.size());
        tot_pkt++;
        waiting++;
        Sender_ToLowerLayer(it->second.pkt);
    }
    // printf("end loop\n");
    Sender_StartTimer(TIMEOUT);
}

void Sender_Init()
{
    // pthread_mutex_init(&mutex, NULL);
    waiting = 0;
    nack = 0;
    fprintf(stdout, "At %.2fs: sender initializing ...\n", GetSimulationTime());
}

void Sender_Final()
{
    for (int i=0; i<WINDOW_SIZE; i++){
        waiting = 0;
        // printf("remaining:%d\n", send_buffer_map.size());
        flush_outof_queue();
    }

    for (std::map<long, struct pkt_info>::iterator it = send_buffer_map.begin(); it != send_buffer_map.end(); it++)
    {
        free(it->second.pkt);
    }


    fprintf(stdout, "At %.2fs: sender finalizing ...\n", GetSimulationTime());
    printf("Total pkts:%d\n", tot_pkt);
    printf("Total acks:%d\n", nack);
    printf("Total messages:%d\n", tot_msg);
}

/* event handler, called when a message is passed from the upper layer at the 
   sender */
static unsigned int msg_seq = 0;

void Sender_FromUpperLayer(struct message *msg)
{
    // 4 bytes for the message size in the first packet.
    int pkt_needed = ((msg->size + 4) % PAYLOAD_SIZE == 0) ? (int)((msg->size + 4) / PAYLOAD_SIZE) : (int)((msg->size + 4) / PAYLOAD_SIZE) + 1;
    tot_msg++;

    int rest_size = msg->size;
    int cursor = 0;

    // printf("message:%s\n", msg->data);
    fflush(stdout);

    for (int i = 0; i < pkt_needed; ++i)
    {
        struct packet *pkt = (struct packet *)malloc(sizeof(struct packet));
        memset(pkt->data, 0, RDT_PKTSIZE);
        int pkt_seq = i;
        if (i == 0)
        {
            unsigned char pldsize = rest_size > (PAYLOAD_SIZE - 4) ? (PAYLOAD_SIZE - 4) : rest_size;
            memcpy(pkt->data + LEN_CHECKSUM, &msg_seq, LEN_MSGSEQ);
            memcpy(pkt->data + LEN_CHECKSUM + LEN_MSGSEQ, &pkt_seq, LEN_PKTSEQ);
            memcpy(pkt->data + LEN_CHECKSUM + LEN_MSGSEQ + LEN_PKTSEQ, &pldsize, LEN_PLDSIZE);
            // additional message size
            memcpy(pkt->data + LEN_CHECKSUM + LEN_MSGSEQ + LEN_PKTSEQ + LEN_PLDSIZE, &(msg->size), sizeof(int));
            memcpy(pkt->data + LEN_CHECKSUM + LEN_MSGSEQ + LEN_PKTSEQ + LEN_PLDSIZE + sizeof(int), msg->data + cursor, pldsize);

            checksum(pkt);

            rest_size -= pldsize;
            cursor += pldsize;

            long index = (msg_seq << 16) + pkt_seq;
            struct pkt_info info = pkt_info();
            info.pkt = pkt;
            send_buffer_map[index] = info;
        }
        else
        {
            unsigned char pldsize = MIN(PAYLOAD_SIZE, rest_size);
            memcpy(pkt->data + LEN_CHECKSUM, &msg_seq, LEN_MSGSEQ);
            memcpy(pkt->data + LEN_CHECKSUM + LEN_MSGSEQ, &pkt_seq, LEN_PKTSEQ);
            memcpy(pkt->data + LEN_CHECKSUM + LEN_MSGSEQ + LEN_PKTSEQ, &pldsize, LEN_PLDSIZE);
            memcpy(pkt->data + LEN_CHECKSUM + LEN_MSGSEQ + LEN_PKTSEQ + LEN_PLDSIZE, msg->data + cursor, pldsize);

            checksum(pkt);

            rest_size -= pldsize;
            cursor += pldsize;

            long index = (msg_seq << 16) + pkt_seq;
            struct pkt_info info = pkt_info();
            info.pkt = pkt;
            send_buffer_map[index] = info;
        }
        // printf("sendpkt:%s\n", pkt->data+12);
        // buff_queue.push(pkt);
    }
    flush_outof_queue();
    
    ++msg_seq;

    
}

/* event handler, called when a packet is passed from the lower layer at the 
   sender */
void Sender_FromLowerLayer(struct packet *pkt)
{
    // printf("sender from lower!\n");
    nack++;
    if (!check_checksum(pkt))
    {
        return;
    }
    unsigned int msg_seq;
    int pkt_seq;
    memcpy(&msg_seq, pkt->data+LEN_CHECKSUM, LEN_MSGSEQ);
    memcpy(&pkt_seq, pkt->data+LEN_CHECKSUM+LEN_MSGSEQ, LEN_PKTSEQ);
    long index = (msg_seq << 16) + pkt_seq;
    // send_buffer_map[index].acked = true;
    send_buffer_map.erase(index);
    waiting--;
    if (waiting==0) {
        Sender_StopTimer();
        flush_outof_queue();
    }
}

/* event handler, called when the timer expires */
void Sender_Timeout()
{
    // printf("time out!\n");
    waiting = 0;
    flush_outof_queue();
}
