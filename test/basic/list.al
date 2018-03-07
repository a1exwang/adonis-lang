extern {
  fn putsInt(val: int32);
  fn nvAllocInt32(pp: **int32);
  fn getThreadName() *int8;
  fn putsInt8Str(str: *int8);
}

fn AL__main() {
  i1: int32 = 0;
  pNvm: *int32 = &i1;
  nvAllocInt32(volatile(&pNvm));
  *pNvm = 123;
  putsInt(*pNvm);
  putsInt8Str(getThreadName());
}
