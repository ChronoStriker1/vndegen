import CSystemAoi
import Testing

@Suite struct SystemAoiWallTests {
    @Test(arguments: [
        (2, 9, 2, 1),
        (3, 9, 2, 0),
        (11, 5, 0, 2),
        (0, 1, 1, 1),
        (4, 0, 0, 0),
    ])
    func iceDungeonWallsShareOneKey(x: Int32, y: Int32, side: Int32, variant: Int32) {
        #expect(vnsystemaoi_wall_variant(0x080200, true, x, y, side) == variant)
    }

    @Test(arguments: [(0, 1), (1, 0), (2, 2), (3, 2)])
    func fireCaveSidesHaveTheirOwnKeys(side: Int32, variant: Int32) {
        #expect(vnsystemaoi_wall_variant(0x020300, false, 4, 1, side) == variant)
    }
}
