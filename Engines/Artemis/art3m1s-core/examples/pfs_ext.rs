fn main() {
    let mut counts = std::collections::BTreeMap::new();
    for path in std::env::args().skip(1) {
        let a = art3m1s_core::archive::PfsArchive::open_with_encoding(
            std::path::Path::new(&path),
            encoding_rs::UTF_8,
        )
        .unwrap();
        for e in a.entries() {
            let p = e.path().to_string_lossy().to_lowercase();
            let ext = p.rsplit('.').next().unwrap_or("").to_string();
            *counts.entry(ext).or_insert(0) += 1;
        }
    }
    for (k, v) in counts {
        print!("{k}:{v} ");
    }
    println!();
}
