/*******************************************************************************
 * BAREFOOT NETWORKS CONFIDENTIAL & PROPRIETARY
 *
 * Copyright (c) 2019-present Barefoot Networks, Inc.
 *
 * All Rights Reserved.
 *
 * NOTICE: All information contained herein is, and remains the property of
 * Barefoot Networks, Inc. and its suppliers, if any. The intellectual and
 * technical concepts contained herein are proprietary to Barefoot Networks, Inc.
 * and its suppliers and may be covered by U.S. and Foreign Patents, patents in
 * process, and are protected by trade secret or copyright law.  Dissemination of
 * this information or reproduction of this material is strictly forbidden unless
 * prior written permission is obtained from Barefoot Networks, Inc.
 *
 * No warranty, explicit or implicit is provided, unless granted under a written
 * agreement with Barefoot Networks, Inc.
 *
 ******************************************************************************/

#include <core.p4>
#if __TARGET_TOFINO__ == 2
#include <t2na.p4>
#else
#include <tna.p4>
#endif

#include "common/headers.p4"
#include "common/util.p4"


struct metadata_t {}
struct hash_info_t {
	ipv4_addr_t src_addr;
    ipv4_addr_t dst_addr;
}

// ---------------------------------------------------------------------------
// Ingress parser
// ---------------------------------------------------------------------------

parser SwitchIngressParser(
		packet_in pkt,
		out header_t hdr,
		out metadata_t ig_md,
		out ingress_intrinsic_metadata_t ig_intr_md) {
	TofinoIngressParser() tofino_parser;

	state start {
		tofino_parser.apply(pkt, ig_intr_md);
		transition parse_ethernet;
	}

	state parse_ethernet{
		pkt.extract(hdr.ethernet);
		pkt.extract(hdr.ipv4);
		transition accept;
	}
}

// ---------------------------------------------------------------------------
// Ingress Deparser
// ---------------------------------------------------------------------------
control SwitchIngressDeparser(
        packet_out pkt,
        inout header_t hdr,
        in metadata_t ig_md,
        in ingress_intrinsic_metadata_for_deparser_t ig_dprsr_md) {

    apply {
        pkt.emit(hdr);
    }
}

control SwitchIngress(
		inout header_t hdr,
		inout metadata_t ig_md,
		in ingress_intrinsic_metadata_t ig_intr_md,
		in ingress_intrinsic_metadata_from_parser_t ig_prsr_md,
		inout ingress_intrinsic_metadata_for_deparser_t ig_dprsr_md,
		inout ingress_intrinsic_metadata_for_tm_t ig_tm_md) {

	const bit<32> table_size = 1 << 16;

	Register<bit<32>, bit<32>>(table_size, 32w0) sketch_1;
	Register<bit<32>, bit<32>>(table_size, 32w0) sketch_2;
	Register<bit<32>, bit<32>>(table_size, 32w0) sketch_3;

	RegisterAction<bit<32>, bit<32>, bit<32>>(sketch_1) sk1_action{
		void apply(inout bit<32> value, out bit<32> read_value){
			value = value + 1;
			read_value = value;
		}
	};
	RegisterAction<bit<32>, bit<32>, bit<32>>(sketch_2) sk2_action{
		void apply(inout bit<32> value, out bit<32> read_value){
			value = value + 1;
			read_value = value;
		}
	};
	RegisterAction<bit<32>, bit<32>, bit<32>>(sketch_3) sk3_action{
		void apply(inout bit<32> value, out bit<32> read_value){
			value = value + 1;
			read_value = value;
		}
	};

	CRCPolynomial<bit<16>>(16w0x1DB7, // polynomial
                           false,          // reversed
                           false,         // use msb?
                           false,         // extended?
                           16w0xFFFF, // initial shift register value
                           16w0xFFFF  // result xor
                           ) poly1;
    Hash<bit<16>>(HashAlgorithm_t.CUSTOM, poly1) hash1;

    CRCPolynomial<bit<16>>(16w0x7D91, 
                           false, 
                           false, 
                           false, 
                           16w0xFFFF,
                           16w0xFFFF
                           ) poly2;
    Hash<bit<16>>(HashAlgorithm_t.CUSTOM, poly2) hash2;

    CRCPolynomial<bit<16>>(16w0xA4C3, 
                           false, 
                           false, 
                           false, 
                           16w0xFFFF,
                           16w0xFFFF
                           ) poly3;
    Hash<bit<16>>(HashAlgorithm_t.CUSTOM, poly2) hash3;

    bit<16> hash_r1 = 0;
    bit<16> hash_r2 = 0;
    bit<16> hash_r3 = 0;
    bit<32> reg_r1 = 0;
    bit<32> reg_r2 = 0;
    bit<32> reg_r3 = 0;

    const bit<32> cm_threshold = 1 << 16;

    hash_info_t hash_info = {
    	hdr.ipv4.src_addr,
    	hdr.ipv4.dst_addr
    };

    action set_output_port(PortId_t port_id) {
        ig_tm_md.ucast_egress_port = port_id;
    }

    table output_port {
        actions = {
            set_output_port;
        }
    }

    action hash_match(){
    	hash_r1 = hash1.get(hash_info);
    	hash_r2 = hash2.get(hash_info);
    	hash_r3 = hash3.get(hash_info);
    }

    action update_reg(){
        reg_r1 = sk1_action.execute(hash_r1);
        reg_r2 = sk2_action.execute(hash_r2);
        reg_r3 = sk3_action.exxcute(hash_r3);
    }

    /// 不知道这里用 cmp(out bit<32> res) 行不行
    bit <32> res = 0;
    action cmp(){
        res = min<bit<32>>(reg_r1, reg_r2);
        res = min<bit<32>>(res, reg_r3);
        if(res >= cm_threshold){
            
        }
    }
    apply{
        output_port.apply();

    }

}