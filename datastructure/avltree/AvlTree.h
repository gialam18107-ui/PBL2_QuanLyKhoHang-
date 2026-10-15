#ifndef AVLTREE_H
#define AVLTREE_H

#include "datastructure/vector/Vector.h"
#include <cstddef>
#include <iostream>
#include <utility>

template<typename K, typename V>
class AvlTree{
public:
    struct Node{
        K key;
        V value;
        int height;
        Node *left;
        Node *right;

        Node(const K& k, const V& v, const int height = 1, Node *left = nullptr, Node *right = nullptr);
        Node(const K& k, V&& v, const int height = 1, Node *left = nullptr, Node *right = nullptr);
        Node(K&& k, const V& v, const int height = 1, Node *left = nullptr, Node *right = nullptr)
    : key(std::move(k)), value(v), height(height), left(left), right(right) {}
        Node(K&& k, V&& v, const int height = 1, Node *left = nullptr, Node *right = nullptr)
    : key(std::move(k)), value(std::move(v)), height(height), left(left), right(right) {}
        Node(const Node& other) : key(other.key), value(other.value), height(1), left(nullptr), right(nullptr){}
        void update_height();
        int get_height();
    };

private:
    Node *root;
    size_t size;
    static int __check_balanced(const Node *x);
    //Rotate right
    static void __ll(Node *&x);
    //Rotate left
    static void __rr(Node *&x);
    //Rotate left + right
    static void __lr(Node *&x);
    //Rotate right + left
    static void __rl(Node *&x);
    static void __balance(Node *&x);
    static void __copy_constructor(const Node *other, Node *&self);
    static void __insert(Node *&curr, const K &k, const V &v, bool &flag);
    static void __insert(Node *&curr, const K &k, V &&v, bool &flag);
    static void __insert(Node *&curr, K &&k, const V &v, bool &flag);
    static void __insert(Node *&curr, K &&k, V &&v, bool &flag);
    static Node* __delete_leftmost(Node *&curr, Node *&backup);
    static Node* __delete_rightmost(Node *&curr, Node *&backup);
    static Node* __find__delete(Node *&curr, const K &key);

public:
    // ========================================================
    // ITERATORS (IN-ORDER TRAVERSAL: MIN -> MAX)
    // ========================================================

    class iterator
    {
    private:
        Vector<Node*> nodeStack;
        void pushLeft(Node* node);

    public:
        iterator();
        explicit iterator(Node* root);

        Node& operator*() const;
        Node* operator->() const;

        iterator& operator++();
        iterator operator++(int);

        bool operator==(const iterator& other) const;
        bool operator!=(const iterator& other) const;
    };

    class const_iterator
    {
    private:
        Vector<const Node*> nodeStack;
        void pushLeft(const Node* node);

    public:
        const_iterator();
        explicit const_iterator(const Node* root);

        const Node& operator*() const;
        const Node* operator->() const;

        const_iterator& operator++();
        const_iterator operator++(int);

        bool operator==(const const_iterator& other) const;
        bool operator!=(const const_iterator& other) const;
    };

    iterator begin();
    iterator end();

    const_iterator begin() const;
    const_iterator end() const;
    const_iterator cbegin() const;
    const_iterator cend() const;

    AvlTree();
    AvlTree(const AvlTree &other);
    AvlTree(AvlTree &&other) noexcept;
    AvlTree& operator=(const AvlTree &other);
    AvlTree& operator=(AvlTree &&other) noexcept;
    V &operator[](const K &k);
    
    ~AvlTree();

    bool insert(const K &k, const V &v);
    bool insert(const K &k, V &&v);
    bool insert(K &&k, const V &v);
    bool insert(K &&k, V &&v);

    V& get(const K& key) const;

    bool contains(const K &k) const;

    bool remove(const K &k);

    void clear();

    size_t getSize() const;
    bool empty() const;
    void print() const;
};

#include "AvlTree.cpp"

#endif