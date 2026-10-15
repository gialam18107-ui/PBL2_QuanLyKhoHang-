#ifndef AVLTREE_CPP
#define AVLTREE_CPP
#include "AvlTree.h"

#include "datastructure/vector/Vector.h"
#include <stdexcept>
#include <utility>

// ========================================================
// NODE CONSTRUCTORS
// ========================================================

template <typename K, typename V>
AvlTree<K, V>::Node::Node(const K& k, const V& v, const int height, Node* left, Node* right)
    : key(k), value(v), height(height), left(left), right(right)
{
}

template <typename K, typename V>
AvlTree<K, V>::Node::Node(const K& k, V&& v, const int height, Node* left, Node* right)
    : key(k), value(std::move(v)), height(height), left(left), right(right)
{
}

// ========================================================
// ITERATOR IMPLEMENTATION
// ========================================================

template <typename K, typename V>
void AvlTree<K, V>::iterator::pushLeft(Node* node)
{
    while (node != nullptr)
    {
        nodeStack.push_back(node);
        node = node->left;
    }
}

template <typename K, typename V>
AvlTree<K, V>::iterator::iterator()
    : nodeStack()
{
}

template <typename K, typename V>
AvlTree<K, V>::iterator::iterator(Node* root)
    : nodeStack()
{
    pushLeft(root);
}

template <typename K, typename V>
typename AvlTree<K, V>::Node& AvlTree<K, V>::iterator::operator*() const
{
    return *nodeStack.back();
}

template <typename K, typename V>
typename AvlTree<K, V>::Node* AvlTree<K, V>::iterator::operator->() const
{
    return nodeStack.back();
}

template <typename K, typename V>
typename AvlTree<K, V>::iterator& AvlTree<K, V>::iterator::operator++()
{
    if (!nodeStack.empty())
    {
        Node* curr = nodeStack.back();
        nodeStack.pop_back();
        if (curr->right != nullptr)
        {
            pushLeft(curr->right);
        }
    }
    return *this;
}

template <typename K, typename V>
typename AvlTree<K, V>::iterator AvlTree<K, V>::iterator::operator++(int)
{
    iterator temp = *this;
    ++(*this);
    return temp;
}

template <typename K, typename V>
bool AvlTree<K, V>::iterator::operator==(const iterator& other) const
{
    Node* thisTop = nodeStack.empty() ? nullptr : nodeStack.back();
    Node* otherTop = other.nodeStack.empty() ? nullptr : other.nodeStack.back();
    return thisTop == otherTop;
}

template <typename K, typename V>
bool AvlTree<K, V>::iterator::operator!=(const iterator& other) const
{
    return !(*this == other);
}

// ========================================================
// CONST_ITERATOR IMPLEMENTATION
// ========================================================

template <typename K, typename V>
void AvlTree<K, V>::const_iterator::pushLeft(const Node* node)
{
    while (node != nullptr)
    {
        nodeStack.push_back(node);
        node = node->left;
    }
}

template <typename K, typename V>
AvlTree<K, V>::const_iterator::const_iterator()
    : nodeStack()
{
}

template <typename K, typename V>
AvlTree<K, V>::const_iterator::const_iterator(const Node* root)
    : nodeStack()
{
    pushLeft(root);
}

template <typename K, typename V>
const typename AvlTree<K, V>::Node& AvlTree<K, V>::const_iterator::operator*() const
{
    return *nodeStack.back();
}

template <typename K, typename V>
const typename AvlTree<K, V>::Node* AvlTree<K, V>::const_iterator::operator->() const
{
    return nodeStack.back();
}

template <typename K, typename V>
typename AvlTree<K, V>::const_iterator& AvlTree<K, V>::const_iterator::operator++()
{
    if (!nodeStack.empty())
    {
        const Node* curr = nodeStack.back();
        nodeStack.pop_back();
        if (curr->right != nullptr)
        {
            pushLeft(curr->right);
        }
    }
    return *this;
}

template <typename K, typename V>
typename AvlTree<K, V>::const_iterator AvlTree<K, V>::const_iterator::operator++(int)
{
    const_iterator temp = *this;
    ++(*this);
    return temp;
}

