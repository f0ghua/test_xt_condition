#!/bin/bash
# Integration tests for nftables condition module

# Exit on any error
set -e

# Test setup
setup() {
    echo "Setting up test environment..."
    
    # Load kernel module if not loaded
    if ! lsmod | grep -q "^nft_condition"; then
        modprobe nft_condition
    fi
    
    # Clear any existing rules
    nft flush ruleset
    
    # Create test table and chain
    nft add table ip test
    nft add chain ip test input { type filter hook input priority 0 \; }
    
    echo "Test environment ready"
}

# Test cleanup
cleanup() {
    echo "Cleaning up test environment..."
    
    # Remove test rules and tables
    nft flush ruleset
    nft delete table ip test
    
    echo "Cleanup complete"
}

# Verify procfs interface
test_procfs() {
    echo "Testing procfs interface..."
    
    # Create a rule with condition
    nft add rule ip test input condition "test_condition" accept
    
    # Check if procfs entry was created
    if [ ! -f "/proc/condition/test_condition" ]; then
        echo "ERROR: procfs entry not created"
        return 1
    fi
    
    # Check initial state (should be 0)
    if [ "$(cat /proc/condition/test_condition)" != "0" ]; then
        echo "ERROR: unexpected initial state"
        return 1
    fi
    
    # Test state changes
    echo 1 > /proc/condition/test_condition
    if [ "$(cat /proc/condition/test_condition)" != "1" ]; then
        echo "ERROR: state change failed"
        return 1
    fi
    
    echo "Procfs interface tests passed"
}

# Test rule evaluation
test_rule_evaluation() {
    echo "Testing rule evaluation..."
    
    # Create test rules
    nft add rule ip test input tcp dport 22 condition "ssh_access" accept
    nft add rule ip test input drop
    
    # Initial state (should block)
    if nc -zv localhost 22 2>/dev/null; then
        echo "ERROR: connection succeeded when condition disabled"
        return 1
    fi
    
    # Enable condition
    echo 1 > /proc/condition/ssh_access
    
    # Should allow connection
    if ! nc -zv localhost 22 2>/dev/null; then
        echo "ERROR: connection failed when condition enabled"
        return 1
    fi
    
    echo "Rule evaluation tests passed"
}

# Test concurrent operations
test_concurrent() {
    echo "Testing concurrent operations..."
    
    # Create test condition
    nft add rule ip test input condition "concurrent_test" accept
    
    # Run concurrent state changes
    for i in {1..10}; do
        (
            for j in {1..100}; do
                echo $((j % 2)) > /proc/condition/concurrent_test
                sleep 0.01
            done
        ) &
    done
    
    # Wait for all operations to complete
    wait
    
    echo "Concurrent operation tests passed"
}

# Test error handling
test_error_handling() {
    echo "Testing error handling..."
    
    # Test invalid condition names
    if nft add rule ip test input condition "invalid/name" accept 2>/dev/null; then
        echo "ERROR: accepted invalid condition name"
        return 1
    fi
    
    # Test too long names
    if nft add rule ip test input condition "this_name_is_way_too_long_and_should_be_rejected" accept 2>/dev/null; then
        echo "ERROR: accepted too long condition name"
        return 1
    fi
    
    # Test invalid procfs writes
    echo "invalid" > /proc/condition/concurrent_test 2>/dev/null && {
        echo "ERROR: accepted invalid procfs write"
        return 1
    }
    
    echo "Error handling tests passed"
}

# Main test execution
main() {
    echo "Starting integration tests..."
    
    setup
    
    # Run tests
    test_procfs
    test_rule_evaluation
    test_concurrent
    test_error_handling
    
    cleanup
    
    echo "All integration tests passed successfully"
}

# Run tests
main