#ifndef __TREAP_
#define __TREAP_
#include "csapp.h"

typedef int KeyType;
typedef struct _Node{
	KeyType key;
	int left_stock, price;

	int size,priority;
	struct _Node* left,* right;
}Node;
typedef struct _NodePair {
    Node* first;
    Node* second;
} NodePair;

//Readers-Writers Problem
int read_tree;
sem_t mutex_read_tree,w_tree;

void init_node(Node* node, KeyType _key,int _left_stock,int _price);
void calc_size(Node* node);
void set_left(Node* node, Node* new_left);
void set_right(Node* node,Node* new_right);
NodePair split(Node* root, KeyType key);
Node* insert(Node* root, Node* node);
Node* merge(Node* a, Node* b);
Node* erase(Node* root, KeyType key);
Node* find_key(Node* root,KeyType key);
void print_in_buf(Node* root,char* buf);
void print_console(Node* root);
void update_stock_txt(Node* root);



#endif