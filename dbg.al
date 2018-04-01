extern {
  fn putsInt(val: int32);
}

fn test(m: int32) {
  sum: int32 = 0;
  @batch(sum, 1)
  for k: int32 = 0; k < m; k = k + 1 {
    sum = sum + k;
  };
  putsInt(sum);
}

fn AL__main() {
  sum: int32 = 0;
  twoExp: int32 = 1;
  for i: int32 = 1; i < 10; i = i + 1 {
    test(i);
  };
}
