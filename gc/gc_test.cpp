#include <iostream>
#include "gc.h"

using namespace std;

struct ListNode {
  ListNode *next;
  ListNode *prev;
  int data;
};

void append(ListNode *head, ListNode *newNode) {
  auto last2 = head->prev;

  newNode->prev = last2;
  newNode->next = head;

  head->prev = newNode;
}

void unattachNode(ListNode *node) {
  auto next = node->next;
  auto prev = node->prev;

  next->prev = prev;
  prev->next = next;

  node->next = nullptr;
  node->prev = nullptr;
}

int main() {
  Algc gc(Algc::Manual, 0);


  auto root = gc.allocate(sizeof(ListNode), {(uint64_t)&((ListNode*)0)->next});
  ListNode *rootList = (ListNode*)root->data;

  auto p1 = gc.allocate(sizeof(ListNode), {(uint64_t)&((ListNode*)0)->next});
  ListNode *p1List = (ListNode*)p1->data;
  rootList->next = p1List;
  p1List->next = rootList;

  gc.allocate(sizeof(ListNode), {(uint64_t)&((ListNode*)0)->next});

  gc.setGetRoots([&root]() -> vector<AlgcBlock*> {
    vector<AlgcBlock*> ret;
    ret.push_back(root);
    return ret;
  });
  gc.doGc();

  return 0;
}