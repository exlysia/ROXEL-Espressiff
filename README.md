## Introduction
To make things easier, download this repository and open the project. `main.cpp` contains only sample code that you can clean up and add to yourself.
> [!WARNING]
> This project already has an `sdkconfig` configured for the `ESP32-S3-N16R8` board. If yours is different, reconfigure the `sdkconfig`.

## Quick start
Import the library
```c++
#include "ROXEL.h"
```

Create an internet interface
```c++
Network network("<YOUR_WIFI_SSID>", "<YOUR_WIFI_PASSKEY>");
```

Create an instance
```c++
ROXEL instance(network, "<AUTH_TOKEN>");
```

Create functions
```c++
inline static void setup(Instance self)
{
}

inline static void loop(Instance self, bool isConnected)
{
}
```

Call the following methods in `app_main()`
> [!NOTE]
> You can skip the previous step by not specifying anything in the methods below (or by explicitly specifying `NULL`)
```c++
extern "C" void app_main(void)
{
    instance.initialize(setup);
    instance.launch(loop);
}
```

## Example of working with `Request`
Create a simple request instance
```c++
Request myRequest("my_request", onRequestIncoming);
```
> If you need client connection events, please add an instance
```c++
Request myRequest("my_request", onRequestIncoming, onClientConnected);
```

Create an incoming request listener
```c++
inline static void onRequestIncoming(Incoming incoming, ClientID client_id, Payload payload) {
}
```
> If you need client connection events, add a callback
```c++
inline static void onClientConnected(Incoming incoming, ClientID client_id) {
}
```

Use a simple comparison to determine the purpose of a request
```c++
inline static void onRequestIncoming(Incoming incoming, ClientID client_id, Payload payload) {
    if (myRequest == incoming) {
        // "my_request" is called
    }
}

inline static void onClientConnected(Incoming incoming, ClientID client_id) {
    if (myRequest == incoming) {
        // "my_request" is called
    }
}
```

To send an answer, use the `Answer` constructor.
> [!WARNING]
> One `Answer` constructor can only be used for one answer.

> [!NOTE]
> You can use the Answer constructor outside of standard callbacks, but you must pass `Request` instead of `Incoming`
```c++
inline static void onRequestIncoming(Incoming incoming, ClientID client_id, Payload payload) {
    if (myRequest == incoming) {
        Answer()
            .boolean("success", true)
            .number("size", 1024)
            .text("message", "Simple answer")
            .respond(client_id, incoming);
    }
}

inline static void onClientConnected(Incoming incoming, ClientID client_id) {
    if (myRequest == incoming) {
        Answer()
            .boolean("success", true)
            .number("size", 1024)
            .text("message", "Simple answer")
            .respond(client_id, incoming);
    }
}
```

To send a response to all clients, use `broadcast`
```c++
inline static void onRequestIncoming(Incoming incoming, ClientID client_id, Payload payload) {
    if (myRequest == incoming) {
        Answer()
            .boolean("success", true)
            .number("size", 1024)
            .text("message", "Simple answer")
            .broadcast(incoming);
    }
}
```

To read incoming data, use the following `Payload` methods
```c++
const char *text(const char *key, const char *defaultValue = nullptr)
int number(const char *key, int defaultValue = -1)
bool boolean(const char *key, bool defaultValue = false)
Payload &object(const char *key)
```

Example of reading incoming `Payload` data
```c++
inline static void onRequestIncoming(Incoming incoming, ClientID client_id, Payload payload) {
    if (myRequest == incoming) {
        const char *message = payload.text("message", "null");
        bool running = payload.boolean("running", false);
        int count = payload.number("count", 0);

        // echo response
        Answer()
            .boolean("running", running)
            .number("count", count)
            .text("echo", message)
            .broadcast(incoming);
    }
}
```

Installing `requests` into instance
```c++
inline static void setup(Instance self)
{
    LOAD_REQUESTS(self, &myRequest, &request2, &request3);
}
```

## Example of working with `Effect`
Create a simple effect instance
```c++
Effect myEffect("my_effect");
```
> If you want to listen to incoming updates, add a listener
```c++
Effect myEffect("my_effect", onEffectUpdate);
```
> If you want to set the initial value of the hook, add the instance

> [!WARNING]
> The effect only supports `int` data type.
```c++
Effect myEffect("my_effect", onEffectUpdate, 100);
```

Create a state update listener
```c++
inline static void onEffectUpdate(Hook hook, State state) {
}
```

Use a simple comparison to determine the purpose of a hook
```c++
inline static void onEffectUpdate(Hook hook, State state) {
    if (myEffect == hook) {
        // "my_effect" state is updated
    }
}
```

To manually update the state, use equating
> [!WARNING]
> The effect only supports `int` data type.
```c++
extern "C" void app_name(void) {
    my_effect = 35;
}
```

To get the current state of a hook, use transform
> [!WARNING]
> The effect only supports `int` data type.
```c++
extern "C" void app_name(void) {
    int state = (int) my_effect;
}
```

Installing `effects` into instance
```c++
inline static void setup(Instance self)
{
    LOAD_EFFECTS(self, &myEffect, &effect2, &effect3);
}
```

