extern {
  fn putsInt(val: int32);
  fn tic() *int32;
  fn toc(ticVal: *int32) int32;
}

fn sum_no_opt(max: int32) {
  t1: *int32 = tic();

  sum: int32 = 0;

  for k: int32 = 0; k < max; k = k + 1 {
    sum = sum + k;
  };

  putsInt(toc(t1));
}
fn sum_batch_opt(max: int32) {
  t1: *int32 = tic();

  sum: int32 = 0;

  @batch(sum, 100)
  for k: int32 = 0; k < max; k = k + 1 {
    sum = sum + k;
  };

  putsInt(toc(t1));
}

fn AL__main() {
  twoExp: int32 = 1;

  for i: int32 = 1; i < 10; i = i + 1 {
    twoExp = 1;
    for j: int32 = 0; j < i; j = j + 1 {
      twoExp = twoExp << 1;
    };
    sum_batch_opt(twoExp);
    sum_no_opt(twoExp);
  };
}
