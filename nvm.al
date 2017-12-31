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
  p_user_1.counter = p_user_1.counter + 1;
  p_user_1.id = p_user_1.id + 1;

  p_user_2 = p_user_1;

  putsInt(p_user_2.id);
  putsInt(p_user_2.counter);
}