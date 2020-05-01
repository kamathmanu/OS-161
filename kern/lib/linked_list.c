/*
 * Linked list of void pointers. See linked_list.h.
 */
#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <linked_list.h>

List *list_create(){

	List *l_list = kmalloc(sizeof(*l_list));
	if (l_list == NULL)
		return NULL;

	l_list->head = NULL;

	return (l_list); 
}

void list_destroy (List *list, void (*destroy_each_item)(void*)){

	Node *node, *placeholder;

	assert(destroy_each_item != NULL); //we must have a proper destroy fn.

	if(list != NULL){

		node = list->head;

		while (node != NULL){

		destroy_each_item(node->data);
		placeholder = node;
		node = node->next;
		kfree(placeholder);
	}
		kfree(list);
	}

	return;	
}

Node *insert_item (List *list, void *value){

	assert(list != NULL);

	if (list->head == NULL){
		list->head = kmalloc(sizeof(Node));
		if(list->head == NULL)
			return list->head;
		list->head->data = value;
		list->head->next = NULL;
	}
	else {
		Node *to_be_inserted = kmalloc(sizeof(*to_be_inserted));
		if (to_be_inserted == NULL)
			return list->head;
		to_be_inserted->data = value;
		to_be_inserted->next = list->head;
		list->head = to_be_inserted;
	}
	return (list->head);
}

Node *delete_item (List *list, void *value){

	assert(list != NULL);

	Node *node = list->head;
	Node *preceding = NULL;

	while (node != NULL){
		
		if (node->data == value){

			if (node == list->head){
				//deletion @ head
				preceding = list->head;
				list->head = preceding->next;
				kfree(preceding);
			}
			else {
				//general deletion case
				preceding->next = node->next;
				kfree(node);
			}
		}
		preceding = node;
		node = node->next;
	}
	return(list->head);
}

Node *search (struct list *list, void *value){
	assert(list != NULL);

	struct list_node *node = list->head;
	while (node != NULL){

		if (node->data == value){
			return(node);
		}
		node = node->next;
	}
	return (NULL); //not found
}