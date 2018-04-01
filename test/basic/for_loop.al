extern {
  fn putsInt(val: int32);
  fn tic() *int32;
  fn toc(ticVal: *int32) int32;
}

fn AL__main() {
  sum: int32 = 0;
  for i: int32 = 1; i < 100; i = i + 1 {
    sum = sum + i;
  };
  putsInt(sum);
}
