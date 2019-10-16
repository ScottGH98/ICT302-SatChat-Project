static void process_gui(void)
{
	switch(deviceState.menuContext.menu)
	{
		case MENU_NONE:
			process_menu_none();
			break;
		case MENU_MAIN:
			process_menu_main();
			break;
		case MENU_SETTINGS:
			process_menu_settings();
			break;
		case MENU_MESSAGES:
			process_menu_messages();
			break;
		case MENU_COORDINATES:
			process_menu_coordinates();
			break;
	}
}

static bool process_touch_buttons(const enum menu menu)
{
	TSPoint p = ts.getPoint();
	struct touch_point tp;
	tp.x = map(p.y, TS_MAXY, TS_MINY, 0, tft.width());//THIS LINE AND THE NEXT ARE RATHER CONFUSING.
	tp.y = map(p.x, TS_MINX, TS_MAXX, 0, tft.height());
	tp.isPressed = (p.z >= MINPRESSURE && p.z <= MAXPRESSURE);
	tp.justPressed = (tp.isPressed && !(lastTouchPoint.isPressed));
	tp.justReleased = (lastTouchPoint.isPressed && !(tp.isPressed));
	
	bool buttonInteraction = false;
	if(!(tp.justPressed) && deviceState.settings.instantButtons) return buttonInteraction;//RETURN FALSE IF THE SCREEN WAS NOT JUST PRESSED AND INSTANT BUTTONS IS DISABLED.
	
	struct menu_buttons *mb = menuButtons[menu];
	
	if(!(deviceState.settings.instantButtons))
	{
		for(uint_fast8_t i = 0; i < mb.buttonCount; ++i)
		{
			if(mb->buttonState[i])//IF THE BUTTON IS IN THE PRESSED STATE.
			{
				if(point2aabb(tp.x, tp.y, mb->button[i].x, mb->button[i].y, mb->button[i].w, mb->button[i].h))
				{
					if(tp.justReleased)
					{
						if(mb->button[i].function)
						{
							(*mb->button[i].function)(mb->button[i].functionData);
						}
						mb->buttonState[i] = false;
						buttonInteraction = true;
					}
				}
				else
				{
					if(tp.justReleased)
					{
						mb->buttonState[i] = false;
						tft.fillRect(mb->button[i].x, mb->button[i].y, mb->button[i].w, mb->button[i].h, mb->button[i].colour);
						buttonInteraction = true;
					}
				}
				break;
			}
		}
	}
	
	for(uint_fast8_t i = 0; i < mb.buttonCount; ++i)
	{
		if(point2aabb(tp.x, tp.y, mb->button[i].x, mb->button[i].y, mb->button[i].w, mb->button[i].h))
		{
			if(deviceState.settings.instantButtons)
			{
				if(mb->button[i].function)
				{
					(*mb->button[i].function)(mb->button[i].functionData);
					buttonInteraction = true;
				}
			}
			else
			{
				if(tp.justPressed)
				{
					mb->buttonState[i] = true;
					tft.fillRect(mb->button[i].x, mb->button[i].y, mb->button[i].w, mb->button[i].h, mb->button[i].colourPressed);
					buttonInteraction = true;
				}
			}
			break;
		}
	}
	
	deviceState.lastTouchPoint = tp;
	return buttonInteraction;
}

inline static void draw_menu_button(const struct menu_buttons *mb, const uint_fast8_t index)
{
	uint16_t colour = (mb->buttonState[index]) ? b.buttonColourPressed : b.buttonColour;
	tft.fillRect(mb->button[index].x, mb->button[index].y, mb->button[index].w, mb->button[index].y, colour);
}

inline static void draw_menu_buttons(const enum menu menu)
{
	struct menu_buttons mb = menuButtons[menu];
	for(uint_fast8_t i = 0; i < mb.buttonCount; ++i)
	{
		draw_menu_button(&menuButtons[menu], i);
	}
}

inline static bool point2aabb(uint_fast16_t px, uint_fast16_t py, uint_fast16_t bx, uint_fast16_t by, uint_fast16_t bw, uint_fast16_t bh)
{
	return (bx < px && bx + bw > px && by < py && by + bh > py);//CHECK IF POINT IS WITHIN BOUNDING BOX.
}

inline static void say(const char *message)
{
	Serial3.print("\nX\nS");
	Serial3.println(message);
}

static void process_menu_main(void)
{
	if(deviceState.menuContext.justChanged)
	{
		deviceState.menuContext.justChanged = false;
		
		tft.fillScreen(0x0000);
		draw_menu_buttons(MENU_MAIN);
		
		if(deviceState.settings.voicedMenus)
		{
			say("Main menu.");
		}
	}
	process_touch_buttons(MENU_MAIN);
}

static void process_menu_settings(void)
{
	if(deviceState.menuContext.justChanged)
	{
		deviceState.menuContext.justChanged = false;
		
		tft.fillScreen(0x0000);
		draw_menu_buttons(MENU_SETTINGS);
		
		if(deviceState.settings.voicedMenus)
		{
			say("Settings menu.");
		}
	}
	process_touch_buttons(MENU_SETTINGS);
	draw_time();
}

