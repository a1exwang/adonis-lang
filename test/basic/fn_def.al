extern {
  fn putsInt(val: int32);
}

fn plus(a: int32, b: int32) {
  putsInt(a + b);
}

fn AL__main() {
  plus(111, 122);
}
