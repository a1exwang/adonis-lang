extern {
  fn putsInt(val: int32);
}

fn AL__main() {

  ttt: int32 = 233;
  putsInt(ttt + 1);
  putsInt(ttt < 234);
  putsInt(ttt << 1);
  putsInt(ttt != 233);
}
