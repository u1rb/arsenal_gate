#include <fmt/format.h>
#include <librdkafka/rdkafkacpp.h>

#include <catch2/catch_test_macros.hpp>
#include <memory>
#include <string>

class TestEventCb : public RdKafka::EventCb {
public:
  void event_cb(RdKafka::Event &event) override {
    switch (event.type()) {
    case RdKafka::Event::EVENT_ERROR:
      fmt::print("ERROR ({}): {}\n", RdKafka::err2str(event.err()),
                 event.str());
      break;
    case RdKafka::Event::EVENT_STATS:
      fmt::print("STATS: {}\n", event.str());
      break;
    case RdKafka::Event::EVENT_LOG:
      fmt::print("LOG-{}-{}: {}\n", static_cast<int>(event.severity()),
                 event.fac(), event.str());
      break;
    default:
      fmt::print("EVENT {} ({}): {}\n", static_cast<int>(event.type()),
                 RdKafka::err2str(event.err()), event.str());
      break;
    }
  }
};

class TestDeliveryReportCb : public RdKafka::DeliveryReportCb {
public:
  void dr_cb(RdKafka::Message &message) override {
    if (message.err()) {
      fmt::print("Message delivery failed: {}\n", message.errstr());
    } else {
      fmt::print("Message delivered to topic {} [{}] at offset {}\n",
                 message.topic_name(), message.partition(), message.offset());
    }
  }
};

TEST_CASE("librdkafka version", "[librdkafka]") {
  SECTION("Get library version") {
    std::string version = RdKafka::version_str();
    REQUIRE(!version.empty());
    fmt::print("librdkafka version: {}\n", version);

    int version_int = RdKafka::version();
    REQUIRE(version_int > 0);
    fmt::print("librdkafka version (int): 0x{:08x}\n", version_int);
  }
}

TEST_CASE("Kafka configuration", "[librdkafka]") {
  SECTION("Create and modify configuration") {
    std::string errstr;
    std::unique_ptr<RdKafka::Conf> conf(
        RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL));

    REQUIRE(conf != nullptr);

    auto result = conf->set("bootstrap.servers", "localhost:9092", errstr);
    REQUIRE(result == RdKafka::Conf::CONF_OK);

    result = conf->set("client.id", "test-client", errstr);
    REQUIRE(result == RdKafka::Conf::CONF_OK);

    result = conf->set("invalid.property", "value", errstr);
    REQUIRE(result == RdKafka::Conf::CONF_UNKNOWN);
    REQUIRE(!errstr.empty());
  }

  SECTION("Get configuration values") {
    std::string errstr;
    std::string value;
    std::unique_ptr<RdKafka::Conf> conf(
        RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL));

    conf->set("client.id", "test-client-get", errstr);

    auto result = conf->get("client.id", value);
    REQUIRE(result == RdKafka::Conf::CONF_OK);
    REQUIRE(value == "test-client-get");

    result = conf->get("non.existent.property", value);
    REQUIRE(result == RdKafka::Conf::CONF_UNKNOWN);
  }
}

TEST_CASE("Kafka producer creation", "[librdkafka][producer]") {
  SECTION("Create producer with valid config") {
    std::string errstr;
    std::unique_ptr<RdKafka::Conf> conf(
        RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL));

    conf->set("bootstrap.servers", "localhost:9092", errstr);
    conf->set("client.id", "test-producer", errstr);

    TestEventCb event_cb;
    conf->set("event_cb", &event_cb, errstr);

    TestDeliveryReportCb dr_cb;
    conf->set("dr_cb", &dr_cb, errstr);

    std::unique_ptr<RdKafka::Producer> producer(
        RdKafka::Producer::create(conf.get(), errstr));

    if (!producer) {
      fmt::print("Failed to create producer: {}\n", errstr);
      REQUIRE(producer != nullptr);
    }

    REQUIRE(!producer->name().empty());
    fmt::print("Producer created: {}\n", producer->name());
  }
}

TEST_CASE("Kafka consumer creation", "[librdkafka][consumer]") {
  SECTION("Create consumer with valid config") {
    std::string errstr;
    std::unique_ptr<RdKafka::Conf> conf(
        RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL));

    conf->set("bootstrap.servers", "localhost:9092", errstr);
    conf->set("group.id", "test-consumer-group", errstr);
    conf->set("client.id", "test-consumer", errstr);
    conf->set("auto.offset.reset", "earliest", errstr);

    TestEventCb event_cb;
    conf->set("event_cb", &event_cb, errstr);

    std::unique_ptr<RdKafka::KafkaConsumer> consumer(
        RdKafka::KafkaConsumer::create(conf.get(), errstr));

    if (!consumer) {
      fmt::print("Failed to create consumer: {}\n", errstr);
      REQUIRE(consumer != nullptr);
    }

    REQUIRE(!consumer->name().empty());
    fmt::print("Consumer created: {}\n", consumer->name());

    consumer->close();
  }
}

TEST_CASE("Topic metadata", "[librdkafka][metadata]") {
  SECTION("Get metadata from producer") {
    std::string errstr;
    std::unique_ptr<RdKafka::Conf> conf(
        RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL));

    conf->set("bootstrap.servers", "localhost:9092", errstr);
    conf->set("client.id", "test-metadata", errstr);

    std::unique_ptr<RdKafka::Producer> producer(
        RdKafka::Producer::create(conf.get(), errstr));

    if (!producer) {
      fmt::print("Cannot test metadata - producer creation failed: {}\n",
                 errstr);
      SKIP("Kafka broker not available");
    }

    RdKafka::Metadata *metadata = nullptr;
    RdKafka::ErrorCode err = producer->metadata(true, nullptr, &metadata, 5000);

    if (err != RdKafka::ERR_NO_ERROR) {
      fmt::print("Failed to fetch metadata: {}\n", RdKafka::err2str(err));
      SKIP("Cannot fetch metadata");
    }

    REQUIRE(metadata != nullptr);

    fmt::print("Metadata for {} brokers and {} topics:\n",
               metadata->brokers()->size(), metadata->topics()->size());

    for (const auto &broker : *metadata->brokers()) {
      fmt::print("  Broker {}: {}:{}\n", broker->id(), broker->host(),
                 broker->port());
    }

    delete metadata;
  }
}

TEST_CASE("Error codes", "[librdkafka][errors]") {
  SECTION("Convert error codes to strings") {
    REQUIRE(RdKafka::err2str(RdKafka::ERR_NO_ERROR) == "Success");
    REQUIRE(RdKafka::err2str(RdKafka::ERR__TIMED_OUT) == "Local: Timed out");
    REQUIRE(RdKafka::err2str(RdKafka::ERR_UNKNOWN_TOPIC_OR_PART) ==
            "Broker: Unknown topic or partition");

    fmt::print("Error string for NO_ERROR: {}\n",
               RdKafka::err2str(RdKafka::ERR_NO_ERROR));
    fmt::print("Error string for TIMED_OUT: {}\n",
               RdKafka::err2str(RdKafka::ERR__TIMED_OUT));
  }
}