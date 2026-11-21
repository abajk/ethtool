/* SPDX-License-Identifier: GPL-2.0-only */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_NO_WRAPPERS
#include "internal.h"

static const struct cmd_expect *expect_next;

int send_ioctl(struct cmd_context *ctx __maybe_unused, void *cmd)
{
	int rc = test_ioctl(expect_next, cmd);

	if (rc == TEST_IOCTL_MISMATCH) {
		errno = EINVAL;
		return -1;
	}

	expect_next++;
	return rc;
}

#ifdef ETHTOOL_ENABLE_NETLINK
struct nl_socket;
struct nl_msg_buff;

ssize_t nlsock_sendmsg(struct nl_socket *nlsk __maybe_unused,
		       struct nl_msg_buff *altbuff __maybe_unused)
{
	fprintf(stderr, "Unexpected netlink request\n");
	errno = EINVAL;
	return -1;
}
#endif

int main(int argc, char **argv)
{
	struct ethtool_modinfo info_request = { .cmd = ETHTOOL_GMODULEINFO };
	struct ethtool_modinfo info = { .cmd = ETHTOOL_GMODULEINFO };
	struct ethtool_eeprom request = { .cmd = ETHTOOL_GMODULEEEPROM };
	struct ethtool_eeprom *response;
	struct cmd_expect steps[3] = {};
	char *raw_args[] = { "raw", "on", NULL };
	struct cmd_context ctx = {};
	size_t size;
	int rc;

	if (argc != 3) {
		fprintf(stderr, "Usage: %s MODULE_TYPE raw|text|json < DUMP\n", argv[0]);
		return 1;
	}

	if (!strcmp(argv[1], "sff8079")) {
		info.type = ETH_MODULE_SFF_8079;
		info.eeprom_len = ETH_MODULE_SFF_8079_LEN;
	} else if (!strcmp(argv[1], "sff8472")) {
		info.type = ETH_MODULE_SFF_8472;
		info.eeprom_len = ETH_MODULE_SFF_8472_LEN;
	} else {
		fprintf(stderr, "Unsupported module type: %s\n", argv[1]);
		return 1;
	}

	if (!strcmp(argv[2], "raw")) {
		ctx.argc = ARRAY_SIZE(raw_args) - 1;
		ctx.argp = raw_args;
	} else if (!strcmp(argv[2], "json")) {
		ctx.json = true;
	} else if (strcmp(argv[2], "text")) {
		fprintf(stderr, "Unsupported output format: %s\n", argv[2]);
		return 1;
	}

	request.len = info.eeprom_len;
	size = sizeof(*response) + request.len;
	response = malloc(size);
	if (!response) {
		perror("malloc");
		return 1;
	}
	*response = request;
	if (fread(response->data, 1, request.len, stdin) != request.len ||
	    getchar() != EOF || ferror(stdin)) {
		fprintf(stderr, "Expected exactly %u EEPROM bytes for %s\n",
			request.len, argv[1]);
		free(response);
		return 1;
	}

	steps[0] = (struct cmd_expect) {
		.cmd = &info_request,
		.cmd_len = sizeof(info_request.cmd),
		.resp = &info,
		.resp_len = sizeof(info),
	};
	steps[1] = (struct cmd_expect) {
		.cmd = &request,
		.cmd_len = sizeof(request),
		.resp = response,
		.resp_len = size,
	};

	expect_next = steps;
	rc = test_run_getmodule(&ctx);
	free(response);
	if (expect_next != &steps[2]) {
		fprintf(stderr, "Expected both module EEPROM ioctls\n");
		return 1;
	}
	return rc;
}
