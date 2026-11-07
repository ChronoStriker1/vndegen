import CNekoSDK
import Foundation
import Testing
import TestSupport

// Scripts name a variable by scope and its place in macrolist.ini's list, which the PC keeps by name: Test Game 94's
// file lists GLB_PROLOGUE_OVER first, and its flows count it fifth, after the four endings. A global's place is
// parameter 1 of a condition or change and a scenario variable's parameter 2, as Test Game 88's flows show.
@Suite struct NekoSDKMacroListTests {
    private let macros = """
    #p Global
    GLB_PROLOGUE_OVER\t1000\t//comment
    GLB_ENDING_A\t\t51
    GLB_ENDING_B\t\t52
    GLB_TITLE_BG\t\t55
    ##
    #p Senario
    SENARIO_STATUS\t\t4000
    SENARIO_ROUTE_A\t\t100
    SENARIO_ROUTE_B\t\t101
    ##
    """

    @Test func placesCountInNameOrder() throws {
        let root = try NekoSDKFlowFixture.game(["cfg/macrolist.ini": Data(macros.utf8)])
        defer { try? FileManager.default.removeItem(at: root) }
        #expect((0..<4).map { vnneko_debug_macro_number(root.path, 0, $0) } == [51, 52, 1000, 55])
        #expect((0..<3).map { vnneko_debug_macro_number(root.path, 1, $0) } == [100, 101, 4000])
        #expect(vnneko_debug_macro_number(root.path, 0, 4) == -1)
    }

    @Test func aChangeTakesAScenarioVariablesPlaceFromParameter2() throws {
        // Each change keeps a stale place for the other scope, as the editor does.
        try NekoSDKFlowFixture.run([
            .init(op: 0, next: 2),
            .init(op: 6, next: 3, p: [0: 1, 1: 3, 2: 1, 4: 2, 5: 5]),
            .init(op: 6, next: 4, p: [0: 0, 1: 2, 2: 2, 4: 2, 5: 9]),
            .init(op: 2, next: 0),
        ], macros: macros) { e in
            #expect(vnneko_debug_variable(e, 1, 101) == 5)
            #expect(vnneko_debug_variable(e, 1, 100) == 0)
            #expect(vnneko_debug_variable(e, 0, 1000) == 9)
        }
    }

    // The changes' operations, parameter 3, read from Test Game 94's exe: 0 sets, 1 adds one, 2 takes one, 3 adds the
    // operand and 4 takes it. Test Game 92's `秋穂カウンタ++` is 3 with an operand of 1, run once per choice of Akiho.
    @Test func changesSetStepAddAndTake() throws {
        try NekoSDKFlowFixture.run([
            .init(op: 0, next: 2),
            .init(op: 6, next: 3, p: [0: 1, 2: 0, 3: 3, 4: 2, 5: 1]),
            .init(op: 6, next: 4, p: [0: 1, 2: 0, 3: 3, 4: 2, 5: 1]),
            .init(op: 6, next: 5, p: [0: 1, 2: 1, 3: 0, 4: 2, 5: 10]),
            .init(op: 6, next: 6, p: [0: 1, 2: 1, 3: 4, 4: 2, 5: 3]),
            .init(op: 6, next: 7, p: [0: 1, 2: 1, 3: 2]),
            .init(op: 6, next: 8, p: [0: 0, 1: 1, 3: 1]),
            .init(op: 6, next: 9, p: [0: 0, 1: 1, 3: 1]),
            .init(op: 2, next: 0),
        ], macros: macros) { e in
            #expect(vnneko_debug_variable(e, 1, 100) == 2)
            #expect(vnneko_debug_variable(e, 1, 101) == 6)
            #expect(vnneko_debug_variable(e, 0, 52) == 2)
        }
    }

    // An operand of kind 0 or 1, parameter 4, is the global or scenario variable parameter 5 numbers, by its number.
    @Test func anOperandNamesAVariableByItsNumber() throws {
        try NekoSDKFlowFixture.run([
            .init(op: 0, next: 2),
            .init(op: 6, next: 3, p: [0: 0, 1: 0, 4: 2, 5: 7]),
            .init(op: 6, next: 4, p: [0: 1, 2: 0, 3: 0, 4: 0, 5: 51]),
            // Holds, so it goes to its other target, which sets the marker.
            .init(op: 5, next: 6, otherwise: 5, p: [0: 1, 2: 0, 3: 0, 4: 0, 5: 51]),
            .init(op: 6, next: 6, p: [0: 1, 2: 1, 4: 2, 5: 1]),
            .init(op: 2, next: 0),
        ], macros: macros) { e in
            #expect(vnneko_debug_variable(e, 1, 100) == 7)
            #expect(vnneko_debug_variable(e, 1, 101) == 1)
        }
    }

    // A scene end with parameter 0 set is the staff roll, after which the flow goes on: Test Game 94 sets its endings
    // after it. A game whose table has no staff roll goes straight on.
    // A condition that holds goes to its other target, as a choice's branch does: Test Game 84's `1:デス＆エス` leads
    // there to デストラ＆エスケルダを選んでた, and on to the next heroine's question otherwise.
    @Test func aConditionThatHoldsTakesItsOtherTarget() throws {
        try NekoSDKFlowFixture.run([
            .init(op: 0, next: 2),
            .init(op: 6, next: 3, p: [0: 1, 2: 0, 4: 2, 5: 2]),
            .init(op: 5, next: 4, otherwise: 5, p: [0: 1, 2: 0, 3: 0, 4: 2, 5: 1]),
            .init(op: 5, next: 6, otherwise: 7, p: [0: 1, 2: 0, 3: 0, 4: 2, 5: 2]),
            .init(op: 6, next: 8, p: [0: 1, 2: 1, 4: 2, 5: 1]),
            .init(op: 6, next: 8, p: [0: 1, 2: 1, 4: 2, 5: 2]),
            .init(op: 6, next: 8, p: [0: 1, 2: 1, 4: 2, 5: 3]),
            .init(op: 2, next: 0),
        ], macros: macros) { e in
            #expect(vnneko_debug_variable(e, 1, 101) == 3)
        }
    }

    @Test func theStoryGoesOnAfterAStaffRoll() throws {
        try NekoSDKFlowFixture.run([
            .init(op: 0, next: 2),
            .init(op: 2, next: 3, p: [0: 1]),
            .init(op: 6, next: 4, p: [0: 0, 1: 0, 4: 2, 5: 1]),
            .init(op: 2, next: 0),
        ], macros: macros) { e in
            #expect(vnneko_debug_variable(e, 0, 51) == 1)
        }
    }
}
