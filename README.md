# NFTables Condition Module

A module for nftables that adds a new `condition` expression type for dynamic rule control through procfs.

## Overview

The condition module allows dynamic enabling/disabling of nftables rules through procfs entries. Each condition rule references a named state that can be toggled through `/proc/condition/<name>`.

### Example Usage

```nft
# Allow SSH access only when the condition is enabled
add rule ip filter input tcp dport 22 condition "ssh_allowed" accept

# Control the condition through procfs
echo 1 > /proc/condition/ssh_allowed  # Enable SSH access
echo 0 > /proc/condition/ssh_allowed  # Disable SSH access
```

## Implementation

The module consists of the following components:

### Kernel Space Components

1. **Expression Handler** (`src/nft_expr_condition.c`)
   - Implements the nftables expression interface
   - Handles condition initialization and evaluation
   - Integrates with nftables core

2. **State Management** (`src/condition_state.c`, `src/condition_state.h`)
   - Manages condition states using atomic operations
   - Implements procfs interface
   - Handles reference counting and cleanup

### User Space Components

1. **Expression Support** (`src/expr_condition.c`)
   - Implements expression parsing and serialization
   - Handles netlink message construction
   - Provides user space expression operations

2. **Parser Integration** (patches)
   - Adds lexical scanner support for `condition` keyword
   - Implements grammar rules for condition expressions

## Features

- Atomic state operations for thread safety
- Efficient expression evaluation
- Dynamic procfs interface
- Reference counted state management
- Comprehensive error handling

## Building

Requirements:
- Linux kernel headers
- nftables development files
- libnftnl development files

```bash
# Build kernel module
make -C /lib/modules/$(uname -r)/build M=$(pwd)/src modules

# Build user space components
make
```

## Testing

The module includes three levels of testing:

1. Kernel Module Tests (`test/kernel/`)
   ```bash
   make -C /lib/modules/$(uname -r)/build M=$(pwd)/test/kernel modules
   insmod test_condition.ko
   ```

2. User Space Tests (`test/userspace/`)
   ```bash
   make -C test/userspace
   ./test_condition
   ```

3. Integration Tests (`test/integration/`)
   ```bash
   sudo ./test_condition_integration.sh
   ```

## Security

- Root privileges required for condition creation
- Atomic operations prevent race conditions
- Resource limits enforced
- Input validation for condition names

## Performance

- Fast path evaluation using atomic operations
- No file I/O during rule matching
- Minimal memory footprint
- Efficient state caching

## License

This module is licensed under GPL-2.0.

## Author

Your Name <your.email@example.com>