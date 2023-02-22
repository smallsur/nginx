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
    Memory_Pool *s =new Memory_Pool();
    cout<<s<<endl;
    ::clock_t start,end;

    return 0;
}
