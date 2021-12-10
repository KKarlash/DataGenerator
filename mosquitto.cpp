#include <exception>
#include <fstream>
#include <mutex>
#include <string>

#include "mosquitto.hpp"
#include "utilities/logger.hpp"

namespace
{
// Mosquitto callbacks
void
connectCallback(mosquitto*, void*, int resultCode)
{
    if (resultCode != 0) {
        // LOG_WARN("MOSQUITTO Connect ACK returned result code: " << resultCode);
    }
}

void
publishCallback(mosquitto*, void*, int)
{
    LOG_DEBUG("MOSQUITTO Publish ACK");
}

void
subscribeCallback(mosquitto*, void*, int, int, const int*)
{
    LOG_DEBUG("MOSQUITTO Subscribe ACK")
}

void
messageCallback(mosquitto*, void* userdata, const mosquitto_message* message)
{
    auto payload = static_cast<char*>(message->payload);
    LOG_DEBUG("Received new message from MQTT brocker for topic ['" << message->topic << "']:\n" << payload);

    MqttClient::SubscriptionTable const& subscriptionTable = static_cast<MqttClient*>(userdata)->getSubscriptionTable();
    auto                                 it                = subscriptionTable.find(std::string(message->topic));
    if ((it == subscriptionTable.end()) || (nullptr == it->second)) {
        // LOG_ERROR("No handler for topic '" << message->topic << "' is found");
        return;
    }
    it->second(message->topic, payload);
}

const std::string kCertificatesFolder{"certificates"};
const std::string kCaCertificateFile     = kCertificatesFolder + "/ca.crt";
const std::string kClientCertificateFile = kCertificatesFolder + "/client01.crt";
const std::string kClientKeyFile         = kCertificatesFolder + "/client01.key";
constexpr int     kKeepaliveSec{60};
constexpr int     kQoS{1};

}  // namespace

// TODO: store in vector<weak_ptr<MqttClient>>,  map<>
MqttClient::SubscriptionTable MqttClient::mSubscriptionHandlers;

MqttClient::MqttClient(std::string const& deviceId, std::string const& username, std::string const& password)
{
    auto mosquitoErrorCode{mosquitto_lib_init()};
    if (MOSQ_ERR_SUCCESS != mosquitoErrorCode) {
        std::string message{"Error while initializing MOSQUITTO library: " +
                            std::string(mosquitto_strerror(mosquitoErrorCode))};
        // LOG_ERROR(message);
        throw std::runtime_error(message);
    }

    mMosquitoLibHandler = mosquitto_new(deviceId.c_str(), false, this);
    mosquitto_connect_callback_set(mMosquitoLibHandler, connectCallback);
    mosquitto_publish_callback_set(mMosquitoLibHandler, publishCallback);
    mosquitto_subscribe_callback_set(mMosquitoLibHandler, subscribeCallback);
    mosquitto_message_callback_set(mMosquitoLibHandler, messageCallback);
    mosquitto_username_pw_set(mMosquitoLibHandler, username.c_str(), password.c_str());
}

MqttClient::~MqttClient()
{
    disconnect();
    mosquitto_lib_cleanup();
}

bool
MqttClient::connect(const std::string& host, uint16_t port)
{
    if (MOSQ_ERR_SUCCESS != setupOptions()) {
        return false;
    }

    LOG_INFO("Connecting to " << host << ":" << port << "...");
    auto mosquitoErrorCode{mosquitto_connect_async(mMosquitoLibHandler, host.c_str(), port, kKeepaliveSec)};
    if (MOSQ_ERR_SUCCESS != mosquitoErrorCode) {
        // LOG_ERROR("Error while calling mosquitto_connect: " << mosquitto_strerror(mosquitoErrorCode));
        return false;
    }
    LOG_INFO("Connection established");

    mosquitoErrorCode = mosquitto_loop_start(mMosquitoLibHandler);
    if (MOSQ_ERR_SUCCESS != mosquitoErrorCode) {
        // LOG_ERROR("Error while calling mosquitto_loop_start: " << mosquitto_strerror(mosquitoErrorCode));
        return false;
    }
    LOG_INFO("MOSQUITTO loop is started");

    return true;
}

bool
MqttClient::disconnect()
{
    auto mosquitoErrorCode{mosquitto_disconnect(mMosquitoLibHandler)};
    if (MOSQ_ERR_SUCCESS != mosquitoErrorCode) {
        // LOG_ERROR("Error while calling mosquitto_disconnect: " << mosquitto_strerror(mosquitoErrorCode));
        return false;
    }
    return true;
}

bool
MqttClient::send(std::string const& topic, std::string const& message)
{
    LOG_DEBUG("Publishing to MQTT topic [" << topic << "]: " << message);

    auto mosquitoErrorCode{mosquitto_publish(
        mMosquitoLibHandler, nullptr, topic.c_str(), static_cast<int>(message.size()), message.c_str(), kQoS, true)};
    if (MOSQ_ERR_SUCCESS != mosquitoErrorCode) {
        // LOG_ERROR("Error while calling mosquitto_publish: " << mosquitto_strerror(mosquitoErrorCode));
        return false;
    }
    return true;
}

bool
MqttClient::subscribe(const std::string& topic, CallbackType callback)
{
    LOG_DEBUG("Subsribe to topic [" << topic << "]");
    auto mosquitoErrorCode{mosquitto_subscribe(mMosquitoLibHandler, nullptr, topic.c_str(), kQoS)};
    if (MOSQ_ERR_SUCCESS != mosquitoErrorCode) {
        // LOG_ERROR("Error while calling mosquitto_subscribe: " << mosquitto_strerror(mosquitoErrorCode));
        return false;
    }

    mSubscriptionHandlers[topic] = callback;
    return true;
}

MqttClient::SubscriptionTable const&
MqttClient::getSubscriptionTable() const
{
    return mSubscriptionHandlers;
}

int
MqttClient::setupOptions() const
{
    std::ifstream infile(kCaCertificateFile);
    if (!infile.good()) {
        // LOG_WARN("Could not find certificate file! The mosquitto loop may fail");
    }
    infile.close();

    LOG_DEBUG("MOSQUITTO Using certificate: " << kCaCertificateFile);
    int mosquitoErrorCode{MOSQ_ERR_UNKNOWN};
    mosquitoErrorCode = mosquitto_tls_set(mMosquitoLibHandler,
                                          kCaCertificateFile.c_str(),
                                          kCertificatesFolder.c_str(),
                                          kClientCertificateFile.c_str(),
                                          kClientKeyFile.c_str(),
                                          nullptr);
    if (MOSQ_ERR_SUCCESS != mosquitoErrorCode) {
        // LOG_ERROR("Error while calling mosquitto_tls_set: " << mosquitto_strerror(mosquitoErrorCode));
        return mosquitoErrorCode;
    }

    int mqttProtocolVersion{MQTT_PROTOCOL_V311};
    mosquitoErrorCode = mosquitto_opts_set(mMosquitoLibHandler, MOSQ_OPT_PROTOCOL_VERSION, &mqttProtocolVersion);
    if (MOSQ_ERR_SUCCESS != mosquitoErrorCode) {
        // LOG_ERROR("Error while calling mosquitto_opts_set: " << mosquitto_strerror(mosquitoErrorCode));
        return mosquitoErrorCode;
    }

    return mosquitoErrorCode;
}