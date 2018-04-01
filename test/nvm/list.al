struct Node {
  prev: *persistent Node
  next: *persistent Node
  data: int32
}

extern {
  fn nvAllocInt32(pp: * persistent* persistent int32);
  fn nvAllocNBytes(pp: ** persistent Node, nBytes: int32);
  fn putsInt(val: int32);
  fn plus(i1: int32, i2: int32) int32;
  fn getThreadName() *int8;
  fn putsInt8Str(str: *int8);
  fn thread(fun: fn(val: int32), i: int32);
}

persistent {
  root: Node
}

fn traverseList(head: *Node) {
  for node: *Node = head; node != head; node = volatile((*node).next) {
    putsInt((*node).data);
  };
}

fn AL__main() void {
  tmp: *persistent Node = &root;
  nvAllocNBytes(&tmp, sizeof(Node));
  root.next = tmp;
  (*tmp).data = 1;


}
