struct User {
  id: int32;
  counter: int32;
}

persistent {
  p_0: int32;
  p_user_1: User;
  p_user_2: User;
}

fn main() {
  p_0 = p_0 + 1;
  putsInt(p_0);

  p_user_1.counter = p_user_1.counter + 1;
  putsInt(p_user_1.counter);
}