extern {
  fn putsInt(val: int32);
}

persistent {
  sum: int32
}

fn AL__main() {
  @batch(sum, 50)
  for i: int32 = 1; i < 100; i = i + 1 {
    putsInt(i);
    sum = sum + i;
  };
  putsInt(sum);
}
