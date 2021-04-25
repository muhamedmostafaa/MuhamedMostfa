#include "main_config.h"
#include "LCD.h"
#include "EEPROM.h"
#include "timer_driver.h"
#include "keypad_driver.h"
#include "SPI.h"
#include "LED.h"
#include "menu.h"
#include <avr/io.h>

volatile uint16 session_counter = 0;//indicate session time
uint8 timeout_flag = FALSE;//stores if the session is still valid or outdated

int main(void)
{

	
	uint8 login_mode = NO_MODE; //Store the current user mode admin or guest or not logged in
	uint8 block_mode_flag = FALSE;//is true if the login is blocked or false if not blocked
	uint8 key_pressed = NOT_PRESSED;//
	/*****************  INITIALIZE  ***********************/

	LCD_vInit();//initializes the LCD screen
	keypad_vInit();//initializes the keypad
	SPI_vInitMaster();//initializes the communication protocol of SPI

	/*Setting Admin password if not set */
	//read the state of the the passwords of the admin and if it is set or not set
	if ( (EEPROM_ui8ReadByteFromAddress(ADMIN_PASS_STATUS_ADDRESS)!=PASS_SET)  )
	{
		LCD_vSend_string("Login for");//printing login menu
		LCD_movecursor(2,1);//move the cursor to the second line
		LCD_vSend_string("first time");
		_delay_ms(1000);//Halt the system for the given time in (ms)
		LCD_clearscreen();//remove all previously printed characters on the LCD and move the cursor to the first column of the first row
		LCD_vSend_string("Set Admin pass");//printing the set admin password menu
		LCD_movecursor(2,1);
		LCD_vSend_string("Admin pass:");

		/********************************* setting Admin password **********************************************/
		uint8 pass_counter=0;//the counter of the characters of the password
		uint8 pass[PASS_SIZE]={NOT_STORED,NOT_STORED,NOT_STORED,NOT_STORED};//the array where it stored the password
		while (pass_counter<PASS_SIZE)//loop till the user finish inserting the pass
		{
			key_pressed = NOT_PRESSED;//return the variable that holds the pressed key from keypad to its initial value
			while (key_pressed == NOT_PRESSED)//repeat till the user press any key
			{
				key_pressed = keypad_u8check_press();//if the user pressed any button in keypad save the value in key_pressed
			}

			pass[pass_counter]=key_pressed;//add the entered character to the pass array
			LCD_vSend_char(key_pressed);//print the entered character
			_delay_ms(CHARACTER_PREVIEW_TIME);//Halt the system for the given time in (ms)
			LCD_movecursor(2,12+pass_counter);//move the lcd cursor to the previous location to write the password symbol over the character
			LCD_vSend_char(PASSWORD_SYMBOL); // to display (Password sign *)
			_delay_ms(100);//Halt the system for the given time in (ms)
			pass_counter++;//increase the characters count
		}
		EEPROM_vWriteBlockToAddress(EEPROM_ADMIN_ADDRESS,pass,PASS_SIZE);//save the entire password as a block to the EEPROM
		EEPROM_vWriteByteToAddress(ADMIN_PASS_STATUS_ADDRESS,PASS_SET);//write the status of pass as it is set
		LCD_clearscreen();//remove all previously printed characters on the LCD and move the cursor to the first column of the first row
		LCD_vSend_string("Pass Saved");// show pass saved message
		_delay_ms(500);//Halt the system for the given time in (ms)
		LCD_clearscreen();//remove all previously printed characters on the LCD and move the cursor to the first column of the first row


	}//The end of if admin password is set
	else//this code of else run only if the system is not running for the first time (ADMIN and GUEST passwords are set)
	{
		block_mode_flag = EEPROM_ui8ReadByteFromAddress(LOGIN_BLOCKED_ADDRESS); //read the blocked location from EEPROM
	}
	while (1)//The start of the periodic code
	{
		key_pressed = NOT_PRESSED;//return the variable that holds the pressed key from keypad to its initial value
		uint8 pass_tries_count=0;//stores how many times the user tried to log in to the system and failed
		
		if ( timeout_flag==TRUE )//check for timeout
		{//if timeout flag was raised
			timer0_stop();//stop the timer that increase the session counter
			session_counter = 0;//clear session counter
			timeout_flag=FALSE;//clear time out flag
			login_mode=NO_MODE;//log the user out
			key_pressed = NOT_PRESSED;//clear the key_pressed to avoid unwanted selection in the menu switch

			LCD_clearscreen();//remove all previously printed characters on the LCD and move the cursor to the first column of the first row
			LCD_vSend_string("Session Timeout");//print session timeout message
			_delay_ms(1000);//Halt the system for the given time in (ms)
		}
		while (login_mode==NO_MODE)//The user can only leave the loop only in case of he was logged in as admin
		{
			if(block_mode_flag==TRUE)//if the login process was blocked wait till the end of the block period
			{
				LCD_clearscreen();//remove all previously printed characters on the LCD and move the cursor to the first column of the first row
				LCD_vSend_string("Login blocked");
				LCD_movecursor(2,1);
				LCD_vSend_string("wait 20 seconds");

				_delay_ms(BLOCK_MODE_TIME);//Halt the system for the given time in (ms)
				pass_tries_count = 0; //Clear the count on number of wrong tries
				block_mode_flag = FALSE;//Disable block of runtime
				EEPROM_vWriteByteToAddress(LOGIN_BLOCKED_ADDRESS,FALSE);//write false at blocked location in EEPROM
			}
			LCD_clearscreen();//remove all previously printed characters on the LCD and move the cursor to the first column of the first row
			LCD_vSend_string("Select mode :");
			LCD_movecursor(2,1);
			LCD_vSend_string("0:Admin ");
			while(key_pressed==NOT_PRESSED)//wait for the selection of the mode
			{
				key_pressed = keypad_u8check_press();//if the user pressed any button in keypad save the value in key_pressed
			}
			if ( key_pressed!=CHECK_ADMIN_MODE  )
			{
				LCD_clearscreen();//remove all previously printed characters on the LCD and move the cursor to the first column of the first row
				LCD_vSend_string("Wrong input.");//Prints error message on the LCD
				key_pressed = NOT_PRESSED;//return the variable that holds the pressed key from keypad to its initial value
				_delay_ms(1000);//Halt the system for the given time in (ms)
				continue;//return to the loop of login #while (login_mode==NO_MODE)# line 128
			}

			uint8 pass_counter=0;//counts the entered key of the password from the keypad
			uint8 pass[PASS_SIZE]={NOT_STORED,NOT_STORED,NOT_STORED,NOT_STORED};//temporarily hold the entire password that will be entered by the user to be check
			uint8 stored_pass[PASS_SIZE]={NOT_STORED,NOT_STORED,NOT_STORED,NOT_STORED};//temporarily hold the entire stored password that is written by the user for the first time
			
			switch(key_pressed)
			{
				/********************************* Admin login **********************************************/
				case CHECK_ADMIN_MODE:
				while(login_mode!=ADMIN)//this loop is to repeat the login for admin in case of wrong password
				{
					key_pressed = NOT_PRESSED;//return the variable that holds the pressed key from keypad to its initial value
					LCD_clearscreen();//remove all previously printed characters on the LCD and move the cursor to the first column of the first row
					LCD_vSend_string("Admin mode");
					LCD_movecursor(2,1);
					LCD_vSend_string("Enter Pass:");
					_delay_ms(200);//Halt the system for the given time in (ms)
					pass_counter=0;//counts the number of entered characters
					while(pass_counter<PASS_SIZE)
					{
						while (key_pressed == NOT_PRESSED)//repeat till the user press any key
						{
							key_pressed = keypad_u8check_press();//if the user pressed any button in keypad save the value in key_pressed
						}
						pass[pass_counter]=key_pressed;//add the entered character to the pass array
						LCD_vSend_char(key_pressed);//print the entered character
						_delay_ms(CHARACTER_PREVIEW_TIME);//Halt the system for the given time in (ms)
						LCD_movecursor(2,12+pass_counter);//move the cursor of the lcd screen to the previous location
						LCD_vSend_char(PASSWORD_SYMBOL);// to display (Password sign *)
						_delay_ms(100);//Halt the system for the given time in (ms)
						pass_counter++;//increase the password counter that count the characters of the pass
						key_pressed = NOT_PRESSED;//return the variable that holds the pressed key from keypad to its initial value
					}
					EEPROM_vReadBlockFromAddress(EEPROM_ADMIN_ADDRESS,stored_pass,PASS_SIZE);//read the stored pass from the EEPROM

					/*compare passwords*/
					if ((ui8ComparePass(pass,stored_pass,PASS_SIZE)) == TRUE)//in case of right password
					{
						login_mode = ADMIN;//set the login mode to admin mode
						pass_tries_count=0;//clear the counter of wrong tries
						LCD_clearscreen();//remove all previously printed characters on the LCD and move the cursor to the first column of the first row
						LCD_vSend_string("Right pass");
						LCD_movecursor(2,1);
						LCD_vSend_string("Admin mode");
						_delay_ms(500);//Halt the system for the given time in (ms)
						LED_vTurnOn(ADMIN_LED_PORT,ADMIN_LED_PIN);//turn on the led of admin
						timer0_initializeCTC();//start the timer that counts the session time
						LCD_clearscreen();//remove all previously printed characters on the LCD and move the cursor to the first column of the first row
					}
					else//in case of wrong password
					{
						pass_tries_count++;//increase the number of wrong tries to block login if it exceeds the allowed tries
						login_mode = NO_MODE;//set the mode as not logged in
						LCD_clearscreen();//remove all previously printed characters on the LCD and move the cursor to the first column of the first row
						LCD_vSend_string("Wrong Pass");
						LCD_movecursor(2,1);
						LCD_vSend_string("Tries left:");
						LCD_vSend_char(TRIES_ALLOWED-pass_tries_count+ASCII_ZERO);//print the number of tries left before block mode to be activated
						_delay_ms(1000);//Halt the system for the given time in (ms)
						LCD_clearscreen();//remove all previously printed characters on the LCD and move the cursor to the first column of the first row
						if (pass_tries_count>=TRIES_ALLOWED)//if the condition of the block mode is true
						{
							EEPROM_vWriteByteToAddress(LOGIN_BLOCKED_ADDRESS,TRUE);//write to the EEPROM TRUE to the the block mode address
							block_mode_flag = TRUE;//turn on block mode
							break;//break the loop of admin login #while(login_mode!=ADMIN)# at line 169
						}
					}
				}
				break;//bREAK SWITCH case

			}//end of switch
			
		}
		
		/*************************************************************************************************/
		uint8 show_menu = MAIN_MENU;
		
		
		while(timeout_flag!=TRUE)//Show the menu in case of the time is not out
		{
			key_pressed = NOT_PRESSED;//Set the key pressed by the user to its default value
			switch (show_menu)
			{
				case MAIN_MENU:
				do
				{
					/******************** print main Menu ******************/
					LCD_clearscreen();
					LCD_vSend_string("1:Room1 2:Room2");
					LCD_movecursor(2,1);
					if(login_mode==ADMIN)//check login mode
					{
						LCD_vSend_string("3:Room3 ");//this menu options only printed if the logged in user is an admin
					}
					else //check input
					{
						LCD_vSend_string("Wrong input");//this menu options only printed if the logged in user is a guest
					}
					/*******************************************************/
					
					key_pressed = u8GetKeyPressed(login_mode);//wait for the user till key is pressed or the time is out
					_delay_ms(100);//to avoid the duplication of the pressed key
					
					if (key_pressed == SELECT_ROOM1)//If key pressed is 1
					{
						show_menu = ROOM1_MENU;//Set the next menu to be shown to room1 menu
					}
					else if (key_pressed == SELECT_ROOM2)//If key pressed is 2
					{
						show_menu = ROOM2_MENU;//Set the next menu to be shown to room2 menu
					}
					else if (key_pressed == SELECT_ROOM3)//If key pressed is 3
					{
						show_menu = ROOM3_MENU;//Set the next menu to be shown to room3 menu
					}

					else if(key_pressed != NOT_PRESSED)//show wrong input message if the user pressed wrong key
					{
						LCD_clearscreen();//remove all previously printed characters on the LCD and move the cursor to the first column of the first row
						LCD_vSend_string("Wrong input");//print error message
						_delay_ms(500);//Halt the system for the given time in (ms)
					}
				} while ( ((key_pressed < '1') || (key_pressed > '3') ) && (timeout_flag == FALSE) );//break the loop in case of valid key or time is out
				
				break;//End of main menu case
				
				
				case ROOM1_MENU:
				vMenuOption(ROOM1_MENU,login_mode);//call the function that show the menu of room 1
				show_menu = MAIN_MENU;//Set the next menu to be shown to main menu
				break;//End of room1 menu case
				
				case ROOM2_MENU:
				vMenuOption(ROOM2_MENU,login_mode);//call the function that show the menu of room 2
				show_menu = MAIN_MENU;//Set the next menu to be shown to main menu
				break;//End of room2 menu case
				
				case ROOM3_MENU:
				vMenuOption(ROOM3_MENU,login_mode);//call the function that show the menu of room 3
				show_menu = MAIN_MENU;//Set the next menu to be shown to main menu
				break;//End of room3 menu case
				
				
			}//End of the switch
		}//End of while that repeats the menu after each successful action till session timeout
	}// end of the main while(1)
}//end of main function

ISR(TIMER0_COMP_vect)
{
	session_counter++;//increase the indicator of session time for every tick
}
