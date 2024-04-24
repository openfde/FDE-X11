#include "node.h"
#define log(prio, ...) __android_log_print(ANDROID_LOG_ ## prio, "huyang_node", __VA_ARGS__)


struct Node* createNode(WindowPtr data) {
    struct Node* newNode = (struct Node*)malloc(sizeof(struct Node));
    if (newNode == NULL) {
        log(ERROR, "内存分配失败\n");
        exit(1);
    }
    newNode->data = data;
    newNode->next = NULL;
    return newNode;
}

void insertAtBeginning(struct Node** head, WindowPtr data) {
    struct Node* newNode = createNode(data);
    newNode->next = *head;
    *head = newNode;
}

void insertAtEnd(struct Node** head, WindowPtr data) {
    struct Node* newNode = createNode(data);
    if (*head == NULL) {
        *head = newNode;
        return;
    }
    struct Node* temp = *head;
    while (temp->next != NULL) {
        temp = temp->next;
    }
    temp->next = newNode;
}

struct Node* getNodeAtPosition(struct Node* head, int position) {
    if (position < 1) {
        return NULL;
    }
    struct Node* temp = head;
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

void deleteNode(struct Node** head, WindowPtr key) {
    struct Node *temp = *head, *prev = NULL;
    if (temp != NULL && temp->data == key) {
        *head = temp->next;
        free(temp);
        return;
    }
    while (temp != NULL && temp->data != key) {
        prev = temp;
        temp = temp->next;
    }
    if (temp == NULL) {
        log(ERROR, "节点未找到\n");
        return;
    }
    prev->next = temp->next;
    free(temp);
}

int search(struct Node* head, WindowPtr key) {
    struct Node* temp = head;
    XID found = 0;
    XID position = 1;
    while (temp != NULL) {
        if (temp->data == key) {
            log(ERROR, "元素 %d 找到在位置 %d\n", key, position);
            found = 1;
            break;
        }
        temp = temp->next;
        position++;
    }
    if (!found) {
        log(ERROR, "元素 %d 未找到\n", key);
    }
    return found;
}
