struct device_state deviceState;

const struct touch_button menuMainButtons[] =
{
	{0, 53, 159, 93, PASTEL_GREEN, NES_RED, tb_set_menu, MENU_MESSAGES},
	{162, 53, 159, 93, PASTEL_RED, NES_RED, tb_set_menu, MENU_KEYBOARD},//TEMPORARY MENU ASSIGNMENT FOR TESTING
	{0, 148, 159, 93, PASTEL_BLUE, NES_RED, tb_set_menu, MENU_COORDINATES},
	{162, 148, 159, 93, PASTEL_YELLOW, NES_RED, tb_set_menu, MENU_SETTINGS}
};
bool menuMainButtonStates[sizeof(menuMainButtons) / sizeof(struct touch_button)];

const struct touch_button menuKeyboardButtons[] =
{
	{0, 0, 106, 51, ILI9341_WHITE, NES_RED, tb_set_menu, MENU_MAIN},//TEMPORARY MENU ASSIGNMENT FOR TESTING
	{9, 111, 25, 25, PASTEL_YELLOW, NES_RED, tb_key, '1'},
	{35, 111, 25, 25, PASTEL_YELLOW, NES_RED, tb_key, '2'},
	{61, 111, 25, 25, PASTEL_YELLOW, NES_RED, tb_key, '3'},
	{87, 111, 25, 25, PASTEL_YELLOW, NES_RED, tb_key, '4'},
	{113, 111, 25, 25, PASTEL_YELLOW, NES_RED, tb_key, '5'},
	{139, 111, 25, 25, PASTEL_YELLOW, NES_RED, tb_key, '6'},
	{165, 111, 25, 25, PASTEL_YELLOW, NES_RED, tb_key, '7'},
	{191, 111, 25, 25, PASTEL_YELLOW, NES_RED, tb_key, '8'},
	{217, 111, 25, 25, PASTEL_YELLOW, NES_RED, tb_key, '9'},
	{243, 111, 25, 25, PASTEL_YELLOW, NES_RED, tb_key, '0'},
	{269, 111, 25, 25, PASTEL_YELLOW, NES_RED, tb_key, '-'},
	{295, 111, 25, 25, PASTEL_YELLOW, NES_RED, tb_key, '='},
	{22, 137, 25, 25, PASTEL_YELLOW, NES_RED, tb_key, 'q'},
	{48, 137, 25, 25, PASTEL_YELLOW, NES_RED, tb_key, 'w'},
	{74, 137, 25, 25, PASTEL_YELLOW, NES_RED, tb_key, 'e'},
	{100, 137, 25, 25, PASTEL_YELLOW, NES_RED, tb_key, 'r'},
	{126, 137, 25, 25, PASTEL_YELLOW, NES_RED, tb_key, 't'},
	{152, 137, 25, 25, PASTEL_YELLOW, NES_RED, tb_key, 'y'},
	{178, 137, 25, 25, PASTEL_YELLOW, NES_RED, tb_key, 'u'},
	{204, 137, 25, 25, PASTEL_YELLOW, NES_RED, tb_key, 'i'},
	{230, 137, 25, 25, PASTEL_YELLOW, NES_RED, tb_key, 'o'},
	{256, 137, 25, 25, PASTEL_YELLOW, NES_RED, tb_key, 'p'},
	{282, 137, 38, 25, PASTEL_YELLOW, NES_RED, tb_key, '\0'},//BACKSPACE
	{1, 163, 32, 25, PASTEL_YELLOW, NES_RED, tb_key_caps, NULL},//CAPS
	{35, 163, 25, 25, PASTEL_YELLOW, NES_RED, tb_key, 'a'},
	{61, 163, 25, 25, PASTEL_YELLOW, NES_RED, tb_key, 's'},
	{87, 163, 25, 25, PASTEL_YELLOW, NES_RED, tb_key, 'd'},
	{113, 163, 25, 25, PASTEL_YELLOW, NES_RED, tb_key, 'f'},
	{139, 163, 25, 25, PASTEL_YELLOW, NES_RED, tb_key, 'g'},
	{165, 163, 25, 25, PASTEL_YELLOW, NES_RED, tb_key, 'h'},
	{191, 163, 25, 25, PASTEL_YELLOW, NES_RED, tb_key, 'j'},
	{217, 163, 25, 25, PASTEL_YELLOW, NES_RED, tb_key, 'k'},
	{243, 163, 25, 25, PASTEL_YELLOW, NES_RED, tb_key, 'l'},
	{269, 163, 25, 25, PASTEL_YELLOW, NES_RED, tb_key, '\n'},//ENTER
	{22, 189, 25, 25, PASTEL_YELLOW, NES_RED, tb_key_shift, NULL},//SHIFT
	{48, 189, 25, 25, PASTEL_YELLOW, NES_RED, tb_key, 'z'},
	{74, 189, 25, 25, PASTEL_YELLOW, NES_RED, tb_key, 'x'},
	{100, 189, 25, 25, PASTEL_YELLOW, NES_RED, tb_key, 'c'},
	{126, 189, 25, 25, PASTEL_YELLOW, NES_RED, tb_key, 'v'},
	{152, 189, 25, 25, PASTEL_YELLOW, NES_RED, tb_key, 'b'},
	{178, 189, 25, 25, PASTEL_YELLOW, NES_RED, tb_key, 'n'},
	{204, 189, 25, 25, PASTEL_YELLOW, NES_RED, tb_key, 'm'},
	{230, 189, 25, 25, PASTEL_YELLOW, NES_RED, tb_key, ','},
	{256, 189, 25, 25, PASTEL_YELLOW, NES_RED, tb_key, '.'},
	{282, 189, 25, 25, PASTEL_YELLOW, NES_RED, tb_key, '/'},
	{35, 215, 25, 25, PASTEL_YELLOW, NES_RED, tb_key, ';'},
	{61, 215, 25, 25, PASTEL_YELLOW, NES_RED, tb_key, '\''},
	{87, 215, 25, 25, PASTEL_YELLOW, NES_RED, tb_key, '`'},
	{113, 215, 129, 25, PASTEL_YELLOW, NES_RED, tb_key, ' '},//SPACE
	{243, 215, 25, 25, PASTEL_YELLOW, NES_RED, tb_key, '['},
	{269, 215, 25, 25, PASTEL_YELLOW, NES_RED, tb_key, ']'}
};
bool menuKeyboardButtonStates[sizeof(menuKeyboardButtons) / sizeof(struct touch_button)];

const struct menu_buttons menuButtons[MENU_COUNT] =
{
	{NULL, NULL, 0},//MENU_NONE
	{menuMainButtons, menuMainButtonStates, sizeof(menuMainButtons) / sizeof(struct touch_button)};//MENU_MAIN
	{NULL, NULL, 0},//MENU_SETTINGS
	{NULL, NULL, 0},//MENU_COORDINATES
	{NULL, NULL, 0},//MENU_MESSAGES
	{menuKeyboardButtons, menuKeyboardButtonStates, sizeof(menuKeyboardButtons) / sizeof(struct touch_button)},//MENU_KEYBOARD
};