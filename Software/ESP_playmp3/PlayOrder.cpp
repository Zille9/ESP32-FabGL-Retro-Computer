#include <Arduino.h>
#include "PlayOrder.h"

void PlayOrder::addToList(String v){
    num++;
    if(num>maxNum)maxNum=num;
    list[num]=v;
}
String PlayOrder::prev(){
    if(num>0)
        num--;
    String ret = list[num];
    return ret;
}
String PlayOrder::next(){
    if(num<maxNum){
        num++;
        return list[num];
    }
    return "";
}
