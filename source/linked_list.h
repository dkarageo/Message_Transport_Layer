/**
 * linked_list.h
 *
 * Created by Dimitrios Karageorgiou,
 *  for course "Embedded And Realtime Systems".
 *  Electrical and Computers Engineering Department, AuTh, GR - 2017-2018
 *
 * A header file that declares routines in order to create and manage a linked
 * list.
 *
 * Types defined in linked_list.h:
 *  -linked_list_t
 *  -iterator_t
 *  -node_t
 *
 * Routines defined in linked_list.h:
 *  -linked_list_t *
 *   linked_list_create()
 *  -void
 *   linked_list_destroy(linked_list_t *list)
 *  -void *
 *   linked_list_get_first(linked_list_t *list)
 *  -node_t *
 *   linked_list_append(linked_list_t *list, void *data)
 *  -void *
 *   linked_list_pop(linked_list_t *list)
 *  -node_t *
 *   linked_list_push(linked_list_t *list, void *data)
 *  -void *
 *   linked_list_remove(linked_list_t *list, node_t *node)
 *  -int
 *   linked_list_size(linked_list_t *list)
 *  -iterator_t *
 *   linked_list_iterator(linked_list_t *list)
 *  -int
 *   iterator_has_next(iterator_t *iter)
 *  -void *
 *   iterator_next(iterator_t *iter)
 *  -void
 *   iterator_destroy(iterator_t *iter)
 *
 * Version: 0.2
 */

#ifndef __linked_list_h__
#define __linked_list_h__


typedef struct Node node_t;

struct Node {
    void *data;
    node_t *next;
    node_t *prev;
};

typedef struct {
    node_t *root;
    node_t *tail;
	int size;
} linked_list_t;

typedef struct {
    linked_list_t *list;
    node_t *next;
} iterator_t;


/**
 * Creates a new linked list.
 *
 * Returns:
 *  A reference to the linked list object.
 */
linked_list_t *
linked_list_create();

/**
 * Destroys a linked list.
 *
 * Parameters:
 *  -list: A reference to linked list to be destroyed.
 */
void
linked_list_destroy(linked_list_t *list);

/**
 * Returns the first item in the list.
 *
 * Parameters:
 *  -list: A reference to a linked list.
 */
void *
linked_list_get_first(linked_list_t *list);

/**
 * Inserts given item to the end of the list.
 *
 * Parameters:
 *  -list: List in which data will be appended.
 *  -data: Value to append to the list.
 *
 * Returns:
 *  A reference to the interal node used by linked list to store given item.
 */
node_t *
linked_list_append(linked_list_t *list, void *data);

/**
 * Removes and returns the first item of the list.
 *
 * Parameters:
 *  -list: List from which an item will be popped.
 *
 * Returns:
 *  The item removed from the list, if list contained at least one item.
 *  Otherwise, it returns NULL.
 */
void *
linked_list_pop(linked_list_t *list);

/**
 * Inserts given item to the beggining of the list.
 *
 * Parameters:
 *  -list: List in which new item will be inserted.
 *  -data: Item to be inserted to the list.
 *
 * Returns:
 * A reference to the interal node used by linked list to store given item.
 */
node_t *
linked_list_push(linked_list_t *list, void *data);

/**
 * Removes from the list and returns the item contained in given node.
 *
 * Since a direct reference to the node is used, this is a O(1) remove
 * function.
 *
 * Parameters:
 *  -list: A reference to a linked list.
 *  -node: Node to be removed from linked list.
 *
 * Returns:
 *  The item contained in the removed node.
 */
void *
linked_list_remove(linked_list_t *list, node_t *node);

/**
 * Returns the number of items contained in the list.
 *
 * Parameters:
 *  -list: A reference to a linked list.
 */
int
linked_list_size(linked_list_t *list);

/**
 * Creates and returns a forward iterator for the given list.
 *
 * Returned iterator should be destroyed when no longer needed using
 * iterator_destroy() routine.
 *
 * Its safe to create multiple iterators for the same list and traversing them
 * simultaneously. Though, this means nothing for altering list by using
 * values returned from iterator routines, something that is not thread-safe.
 *
 * Parameters:
 *  -list: A reference to the linked list for which a new iterator will be
 *          created.
 *
 * Returns:
 *  A reference to the created iterator.
 */
iterator_t *
linked_list_iterator(linked_list_t *list);

/**
 * Checks whether there are more items in the list after the one given
 * iterator points to.
 *
 * Parameters:
 *  -iter: A reference to the iterator to be used.
 *
 * Returns:
 *  1 if the item iterator currently points to is not the last one. If it is
 *  the last one, it returns 0.
 */
int
iterator_has_next(iterator_t *iter);

/**
 * Returns next item in iterator and moves iterator one item forward.
 *
 * Parameters:
 *  -iter: A reference to the iterator to be used.
 *
 * Returns:
 *  Next item in iteraotr.
 */
void *
iterator_next(iterator_t *iter);

/**
 * Destroys given iterator.
 *
 * After destruction, given iterator reference should not be used again.
 *
 * Parameters:
 *  -iter: Iterator to be destroyed.
 */
void
iterator_destroy(iterator_t *iter);


#endif
