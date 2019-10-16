static void process_gui(void)
{
	switch(menuContext.menu)
	{
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
	
	bool buttonExecuted = false;
	if(!(tp.justPressed) && deviceState.settings.instantButtons) return buttonExecuted;//RETURN FALSE IF THE SCREEN WAS NOT JUST PRESSED AND INSTANT BUTTONS IS DISABLED.
	
	struct menu_buttons mb = menuButtons[menu];
	
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
							mb->buttonState[i] = false;
							buttonExecuted = true;
						}
					}
				}
				else
				{
					if(tp.justReleased)
					{
						mb->buttonState[i] = false;
						tft.fillRect(mb->button[i].x, mb->button[i].y, mb->button[i].w, mb->button[i].h, mb->button[i].colour);
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
					buttonExecuted = true;
				}
			}
			else
			{
				if(tp.justPressed)
				{
					mb->buttonState[i] = true;
					tft.fillRect(mb->button[i].x, mb->button[i].y, mb->button[i].w, mb->button[i].h, mb->button[i].colourPressed);
				}
			}
			break;
		}
	}
	
	deviceState.settings.lastTouchPoint = tp;
	return buttonExecuted;
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
	if(menuContext.justChanged)
	{
		menuContext.justChanged = false;
		
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
	if(menuContext.justChanged)
	{
		menuContext.justChanged = false;
		
		draw_menu_buttons(MENU_SETTINGS);
		
		if(deviceState.settings.voicedMenus)
		{
			say("Settings menu.");
		}
	}
	process_touch_buttons(MENU_SETTINGS);
}

static void process_menu_messages(void)
{
	if(menuContext.justChanged)
	{
		menuContext.justChanged = false;
		
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
	if(menuContext.justChanged)
	{
		menuContext.justChanged = false;
		
		draw_menu_buttons(MENU_COORDINATES);
		
		if(deviceState.settings.voicedMenus)
		{
			say("Coordinates menu.");
		}
	}
	process_touch_buttons(MENU_COORDINATES);
}

static void tb_null(const uintptr_t functionData)
{
	;
}