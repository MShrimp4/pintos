# -*- perl -*-
use strict;
use warnings;
use tests::tests;
check_expected ([<<'EOF']);
(priority-donate-chain-autorelease) begin
(priority-donate-chain-autorelease) fake-main got lock.
(priority-donate-chain-autorelease) fake-main should have priority 3.  Actual priority: 3.
(priority-donate-chain-autorelease) fake-main should have priority 6.  Actual priority: 6.
(priority-donate-chain-autorelease) fake-main should have priority 9.  Actual priority: 9.
(priority-donate-chain-autorelease) fake-main should have priority 12.  Actual priority: 12.
(priority-donate-chain-autorelease) fake-main should have priority 15.  Actual priority: 15.
(priority-donate-chain-autorelease) fake-main should have priority 18.  Actual priority: 18.
(priority-donate-chain-autorelease) fake-main should have priority 21.  Actual priority: 21.
(priority-donate-chain-autorelease) thread 1 got lock
(priority-donate-chain-autorelease) thread 1 should have priority 21. Actual priority: 21
(priority-donate-chain-autorelease) thread 2 got lock
(priority-donate-chain-autorelease) thread 2 should have priority 21. Actual priority: 21
(priority-donate-chain-autorelease) thread 3 got lock
(priority-donate-chain-autorelease) thread 3 should have priority 21. Actual priority: 21
(priority-donate-chain-autorelease) thread 4 got lock
(priority-donate-chain-autorelease) thread 4 should have priority 21. Actual priority: 21
(priority-donate-chain-autorelease) thread 5 got lock
(priority-donate-chain-autorelease) thread 5 should have priority 21. Actual priority: 21
(priority-donate-chain-autorelease) thread 6 got lock
(priority-donate-chain-autorelease) thread 6 should have priority 21. Actual priority: 21
(priority-donate-chain-autorelease) thread 7 got lock
(priority-donate-chain-autorelease) thread 7 should have priority 21. Actual priority: 21
(priority-donate-chain-autorelease) interloper 7 finished.
(priority-donate-chain-autorelease) interloper 6 finished.
(priority-donate-chain-autorelease) interloper 5 finished.
(priority-donate-chain-autorelease) interloper 4 finished.
(priority-donate-chain-autorelease) interloper 3 finished.
(priority-donate-chain-autorelease) interloper 2 finished.
(priority-donate-chain-autorelease) interloper 1 finished.
(priority-donate-chain-autorelease) fake-main finished.
(priority-donate-chain-autorelease) end
EOF
pass;
