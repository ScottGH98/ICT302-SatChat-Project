struct device_state deviceState;

const struct touch_button menuMainButtons[] =
{
	{0, 53, 159, 93, PASTEL_GREEN, NES_RED, tb_set_menu, MENU_MESSAGES},
	{162, 53, 159, 93, PASTEL_RED, NES_RED, tb_set_menu, MENU_MESSAGES},
	{0, 148, 159, 93, PASTEL_BLUE, NES_RED, tb_set_menu, MENU_COORDINATES},
	{162, 148, 159, 93, PASTEL_YELLOW, NES_RED, tb_set_menu, MENU_SETTINGS}
};
bool menuMainButtonStates[sizeof(menuMainButtons) / sizeof(struct touch_button)];

struct menu_buttons menuButtons[MENU_COUNT] =
{
	{NULL, 0, 0},
	{menuMainButtons, menuMainButtonStates, sizeof(menuMainButtons) / sizeof(struct touch_button)};
};