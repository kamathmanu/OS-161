#ifndef _LINKED_LIST_H_
#define _LINKED_LIST_H_

//Generic linked list data structure.

struct list_node{
	void *data;
	struct list_node *next;
};

typedef struct list_node Node;

struct list{
	struct list_node *head;
};

typedef struct list List;


//Function to create an empty list.

List *list_create();

//Destroy the list.
//destroy_each_item is a pointer to a function that destroys the
//corresponding data item. Example if we have a list of locks then we
//pass in lock_destroy. 

void list_destroy (List *list, void (*destroy_each_item)(void*));

//Insert a new item at the head of the list

Node *insert_item (List *list, void *value);

//Delete an item that u have searched from the list.

Node *delete_item (List *list, void *value);

//Search for an item in the list

Node *search (List *list, void *value);

#endif /* _LINKED_LIST_H_ */
