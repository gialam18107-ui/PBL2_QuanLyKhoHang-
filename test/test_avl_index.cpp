#include "database/Index.h"
#include <iostream>
#include <cassert>

int main()
{
    AVLTreeIndex avl("idx_price", "price");
    avl.insert(Value(100), 10);
    avl.insert(Value(50), 5);
    avl.insert(Value(100), 11);
    avl.insert(Value(200), 20);
    avl.insert(Value(150), 15);
    avl.insert(Value(100), 12);
    avl.insert(Value(25), 2);

    // Test find (sử dụng lower_bound, không duyệt toàn cây)
    auto f100 = avl.find(Value(100));
    std::cout << "f100 size: " << f100.size() << std::endl;
    assert(f100.size() == 3);
    assert(f100[0] == 10 && f100[1] == 11 && f100[2] == 12);

    auto fNotFound = avl.find(Value(99));
    std::cout << "fNotFound size: " << fNotFound.size() << std::endl;
    assert(fNotFound.size() == 0);

    // Test findRange (sử dụng lower_bound và dừng sớm khi vượt maxVal)
    auto range = avl.findRange(Value(50), Value(150));
    std::cout << "range [50, 150] size: " << range.size() << std::endl;
    // Kỳ vọng: 50 (row 5), 100 (rows 10, 11, 12), 150 (row 15) -> tổng cộng 5 rows
    assert(range.size() == 5);
    assert(range[0] == 5);
    assert(range[1] == 10);
    assert(range[2] == 11);
    assert(range[3] == 12);
    assert(range[4] == 15);

    auto rangeEmpty = avl.findRange(Value(101), Value(149));
    std::cout << "rangeEmpty size: " << rangeEmpty.size() << std::endl;
    assert(rangeEmpty.size() == 0);

    // Test remove
    avl.remove(Value(100), 11);
    auto f100After = avl.find(Value(100));
    assert(f100After.size() == 2);
    assert(f100After[0] == 10 && f100After[1] == 12);

    std::cout << "ALL AVLTreeIndex TESTS PASSED!" << std::endl;
    return 0;
}
