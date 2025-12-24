# CMD Service Component

Service Manager console commands component for ESP Brookesia framework.

## Features

This component provides console commands to interact with the Service Manager:

- **`svc_list`** - List all registered services
- **`svc_funcs <service>`** - List all functions in a specific service
- **`svc_call <service> <function> [params]`** - Call a service function with JSON parameters

## Usage

### 1. Include the header

```c
#include "cmd_service.h"
```

### 2. Register commands

```c
extern "C" void app_main(void)
{    // ... initialize console ...

    // Register service commands
    register_service_commands();

    // ... start console REPL ...
}
```

### 3. Use commands

#### List all services

```bash
esp32> svc_list

=== Registered Services ===
  audio                [initialized] [running]

Total: 1 service(s)
```

#### List functions in a service

```bash
esp32> svc_funcs audio

=== Functions in service 'audio' ===

  Function: set_volume
    Description: Set audio volume
    Parameters:
      {"type":"object","properties":{"volume":{"type":"integer","minimum":0,"maximum":100}}}

  Function: play_url
    Description: Play audio from URL
    Parameters:
      {"type":"object","properties":{"url":{"type":"string"}}}

Total: 2 function(s)
```

#### Call a service function

```bash
# Set volume to 80
esp32> svc_call audio set_volume {"volume":80}

Calling: audio.set_volume({"volume":80})

Result:
  {"success":true}

# Play an audio file
esp32> svc_call audio play_url {"url":"file://spiffs/test.mp3"}

Calling: audio.play_url({"url":"file://spiffs/test.mp3"})

Result:
  {"success":true,"message":"Playing audio"}

# Call function without parameters
esp32> svc_call audio get_status {}

Calling: audio.get_status({})

Result:
  {"volume":80,"is_playing":true}
```

## Implementation Details

### Client Management

The component automatically creates and manages a global client instance that is used for all function calls. The client is created on first use and automatically reconnects if needed.

### JSON Parameter Parsing

Parameters must be provided as valid JSON objects. The component will:
- Parse the JSON string
- Validate that it's an object (not array or primitive)
- Pass it to the service function

### Error Handling

The component provides clear error messages for:
- Service not found
- Invalid JSON syntax
- Function call failures
- Connection issues

## Example Integration

See the main example application in `examples/console_cmd/main/main.cpp` for a complete integration example.

## Requirements

- ESP-IDF console component
- ESP Brookesia Service Manager
- Boost.JSON (for parameter handling)

## CMake Configuration

```cmake
idf_component_register(
    SRCS "cmd_service.cpp"
    INCLUDE_DIRS .
    REQUIRES console brookesia_service_manager
)
```

## License

Apache-2.0

