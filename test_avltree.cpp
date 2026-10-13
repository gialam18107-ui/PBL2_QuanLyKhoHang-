#include "avltree/AvlTree.h"
using namespace std;
int main(){
    AvlTree<int, string> test1;
    string default_val = "testing";
    test1.insert(2, default_val);
    test1.insert(3, default_val);
    test1.insert(9, default_val);
    test1.insert(1, default_val);
    test1.insert(12, default_val);
    test1.insert(0, default_val);
    test1.insert(22, default_val);
    test1.insert(3, "haha");
    test1.print();
    string a = test1[4];
    test1.print();
    test1.remove(3);
    test1.remove(1);
    test1.print();
}