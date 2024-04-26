#include "node.h"

#define log(prio, ...) __android_log_print(ANDROID_LOG_ ## prio, "huyang_node", __VA_ARGS__)


WindowNode *node_create(WindAttribute data) {
    WindowNode *newNode = (WindowNode *) malloc(sizeof(WindowNode));
    if (newNode == NULL) {
        log(ERROR, "malloc fail\n");
        exit(1);
    }
    newNode->data = data;
    newNode->next = NULL;
    return newNode;
}

void node_insert_at_begin(WindowNode **head, WindAttribute data) {
    WindowNode *newNode = node_create(data);
    newNode->next = *head;
    *head = newNode;
}

void node_append(WindowNode **head, WindAttribute data) {
    WindowNode *newNode = node_create(data);
    if (*head == NULL) {
        *head = newNode;
        return;
    }
    WindowNode *temp = *head;
    while (temp->next != NULL) {
        temp = temp->next;
    }
    temp->next = newNode;
}

int node_get_length(WindowNode *head) {
    int length = 0;
    WindowNode *temp = head;
    while (temp != NULL) {
        length++;
        temp = temp->next;
    }
    return length;
}

WindowNode *node_get_at_position(WindowNode *head, int position) {
    if (position < 1) {
        return NULL;
    }
    WindowNode *temp = head;
    int count = 1;
    while (temp != NULL && count < position) {
        temp = temp->next;
        count++;
    }
    if (temp == NULL) {
        return NULL;
    }
    return temp;
}

WindowNode *node_get_at_index(WindowNode *head, int index) {
//    log(ERROR, "want index:%d\n", index);
    if (!head) {
        return NULL;
    }
    if (index < 1) {
        return NULL;
    }
    WindowNode *temp = head;
    while (temp != NULL && temp->data.index != index) {
        temp = temp->next;
    }
    if (temp == NULL) {
        return NULL;
    }
//    log(ERROR, "getNodeByIndex:%d\n", index);
    return temp;
}

void node_delete(WindowNode **head, WindAttribute key) {
    if (!head) {
        return;
    }
    WindowNode *temp = head, *prev = NULL;
    if (temp != NULL && temp->data.pWin == key.pWin) {
        *head = temp->next;
        free(temp);
        return;
    }
    while (temp != NULL && temp->data.pWin != key.pWin) {
        prev = temp;
        temp = temp->next;
    }
    if (temp == NULL) {
        log(ERROR, "Node not found\n");
        return;
    }
    prev->next = temp->next;
    free(temp);
}

WindowNode *node_search(WindowNode *head, WindowPtr ptr) {
    if (!head) {
        return NULL;
    }
    WindowNode *temp = head;
    XID position = 1;
    while (temp) {
        if (temp->data.pWin == ptr) {
            log(ERROR, "WindowPtr %p found at position %d\n", (void *) ptr, position);
            return temp;
        }
        temp = temp->next;
        position++;
    }
    return NULL;
}

void node_replace_at_position(WindowNode *head, int position, WindAttribute newData) {
    WindowNode *temp = node_get_at_position(head, position);
    if (temp == NULL) {
        return;
    }
    temp->data = newData;
}

int node_get_max_index(WindowNode *head) {
    if (!head) {
        return 0;
    }
    WindowNode *temp = head;
    int index = 1;
    while (temp) {
        if (temp->data.index > index) {
            index = temp->data.index;
        }
        temp = temp->next;
    }
    return index;
}