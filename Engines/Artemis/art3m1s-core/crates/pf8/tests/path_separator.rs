use pf8::Pf8Reader;
use sha1::{Digest, Sha1};
use std::io::Cursor;

#[test]
fn backslash_entry_is_readable_with_both_separators() {
    let name = b"Dir\\file\0";
    let payload = b"separator regression";
    let index_len = 4 + 4 + name.len() + 12;
    let mut index = 1_u32.to_le_bytes().to_vec();
    index.extend_from_slice(&(name.len() as u32).to_le_bytes());
    index.extend_from_slice(name);
    index.extend_from_slice(&0_u32.to_le_bytes());
    index.extend_from_slice(&((7 + index_len) as u32).to_le_bytes());
    index.extend_from_slice(&(payload.len() as u32).to_le_bytes());
    let key = Sha1::digest(&index);
    let mut archive = b"pf8".to_vec();
    archive.extend_from_slice(&(index.len() as u32).to_le_bytes());
    archive.extend_from_slice(&index);
    archive.extend(
        payload
            .iter()
            .enumerate()
            .map(|(i, byte)| byte ^ key[i % key.len()]),
    );
    let mut reader = Pf8Reader::open_reader(Box::new(Cursor::new(archive))).unwrap();
    assert_eq!(reader.read_file("dir/file").unwrap(), payload);
    assert_eq!(reader.read_file("DIR\\file").unwrap(), payload);
    assert!(reader.read_file("dirfile").is_err());
}
