extern {
  fn putsInt(val: int32);
  fn getsInt() int32;
}

fn AL__main() {
  i: int32 = getsInt();
  if i < 10 {
    putsInt(1);
  } else {
    putsInt(0);
  };
}
