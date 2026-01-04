
#ifndef LOGTO_H
#define LOGTO_H

class log {
public:
    static bool logToSerial;
    log() {}
    static void toAll(String s);
    static const int ASIZE = 20;
    static String commandList[ASIZE];
};
#endif