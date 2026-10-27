// pfs_cat <archive> <entry>: writes one entry to stdout.
use std::io::Write;
fn main() {
    let mut a = std::env::args().skip(1);
    let path = a.next().unwrap();
    let name = a.next().unwrap();
    let mut ar = art3m1s_core::archive::PfsArchive::open_with_encoding(
        std::path::Path::new(&path),
        encoding_rs::UTF_8,
    )
    .unwrap();
    let e = ar.find(&name).expect("no such entry").clone();
    let mut out = vec![0u8; e.size() as usize];
    let mut done = 0;
    while done < out.len() {
        let n = ar.read_entry(&e, done as u64, &mut out[done..]).unwrap();
        if n == 0 {
            break;
        }
        done += n;
    }
    std::io::stdout().write_all(&out).unwrap();
}
