// pc_save_check <system.dat|saveg.dat>...: reads PC Artemis system and global saves, writes them back, and compares.
use art3m1s_core::save::pc;
use encoding_rs::SHIFT_JIS;

fn main() {
    for path in std::env::args().skip(1) {
        let bytes = std::fs::read(&path).unwrap();
        if bytes.starts_with(art3m1s_core::save::pc_slot::MAGIC) {
            let save = art3m1s_core::save::pc_slot::read(&bytes, SHIFT_JIS).unwrap();
            println!("{path}: at {}:{} stack {:?}, {} locals, {} replay tags: {:?}", save.position.script, save.position.index,
                save.call_stack.iter().map(|f| format!("{}:{} +{}", f.script, f.index, f.queued.len())).collect::<Vec<_>>(),
                save.locals.len(), save.replay.len(), save.replay.iter().map(|t| t.name.as_str()).collect::<std::collections::BTreeSet<_>>());
        } else if bytes.starts_with(pc::SYSTEM_MAGIC) {
            let variables = pc::read_system(&bytes, SHIFT_JIS).unwrap();
            let written = pc::write_system(&variables, SHIFT_JIS);
            println!("{path}: {} system variables, rewrite identical: {}", variables.len(), written == bytes);
        } else {
            let (variables, lines) = pc::read_global(&bytes, SHIFT_JIS).unwrap();
            let written = pc::write_global(&variables, &lines, SHIFT_JIS);
            let (variables2, lines2) = pc::read_global(&written, SHIFT_JIS).unwrap();
            let same_lines = lines.len() == lines2.len() && lines.iter().all(|(k, v)| {
                let (mut a, mut b) = (v.clone(), lines2[k].clone());
                a.sort_unstable(); b.sort_unstable(); a == b
            });
            println!("{path}: {} globals, {} scripts, same after rewrite: {}, size {} -> {}",
                variables.len(), lines.len(), variables == variables2 && same_lines, bytes.len(), written.len());
        }
    }
}
