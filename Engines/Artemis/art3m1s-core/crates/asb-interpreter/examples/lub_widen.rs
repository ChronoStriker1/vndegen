// lub_widen <in> <out>: rewrites 32-bit Lua 5.1 bytecode for a 64-bit Lua.
fn main() {
    let a: Vec<String> = std::env::args().collect();
    let bytes = std::fs::read(&a[1]).unwrap();
    std::fs::write(&a[2], asb_interpreter::lua51_bytecode::widen_size_t(&bytes).expect("not a 32-bit chunk")).unwrap();
}
