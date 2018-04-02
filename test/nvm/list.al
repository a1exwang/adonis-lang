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
  c: int32
  recovery0: *persistent Node
  recovery1: *persistent Node
  recovery2: *persistent Node
  recovery3: *persistent Node
  recovery4: int32
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

fn appendList(node: *persistent Node, i: int32) int32 {
  newNode: *persistent Node = node;
  last2: *persistent Node = node;
  ret: int32 = 1;

  nvAllocNBytes(&newNode, sizeof(Node));
  last2 = (*node).prev;

  if c >= 1 {
    (*node).prev = recovery0;
  };
  if c >= 2 {
    (*last2).next = recovery1;
  };
  if c >= 3 {
    (*newNode).prev = recovery2;
  };
  if c >= 4 {
    (*newNode).next = recovery3;
  };
  if c >= 5 {
    (*newNode).data = recovery4;
  };
  if c != 0 {
    putsInt(c);
    c = 0;
    ret = 0;
  } else {
    recovery0 = (*node).prev;
    c = 1;
    (*node).prev = newNode;

    recovery1 = (*last2).next;
    c = 2;
    (*last2).next = newNode;

    recovery2 = (*newNode).prev;
    c = 3;
    (*newNode).prev = last2;

    recovery3 = (*newNode).next;
    c = 4;
    (*newNode).next = node;

    recovery4 = (*newNode).data;
    c = 5;
    (*newNode).data = i;

    c = 0;
    ret = 1;
  };

  return (ret);
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
  c = 0;
  for i: int32 = 1; i < 15; i = i + 1 {
    times: int32 = 1 << i;
    perfList(times);
  };
}
