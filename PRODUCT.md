# OpenCDMi Plugin - Product Overview

## Introduction

The OpenCDMi (Open Content Decryption Module interface) plugin is a production-ready WPEFramework component that delivers enterprise-grade Digital Rights Management (DRM) capabilities to RDK-powered devices. It enables secure playback of premium encrypted content from major streaming services and content providers by providing a unified interface to industry-standard DRM systems including Microsoft PlayReady, Google Widevine, and W3C ClearKey. Built with security, performance, and multi-DRM flexibility as core principles, OpenCDMi is the foundation for protected content delivery on millions of deployed devices.

## Key Features

### Multi-DRM Support
- **PlayReady**: Microsoft's enterprise DRM solution supporting HLS and DASH streaming with hardware-backed security
- **Widevine**: Google's DRM system with L1 (hardware) and L3 (software) security levels for YouTube, Netflix, and premium services
- **ClearKey**: W3C standard clear-key encryption for testing, development, and basic content protection
- Simultaneous multi-DRM support enables seamless switching between content providers without reconfiguration
- Automatic DRM system detection from PSSH (Protection System Specific Header) in media streams

### Secure Content Pipeline
- End-to-end hardware-enforced content protection from network reception to display output
- Integration with Trusted Execution Environment (TEE) for secure key storage and decryption operations
- HDCP (High-bandwidth Digital Content Protection) enforcement for protected HDMI output
- Secure memory buffers prevent unauthorized access to decrypted content
- Anti-screen-capture and anti-recording protection for premium content

### Standards Compliance
- **W3C EME (Encrypted Media Extensions)**: Full HTML5 DRM specification compliance for web-based applications
- **ISO CENC (Common Encryption)**: Support for ISO/IEC 23001-7 standard encryption format
- **DASH and HLS**: Compatible with encrypted MPEG-DASH and Apple HLS adaptive streaming
- **Thunder Framework Integration**: Native plugin architecture with JSON-RPC and C++ APIs

### Session Management
- Concurrent multi-session support for picture-in-picture and multi-window scenarios
- Persistent license storage for offline playback and rental content
- Session renewal and key rotation during long-form content playback
- Graceful session cleanup and resource management
- Secure stop tokens for rental and pay-per-view business models

### License Acquisition
- Flexible license request generation with customizable challenge data
- Support for license server protocols including LA_URL and custom HTTP workflows
- License response parsing and key extraction with error handling
- Pre-provisioning support for fast channel change and reduced startup latency
- Individualization and device binding for enhanced security

## Use Cases

### Premium Streaming Services
A subscriber launches their favorite streaming app (Netflix, Disney+, Prime Video) on their RDK-powered set-top box:
1. Application requests playback of encrypted movie
2. OpenCDMi creates DRM session using service's preferred DRM system (Widevine/PlayReady)
3. License request generated and sent to content provider's license server
4. License acquired with content keys and usage rules (HD/4K, expiration, output protection)
5. Secure hardware decryption enables smooth playback with full resolution
6. Keys automatically refreshed for multi-hour movies without interruption

**Benefits**: Secure premium content delivery, support for all major streaming services, full HD/4K resolution with hardware protection.

### Live Sports and Events
Viewer watches live sporting event with strict content protection requirements:
1. Live stream delivered as encrypted HLS or DASH segments
2. Short license duration (minutes) enforced for live content
3. Keys automatically rotated during event without playback interruption
4. HDCP verified before allowing HD/4K output
5. Recording prevention enforced via usage rules

**Benefits**: Real-time content protection, prevents unauthorized redistribution, supports premium live event monetization.

### Rental and Transactional VOD
User rents a movie for 48-hour viewing period:
1. Initial license request includes rental token and payment verification
2. Persistent license stored locally with expiration time
3. Movie plays immediately without additional server contact
4. Secure stop token generated when user stops playback
5. Token sent to server to update rental status and enable playback on another device
6. License expires automatically after rental period

**Benefits**: Flexible business models (rental, purchase, subscription), offline viewing support, accurate usage tracking, fraud prevention.