template <typename K, typename V>
bool AvlTree<K, V>::const_iterator::operator==(const const_iterator& other) const
{
    const Node* thisTop = nodeStack.empty() ? nullptr : nodeStack.back();
    const Node* otherTop = other.nodeStack.empty() ? nullptr : other.nodeStack.back();
    return thisTop == otherTop;
}

template <typename K, typename V>
bool AvlTree<K, V>::const_iterator::operator!=(const const_iterator& other) const
{
    return !(*this == other);
}

// ========================================================
// AVL TREE BEGIN / END METHODS
// ========================================================

template <typename K, typename V>
typename AvlTree<K, V>::iterator AvlTree<K, V>::begin()
{
    return iterator(root);
}

template <typename K, typename V>
typename AvlTree<K, V>::iterator AvlTree<K, V>::end()
{
    return iterator(nullptr);
}

template <typename K, typename V>
typename AvlTree<K, V>::const_iterator AvlTree<K, V>::begin() const
{
    return const_iterator(root);
}

template <typename K, typename V>
typename AvlTree<K, V>::const_iterator AvlTree<K, V>::end() const
{
    return const_iterator(nullptr);
}

template <typename K, typename V>
typename AvlTree<K, V>::const_iterator AvlTree<K, V>::cbegin() const
{
    return const_iterator(root);
}

template <typename K, typename V>
typename AvlTree<K, V>::const_iterator AvlTree<K, V>::cend() const
{
    return const_iterator(nullptr);
}

template <typename K, typename V>
template <typename TargetKey>
typename AvlTree<K, V>::iterator AvlTree<K, V>::lower_bound(const TargetKey& key)
{
    iterator it;
    Node* curr = root;
    while (curr != nullptr)
    {
        if (!(curr->key < key))
        {
            it.nodeStack.push_back(curr);
            curr = curr->left;
        }
        else
        {
            curr = curr->right;
        }
    }
    return it;
}

template <typename K, typename V>
template <typename TargetKey>
typename AvlTree<K, V>::const_iterator AvlTree<K, V>::lower_bound(const TargetKey& key) const
{
    const_iterator it;
    const Node* curr = root;
    while (curr != nullptr)
    {
        if (!(curr->key < key))
        {
            it.nodeStack.push_back(curr);
            curr = curr->left;
        }
        else
        {
            curr = curr->right;
        }
    }
    return it;
}

template <typename K, typename V>
size_t AvlTree<K, V>::getSize() const
{
    return size;
}

template <typename K, typename V>
bool AvlTree<K, V>::empty() const
{
    return size == 0;
}

// ========================================================
// PLACEHOLDER: USER IMPLEMENTS AVL TREE METHODS BELOW
// ========================================================

template <typename K, typename V>
void AvlTree<K, V>::Node::update_height() {
    int hl = (left != nullptr) ? left->height : 0;
    int hr = (right != nullptr) ? right->height : 0;
    height = 1 + (hl > hr ? hl : hr);
}

template <typename K, typename V>
int AvlTree<K, V>::Node::get_height(){
    return  this == nullptr ? 0 : height;
}

template <typename K, typename V>
void AvlTree<K, V>::__ll(Node *&x){
    Node* left = x->left;
    x->left = left->right;
    left->right = x;
    
    x->update_height();
    left->update_height();
    x = left;
}

template <typename K, typename V>
void AvlTree<K, V>::__rr(Node *&x){
    Node *right = x->right;
    x->right = right->left;
    right->left = x;

    x->update_height();
    right->update_height();
    x = right;
}

template <typename K, typename V>
void AvlTree<K, V>::__lr(Node *&x){
    __rr(x->left);
    __ll(x);
}

template <typename K, typename V>
void AvlTree<K, V>::__rl(Node *&x){
    __ll(x->right);
    __rr(x);
}

