extern {
  fn putsInt(val: int32);
  fn thread(fun: fn(val: int32), val: int32);
}

fn thread_cb(val: int32) {
  putsInt(123);
}

fn AL__main() {
  thread(thread_cb, 1);
}
