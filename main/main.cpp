#include "ROXEL.h"

inline static void onStatesUpdate(StatesHook hook, States state);
inline static void onEffectUpdate(Hook hook, State state);
inline static void onRequestIncoming(Incoming incoming, ClientID client_id, Payload payload);
inline static void onClientConnected(Incoming incoming, ClientID client_id);

Network network("<YOUR_WIFI_SSID>", "<YOUR_WIFI_PASSKEY>", {.ip = "192.168.0.120", .gateway = "192.168.0.1", .netmask = "255.255.255.0"});
ROXEL sdk(network, "<AUTH_TOKEN>");

Request myRequest("my_request", onRequestIncoming, onClientConnected);
StatesEffect myStates("my_states", onStatesUpdate, 16);
Effect myEffect("my_effect", onEffectUpdate, 100);

inline static void onStatesUpdate(StatesHook hook, States state)
{
    if (myStates == hook)
    {
        // "my_states" state is updated
    }
}

inline static void onEffectUpdate(Hook hook, State state)
{
    if (myEffect == hook)
    {
        // "my_effect" state is updated
    }
}

inline static void onRequestIncoming(Incoming incoming, ClientID client_id, Payload payload)
{
    if (myRequest == incoming)
    {
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

inline static void onClientConnected(Incoming incoming, ClientID client_id)
{
    if (myRequest == incoming)
    {
        Answer()
            .boolean("success", true)
            .number("size", 1024)
            .text("message", "Simple answer")
            .respond(client_id, incoming);
    }
}

inline static void setup(Instance self)
{
    LOAD_REQUESTS(self, &myRequest);
    LOAD_EFFECTS(self, &myEffect, &myStates);
    INITIALIZE_OTA(self, 3232);
}

inline static void loop(Instance, bool isConnected)
{
}

extern "C" void app_main(void)
{
    sdk.initialize(setup);
    sdk.launch(loop);
}