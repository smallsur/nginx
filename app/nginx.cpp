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
#include <vector>
#include "Solution.h"


int main(){

    string s1("ulacfd");
    string s2("jizalu");
    Solution s;
    int i = 2;
    vector<int> v={1,1,1,1,1};
    s.findTargetSumWays(v,3);
    return 0;
}


