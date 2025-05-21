# NFTables Condition Module Specification

## 1. Module Components

### 1.1 User Space Components

```pseudocode
// User space component hierarchy
UserSpaceComponents = {
    ParserLayer: {
        LexicalParser: {
            - Tokenize input commands
            - Recognize keywords and literals
            - Track source positions
        },
        SyntaxParser: {
            - Validate token sequences
            - Build parse tree
            - Handle syntax errors
        },
        ASTBuilder: {
            - Construct AST nodes
            - Perform semantic validation
            - Apply syntax normalizations
        }
    },
    ExpressionLayer: {
        ExpressionBuilder: {
            - Convert AST to nftables expressions
            - Add type information
            - Prepare for serialization
        },
        NetlinkGenerator: {
            - Build Netlink message headers
            - Serialize expression data
            - Handle message fragmentation
        }
    }
}
```

### 1.2 Kernel Space Components

```pseudocode
// Kernel space component hierarchy
KernelComponents = {
    NFTExpressionHandler: {
        - Register condition expression type
        - Handle rule initialization/cleanup
        - Evaluate condition state
    },
    StateManager: {
        - Maintain condition states
        - Handle reference counting
        - Ensure thread safety
    },
    ProcfsManager: {
        - Manage /proc/condition/ directory
        - Handle file operations
        - Process state changes
    }
}
```

## 2. Data Structures

### 2.1 User Space Structures

```pseudocode
// Parser structures
struct token {
    enum token_type type;    // Token type identifier
    char *value;             // Token value
    size_t line;            // Source line number
    size_t column;          // Source column number
}

struct ast_node {
    enum node_type type;    // Node type
    struct ast_node *left;  // Left child
    struct ast_node *right; // Right child
    void *data;            // Node-specific data
}

// Expression structures
struct nft_expr_info {
    const char *type;       // Expression type
    void *data;            // Expression data
    unsigned int size;      // Data size
}

struct nft_condition_expr {
    char name[NFT_NAME_MAX]; // Condition name
    uint32_t flags;         // Expression flags
}
```

### 2.2 Kernel Space Structures

```pseudocode
struct condition_state {
    // Core state
    atomic_t enabled;              // 0 or 1
    char name[NAME_MAX];          // Condition name
    atomic_t refcount;            // Reference counter
    
    // Procfs integration
    struct proc_dir_entry *proc_entry;
    
    // List management 
    struct list_head list;        // For global state tracking
    struct mutex state_lock;      // Protects structural changes
}

// Global state management
struct condition_mgr {
    struct list_head conditions;  // List of all conditions
    struct mutex conditions_lock; // Protects list modifications
    struct proc_dir_entry *proc_dir; // /proc/condition directory
}

struct nft_condition {
    struct condition_state *state; // Points to shared state
    char name[NAME_MAX];          // Cached name for dumping
}
```

## 3. Interface Definitions

### 3.1 Parser Interfaces

```pseudocode
// Lexical parser interface
LexicalParserOps = {
    next_token: () => Token,
    peek_token: () => Token,
    error_location: () => Location,
    reset: () => void
}

// Syntax parser interface
SyntaxParserOps = {
    parse_condition: () => ASTNode,
    parse_error: () => ErrorInfo,
    recover: () => bool
}
```

### 3.2 Expression Interfaces

```pseudocode
// Expression builder interface
ExpressionBuilderOps = {
    build_condition: (ast: ASTNode) => nft_expr_info,
    validate: () => ErrorList,
    cleanup: () => void
}

// Netlink message interface
NetlinkOps = {
    build_message: (expr: nft_expr_info) => nlmsghdr,
    send_message: (msg: nlmsghdr) => int,
    receive_response: () => nlmsghdr
}
```

### 3.3 Kernel Module Interfaces

```pseudocode
struct nft_expr_ops condition_ops = {
    .type = "condition",
    .size = sizeof(struct nft_condition),
    .eval = nft_condition_eval,
    .init = nft_condition_init,
    .destroy = nft_condition_destroy,
    .dump = nft_condition_dump
}

struct proc_ops condition_fops = {
    .proc_read = condition_proc_read,
    .proc_write = condition_proc_write,
    .proc_open = condition_proc_open,
    .proc_release = condition_proc_release
}
```

## 4. Processing Flows

### 4.1 Rule Creation Flow

```pseudocode
RuleCreationFlow = {
    UserSpace: {
        1. LexicalParser tokenizes input
        2. SyntaxParser builds parse tree
        3. ASTBuilder creates AST
        4. ExpressionBuilder creates nft_expr_info
        5. NetlinkGenerator sends to kernel
    },
    KernelSpace: {
        1. Validate condition name
        2. Allocate/find condition state
        3. Create procfs entry
        4. Initialize expression
        5. Return success/failure
    }
}
```

### 4.2 State Management Flow

```pseudocode
StateManagementFlow = {
    Creation: {
        1. Check name validity
        2. Allocate state structure
        3. Initialize atomic fields
        4. Setup procfs entry
        5. Add to global list
    },
    
    Update: {
        1. Validate new value
        2. Acquire state lock
        3. Update atomic state
        4. Release lock
        5. Notify listeners
    },
    
    Cleanup: {
        1. Remove procfs entry
        2. Wait for references
        3. Remove from list
        4. Free memory
    }
}
```

## 5. Error Handling

### 5.1 User Space Errors

```pseudocode
UserSpaceErrors = {
    Parsing: {
        - Invalid syntax
        - Unknown keywords
        - Name validation
    },
    Expression: {
        - Type mismatches
        - Invalid parameters
        - Resource limits
    },
    Netlink: {
        - Communication failures
        - Message size limits
        - Timeout handling
    }
}
```

### 5.2 Kernel Space Errors

```pseudocode
KernelSpaceErrors = {
    Initialization: {
        - Memory allocation
        - Name conflicts
        - Resource limits
    },
    Runtime: {
        - Invalid state transitions
        - Reference counting
        - Lock failures
    },
    Cleanup: {
        - Resource leaks
        - Deadlock prevention
        - Error propagation
    }
}
```

## 6. Security Requirements

```pseudocode
SecurityRequirements = {
    AccessControl: {
        - Root-only rule creation
        - Configurable state modifications
        - Read permissions for all
    },
    
    ResourceProtection: {
        - Memory usage limits
        - File descriptor quotas
        - CPU usage bounds
    },
    
    InputValidation: {
        - Name length limits
        - Character set restrictions
        - Format verification
    }
}
```

## 7. Testing Requirements

### 7.1 Functional Tests

```pseudocode
FunctionalTests = {
    ParserTests: {
        - Keyword recognition
        - String handling
        - Error recovery
    },
    
    StateTests: {
        - Creation/deletion
        - Value updates
        - Concurrent access
    },
    
    IntegrationTests: {
        - Rule evaluation
        - State persistence
        - Error handling
    }
}
```

### 7.2 Performance Tests

```pseudocode
PerformanceTests = {
    Benchmarks: {
        - Rule evaluation latency (target: < 1μs)
        - Concurrent operations (target: 10k/s)
        - Memory usage (target: < 4KB/condition)
    },
    
    LoadTests: {
        - High concurrency
        - Resource exhaustion
        - Error conditions
    }
}
```

This specification has been updated to align with the architecture document, particularly in areas of:
1. User space component details
2. Parser and expression handling
3. Detailed interface definitions
4. Comprehensive error handling
5. Security considerations
6. Testing requirements

The specification now provides concrete implementation details while maintaining consistency with the architectural design decisions.