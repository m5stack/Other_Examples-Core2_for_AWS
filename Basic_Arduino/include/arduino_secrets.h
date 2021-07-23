// Fill in  your WiFi networks SSID and password
#define WIFI_SSID "AWSWorkshop"
#define WIFI_PASS "IoTP$AK1t"

// Fill in the hostname of your AWS IoT broker
// Should look like asdf12345-ats.iot.us-west-2.amazonaws.com
// Do not add protocol prefix (e.g. mqtt:// or http://)
#define AWS_IOT_ENDPOINT_ADDRESS ""

// The ATECC608 TNG Device Certificate.
// Paste the contents between the "BEGIN CERTIFICATE" and
// "END CERTIFICATE" headers/footers.
const char THING_CERTIFICATE[] = R"(
-----BEGIN CERTIFICATE-----

-----END CERTIFICATE-----
)";