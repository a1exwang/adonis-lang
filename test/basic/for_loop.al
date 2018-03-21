extern {
  fn putsInt(val: int32);
}

fn AL__main() {

  a: int32 = 233;
  a = a + 1;
  putsInt(a);

  b: int32 = a < 2;
  putsInt(b);

  sum: int32 = 0;
  i: int32 = 0;
  for 1; i < 100; i = i + 1 {
    putsInt(i);
    sum = sum + i;
  };
  putsInt(sum);
}
