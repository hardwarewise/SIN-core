#!/usr/bin/env python3
# Copyright (c) 2017-2018 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test basic validation checks on a simulated IN chain"""
# Imports should be in PEP8 ordering (std library first, then third party
# libraries then local imports).
from collections import defaultdict

# Avoid wildcard * imports if possible
from test_framework.blocktools import (create_block, create_coinbase)
from test_framework.messages import CInv
from test_framework.mininode import (
    P2PInterface,
    mininode_lock,
    msg_block,
    msg_getdata,
)
from test_framework.test_framework import SinTestFramework
from test_framework.util import (
    assert_equal,
    connect_nodes,
    wait_until,
)

class INSetupTest(SinTestFramework):

    def set_test_params(self):
        """Override test parameters for your individual test.

        This method must be overridden and num_nodes must be exlicitly set."""
        self.setup_clean_chain = True
        self.num_nodes = 12
        # Use self.extra_args to change command-line arguments for the nodes
        #self.extra_args = [[], ["-logips"], []]
        self.enable_mocktime()

    def run_test(self):
        """Main test logic"""
        self.nodes[0].add_p2p_connection(BaseNode())

        self.setup_infinitynode_params(3)

        self.log.info("Deterministic IN setup starting")

        self.init_infinitynodes(True)

        self.nodes[0].generate(20)

        self.log.info("Deterministic IN setup concluded successfully")

if __name__ == '__main__':
    INSetupTest().main()