## Example of working with `StatesEffect`
`StatesEffect` is essentially the same as `Effect`, just with some changes in functionality.

Create a states-effect instance
```c++
StatesEffect myStates("my_states");
```
> If you want to listen to incoming updates, add a listener
```c++
StatesEffect myStates("my_states", onStatesUpdate);
```
> You can also customize the number of states

> [!NOTE]
> Maximum number of states is `32`
```c++
StatesEffect myStates("my_states", onStatesUpdate, 16);
```

Create a state update listener
```c++
inline static void onStatesUpdate(StatesHook hook, States state) {
}
```

Use a simple comparison to determine the purpose of a hook
```c++
inline static void onStatesUpdate(StatesHook hook, States state) {
    if (myStates == hook) {
        // "my_states" state is updated
    }
}
```

To manually update the states, use the following method
> [!WARNING]
> The states-effect only supports `bool` data type.
```c++
extern "C" void app_name(void) {
    myStates.set(0, true);
    myStates.set(3, false);
}
```

You can also set all the values ​​at once using the following method
> [!WARNING]
> The states-effect only supports `bool` data type.
```c++
extern "C" void app_name(void) {
    myStates.all(true);
}
```

To reset all values, use the `clear` method
```c++
extern "C" void app_name(void) {
    myStates.clear();
}
```

To find out if any value is `true`, use the `any` method.
```c++
extern "C" void app_name(void) {
    bool has = myStates.any();
}
```

To find out if all states are `true` use the transformation
> [!WARNING]
> The states-effect only supports `bool` data type.
```c++
extern "C" void app_name(void) {
    bool is_all = (bool) myStates;
}
```

To get a specific state, use `array-expression`
> [!WARNING]
> The states-effect only supports `bool` data type.
```c++
extern "C" void app_name(void) {
    bool first = myStates[0];
    bool second = myStates[1];
}
```

> [!NOTE]
> The effect installation to instance is identical to regular effects.

## OTA (Over-The-Air)
> [!WARNING]
> This is an experimental feature. Don't rely too much on him.

To connect to `OTA`, simply call the macro by specifying the connection `port`.
```c++
inline static void setup(Instance self)
{
    INITIALIZE_OTA(self, 3232); // 3232 is server port
}
```

To start, you need to send the following packet via `TCP`
```json
{"e":"ota","d":{}}
```

After launching `OTA`, download the firmware in `bin` format via PowerShell
```powershell
$client = New-Object System.Net.Sockets.TcpClient("<YOUR_ROXEL_IP>", <YOUR_OTA_PORT>)
$stream = $client.GetStream()
[byte[]]$bytes = [System.IO.File]::ReadAllBytes("<YOUR_FIRMWARE_PATH>")
$stream.Write($bytes, 0, $bytes.Length)
$stream.Close()
$client.Close()
```

After a successful download, you will see logs similar to these.
```
I (14755) ROXEL.SYSTEM: [OTA] Server listening on port 3232
I (14765) ROXEL.SYSTEM: [OTA] Initializing update partition...
I (14815) ROXEL.SYSTEM: [OTA] Server task is started
I (18145) ROXEL.SYSTEM: [OTA] Update task is started
W (127505) ROXEL.SYSTEM: [OTA] Server is shutting down...
W (127505) ROXEL.SYSTEM: [OTA] Server socket closed
W (127505) ROXEL.SYSTEM: [OTA] Server task is finished
I (127545) ROXEL.SYSTEM: [OTA] Partition is written: 848400 bytes
I (127545) esp_image: segment 0: paddr=00410020 vaddr=3c0a0020 size=1cb04h (117508) map
I (127565) esp_image: segment 1: paddr=0042cb2c vaddr=3fc9c100 size=034ech ( 13548) 
I (127565) esp_image: segment 2: paddr=00430020 vaddr=42000020 size=954ach (611500) map
I (127645) esp_image: segment 3: paddr=004c54d4 vaddr=3fc9f5ec size=01c20h (  7200) 
I (127655) esp_image: segment 4: paddr=004c70fc vaddr=40374000 size=180c4h ( 98500)
I (127665) esp_image: segment 5: paddr=004df1c8 vaddr=50000000 size=00020h (    32) 
I (127665) ROXEL.SYSTEM: [OTA] Setting up boot partition...
I (127665) esp_image: segment 0: paddr=00410020 vaddr=3c0a0020 size=1cb04h (117508) map
I (127695) esp_image: segment 1: paddr=0042cb2c vaddr=3fc9c100 size=034ech ( 13548) 
I (127695) esp_image: segment 2: paddr=00430020 vaddr=42000020 size=954ach (611500) map
I (127775) esp_image: segment 3: paddr=004c54d4 vaddr=3fc9f5ec size=01c20h (  7200) 
I (127775) esp_image: segment 4: paddr=004c70fc vaddr=40374000 size=180c4h ( 98500)
I (127785) esp_image: segment 5: paddr=004df1c8 vaddr=50000000 size=00020h (    32) 
I (127875) ROXEL.SYSTEM: [OTA] Boot partition is installed successfully. Rebooting...
```
