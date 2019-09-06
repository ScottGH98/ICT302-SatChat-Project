enum clock_format {CLOCK_12_HOUR, CLOCK_24_HOUR};
enum connection_status {DISCONNECTED, CONNECTING, CONNECTED};

struct timestamp
{
	time_t timestamp;//Arduino provides time_t.
};

struct message
{
	struct timestamp timestamp;
	uint8_t length;
	char message[160];//160 characters.
};

struct coords
{
	struct timestamp timestamp;
	uint32_t coords;//How will coordinates be stored?
};

struct settings
{
	bool reportGPS;
	struct timestamp reportInterval;//How often GPS coordinates should be reported/sent.
	struct timestamp requestInterval;//How often GPS coordinates should be requested.
	enum clock_format clockFormat;
};

struct device_state
{
	enum connection_status connectionStatus;
	struct timestamp deviceTime;
	uint8_t batteryLevel;
};
