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
 */

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


linked_list_t *linked_list_create();
void linked_list_destroy(linked_list_t *list);
node_t *linked_list_append(linked_list_t *list, void *data);
void *linked_list_remove(linked_list_t *list, node_t *node);
int linked_list_size(linked_list_t *list);
iterator_t *linked_list_iterator(linked_list_t *list);
int iterator_has_next(iterator_t *iter);
void *iterator_next(iterator_t *iter);
void iterator_destroy(iterator_t *iter);
