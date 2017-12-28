
struct Attr {
  key: string,
  value: string,
}

struct User {
  id: int,
  name: string,
  description: string,
  attr: ref Attr,
}

fn onRequest(name: string) string {
  let u = User !{1, name, "wtf", Attr !{"gender", "male",}};
  u.name + "(" + u.attr.key + ":" + u.attr.value + ")";
}