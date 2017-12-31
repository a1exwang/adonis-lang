struct User {
  i0: int32;
  i1: int32;
  i2: int32;
}

persistent {
  p0: int32;
  p1: User;
  p2: User;
}

fn main() {
  p1.i0 = p1.i0 + 1;
  p1.i2 = p1.i2 + 1;


  p2 = p1;

  putsInt(p2.i0);
  putsInt(p2.i1);
  putsInt(p2.i2);
}