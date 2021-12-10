#ifndef MQTTCLIENT_HPP_
#define MQTTCLIENT_HPP_

#include <functional>
#include <string>
#include <unordered_map>

#include <mosquitto.h>

class MqttClient
{
public:
    using SubscriptionTable = std::unordered_map<std::string, CallbackType>;

    MqttClient(std::string const& deviceId, std::string const& username, std::string const& password);
    ~MqttClient();

    bool connect(const std::string& host, uint16_t port) override;
    bool disconnect() override;
    bool send(std::string const& topic, std::string const& message) override;
    bool subscribe(const std::string& topic, CallbackType callback) override;

    SubscriptionTable const& getSubscriptionTable() const;

private:
    int setupOptions() const;

    mosquitto* mMosquitoLibHandler{nullptr};
    static SubscriptionTable mSubscriptionHandlers;
};

#endif  // MQTTCLIENT_HPP_