extern {
  fn putsInt(val: int32);
}

fn AL__main() {
  a: int32 = 1;
  b: int32 = 2;
  b <- a;
  putsInt(b);
}
