#include "./include/onair.h"

#include <getopt.h>

#define SENDER 11
#define RECEIVER 12

int main(int argc, char ** argv) {
    string line;
    int mode;

    Uplink::Sender sender(19000, 18800);
    Downlink::Receiver receiver(19000, 18800);

    int option_val = 0;
    option_val = getopt(argc, argv, "m:h");
    switch(option_val) {
        case 'm': {
            if(optarg == NULL) puts("no mode information given");
            if(strcmp(optarg, "sender")==0) mode=SENDER;
            else if(strcmp(optarg, "receiver")==0) mode=RECEIVER;
            else {
                puts("allowed modes are receiver/sender");
                exit(0);
            }
        };break;
        case 'h': {
            puts("-m sender\tstart in receiver mode\n-m receiver\tstart in sender mode\n-h\t\tfor this help message\n");
            exit(0);
        }break;
        default: {
            puts("-m sender\tstart in receiver mode\n-m receiver\tstart in sender mode\n-h\t\tfor this help message\n");
            exit(0);
        }
    }


    while(1) {
        if(mode == RECEIVER) {
            receiver.start();
            if(receiver.isBreak())
            mode = SENDER;
            continue;
        }
        
        line = sender.getInput();
        
        sender.puts(line);

        if(line.length() == 0){
            mode = RECEIVER;
        }
    }
    return 0;
}