template <typename K, typename V>
int AvlTree<K, V>::__check_balanced(const Node *x){
    if (x == nullptr) return 0;
    int hl = (x->left != nullptr) ? x->left->height : 0;
    int hr = (x->right != nullptr) ? x->right->height : 0;
    return hl - hr;
}
//tìm phần tử ở giữa để kéo lên làm nóc nhà!
template <typename K, typename V>
void AvlTree<K, V>::__balance(Node *&x){
    if (x == nullptr) return;
    int interval = __check_balanced(x);
    if (interval > 1){
        Node *left = x->left;
        int hll = (left->left != nullptr) ? left->left->height : 0;
        int hlr = (left->right != nullptr) ? left->right->height : 0;
        if (hll >= hlr) __ll(x);//Xét trường hợp bị xóa thì nên có dấu =
        else __lr(x);
    }
    else if (interval < -1) {
        Node *right = x->right;
        int hrr = (right->right != nullptr) ? right->right->height : 0;
        int hrl = (right->left != nullptr) ? right->left->height : 0;
        if (hrr >= hrl) __rr(x);
        else __rl(x);
    }
    else x->update_height();
    return;
}

template <typename K, typename V>
void AvlTree<K, V>::__copy_constructor(const Node *other, Node *&self){
    if (other == nullptr){
        self = nullptr;
        return;
    }
    self = new Node(*other);
    __copy_constructor(other->left, self->left);
    __copy_constructor(other->right, self->right);
}

template <typename K, typename V>
typename AvlTree<K, V>::Node *AvlTree<K, V>::__delete_leftmost(Node *& curr, Node *&backup){
    if (curr->left != nullptr) curr->left = __delete_leftmost(curr->left, backup);
    else{
        backup = curr;
        return curr->right;
    }
    __balance(curr);
    return curr;
}

template <typename K, typename V>
typename AvlTree<K, V>::Node *AvlTree<K, V>::__delete_rightmost(Node *& curr, Node *&backup){
    if (curr->right != nullptr) curr->right = __delete_rightmost(curr->right, backup);
    else{
        backup = curr;
        return curr->left;
    }
    __balance(curr);
    return curr;
}
//Giả sử đã check xem có node cần tìm không
template <typename K, typename V>
typename AvlTree<K, V>::Node *AvlTree<K, V>::__find__delete(Node *&curr, const K &key){
    if (curr == nullptr) return nullptr;
    if (key < curr->key) curr->left = __find__delete(curr->left, key);
    else if (curr->key < key) curr->right = __find__delete(curr->right, key);
    else{
        Node *backup;
        if (curr->left != nullptr){
            curr->left = __delete_rightmost(curr->left, backup);
            curr->key = std::move(backup->key);
            curr->value = std::move(backup->value);
        }
        else if (curr->right != nullptr){
            curr->right = __delete_leftmost(curr->right, backup);
            curr->key = std::move(backup->key);
            curr->value = std::move(backup->value);
        }
        else{
            backup = curr;
            curr = nullptr;
        }
        delete backup;
    }
    if (curr != nullptr) {
        __balance(curr);
    }
    return curr;
}

template <typename K, typename V>
AvlTree<K, V>::AvlTree() : root(nullptr), size(0){
}


template <typename K, typename V>
AvlTree<K, V>::AvlTree(const AvlTree &other) : root(nullptr), size(0){
    __copy_constructor(other.root, root);
    size = other.size;
}

template <typename K, typename V>
void AvlTree<K, V>::__insert(Node *&curr, const K &k, const V &v, bool &flag){
    if (curr == nullptr){
        curr = new Node(k, v);
        return;
    }
    else if (k < curr->key) __insert(curr->left, k, v, flag);
    else if (curr->key < k) __insert(curr->right, k, v, flag);
    else {
        flag = false;
        return;
    }
    __balance(curr);
}

template <typename K, typename V>
void AvlTree<K, V>::__insert(Node *&curr, const K &k, V &&v, bool &flag){
    if (curr == nullptr){
        curr = new Node(k, std::move(v));
        return;
    }
    else if (k < curr->key) __insert(curr->left, k, std::move(v), flag);
    else if (curr->key < k) __insert(curr->right, k, std::move(v), flag);
    else {
        flag = false;
        return;
    }
    __balance(curr);
}