static void process_menu_messages(void)
{
	if(deviceState.menuContext.justChanged)
	{
		deviceState.menuContext.justChanged = false;
		
		tft.fillScreen(0x0000);
		draw_menu_buttons(MENU_MESSAGES);
		
		if(deviceState.settings.voicedMenus)
		{
			say("Messages menu.");
		}
	}
	process_touch_buttons(MENU_MESSAGES);
}

static void process_menu_coordinates(void)
{
	if(deviceState.menuContext.justChanged)
	{
		deviceState.menuContext.justChanged = false;
		
		tft.fillScreen(0x0000);
		draw_menu_buttons(MENU_COORDINATES);
		
		if(deviceState.settings.voicedMenus)
		{
			say("Coordinates menu.");
		}
	}
	process_touch_buttons(MENU_COORDINATES);
}

static void process_menu_keyboard(void)
{
	if(deviceState.menuContext.justChanged)
	{
		deviceState.menuContext.justChanged = false;
		
		tft.fillScreen(0x0000);
		draw_menu_buttons(MENU_KEYBOARD);
		draw_keyboard_legends();
		
		if(deviceState.settings.voicedMenus)
		{
			say("Custom message menu.");
		}
	}
	process_touch_buttons(MENU_KEYBOARD);
}

static void tb_null(const uintptr_t functionData)
{
	;
}

static void tb_set_menu(const uintptr_t functionData)
{
	deviceState.menuContext.menu = functionData;
	deviceState.menuContext.justChanged = true;
}

static void tb_key(const uintptr_t functionData)
{
	for(uint_fast8_t i = 0; i < MESSAGE_LENGTH; ++i)
	{
		if(deviceState.message.text[i] == '\0')
		{
			char c = (char) functionData;
			if(deviceState.message.shift)
			{
				c = shift_key(c);
				if(deviceState.message.caps) c = tolower(c);
			}
			else
			{
				if(deviceState.message.caps) c = toupper(c);
			}
			
			deviceState.message.text[i] = c;
			break;
		}
	}
	if(deviceState.settings.voicedKeys)
	{
		say((char*) functionData);//THIS CODE LOOKS RISKY.
	}
}

static void tb_key_shift(const uintptr_t functionData)
{
	if(deviceState.message.shift)
	{
		deviceState.message.shift = false;
	}
	else
	{
		deviceState.message.shift = true;
	}
}

static void tb_key_caps(const uintptr_t functionData)
{
	if(deviceState.message.caps)
	{
		deviceState.message.caps = false;
	}
	else
	{
		deviceState.message.caps = true;
	}
}

static void tb_key_enter(const uintptr_t functionData)
{
	for(uint_fast8_t i = 0; i < MESSAGE_LENGTH; ++i)
	{
		if(deviceState.message.text[i] == '\0')
		{
			deviceState.message.text[i] = '\n';
			break;
		}
	}
}

static void tb_key_backspace(const uintptr_t functionData)
{
	for(uint_fast8_t i = 1; i < MESSAGE_LENGTH; ++i)
	{
		if(deviceState.message.text[i] == '\0')
		{
			deviceState.message.text[i - 1] = '\0';
			break;
		}
	}
}

static void draw_time(void)
{
	rtc.update();
	tft.fillRect(215, 0, 106, 51, ILI9341_WHITE);
	tft.setCursor(215, 10);
	tft.setTextColor(0x0000);
	tft.setTextSize(3);
	tft.print(rtc.hour());
	tft.print(":");
	if(rtc.minute() < 10) tft.print("0");
	tft.print(rtc.minute);
}

inline static void shift_key(const char c)
{
	switch(c)
	{
		case '1':
			c = '!';
			break;
		case '2':
			c = '@';
			break;
		case '3':
			c = '#';
			break;
		case '4':
			c = '$';
			break;
		case '5':
			c = '%';
			break;
		case '6':
			c = '^';
			break;
		case '7':
			c = '&';
			break;
		case '8':
			c = '*';
			break;
		case '9':
			c = '(';
			break;
		case '0':
			c = ')';
			break;
		case '-':
			c = '_';
			break;
		case '=':
			c = '+';
			break;
		case ',':
			c = '<';
			break;
		case '.':
			c = '>';
			break;
		case '/':
			c = '?';
			break;
		case ';':
			c = ':';
			break;
		case '`':
			c = '~';
			break;
		case '[':
			c = '{';
			break;
		case ']':
			c = '}';
			break;
		case '\'':
			c = '\"';
			break;
		default:
			c = toupper(c);
			break;
	}
}

static void draw_keyboard_legends(void)
{
	tft.setTextSize(3);
	tft.setTextColor(0x0000);
	
	struct menu_buttons *mb = menuButtons[MENU_KEYBOARD];
	
	for(uint_fast8_t i = 0; i < mb->buttonCount; ++i)
	{
		if(mb->button[i].function == tb_key)
		{
			char legend[2] = {mb->button[i].functionData};
			if(deviceState.message.shift)
			{
				legend[0] = shift_key(legend[0]);
				if(deviceState.message.caps) legend[0] = tolower(legend[0]);
			}
			else
			{
				if(deviceState.message.caps) legend[0] = toupper(legend[0]);
			}
			
			tft.setCursor(mb->button[i].x + 0000, mb->button[i].y + 0000);//CHANGE OFFSETS TO ALIGN LEGEND.
			tft.print(legend);
		}
	}
}