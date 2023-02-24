//#include <iostream>
//#include <boost/type_index.hpp>
//
//template <typename T> void inference(const T& tmp){
//    using boost::typeindex::type_id_with_cvr;
//    std::cout<<type_id_with_cvr<T>().pretty_name()<<std::endl;
//    std::cout<<type_id_with_cvr<decltype(tmp)>().pretty_name()<<std::endl;
//}
#include <iostream>
#include <memory>
#include "Singleton.h"
int main(){
    std::shared_ptr<Singleton> singleton= Singleton::getInstance();
    std::shared_ptr<Singleton> singleton1 = Singleton::getInstance();
    std::cout<<(singleton==singleton1)<<std::endl;
    std::cout<<singleton.use_count()<<std::endl;
    return 0;
}


