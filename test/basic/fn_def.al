extern {
  fn putsInt(val: int32);
}

fn plus(a: int32, b: int32) int32 {
  putsInt(a + b);
  return (a + b);
}

fn AL__main() {
  putsInt(plus(111, 122));
}
