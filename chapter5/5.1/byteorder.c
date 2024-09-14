#include <stdio.h>
#include <bits/socket.h>
#include <netinet/in.h>
void byteorder(){
    union 
    {
        short data; /* data */
        char union_bytes[sizeof(short)];
    } test;  
    
    test.data = 0x0102;

    if(test.union_bytes[0] == 1 && test.union_bytes[1] == 2){
        printf("Big Endian\n");
        return;
    }
    
    if(test.union_bytes[0] == 2 && test.union_bytes[1] == 1){
        printf("Little Endian\n");
        return;
    }
    
    printf("Unknown...\n");
}

int main(){
    byteorder();
    return 0;
}