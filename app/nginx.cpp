#include <iostream>
#include "ngx_signal.h"
using namespace std;

class test{
public:
    const int data_com =20;
    static int data_static;
};

int test::data_static = 10;


int main(){
    int i = 10;
    int &&j = i++;
    int &k = j;
//    int &&u = j;
    k = 100;
    cout<<j<<endl;
    return 0;
}