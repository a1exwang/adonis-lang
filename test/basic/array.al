extern {
  fn putsInt(val: int32);
}

fn AL__main() {
  data: [5]int32 = [0, 1, 2, 3, 4];
  putsInt(data.[1]);
}
