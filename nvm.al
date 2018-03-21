struct User {
  i0: int32
  i1: int32
  i2: int32
}

persistent {
  p0: int32
  p1: User
  p2: User
  pp4: *int32
  pp5: *User

  pp6: *int32
}

extern {
  fn putsInt(val: int32);
  fn plus(i1: int32, i2: int32) int32;
  fn nvAllocInt32(pp: **int32);
  fn getThreadName() *int8;
  fn putsInt8Str(str: *int8);
  fn thread(fun: fn(val: int32));
}

fn thread_cb(val: int32) {
  putsInt(val);
}

fn AL__main() {
  p1.i0 = p1.i0 + 1;
  p1.i2 = p1.i2 + 1;

  p2 = p1;

  putsInt(p2.i0);
  putsInt(p2.i2);
  putsInt(plus(p2.i0, p2.i2));

  pp4 = &p1.i0;
  putsInt(*pp4);

  pp5 = &p1;
  (*pp5).i0 = 23332;
  putsInt((*pp5).i0);

  nvAllocInt32(volatile(&volatile(pp6)));
  *pp6 = *pp6 + 2;
  putsInt(*pp6);

  s0: int32 = 1;
  ps0: *int32 = &s0;
  putsInt(*ps0);

  putsInt8Str(getThreadName());

  thread(thread_cb, 1);
}