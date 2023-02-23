#include <iostream>
#include <memory>
#include <ctime>
#include "memory_pool.h"
#include <array>
#include <functional>
#include <map>
#include <boost/type_index.hpp>

using namespace std;

class test{
public:
    test(){
        cout<<"create"<<endl;
    };
    test(const test& t){
        std::cout<<"copy"<<std::endl;
    }
    int operator()(int a){
        return a;
    }
    int operator()(int a,int b){
        return a+b;
    }
    ~test(){
        cout<<"free"<<endl;
    }
};

int test1(int a){
    return a;
}

template <typename T> void inference(const T& tmp){
    using boost::typeindex::type_id_with_cvr;
    std::cout<<type_id_with_cvr<T>().pretty_name()<<std::endl;
    std::cout<<type_id_with_cvr<decltype(tmp)>().pretty_name()<<std::endl;
}


template <typename F, typename T1, typename T2>
void forward_method(F f,T1 t1,T2 t2){

    f(t1,t2);
}

void fuc1(int v1, int v2){

    std::cout<<v1+v2<<std::endl;
}

int main(){
    int i = 10;
    const int j = i;
    const int &k = i;
    inference(i);
    inference(j);
    inference(k);
    return 0;
}


