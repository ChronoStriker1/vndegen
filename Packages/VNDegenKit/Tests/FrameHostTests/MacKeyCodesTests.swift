import Testing
@testable import FrameHost

@Test func macKeyCodesHoldEveryKeyTheEnginesRead() {
    #expect(MacKeyCodes.hidUsages.count == 102)
    #expect(Set(MacKeyCodes.hidUsages.values).count == MacKeyCodes.hidUsages.count)
    #expect(MacKeyCodes.hidUsage(0x00) == 4)
    #expect(MacKeyCodes.hidUsage(0x24) == 40)
    #expect(MacKeyCodes.hidUsage(0x32) == 53)
    #expect(MacKeyCodes.hidUsage(0x0A) == 100)
    #expect(MacKeyCodes.hidUsage(0x39) == 57)
    #expect(MacKeyCodes.hidUsage(0x47) == 83)
    #expect(MacKeyCodes.hidUsage(0x4B) == 84)
    #expect(MacKeyCodes.hidUsage(0x51) == 103)
    #expect(MacKeyCodes.hidUsage(0x37) == 227)
    #expect(MacKeyCodes.hidUsage(0x36) == 231)
    #expect(MacKeyCodes.hidUsage(0x7F) == nil)
}
