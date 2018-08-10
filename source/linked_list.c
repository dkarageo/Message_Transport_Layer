/**
 * linked_list.c
 *
 * Created by Dimitrios Karageorgiou,
 *  for course "Embedded And Realtime Systems".
 *  Electrical and Computers Engineering Department, AuTh, GR - 2017-2018
 *
 * This file implements routines defines in linked_list.h.
 *
 */

#include <stdlib.h>
#include "linked_list.h"


linked_list_t *linked_list_create()
{
    linked_list_t *list = (linked_list_t *) malloc(sizeof(linked_list_t));

    // Create an empty root node.
    node_t *root = (node_t *) malloc(sizeof(node_t));
    root->data = NULL;
    root->prev = NULL;
    root->next = NULL;

    list->root = root;  // Set root node to the empty one.
    list->tail = root;
	list->size = 0;

    return list;
}

void linked_list_destroy(linked_list_t *list)
{
    // Remove all the nodes contained in the list, including the root one.
    node_t *cur = list->root;
    while (cur != NULL) {
        node_t *to_del = cur;
        cur = cur->next;
        free(to_del);
    }

    free(list);  // Free actual list object.
}

node_t *linked_list_append(linked_list_t *list, void *data)
{
    node_t *node = (node_t *) malloc(sizeof(node_t));

	list->size++;

    node->prev = list->tail;
    node->next = NULL;
    list->tail->next = node;
    list->tail = node;

    node->data = data;

    return node;
}

void *linked_list_remove(linked_list_t *list, node_t *node)
{
	list->size--;

	void *data = node->data;

    if (node->next != NULL) node->next->prev = node->prev;
    else list->tail = node->prev;
    node->prev->next = node->next;

    free(node);

    return data;
}

int linked_list_size(linked_list_t *list)
{
    return list->size;
}

iterator_t *linked_list_iterator(linked_list_t *list)
{
    iterator_t *iter = (iterator_t *) malloc(sizeof(iterator_t));
    iter->list = list;
    iter->next = list->root->next;
    return iter;
}

int iterator_has_next(iterator_t *iter)
{
    return iter->next != NULL;
}

void *iterator_next(iterator_t *iter)
{
    void *data = iter->next->data;
    iter->next = iter->next->next;
    return data;
}

void iterator_destroy(iterator_t *iter)
{
    free(iter);
}
