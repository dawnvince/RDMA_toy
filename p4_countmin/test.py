################################################################################################
# BAREFOOT NETWORKS CONFIDENTIAL & PROPRIETARY
#
# Copyright (c) 2019-present Barefoot Networks, Inc.
#
# All Rights Reserved.
#
# NOTICE: All information contained herein is, and remains the property of
# Barefoot Networks, Inc. and its suppliers, if any. The intellectual and
# technical concepts contained herein are proprietary to Barefoot Networks, Inc.
# and its suppliers and may be covered by U.S. and Foreign Patents, patents in
# process, and are protected by trade secret or copyright law.  Dissemination of
# this information or reproduction of this material is strictly forbidden unless
# prior written permission is obtained from Barefoot Networks, Inc.
#
# No warranty, explicit or implicit is provided, unless granted under a written
# agreement with Barefoot Networks, Inc.
#
################################################################################

import logging
import random
import socket
import struct
import datetime
import json
import os
import sys

from ptf import config
import ptf.testutils as testutils
from bfruntime_client_base_tests import BfRuntimeTest
import bfrt_grpc.bfruntime_pb2 as bfruntime_pb2
import bfrt_grpc.client as gc
import grpc

class TeaTest(BfRuntimeTest):
    def setUp(self):
        client_id = 0
        self.p4_name = "tna_tea"
        BfRuntimeTest.setUp(self, client_id, self.p4_name)

    def runTest(self):
	    bfrt_info = self.interface.bfrt_info_get(self.p4_name);

        ## Initializing all info tables
        self.port_table             = bfrt_info.table_get("$PORT")
        self.port_stat_table        = bfrt_info.table_get("$PORT_STAT")
        self.port_hdl_info_table    = bfrt_info.table_get("$PORT_HDL_INFO")
        self.port_fp_idx_info_table = bfrt_info.table_get("$PORT_FP_IDX_INFO")
        self.port_str_info_table    = bfrt_info.table_get("$PORT_STR_INFO")

        