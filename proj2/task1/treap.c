#include "csapp.h"
#include "treap.h"

void init_node(Node* node, KeyType _key,int _left_stock,int _price){
	node->key=_key;
	node->left_stock=_left_stock;
	node->price=_price;
	node->size=1;
	node->priority=rand();
	node->left=NULL;
	node->right=NULL;
}
void calc_size(Node* node){
	node->size=1;
	if(node->left!=NULL) node->size+=node->left->size;
	if(node->right!=NULL) node->size+=node->right->size;
}
void set_left(Node* node, Node* new_left){
	node->left=new_left;
	calc_size(node);
}
void set_right(Node* node,Node* new_right){
	node->right=new_right;
	calc_size(node);
}

//root를 루트로하는 트리를
//key미만의 값을가진 트리와 key이상의 값을가진 트리 두개로 쪼개어 반환한다.
NodePair split(Node* root, KeyType key) {
	if (root == NULL){
		NodePair ret = {NULL,NULL};
		return ret;
	}
	//key > root->key 이면,
	//key미만의 값을가진 트리와 key이상의 값을가진 트리 두개로 오른쪽 서브트리를 쪼갠다.
	if (key > root->key) {
		NodePair rs = split(root->right, key);
		//key미만의 값을가진 트리를 root의 왼쪽 서브트리로 설정한다.
		set_right(root,rs.first);
		NodePair ret = {root,rs.second};
		return ret;
	}
	else {
		NodePair ls = split(root->left, key);
		set_left(root,ls.second);
		NodePair ret = {ls.first,root};
		return ret;
	}
}
//root를 루트로 하는 트리에 node를 삽입하고,
//그 트리의 루트를 반환한다.
Node* insert(Node* root, Node* node) {
	if (root == NULL) return node;
	//node의 우선순위가 클경우, root를 대체한다.
	if (root->priority < node->priority) {
		NodePair splitted = split(root, node->key);
		set_left(node,splitted.first);
		set_right(node,splitted.second);
		return node;
	}
	else if (root->key > node->key)
		set_left(root,insert(root->left,node));
	else
		set_right(root,insert(root->right,node));
	return root;
}
//a트립와 b트립를 합치고 루트를 반환한다.max(a)<min(b)
Node* merge(Node* a, Node* b) {
	if (a == NULL) return b;
	if (b == NULL) return a;
	//b의 우선순위가 더 클경우, a는 b의 자식이어야한다.
	if (a->priority < b->priority) {
		set_left(b,merge(a,b->left));
		return b;
	}
	else {
		set_right(a,merge(a->right,b));
		return a;
	}
}
//key를 지운 트리의 루트를 반환한다.
Node* erase(Node* root, KeyType key) {
	//기저: key를 찾은경우와 key가 없는경우
	if (root == NULL) return root;
	if (root->key == key) {
		Node* merged = merge(root->left, root->right);
		free(root);
		return merged;
	}
	if (root->key > key)
		set_left(root,erase(root->left,key));
	else
		set_right(root,erase(root->right,key));
	return root;
}
Node* find_key(Node* root,KeyType key){
	if(root==NULL) return NULL;
	if(root->key==key)
		return root;
	else if(root->key>key)
		return find_key(root->left,key);
	else
		return find_key(root->right,key);
}
void print_in_buf(Node* root,char* buf){
	if(root==NULL) return;
	print_in_buf(root->left,buf);
	char temp[MAXLINE]="";
	sprintf(temp,"%d %d %d\n",root->key,root->left_stock,root->price);
	strcat(buf,temp);
	print_in_buf(root->right,buf);
}
void print_console(Node* root){
	if(root==NULL) return;
	print_console(root->left);
	printf("%d %d %d\n",root->key,root->left_stock,root->price);
	print_console(root->right);
}
void update_stock_txt(Node* root){
	char buff[MAXLINE]="";
	print_in_buf(root,buff);
	FILE* stock_txt=Fopen("stock.txt","w");
	Fputs(buff,stock_txt);
	Fclose(stock_txt);
}