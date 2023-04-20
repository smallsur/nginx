#include <iostream>
#include <vector>

using namespace std;
int main() {
    std::vector<int> v{1, 2, 3,};
    std::vector<int>::iterator it = v.begin();

    std::cout << "Vector elements: ";
    for (; it != v.end(); ++it) {
        std::cout << *it << " ";
    }
    cout<<v.capacity()<<endl;
    std::cout << std::endl;

    v.push_back(6);
    v.push_back(7);
    cout<<v.capacity()<<endl;
//    it = v.begin();
    std::cout << "Vector elements after push back: ";
    for (; it != v.end(); ++it) {
        std::cout << *it << " ";  // 这里访问已经失效的迭代器会导致未定义行为
    }
    std::cout << std::endl;

    return 0;
}
