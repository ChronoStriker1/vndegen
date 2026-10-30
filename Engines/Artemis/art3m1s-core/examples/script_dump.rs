// script_dump <script.iet>: lists a UTF-8 script's instructions with their indexes and source lines.
fn main() {
    let path = std::env::args().nth(1).expect("script path");
    let text = std::fs::read_to_string(&path).unwrap();
    let script = asb_interpreter::Script::parse(&path, &text).unwrap();
    for (index, instruction) in script.instructions.iter().enumerate() {
        println!("{index}\t{}\t{}", instruction.line, instruction.tag);
    }
}
