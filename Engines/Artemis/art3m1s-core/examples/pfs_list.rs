fn main() {
    let path = std::env::args().nth(1).unwrap();
    let enc = std::env::args().nth(2).unwrap_or("utf-8".into());
    let enc = encoding_rs::Encoding::for_label(enc.as_bytes()).unwrap();
    match art3m1s_core::archive::PfsArchive::open_with_encoding(std::path::Path::new(&path), enc) {
        Ok(a) => {
            let e: Vec<_> = a.entries().collect();
            println!("{} entries", e.len());
            for x in e.iter().filter(|x| {
                std::env::var("PFS_GREP").map_or(true, |g| {
                    x.path().to_string_lossy().to_lowercase().contains(&g)
                })
            }) {
                println!("{} {}", x.path().display(), x.size());
            }
        }
        Err(e) => println!("ERR {e}"),
    }
}
