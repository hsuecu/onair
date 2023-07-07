#include "./include/onair.h"

#define SENDER 11
#define RECEIVER 12

int main(int argc, const char ** argv) {
    Uplink::Sender sender;
    int sender_previous_line_len = 255;
    string line;

    Downlink::Receiver receiver(19000, 18800, 384000);

    int mode;
    if(argc > 1) mode = SENDER;
    else mode = RECEIVER;

    while(1) {
        if(mode == RECEIVER) {
            receiver.start();
            cout << endl;
            cout.flush();
            if(receiver.isBreak())
            mode = SENDER;
            continue;
        }
        usleep(500000);
        cout << "<<< ";
        cout.flush();
        getline(cin, line, '\n');
        sender.sync(sender_previous_line_len);
        sender.puts(line);
        sender.dsync();
        sender_previous_line_len = line.length();

        sender.sleep_estimator();

        if(line.length() == 0){
            mode = RECEIVER;
            cout << endl;
        }
    }
    return 0;
}