### Testing and Development
Developer builds and tests new streaming application:
1. Uses ClearKey DRM for rapid development cycle
2. Tests DRM integration without commercial license server
3. Validates EME API usage and session lifecycle
4. Debugs encryption metadata and PSSH parsing
5. Transitions to production DRM (Widevine/PlayReady) without application changes

**Benefits**: Accelerated development, reduced costs during testing, simplified debugging, production-ready code path.

### Multi-Device Household
Family streams content across multiple devices simultaneously:
1. Each device creates independent DRM sessions with same service
2. Device-specific licenses issued with appropriate security levels (L1 hardware on TV, L3 software on tablet)
3. Playback quality adjusted per device capability (4K on main TV, HD on mobile)
4. Account-level concurrent stream limits enforced by license server
5. Session persistence allows pause on one device, resume on another

**Benefits**: Flexible viewing experience, device-appropriate security, seamless cross-device content access.

## API Capabilities

The plugin exposes comprehensive JSON-RPC and C++ APIs enabling applications to:

### Session Management APIs
- `createSession`: Initialize new DRM session with initialization data (PSSH)
- `load`: Restore persistent session for offline playback
- `update`: Provide license response to complete key acquisition
- `close`: Terminate session and release resources
- `remove`: Delete persistent session and stored licenses

### Key Status APIs
- Key availability monitoring via events
- Expiration time tracking for rental content
- Output restriction status (HDCP required, analog output blocked)
- Multi-key management for streams with different protection levels

### System Information APIs
- Enumerate supported DRM systems and versions
- Query device security capabilities (TEE available, HDCP version)
- Retrieve DRM-specific identifiers and certificates
- Check license server connectivity and configuration

### Event Notifications
- `keymessage`: License request ready for server transmission
- `keystatuseschange`: Key availability or restrictions updated
- `message`: Generic DRM system messages (individualization, provisioning)

## Performance and Reliability

### Low-Latency License Acquisition
- Optimized RPC communication minimizes session creation overhead
- Pre-provisioning support reduces first-playback latency by up to 500ms
- Parallel license requests for multi-codec adaptive streaming
- Efficient PSSH parsing handles complex multi-DRM scenarios

### Hardware Acceleration
- GPU-accelerated decryption on supported platforms reduces CPU usage by 70%
- Hardware crypto engines offload AES operations
- Secure video path eliminates memory copies
- Dedicated security processor isolates key operations

### Robustness and Error Handling
- Out-of-process architecture prevents CDM crashes from affecting framework
- Automatic session recovery on transient failures
- Graceful degradation on license server connectivity issues
- Comprehensive error codes for actionable diagnostics
- Watchdog monitoring and automatic restart of CDM process

### Scalability
- Concurrent session support limited only by hardware capacity (typically 8-16 simultaneous sessions)
- Memory-efficient key storage supports hundreds of cached licenses
- Background license renewal doesn't impact playback performance
- Adaptive resource allocation based on content resolution and bitrate

## Security Certifications

The OpenCDMi plugin enables device certification for:
- **Google Widevine**: L1 (hardware security) and L3 (software security) certification paths
- **Microsoft PlayReady**: SL3000 (hardware root of trust) and SL2000 (software obfuscation) compliance
- **Content Protection Compliance**: HDCP 2.2 enforcement, DTCP-IP support
- **Common Criteria**: Enables EAL2+ certification for secure media path

## Integration Benefits

### Application Developer Benefits
- Single API for all DRM systems - learn once, support multiple services
- W3C EME compatibility enables web application portability
- Comprehensive error handling reduces debugging complexity
- Event-driven architecture simplifies asynchronous workflows

### Operator Benefits
- Multi-service aggregation without per-service DRM integration
- Centralized DRM management across device portfolio
- Flexible business model support (subscription, rental, purchase)
- Reduced time-to-market for new content partnerships

### Platform Benefits
- Hardware security abstraction across different SoC vendors
- Standardized interface simplifies CDM porting
- Thunder framework integration provides monitoring and management
- Open-source implementation enables customization and transparency
