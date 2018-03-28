extern {
  fn putsInt(val: int32);
  fn tic() *int32;
  fn toc(x: *int32) int32;
}

fn AL__main() {
  a: *int32 = tic();
  sum: persistent int32 = 0;
  tmp: persistent int32 = 0;

  for i: int32 = 1; i < 100; i = i + 1 {
    putsInt(i);
    tmp = sum + 1;
    sum = sum + 1;
  };

  putsInt(sum);
  putsInt(toc(a));
}
