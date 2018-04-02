struct Node {
  prev: *persistent Node
  next: *persistent Node
  data: int32
}

extern {
  fn nvAllocNBytes(pp: ** persistent Node, nBytes: int32);
  fn putsInt(val: int32);
  fn tic() *int32;
  fn toc(ticVal: *int32) int32;
}

persistent {
  root: Node
}

fn sumList(head: *Node) int32 {
  a: int32 = 0;
  sum: int32 = 0;
  for node: *Node = head; 1; a = 1 {
    sum = sum + (*node).data;

    node = volatile((*node).next);
    if (node != head) {} else {
      break;
    };
  };

  return (sum);
}

fn appendList(node: *persistent Node, i: int32) {
  newNode: *persistent Node = node;
  last2: *persistent Node = node;

  nvAllocNBytes(&newNode, sizeof(Node));
  last2 = (*node).prev;

  (*node).prev = newNode;
  (*last2).next = newNode;

  (*newNode).prev = last2;
  (*newNode).next = node;
  (*newNode).data = i;
}

fn perfList(max: int32) {
  root.next = &root;
  root.prev = &root;
  root.data = 0;

  t: *int32 = tic();
  for i: int32 = 1; i < max; i = i + 1 {
    appendList(&root, i);
  };
  putsInt(toc(t));

  sum: int32 = sumList(volatile(&root));
}

fn AL__main() {
  for i: int32 = 1; i < 15; i = i + 1 {
    times: int32 = 1 << i;
    perfList(times);
  };
}
