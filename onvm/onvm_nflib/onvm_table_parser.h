/*********************************************************************
 *                     openNetVM
 *              https://sdnfv.github.io
 *
 *   BSD LICENSE
 *
 *   Copyright(c)
 *            2015-2021 George Washington University
 *            2015-2021 University of California Riverside
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * The name of the author may not be used to endorse or promote
 *       products derived from this software without specific prior
 *       written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * onvm_table_parser.h - a agnostic flow table parser.
 *
 * This file provides a generic API to parse formatted .txt files
 * and adds multiple ipv4 tuple keys to a flow table.
 ********************************************************************/


#include "onvm_flow_table.h"
#include <rte_lpm.h>

#define COMMENT_LEAD_CHAR	('#')

/*
 * This struct extends the onvm_ft struct to support longest-prefix match.
 * The depth or length of the rule is the number of bits of the rule that is stored in a specific entry.
 * Range is from 1-32.
 */
struct onvm_parser_ipv4_5tuple {
	struct onvm_ft_ipv4_5tuple key;
	uint32_t src_addr_depth;
	uint32_t dst_addr_depth;
};
/* Struct that holds info about each flow, and is stored at each flow table entry.
    Dest stores the destination NF or action to take.
 */
struct data {
    uint8_t  dest;
};

const char cb_port_delim[] = ":";

enum {
	CB_FLD_SRC_ADDR,
	CB_FLD_DST_ADDR,
	CB_FLD_SRC_PORT_DLM,
	CB_FLD_SRC_PORT,
	CB_FLD_DST_PORT,
	CB_FLD_DST_PORT_DLM,
	CB_FLD_PROTO,
	CB_FLD_DEST,
	CB_FLD_NUM,
};

enum {
	ONVM_TABLE_EM,
	ONVM_TABLE_LPM,
};

int
get_cb_field(char **in, uint32_t *fd, int base, unsigned long lim,
		char dlm) {
	unsigned long val;
	char *end;

	errno = 0;
	val = strtoul(*in, &end, base);
	if (errno != 0 || end[0] != dlm || val > lim)
		return -EINVAL;
	*fd = (uint32_t)val;
	*in = end + 1;
	return 0;
}

int
parse_ipv4_net(char *in, uint32_t *addr, uint32_t *depth) {
	uint32_t a, b, c, d, m;

	if (get_cb_field(&in, &a, 0, UINT8_MAX, '.'))
		return -EINVAL;
	if (get_cb_field(&in, &b, 0, UINT8_MAX, '.'))
		return -EINVAL;
	if (get_cb_field(&in, &c, 0, UINT8_MAX, '.'))
		return -EINVAL;
	if (get_cb_field(&in, &d, 0, UINT8_MAX, '/'))
		return -EINVAL;
	if (get_cb_field(&in, &m, 0, sizeof(uint32_t) * CHAR_BIT, 0))
		return -EINVAL;
	addr[0] = RTE_IPV4(a, b, c, d);
	depth[0] = m;
	return 0;
}

/* This function fills the key and return the destination or action to be stored in the table entry.*/
int
parse_ipv4_5tuple_rule(char *str, struct onvm_parser_ipv4_5tuple *ipv4_tuple) {
	int i, ret;
	char *s, *sp, *in[CB_FLD_NUM];
	static const char *dlm = " \t\n";
	int dim = CB_FLD_NUM;
	uint32_t temp;

	struct onvm_ft_ipv4_5tuple *key = &ipv4_tuple->key;
	s = str;
	for (i = 0; i != dim; i++, s = NULL) {
		in[i] = strtok_r(s, dlm, &sp);
		if (in[i] == NULL)
			return -EINVAL;
	}

	ret = parse_ipv4_net(in[CB_FLD_SRC_ADDR],
			&key->src_addr, &ipv4_tuple->src_addr_depth);

	ret = parse_ipv4_net(in[CB_FLD_DST_ADDR],
			&key->dst_addr, &ipv4_tuple->dst_addr_depth);

	if (strncmp(in[CB_FLD_SRC_PORT_DLM], cb_port_delim,
			sizeof(cb_port_delim)) != 0)
		return -EINVAL;

	if (get_cb_field(&in[CB_FLD_SRC_PORT], &temp, 0, UINT16_MAX, 0))
		return -EINVAL;
	key->src_port = (uint16_t)temp;

	if (get_cb_field(&in[CB_FLD_DST_PORT], &temp, 0, UINT16_MAX, 0))
		return -EINVAL;
	key->dst_port = (uint16_t)temp;

	if (strncmp(in[CB_FLD_SRC_PORT_DLM], cb_port_delim,
			sizeof(cb_port_delim)) != 0)
		return -EINVAL;

	if (get_cb_field(&in[CB_FLD_PROTO], &temp, 0, UINT8_MAX, 0))
		return -EINVAL;
	key->proto = (uint8_t)temp;

	if (get_cb_field(&in[CB_FLD_DEST], &temp, 0, UINT16_MAX, 0))
		return -EINVAL;

	// Return the destination or action to be performed by the NF.
	return (uint16_t)temp;
}

/* Bypass comment and empty lines */
int
is_bypass_line(char *buff) {
	int i = 0;

	/* comment line */
	if (buff[0] == COMMENT_LEAD_CHAR)
		return 1;
	/* empty line */
	while (buff[i] != '\0') {
		if (!isspace(buff[i]))
			return 0;
		i++;
	}
	return 1;
}

/*
 * This function takes in a file name and parses the file contents to
 * add custom flows to the the flow table passed in. If print_keys is true,
 * print each key that has been added to the flow table. Currently
 * hash and lpm is suppoerted.
 */
int
add_rules(void * tbl, const char *rule_path, uint8_t print_keys, int table_type) {
	FILE *fh;
	char buff[LINE_MAX];
	unsigned int i = 0;
	unsigned int total_num = 0;
	struct onvm_parser_ipv4_5tuple ipv4_tuple;
	int ret;
	fh = fopen(rule_path, "rb");
	if (fh == NULL)
		rte_exit(EXIT_FAILURE, "%s: fopen %s failed\n", __func__,
			rule_path);

	ret = fseek(fh, 0, SEEK_SET);
	if (ret)
		rte_exit(EXIT_FAILURE, "%s: fseek %d failed\n", __func__,
			ret);
	i = 0;
	while (fgets(buff, LINE_MAX, fh) != NULL) {
		i++;

		if (is_bypass_line(buff))
			continue;

        uint8_t dest = parse_ipv4_5tuple_rule(buff, &ipv4_tuple);
		if (dest < 0)
			rte_exit(EXIT_FAILURE,
				"%s Line %u: parse rules error\n",
				rule_path, i);

		struct data *data = NULL;
		int tbl_index = -EINVAL;
		if (table_type == ONVM_TABLE_EM) {
			tbl_index = onvm_ft_add_key((struct onvm_ft*)tbl, &ipv4_tuple.key, (char **)&data);
		} else if (table_type == ONVM_TABLE_LPM) {
			// Adds to the lpm table using the src ip adress.
			tbl_index = rte_lpm_add((struct rte_lpm *)tbl, ipv4_tuple.key.src_addr, ipv4_tuple.src_addr_depth, dest);
		}
		data->dest = dest;
        if (tbl_index < 0)
            rte_exit(EXIT_FAILURE, "Unable to add entry %u\n", i);
        if (print_keys) {
            printf("\nAdding key:");
            _onvm_ft_print_key(&ipv4_tuple.key);
        }
	}

	fclose(fh);
	return 0;
}