template <typename K, typename V>
void AvlTree<K, V>::__insert(Node *&curr, K &&k, const V &v, bool &flag){
    if (curr == nullptr){
        curr = new Node(std::move(k), v);
        return;
    }
    else if (k < curr->key) __insert(curr->left, std::move(k), v, flag);
    else if (curr->key < k) __insert(curr->right, std::move(k), v, flag);
    else {
        flag = false;
        return;
    }
    __balance(curr);
}

template <typename K, typename V>
void AvlTree<K, V>::__insert(Node *&curr, K &&k, V &&v, bool &flag){
    if (curr == nullptr){
        curr = new Node(std::move(k), std::move(v));
        return;
    }
    else if (k < curr->key) __insert(curr->left, std::move(k), std::move(v), flag);
    else if (curr->key < k) __insert(curr->right, std::move(k), std::move(v), flag);
    else {
        flag = false;
        return;
    }
    __balance(curr);
}

template <typename K, typename V>
AvlTree<K, V>::AvlTree(AvlTree &&other) noexcept : root(other.root), size(other.size){
    other.root = nullptr;
    other.size = 0;
}

template <typename K, typename V>
AvlTree<K, V> &AvlTree<K, V>::operator=(const AvlTree &other){
    if (this != &other){
        clear();
        __copy_constructor(other.root, root);
        size = other.size;
    }
    return (*this);
}

template <typename K, typename V>
AvlTree<K, V> &AvlTree<K, V>::operator=(AvlTree &&other) noexcept{
    if (this != &other){
        clear();
        root = other.root;
        size = other.size;
        other.root = nullptr;
        other.size = 0;
    }
    return (*this);
}

template <typename K, typename V>
bool AvlTree<K, V>::insert(const K &k, const V &v){
    bool flag = true;
    __insert(root, k, v, flag);
    size += flag;
    return flag;
}

template <typename K, typename V>
bool AvlTree<K, V>::insert(const K &k, V &&v){
    bool flag = true;
    __insert(root, k, std::move(v), flag);
    size += flag;
    return flag;
}

template <typename K, typename V>
bool AvlTree<K, V>::insert(K &&k, const V &v){
    bool flag = true;
    __insert(root, std::move(k), v, flag);
    size += flag;
    return flag;
}

template <typename K, typename V>
bool AvlTree<K, V>::insert(K &&k, V &&v){
    bool flag = true;
    __insert(root, std::move(k), std::move(v), flag);
    size += flag;
    return flag;
}

template <typename K, typename V>
V& AvlTree<K, V>::operator[](const K &key){
    insert(key, V());
    return get(key);
}

template <typename K, typename V>
V& AvlTree<K, V>::get(const K& key) const {
    Node *tmp = root;
    while (tmp != nullptr){
        if (tmp->key == key) return tmp->value;
        else if (key < tmp->key) tmp = tmp->left;
        else tmp = tmp->right;
    }
    throw std::out_of_range("AVL Tree::get key not found");
}

template <typename K, typename V>
void AvlTree<K, V>::print() const{
    for (auto &node : *this){
        std::cout << node.key << " : " << node.value << std::endl;
    }
}

template <typename K, typename V>
bool AvlTree<K, V>::contains(const K& key) const{
    Node *tmp = root;
    while (tmp != nullptr){
        if (tmp->key == key) return true;
        else if (key < tmp->key) tmp = tmp->left;
        else tmp = tmp->right;
    }
    return false;
} 

template <typename K, typename V>
bool AvlTree<K, V>::remove(const K &k){
    bool check = contains(k);
    if (!check) return false;
    __find__delete(root, k);
    --size;
    return true;
}

template <typename K, typename V>
void AvlTree<K, V>::clear(){
    auto delete_tree = [](auto& self, Node* node) -> void {
        if (node == nullptr) return;
        self(self, node->left);
        self(self, node->right);
        delete node;
    };
    delete_tree(delete_tree, root);
    root = nullptr;
    size = 0;
}

template <typename K, typename V>
AvlTree<K, V>::~AvlTree(){
    clear();
}

#endif // AVLTREE_CPP

