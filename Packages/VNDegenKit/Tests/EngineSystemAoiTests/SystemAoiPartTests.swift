import CSystemAoi
import Testing

@Suite struct SystemAoiPartTests {
    @Test(arguments: [(0x010100, 4, 3, 0), (0x020200, 5, 4, 2), (0x080200, 2, 9, 1)])
    func floorsAlwaysUseTheSharedKey(seed: UInt32, x: Int32, y: Int32, variant: Int32) {
        #expect(vnsystemaoi_floor_variant(seed, x, y) == variant)
    }

    @Test(arguments: [(0x010100, false, 4, 3, 1), (0x020200, false, 5, 4, 0), (0x020200, false, 6, 1, 2), (0x080200, true, 2, 9, 1)])
    func roofsFollowColumnSeven(seed: UInt32, sharedKey: Bool, x: Int32, y: Int32, variant: Int32) {
        #expect(vnsystemaoi_roof_variant(seed, sharedKey, x, y) == variant)
    }

    @Test(arguments: [(11, 5, 2), (12, 5, 2), (11, 6, 2), (12, 6, 2), (12, 7, 0)])
    func polesTakeTheCellNearestTheParty(x: Int32, y: Int32, variant: Int32) {
        #expect(vnsystemaoi_pole_variant(0x080200, true, x, y, 11, 5) == variant)
    }
}
