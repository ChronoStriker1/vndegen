import Testing
@testable import WebRuntime

@Test @MainActor func childProcessOnlyAsksToOpenAWebAddressAndRunsNothing() async throws {
    var opened: [String] = []
    let page = try PageHarness(storage: false) { request in
        if case let .openExternal(url) = request { opened.append(url) }
        return .value(.null)
    }
    try await page.load("<html><body></body></html>")
    let results = try await page.run("""
        const { exec, execSync, execFile, spawn } = require('child_process');
        const run = (command) => new Promise((done) => exec(command, (error) => done(error ? error.code : 'ok')));
        const results = [];
        results.push(await run('open https://www.patreon.com/c/user?u=1'));
        results.push(await run('rundll32.exe url.dll,FileProtocolHandler "https://example.com/home"'));
        results.push(await run('open "https://example.com/a"; touch /tmp/x'));
        results.push(await run('open /Applications/Calculator.app'));
        results.push(await new Promise((done) => execFile('open', ['mailto:someone@example.com'], (error) => done(error ? error.code : 'ok'))));
        results.push(await new Promise((done) => spawn('rm', ['-rf', '/']).on('error', (error) => done(error.code))));
        try { execSync('ls'); results.push('ran'); } catch (e) { results.push(e.code); }
        return results.join(' ');
        """) as? String
    #expect(results == "ok ok ENOSYS ENOSYS ok ENOSYS ENOSYS")
    #expect(opened == ["https://www.patreon.com/c/user?u=1", "https://example.com/home", "mailto:someone@example.com"])
}
