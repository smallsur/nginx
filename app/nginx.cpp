#include <iostream>
#include <memory>
#include <ctime>
#include "memory_pool.h"

using namespace std;

class test{
public:
    test(){
        cout<<"create"<<endl;
    };
    ~test(){
        cout<<"free"<<endl;
    }
};


int main(){
    ::clock_t start,end;
    start = ::clock();
    for (int i = 0; i < 10000; ++i) {
        Memory_Pool *s = new Memory_Pool();
    }
    end = ::clock();
    cout<<end-start<<endl;
    return 0;
}
