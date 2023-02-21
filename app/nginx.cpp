#include <iostream>
#include <memory>
#include "ngx_signal.h"
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
    shared_ptr<int> p = make_shared<int>(100);
    shared_ptr<int> p1 = p;
    cout<<p.use_count()<<endl;
    int *q = p1.get();
    cout<<p.use_count()<<endl;
//    shared_ptr<int> p2(q);
    p1 = nullptr;
    cout<<p.use_count()<<endl;
    return 0;
}