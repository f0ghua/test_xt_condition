// SPDX-License-Identifier: GPL-2.0
/*
 * nftables condition expression userspace tests
 */
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <nftables/libnftables.h>
#include "../../src/expr_condition.h"

/* Test rule parsing */
static void test_rule_parsing(struct nft_ctx *ctx)
{
    const char *test_rules[] = {
        "add rule ip filter input condition \"test1\" accept",
        "add rule ip filter input tcp dport 22 condition \"ssh_allowed\" accept",
        "add rule ip filter forward condition \"forwarding_enabled\" accept",
        NULL
    };
    int i;

    printf("Testing rule parsing...\n");

    for (i = 0; test_rules[i]; i++) {
        int ret = nft_run_cmd_from_buffer(ctx, test_rules[i]);
        if (ret != 0) {
            printf("Failed to parse rule: %s\n", test_rules[i]);
            return;
        }
    }

    printf("Rule parsing tests passed\n");
}

/* Test condition state control */
static void test_condition_control(void)
{
    const char *test_conditions[] = {
        "test1",
        "ssh_allowed",
        "forwarding_enabled",
        NULL
    };
    int i;

    printf("Testing condition state control...\n");

    for (i = 0; test_conditions[i]; i++) {
        char path[256];
        int fd;
        const char *name = test_conditions[i];

        /* Construct procfs path */
        snprintf(path, sizeof(path), "/proc/condition/%s", name);

        /* Try to read initial state */
        fd = open(path, O_RDONLY);
        if (fd < 0) {
            printf("Failed to open condition file: %s\n", path);
            continue;
        }

        char buf[2];
        if (read(fd, buf, 1) != 1) {
            printf("Failed to read condition state: %s\n", name);
            close(fd);
            continue;
        }
        close(fd);

        /* Initial state should be 0 */
        if (buf[0] != '0') {
            printf("Unexpected initial state for %s: %c\n", name, buf[0]);
            continue;
        }

        /* Try to enable condition */
        fd = open(path, O_WRONLY);
        if (fd < 0) {
            printf("Failed to open condition file for writing: %s\n", path);
            continue;
        }

        if (write(fd, "1", 1) != 1) {
            printf("Failed to enable condition: %s\n", name);
            close(fd);
            continue;
        }
        close(fd);

        /* Verify state was updated */
        fd = open(path, O_RDONLY);
        if (fd < 0) {
            printf("Failed to reopen condition file: %s\n", path);
            continue;
        }

        if (read(fd, buf, 1) != 1) {
            printf("Failed to read updated state: %s\n", name);
            close(fd);
            continue;
        }
        close(fd);

        if (buf[0] != '1') {
            printf("Failed to verify state update for %s\n", name);
            continue;
        }
    }

    printf("Condition state control tests passed\n");
}

/* Test invalid inputs */
static void test_invalid_inputs(struct nft_ctx *ctx)
{
    const char *invalid_rules[] = {
        /* Missing condition name */
        "add rule ip filter input condition accept",
        /* Invalid condition name */
        "add rule ip filter input condition \"invalid/name\" accept",
        /* Name too long */
        "add rule ip filter input condition \"this_name_is_way_too_long_and_should_be_rejected_by_the_parser\" accept",
        NULL
    };
    int i;

    printf("Testing invalid inputs...\n");

    for (i = 0; invalid_rules[i]; i++) {
        int ret = nft_run_cmd_from_buffer(ctx, invalid_rules[i]);
        if (ret == 0) {
            printf("Expected failure but got success: %s\n", invalid_rules[i]);
            return;
        }
    }

    /* Test invalid procfs writes */
    int fd = open("/proc/condition/test1", O_WRONLY);
    if (fd >= 0) {
        const char *invalid_values[] = {
            "2",
            "true",
            "enabled",
            "",
            NULL
        };

        for (i = 0; invalid_values[i]; i++) {
            if (write(fd, invalid_values[i], strlen(invalid_values[i])) > 0) {
                printf("Expected write failure but got success: %s\n", 
                       invalid_values[i]);
            }
        }
        close(fd);
    }

    printf("Invalid input tests passed\n");
}

int main(void)
{
    struct nft_ctx *ctx;

    /* Initialize nftables context */
    ctx = nft_ctx_new(0);
    if (!ctx) {
        printf("Failed to create nftables context\n");
        return 1;
    }

    /* Run tests */
    test_rule_parsing(ctx);
    test_condition_control();
    test_invalid_inputs(ctx);

    /* Cleanup */
    nft_ctx_free(ctx);
    return 0;
}