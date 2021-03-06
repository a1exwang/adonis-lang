extern {
  fn putsInt(val: int32);
  fn tic() *int32;
  fn toc(x: *int32) int32;
}

persistent {
  sum: int32
}

fn AL__main() {
  a: *int32 = tic();

  @batch(sum, 100000)
  for i: int32 = 1; i < 100000; i = i + 1 {
    sum = sum + i;
  };

  putsInt(sum);
  putsInt(toc(a));
}
