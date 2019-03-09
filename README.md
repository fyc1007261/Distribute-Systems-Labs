# RDT by Yifan Cai

Student ID: 516030910375

Name: 蔡一凡

Email: fyc1007261@sjtu.edu.cn

### **Note: The program won't stuck, but it may take a relatively long time before the simulator ends. Please wait patiently... Thanks!**

## Packet Design 

A normal packet is 128 bytes in total, which contains
 **checksum(2 bytes) + message sequence(4 bytes) + packet sequence#(4 bytes) + payload size(1 byte) + payload(117 bytes)**. If it is the first packet of a message, the structure may be different. A special part to note the size of the message is located before the payload, whose size is *4 bytes*. Therefore, the structure of the special packet is **checksum(2 bytes) + message sequence(4 bytes) + packet sequence#(4 bytes) + payload size(1 byte) + message size(4 bytes) + payload(113 bytes)**.

 A classic algorithm is used to calculate the checksum. The whole packet (excluding the first two bytes, which are the checksum) is divided into 63 two-byte parts. All the 63 numbers are added and only the last two bytes of the sum are kept to be the checksum.


 ## Protocol
 An `std::map` is used to be the buffer of the packets. The sender will keep the buffer until the packets are sent and the corresponding *ack*s are received. After constructing the packets, the sender inserts all the packets into the buffer. The sender will pass at most `WINDOW_SIZE` packets to the lower layer once at a time and wait for the *ack*s. A timer is also set, and if the sender fails to receive some *ack*s, the corresponding packets will be resent. When the *ack*s of all the sent packets are received, the sender will pass another group of packets to the lower layer.

 When the receiver receives a packet, it will first check will the checksum is correct. It will ignore the packet if not. Next, the receiver sends an *ack* and insert the packet to the buffer. The following struct is used to keep the information of the packets. When the total size of the message (which is indicated in the first packet of the message) is equal to the received size, the packet will be sent to the higher layer, and the `sent` will be set to be true. 

 ```c++
struct message_info_buffer {
    int tot_size;
    int received_size;
    std::map<int, std::string> received_pkts;
    bool sent;
    message_info_buffer(): tot_size(-1), received_size(0), sent(false){
        received_pkts.clear();
    }
};
 ```
 
 The receiver does not care whether the *ack*s are successfully or correctly sent to the sender. If it receives packets with the same message sequence and packet sequence more than once, it will only accept the first one and drop the rest.




