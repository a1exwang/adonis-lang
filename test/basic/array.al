extern {
  fn putsInt(val: int32);
}

fn AL__main() {
  putsInt([0,1,2].[1]);
}
