#ifndef PLAY_ORDER
#define PLAY_ORDER
#include "Arduino.h"

class PlayOrder{
  public:
    String list[100];
    int num=1;
    int maxNum=1;
    void addToList(String v);
    String prev();
    String next();
    void showOrderSerial();
};

#endif
