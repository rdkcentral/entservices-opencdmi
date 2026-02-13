# OpenCDMi Plugin Architecture

## Overview

The OpenCDMi (Open Content Decryption Module interface) plugin is a WPEFramework component that provides content decryption and Digital Rights Management (DRM) capabilities for RDK devices. It serves as a standardized interface between media applications and various DRM systems (PlayReady, Widevine, ClearKey) enabling secure playback of premium encrypted content across different platforms.

## System Architecture

### Component Layers

```
┌─────────────────────────────────────────────────────┐
│          Application Layer (JSON-RPC)               │
│    (Media Players, Streaming Applications)          │
└────────────────┬────────────────────────────────────┘
                 │ JSON-RPC API
┌────────────────▼────────────────────────────────────┐
│            OCDM Plugin (In-Process)                 │
│  ┌─────────────────────────────────────────────┐   │
│  │  OCDM.cpp/h (JSONRPC Handler)              │   │
│  │  - Session management API                   │   │
│  │  - Key status reporting                     │   │
│  │  - DRM system enumeration                   │   │
│  │  - Event notification routing               │   │
│  └──────────────┬──────────────────────────────┘   │
│                 │ C++ Interface (IContentDecryption)│
│  ┌──────────────▼──────────────────────────────┐   │
│  │  OCDMImpl (Out-of-Process)                  │   │
│  │  - License acquisition                      │   │
│  │  - Session lifecycle management             │   │
│  │  - Key system integration                   │   │
│  │  - CENC metadata parsing                    │   │
│  └──────────────┬──────────────────────────────┘   │
└─────────────────┼──────────────────────────────────┘
                  │ OCDM Library Interface
┌─────────────────▼──────────────────────────────────┐
│           libocdm.so (OCDM Framework)              │
│  - DRM Key System Abstraction                      │
│  - CDM (Content Decryption Module) Loading         │
│  - Secure Buffer Management                        │
└────────────────┬───────────────────────────────────┘
                 │ CDM Interface
┌────────────────▼───────────────────────────────────┐
│       DRM-Specific CDMs                            │
│  - PlayReady CDM (Microsoft)                       │
│  - Widevine CDM (Google)                           │
│  - ClearKey CDM (W3C Standard)                     │
└────────────────┬───────────────────────────────────┘
                 │ Hardware Security
┌────────────────▼───────────────────────────────────┐
│    Hardware Security Module (TEE/Secure World)     │
│  - Secure Key Storage                              │
│  - Encrypted Media Path                            │
│  - Content Decryption Operations                   │
└────────────────────────────────────────────────────┘
```

## Core Components

### 1. OCDM Plugin (OCDM.cpp/h)
**Responsibility**: Thunder framework integration and JSON-RPC API exposure

- Implements `PluginHost::IPlugin` and `PluginHost::JSONRPC` interfaces
- Manages plugin lifecycle and out-of-process communication
- Exposes JSON-RPC methods for session management
- Routes DRM events and key status changes to applications
- Provides system-level DRM information (supported key systems, versions)

**Key Features**:
- DRM system enumeration and capability querying
- Session creation and destruction
- License request/response handling
- Key status monitoring and reporting
- Secure stop token management

### 2. CENCParser (CENCParser.cpp/h)
**Responsibility**: Common Encryption (CENC) metadata parsing

- Parses PSSH (Protection System Specific Header) boxes from media streams
- Extracts key IDs and initialization vectors
- Identifies DRM system types (PlayReady, Widevine, ClearKey)
- Validates encryption metadata structure
- Provides unified interface for multi-DRM content

**Supported DRM Systems**:
- **Common Encryption**: Standard CENC format (ISO/IEC 23001-7)
- **PlayReady**: Microsoft DRM with PSSH support
- **Widevine**: Google DRM with Widevine-specific headers
- **ClearKey**: W3C ClearKey for testing and basic encryption

### 3. FrameworkRPC (FrameworkRPC.cpp)
**Responsibility**: RPC communication and CDM integration

