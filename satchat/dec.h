typedef void (touch_button_function)(const uintptr_t functionData);

enum menu {MENU_NONE, MENU_MAIN, MENU_SETTINGS, MENU_COORDINATES, MENU_MESSAGES, MENU_KEYBOARD, MENU_COUNT};//MENU_COUNT MUST BE LAST.

struct menu_context
{
	enum menu menu;
	enum menu lastMenu;
	bool justChanged;
};

struct settings
{
	uint16_t breadcrumbInterval;
	bool breadcrumbs;
	bool militaryTime;
	bool voicedMenus;
	bool voicedKeys;
	bool instantButtons;
};

struct eeprom//USED FOR FINDING OFFSETS AND DOCUMENTING EEPROM LAYOUT. DO NOT CREATE INSTANCE OF THIS STRUCT.
{
	uint32_t header;
	struct settings;
	struct message inbox[8];
	struct message outbox[4];
	struct message predefined[4];
};

struct touch_button
{
	uint16_t x;
	uint16_t y;
	uint16_t w;
	uint16_t h;
	uint16_t colour;
	uint16_t colourPressed;
	touch_button_function *function;
	uintptr_t functionData;
};

struct menu_buttons
{
	struct touch_button *button;
	bool *buttonState;
	uint8_t buttonCount;
};

struct touch_point
{
	union
	{
		struct
		{
			uint16_t x;
			uint16_t y;
		};
		struct
		{
			uint16_t raw[2];
		};
	};
	bool isPressed;
	bool justPressed;
	bool justReleased;
};

struct date_time
{
	uint16_t year;
	uint8_t month;
	uint8_t day;
	uint8_t hour;
	uint8_t minute;
	uint8_t second;
};

struct gps
{
	double longitude;
	double latitude;
	struct date_time timestamp;
};

struct message
{
	char number[16];
	struct gps gps;
	char text[MESSAGE_LENGTH];
};

struct message_editor
{
	char text[MESSAGE_LENGTH];
	bool shift;
	bool caps;
};

struct device_state
{
	struct settings settings;
	struct menu_context menuContext;
	struct touch_point lastTouchPoint;
	struct message_editor message;
};