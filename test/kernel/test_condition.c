// SPDX-License-Identifier: GPL-2.0
/*
 * nftables condition expression tests
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/netfilter.h>
#include <linux/netfilter/nf_tables.h>
#include "../../src/condition_state.h"

/* Test basic state management */
static void test_state_basic(void)
{
    struct condition_state *state;
    const char *name = "test_condition";
    
    /* Test state creation */
    state = condition_state_get(name);
    if (IS_ERR(state)) {
        pr_err("Failed to create condition state\n");
        return;
    }

    /* Test initial state */
    if (condition_state_enabled(state)) {
        pr_err("New state should be disabled\n");
        goto cleanup;
    }

    /* Test state modification */
    atomic_set(&state->enabled, 1);
    if (!condition_state_enabled(state)) {
        pr_err("State should be enabled\n");
        goto cleanup;
    }

    /* Test reference counting */
    struct condition_state *state2 = condition_state_get(name);
    if (IS_ERR(state2)) {
        pr_err("Failed to get existing state\n");
        goto cleanup;
    }
    
    /* Should get same state object */
    if (state != state2) {
        pr_err("Got different state object\n");
        condition_state_put(state2);
        goto cleanup;
    }

    /* Release references */
    condition_state_put(state2);
    
cleanup:
    condition_state_put(state);
}

/* Test concurrent access */
static void test_state_concurrent(void)
{
    struct condition_state *state;
    const char *name = "test_concurrent";
    int i;
    
    /* Create initial state */
    state = condition_state_get(name);
    if (IS_ERR(state)) {
        pr_err("Failed to create condition state\n");
        return;
    }

    /* Simulate concurrent access */
    for (i = 0; i < 1000; i++) {
        /* Toggle state */
        atomic_set(&state->enabled, i % 2);
        
        /* Verify atomic read */
        if (condition_state_enabled(state) != (i % 2)) {
            pr_err("State inconsistency detected\n");
            goto cleanup;
        }

        /* Verify procfs reflects state */
        if (state->proc) {
            char buf[2];
            if (kernel_read(state->proc->file, buf, 1, 0) != 1) {
                pr_err("Failed to read procfs\n");
                goto cleanup;
            }
            if ((buf[0] == '1') != condition_state_enabled(state)) {
                pr_err("Procfs inconsistency detected\n");
                goto cleanup;
            }
        }
    }

cleanup:
    condition_state_put(state);
}

/* Test cleanup handling */
static void test_state_cleanup(void)
{
    struct condition_state *states[5];
    const char *names[] = {
        "test1", "test2", "test3", "test4", "test5"
    };
    int i;

    /* Create multiple states */
    for (i = 0; i < 5; i++) {
        states[i] = condition_state_get(names[i]);
        if (IS_ERR(states[i])) {
            pr_err("Failed to create state %d\n", i);
            goto cleanup;
        }
    }

    /* Release states in reverse order */
    for (i = 4; i >= 0; i--) {
        condition_state_put(states[i]);
        states[i] = NULL;
    }

    return;

cleanup:
    while (--i >= 0)
        condition_state_put(states[i]);
}

static int __init test_condition_init(void)
{
    pr_info("Starting condition expression tests\n");

    test_state_basic();
    test_state_concurrent();
    test_state_cleanup();

    pr_info("Condition expression tests completed\n");
    return 0;
}

static void __exit test_condition_exit(void)
{
    pr_info("Cleaning up condition expression tests\n");
}

module_init(test_condition_init);
module_exit(test_condition_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name <your.email@example.com>");
MODULE_DESCRIPTION("nftables condition expression tests");