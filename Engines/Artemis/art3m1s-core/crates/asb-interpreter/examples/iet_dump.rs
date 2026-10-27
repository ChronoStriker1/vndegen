// iet_dump <file> : parses a UTF-8 script and prints its instructions.
fn main() {
    let path = std::env::args().nth(1).unwrap();
    let text = std::fs::read_to_string(&path).unwrap();
    let s = asb_interpreter::script::Script::parse(&path, &text).unwrap();
    for (i, ins) in s.instructions.iter().enumerate() {
        let mut keys: Vec<_> = ins.params.iter().map(|(k, v)| format!("{k}={}", v.chars().take(40).collect::<String>().replace('\n', "\\n"))).collect();
        keys.sort();
        println!("{i} L{} {} {}", ins.line, ins.tag, keys.join(" "));
    }
}
