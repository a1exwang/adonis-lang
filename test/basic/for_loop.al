extern {
  fn putsInt(val: int32);
  fn tic() *int32;
  fn toc(ticVal: *int32) int32;
}

fn AL__main() {

  t1: *int32 = tic();
  a: int32 = 233;
  a = a + 1;
  putsInt(a);

  b: int32 = a < 2;
  putsInt(b);

  sum: int32 = 0;
  for i: int32 = 1; i < 100; i = i + 1 {
    putsInt(i);
    sum = sum + i;
  };
  putsInt(sum);

  putsInt(toc(t1));
}
