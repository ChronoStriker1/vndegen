// pluto_check <file>...: reads every Pluto value stored in PC Artemis saves, writes each back, and reads it again.
use mlua::{Lua, Value};

fn same(a: &Value, b: &Value) -> bool {
    match (a, b) {
        (Value::Table(a), Value::Table(b)) => {
            a.clone().pairs::<Value, Value>().count() == b.clone().pairs::<Value, Value>().count()
                && a.clone().pairs::<Value, Value>().all(|pair| {
                    let (k, v) = pair.unwrap();
                    same(&v, &b.raw_get::<Value>(k).unwrap())
                })
        }
        _ => a == b,
    }
}

fn main() {
    let lua = Lua::new();
    let (mut ok, mut bad) = (0, 0);
    for path in std::env::args().skip(1) {
        let data = std::fs::read(&path).unwrap();
        // A Pluto value starts with a new table: 1, 1, 5, 0.
        let start: Vec<u8> = [1i32, 1, 5, 0]
            .iter()
            .flat_map(|v| v.to_le_bytes())
            .collect();
        let mut at = 0;
        while let Some(found) = data[at..].windows(16).position(|w| w == start) {
            let begin = at + found;
            let len = u32::from_le_bytes(data[begin - 4..begin].try_into().unwrap()) as usize;
            at = begin + 16;
            if begin + len > data.len() {
                continue;
            }
            let blob = &data[begin..begin + len];
            match asb_interpreter::pluto::unpersist(&lua, blob, encoding_rs::SHIFT_JIS) {
                Ok(value) => {
                    let written =
                        asb_interpreter::pluto::persist(&value, encoding_rs::SHIFT_JIS).unwrap();
                    let again =
                        asb_interpreter::pluto::unpersist(&lua, &written, encoding_rs::SHIFT_JIS)
                            .unwrap();
                    if same(&value, &again) {
                        ok += 1
                    } else {
                        bad += 1;
                        println!("{path}: round trip differs at {begin}")
                    }
                }
                Err(e) => {
                    bad += 1;
                    println!("{path}: {e} at {begin} len {len}");
                }
            }
        }
    }
    println!("blobs ok {ok} bad {bad}");
}
