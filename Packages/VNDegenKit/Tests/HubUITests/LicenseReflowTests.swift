import Testing
@testable import HubUI

@Test func licenseParagraphsJoinTheirLineBreaksAndKeepStructure() {
    let text = """
    Mozilla Public License
    ======================

    1.1. "Contributor"
        means each individual or legal entity that creates, contributes to
        the creation of, or owns Covered Software.

    Redistribution and use are permitted provided that the following
    conditions are met:

      * Redistributions must retain the above copyright notice, this list
        of conditions and the following disclaimer.
      (a) a lettered item
    THIS SOFTWARE IS PROVIDED "AS IS", WHETHER IN CONTRACT OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY.
    """
    #expect(ThirdPartyLicense.reflow(text) == """
    Mozilla Public License
    ======================

    1.1. "Contributor" means each individual or legal entity that creates, contributes to the creation of, or owns Covered Software.

    Redistribution and use are permitted provided that the following conditions are met:

      * Redistributions must retain the above copyright notice, this list of conditions and the following disclaimer.
      (a) a lettered item
    THIS SOFTWARE IS PROVIDED "AS IS", WHETHER IN CONTRACT OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY.
    """)
}