- Bridges Thunder RPC infrastructure with OCDM library
- Manages secure channel establishment with out-of-process CDM
- Implements `IContentDecryption` interface for DRM operations
- Handles asynchronous license acquisition callbacks
- Maintains session state across process boundaries

### 4. Module Infrastructure (Module.cpp/h)
**Responsibility**: Plugin module definition and initialization

- Defines plugin metadata and dependencies
- Configures tracing and logging subsystems
- Manages plugin registration with Thunder framework
- Provides type definitions for RPC interfaces

## Data Flow

### License Acquisition Flow
1. Application initiates playback of encrypted content
2. Media player extracts PSSH box from media initialization segment
3. Application calls `createSession` with PSSH data via JSON-RPC
4. OCDM plugin creates session in out-of-process CDM
5. CENCParser extracts key system information and key IDs
6. CDM generates license request message
7. License request returned to application
8. Application sends request to license server (external HTTP call)
9. License response provided back via `updateSession`
10. CDM processes license, extracts keys, stores in secure storage
11. Keys become available for decryption
12. Key status event fired to application indicating readiness

### Content Decryption Flow
1. Encrypted media samples arrive at decoder
2. Decoder extracts encryption metadata (IV, subsample info)
3. Decoder requests decryption via OCDM session handle
4. OCDM routes request to appropriate CDM
5. CDM performs secure decryption using stored keys
6. Decrypted samples provided to decoder via secure buffer
7. Content plays out without exposing clear stream

## Security Architecture

### Process Isolation
- Plugin runs in Thunder framework process (in-process)
- Actual CDM implementation runs out-of-process for security isolation
- RPC communication via Thunder's secure IPC mechanism
- Crash in CDM does not affect main framework

### Key Protection
- Keys never exposed to application layer
- Key storage in hardware-backed TEE (Trusted Execution Environment)
- Decryption operations performed in secure world
- Clear content accessible only within secure pipeline

### Encrypted Media Path
- End-to-end encryption from network to display
- Hardware-enforced content protection (HDCP)
- Secure video path prevents screen capture
- Memory protection for decrypted buffers

## Configuration

### Plugin Configuration (OCDM.config)
```json
{
  "locator": "libocdm.so",
  "mode": "Local|Off",
  "systems": [
    {
      "name": "PlayReady",
      "designators": ["com.microsoft.playready", "playready"]
    },
    {
      "name": "Widevine",
      "designators": ["com.widevine.alpha", "widevine"]
    },
    {
      "name": "ClearKey",
      "designators": ["org.w3.clearkey", "clearkey"]
    }
  ]
}
```

### Runtime Configuration
- **Mode**: Controls process execution model (Local for out-of-process, Off for disabled)
- **Locator**: Path to OCDM library implementation
- **Systems**: Maps DRM key system strings to CDM implementations
- **Designators**: Alternative names for key system identification

## Integration Points

### Thunder Framework
- Leverages Thunder's plugin architecture and lifecycle management
- Uses Thunder RPC for inter-process communication
- Integrates with Thunder's security and monitoring infrastructure
- Supports Thunder's configuration and tracing systems

### Media Pipeline
- Integrated with GStreamer via gst-plugins-thunder
- Works with HTML5 EME (Encrypted Media Extensions) via WPE WebKit
- Compatible with media frameworks supporting OCDM interface
- Supports clear lead (unencrypted start) and fully encrypted content

### External Services
- Applications handle license server communication (HTTP/HTTPS)
- Supports custom license server protocols and authentication
- Enables integration with operator-specific DRM workflows
- Compatible with multi-CDN and multi-DRM architectures

## Performance Considerations

- Out-of-process design trades slight latency for security and stability
- Efficient RPC minimizes overhead for high-bitrate content
- Hardware acceleration utilized for cryptographic operations
- Bulk key provisioning reduces license acquisition frequency
- Session pooling reduces setup latency for rapid channel changes
