
#include <stdlib.h>
#include <X11/X.h>
#include <jni.h>
#include <android/log.h>
#include <window.h>

struct Node {
    WindowPtr data;
    struct Node* next;
};

struct Node* createNode(WindowPtr data);

void insertAtBeginning(struct Node** head, WindowPtr data);

void insertAtEnd(struct Node** head, WindowPtr data);

void deleteNode(struct Node** head, WindowPtr key);

int search(struct Node* head, WindowPtr key);

struct Node* getNodeAtPosition(struct Node* head, int position);
